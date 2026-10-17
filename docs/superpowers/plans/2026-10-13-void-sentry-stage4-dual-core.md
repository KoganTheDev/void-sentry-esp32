# Stage 4 Dual-Core Task Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Give detection its own priority-3 task on core 1, pin httpd to core 0, and set an explicit priority order. That way network load cannot starve detection.

**Architecture:** FrameStore notifies up to two subscribers. A "detect" task owns `CameraDiffDetection`'s pipeline and processes the newest frame on each notification. The controller only reads snapshots. httpd is pinned to core 0, motor task priority 3 -> 7, camera task priority 2 -> 4.

**Tech Stack:** ESP32 Arduino core 2.0.17 (ESP-IDF 4.4, FreeRTOS SMP), C++17, PlatformIO.

**Spec:** `docs/superpowers/specs/2026-10-13-void-sentry-stage4-dual-core-design.md`

## Global Constraints

- **C++17 only.** No heap on the per-frame path.
- **Locking:** no blocking calls or logging inside portMUX critical sections.
- **Credentials:** never read or print the gitignored WiFi credentials header in `ESP32/include/`. Never run a shell command containing the word that file is named after (a hook blocks it).
- **`pio`:** `~/.platformio/penv/Scripts/pio.exe`, run from `ESP32/`. Never pipe it into `head` / `grep -m1`; log to the scratchpad instead.
- **Native env:** needs WinLibs GCC 14 first on PATH: `export PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin | head -1):$PATH"`.
- **Style:** no em dashes. Device classes use a leading underscore and `this->`. Doxygen `@brief` on headers. clang-format 19 clean.
- **Commits:** trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Board:** do not upload; the controller does hardware runs.

## Review Focus

1. **Shutdown race.** Stopping the stream broadcaster while the detection task stays subscribed must not drop the detection subscription. Removing one subscriber must not touch the other.
2. **Reset ordering.** `reset()` requested while a frame is being processed must take effect before the next frame, never mid-frame.
3. **Lease lifetime.** The detection task must release its lease before blocking again. Otherwise slots run out, which shows up as `dropped_no_slot`.
4. **No CPU spin.** The detection task must not spin when notified without a new frame (same sequence): it must block again.
5. **Motor priority 7 must not starve httpd/IDLE0.** The work per tick stays tiny. The motor loop must be unchanged apart from the priority.

### Task 1: Multi-subscriber FrameStore and detection task

**Files:**
- `ESP32/lib/frame_store/include/frame_store.h`, `src/frame_store.cpp`
- `ESP32/lib/network/src/stream_broadcaster.cpp`
- `ESP32/lib/detection/include/camera_diff_detection.h`, `src/camera_diff_detection.cpp`
- `ESP32/lib/detection/include/base_detection_module.h` (doc only, if needed)
- `ESP32/lib/controller/include/controller.h`, `src/controller.cpp`
- `ESP32/src/main.cpp`, `CLAUDE.md`

- [ ] **FrameStore subscribers.** Replace `set_subscriber(TaskHandle_t)` with `bool add_subscriber(TaskHandle_t)` and `void remove_subscriber(TaskHandle_t)`.
  - Fixed capacity 2, held in `std::atomic<TaskHandle_t>` slots.
  - `add_subscriber` returns false when full; adding an existing handle is a no-op that returns true.
  - Publish calls `xTaskNotifyGive` on every non-null slot.
  - The stream broadcaster uses add/remove at start/stop, replacing `set_subscriber(task)` / `set_subscriber(nullptr)`.
- [ ] **CameraDiffDetection runs its own task.** Add `bool start_task(FrameStore& store)`.
  - Creates the "detect" task: core 1, priority 3, stack 4096, via `xTaskCreatePinnedToCore`. Subscribes it to the store. Idempotent. Returns false on failure. Requires `begin()` first.
  - **Task loop:** `ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200))`. If a reset was requested (`std::atomic<bool>`), reset the detector and tracker and publish an Idle snapshot, as `reset()` does today. Then `acquire_latest()`; if the lease is empty or its sequence equals the last processed one, block again. Otherwise call `detect_object()` with the lease's buffer, as the controller does today, record the sequence, and release the lease before blocking.
  - **`reset()`:** only sets the request flag. Doc: safe from any task; takes effect before the next processed frame.
  - **Stats:** keep the existing fields. Add `uint64_t` totals (`decode_us_total`, `track_us_total`) to `DetectionStats` so main can print per-window values.
- [ ] **Controller.** Remove the FrameStore dependency and the per-frame lease and detect code (constructor, member, includes).
  - **AI mode:** every pass, read the snapshot and run the policy, as today.
  - **Calibrate mode:** reads the snapshot for points, as today; it no longer runs detection itself.
  - Keep `_detection.reset()` on mode toggles; it is now a request.
- [ ] **main.cpp.** Call `detection_manager.start_task(frame_store)` after `detection_manager.begin()` succeeds and the frame pipeline is running; log an error on failure. Update the Controller construction.
  - Stats line: append ` det_frames=%u dec_win_us=%u trk_win_us=%u` at the END, computed per 5 s window from the totals (0 if no frames). Keep every existing field and its order.
- [ ] **CLAUDE.md:** update the Architecture section (task layout table: tasks, cores, priorities) and the stats description.
- [ ] **Verify and commit.**
  - firmware + pin_check builds SUCCESS
  - `pio check --severity=high --severity=medium` clean
  - native suite PASSED
  - clang-format clean

### Task 2: Pin httpd to core 0 and set priorities

**Files:**
- `ESP32/lib/network/src/turret_server.cpp`
- `ESP32/lib/motion/src/turret.cpp` (and the header doc)
- `ESP32/lib/frame_store/src/camera_task.cpp`
- `CLAUDE.md`

- [ ] **httpd:** add `config.core_id = 0;`, with a comment explaining why (keep network work on core 0 so it cannot preempt detection on core 1).
- [ ] **Motor task:** priority 3 -> 7, with a comment: above httpd (5) after the move, still below lwIP 18 / esp_timer 22 / WiFi 23, and about 10-20 us per tick so it is cheap. Update any Doxygen or CLAUDE.md mention of the old priority.
- [ ] **Camera task:** priority 2 -> 4 (above detection 3), with a comment. Update the CLAUDE.md mention of the camera task's priority.
- [ ] **CLAUDE.md:** make the task table consistent.
- [ ] **Verify and commit:** firmware + pin_check builds, check, native, clang-format.

### Task 3 (controller + user): hardware verification

- [ ] Flash. Empty scene with 1 viewer: record `det_frames`, `dec_win_us`, `trk_win_us`, `motor_late`, fps.
- [ ] The user sits close while the turret follows (about 2-3 min): the same numbers per window, against the Stage 3 baseline (25-30 ms).
- [ ] Push, open the PR with the before/after table, and confirm CI is green.
