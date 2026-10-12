# Void Sentry Stage 1: frame ownership and push streaming

- **Date:** 2026-10-12
- **Status:** design approved, spec under review
- **Branch:** `feature/stage1-camera-stream`
- **Roadmap:** Stage 1 in `docs/superpowers/specs/2026-10-11-void-sentry-stage0-design.md`
- **Scope change from the roadmap:** on-device WiFi provisioning, originally part of Stage 1, has been **split out** into its own later design discussion. `secrets.h` stays in place for now.

## Goal

Live video in the HUD with **low latency** (the user's priority over smoothness or image quality), at QVGA 320x240. Camera frames have one clear owner, so the stream can never read freed memory, and no memory is allocated per frame.

**Success means:**
- The HUD shows live video with latency low enough to aim by, at roughly 15-25 fps on the home network.
- No crashes or corrupted frames during long streaming sessions.
- Zero heap allocations per frame. All frame memory is allocated once at boot.
- The new logic (`FrameSlots`, `ViewerSet`) is unit-tested natively.

## Problems this fixes

1. **Use-after-free race.**
   - **Today:** the HTTP server runs as its own FreeRTOS task. `StreamWebsocketHandler` (`ESP32/lib/network/include/websockets.h`) sends the bytes of `Camera::_buffer` directly.
   - **Meanwhile:** `loop()` calls `camera.capture()` (`ESP32/lib/detection/src/camera.cpp`), which frees that same buffer and replaces it.
   - **Result:** the stream can send freed memory.
2. **Allocation on every frame.** `capture()` calls `malloc` for each frame and never checks the result. cppcheck reports this as `nullPointerOutOfMemory`.
3. **Polling stream.**
   - The browser sends `" "` and gets one JPEG back.
   - It then waits 40 ms (`setTimeout`) before asking again, so every frame pays a full round trip and the rate is capped near 25 fps.
   - Each frame is converted to Base64 to display it.

## Design

### 1. `FrameSlots`: slot bookkeeping (pure C++17, native-tested)

**Location:** `ESP32/lib/frames/include/frame_slots.h`. It doesn't include Arduino or ESP-IDF headers.

**Slots:**
- There are `kSlotCount = 4` slots.
- Each slot is in one of these states: **free**, **writing**, or **published**. A published slot carries a reader count, and one published slot is the **latest**.
- Each slot also carries metadata: `length`, `width`, `height`, `sequence` (a monotonically increasing frame number) and `timestamp_ms`.

**Writer API:**
- `std::optional<size_t> begin_write()` returns a slot that is not the latest, has no readers, and is not already being written.
  - It returns `std::nullopt` when none qualifies. The caller then drops the frame, and `FrameSlots` counts the drop.
- `void publish(size_t slot, FrameMeta meta)` marks the slot published, assigns the next `sequence`, and makes it the latest.
- `void abort_write(size_t slot)` returns a slot to free. The writer uses it when a frame is oversized or the capture failed.

**Reader API:**
- `std::optional<FrameLease> acquire_latest()` increments the latest slot's reader count and returns a lease. It returns `nullopt` before the first publish.
- **`FrameLease`** is a move-only RAII handle.
  - It holds the slot index and a copy of the metadata.
  - Its destructor (or `release()`) decrements the reader count. A moved-from lease releases nothing.
- **When a slot becomes reusable:** a slot that is published but no longer the latest becomes free once its reader count drops to 0.

**Stats:** `frames_published()`, `frames_dropped_no_slot()`, and `frames_dropped_oversize()`. The writer reports the oversize case through `abort_write(slot, Reason::Oversize)`.

**Threading:** `FrameSlots` is **not** thread-safe on its own. `FrameStore` serializes every call.

**Why 4 slots:** one being written, one latest, and up to two readers holding older frames at once (detection plus a viewer, or two viewers). If all of them are held, the newest frame is dropped rather than overwriting a frame that's in use.

### 2. `FrameStore`: device wrapper

**Location:** `ESP32/lib/frames/` (`frame_store.h` / `.cpp`).

- **Buffers:** it owns `kSlotCount` buffers of `kSlotBytes = 32 * 1024`, allocated **once** in PSRAM with `heap_caps_malloc(MALLOC_CAP_SPIRAM)` by `begin()`. `begin()` returns `false` if any allocation fails. A QVGA JPEG at quality 12 is typically 10-20 KB.
- **Locking:** a FreeRTOS mutex wraps every `FrameSlots` call. The lock is held only for bookkeeping, never while copying pixels or sending over the network.
- **Data access:** `const uint8_t* data(const FrameLease&)` gives access to a leased slot's bytes.
- **New-frame signal:** `publish` notifies a registered task (the stream task) with a FreeRTOS task notification, so subscribers wake on new frames instead of polling.

### 3. Camera task

- **What `Camera` keeps:** setup (`begin()`, the sensor config) only. `capture()`, `_buffer` and the per-frame `malloc`/`free` are removed.
- **New task:** a FreeRTOS task (`camera_task`, about 4 KB stack, core 1, priority 2, one above `loop()`) runs this loop:
  1. `esp_camera_fb_get()`
  2. `begin_write()`. If there's no free slot, count a drop and skip to step 5.
  3. If the frame is bigger than `kSlotBytes`, call `abort_write(Oversize)` and skip to step 5.
  4. Copy the frame into the slot, then `publish`.
  5. `esp_camera_fb_return()` **immediately**.
- If the camera fails to initialize, the task isn't started, an error is logged, and the HTTP server still starts.

### 4. Controller integration

- **`loop()`** no longer captures. `controller.run()` calls `frame_store.acquire_latest()` and, if it gets a lease, passes a `camera_buffer_t` view of that slot to the detection module. The lease is released at the end of `run()`.
- **Detection** stays the existing no-op until Stage 2.
- **Joystick** mode toggling is unchanged.

### 5. `ViewerSet`: viewer bookkeeping (pure C++17, native-tested)

**Location:** `ESP32/lib/frames/include/viewer_set.h`. It doesn't include Arduino headers.

**What it tracks:** up to `kMaxViewers = 2` viewers, identified by socket fd. Each has a `busy` flag and `last_sequence_sent`.

**API:**
- `bool add(int fd)` returns `false` when the set is full; the caller then closes the new socket.
- `void remove(int fd)`.
- `std::vector<int> ready_for(uint32_t sequence)` returns the fds that are not busy and have `last_sequence_sent < sequence`. It uses a fixed-size array internally and returns at most `kMaxViewers` fds.
- `void mark_sending(int fd, uint32_t sequence)` and `void mark_done(int fd)`.

### 6. Push streaming

- **Connecting:** on the `/stream` WebSocket handshake, the server adds the client's fd to the `ViewerSet`. Client messages on `/stream` are ignored, because the board pushes and the client no longer pulls.
- **Stream task** (core 1, priority 1): it waits for the `FrameStore` new-frame notification, then for each fd in `ready_for(latest sequence)`:
  1. Acquire a lease.
  2. `mark_sending`.
  3. `httpd_queue_work()` a send job.

  The job runs inside the HTTP server's own task, calls `httpd_ws_send_frame_async()` with a binary frame, then calls `mark_done` and releases the lease.
- **Failures:** a failed send removes the viewer and closes its session.
- **Low latency comes from skipping:** a viewer that is still receiving is skipped, so it never builds a backlog and always gets the newest frame next.
- **`ViewerSet` thread safety:** it's touched by both the stream task and the HTTP server task, so the stream module protects it with a mutex.

### 7. Browser HUD (`web/web_ui.html`)

- **`/stream` socket:**
  - Set `binaryType = "blob"`.
  - On each message, show the frame with `URL.createObjectURL(blob)` and revoke the previous URL.
  - Remove the `" "` request and the 40 ms `setTimeout`, and stop converting frames to Base64.
- **Commands socket:** unchanged.
- **WebSocket URL:** fix it to `ws://${location.host}/stream`. It currently has a single slash.
- **FPS:** show the received fps in the HUD, computed client-side over a 1 s window.

### 8. Diagnostics

Every 5 seconds the firmware logs one line:

`[stream] captured=N sent=N dropped_no_slot=N oversize=N viewers=N`

It uses `ESP_LOGI` and never prints credentials.

## Testing

- **Native (`pio test -e native`), `FrameSlots`:**
  - publish → acquire returns the newest frame
  - a leased slot is never handed to `begin_write`
  - dropped when every slot is held
  - a lease's move semantics release exactly once
  - an aborted write returns its slot
  - sequences increase monotonically
  - drop counters
- **Native, `ViewerSet`:**
  - capacity limit
  - busy viewers are skipped
  - a viewer never gets the same sequence twice
  - removing a busy viewer
- **Build:** both firmware envs build, and CI stays green.
- **On hardware:**
  - The HUD shows live video.
  - The serial stats show `captured` and `sent` increasing with no growth in heap usage. The stats line also logs `ESP.getFreeHeap()`.
  - Two browser tabs at once both stream.
  - Closing a tab mid-stream is handled cleanly.
  - 10 minutes of streaming with no reset.

## Non-goals

- WiFi provisioning (deferred to its own design)
- Motors and detection (Stages 2-3)
- Splitting the work properly across both cores (Stage 4)
- More than two simultaneous viewers
- Authentication on the HUD
