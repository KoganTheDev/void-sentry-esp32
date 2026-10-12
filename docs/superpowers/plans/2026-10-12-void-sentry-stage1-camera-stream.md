# Void Sentry Stage 1 (Camera + Push Stream) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Camera frames get one clear owner, with fixed PSRAM slots and leases. That removes the stream's use-after-free race and the per-frame `malloc`. The HUD gets low-latency video pushed over the existing `/stream` WebSocket, with benchmark counters, and CI gains an ASan+UBSan run of the native tests.

**Architecture:**
- **Pure, natively tested C++17:** `lib/frames` holds the slot bookkeeping (`FrameSlots` + RAII `FrameLease`), viewer bookkeeping (`ViewerSet`) and a JPEG completeness check.
- **Device code:**
  - `lib/frame_store` wraps the bookkeeping with a FreeRTOS mutex and PSRAM buffers, and runs the camera task.
  - `lib/network`'s new `StreamBroadcaster` pushes the latest frame to each idle viewer through `httpd_queue_work` + `httpd_ws_send_frame_async`.
- **HUD:** shows blobs directly and counts fps and decode errors.

**Tech Stack:** PlatformIO (`espressif32@6.12.0`, Arduino core 2.0.17, ESP-IDF 4.4 `esp_http_server`, FreeRTOS), C++17, Unity (native), GitHub Actions, vanilla JS in `web/web_ui.html`.

**Spec:** `docs/superpowers/specs/2026-10-12-void-sentry-stage1-camera-stream-design.md`. Read it fully. Its "Benchmarks" table defines how the hardware results are judged.

## Global Constraints

- C++17 only. The firmware toolchain is GCC 8.4, so no C++20 features.
- **Code in `ESP32/lib/frames/` must not include Arduino, ESP-IDF or FreeRTOS headers**, because the native and native_sanitize envs compile it on a PC.
- No heap allocation per frame anywhere on the frame path: camera task, `FrameStore`, `StreamBroadcaster`. All frame memory is allocated once in `FrameStore::begin()`.
- Constants, verbatim from the spec: `kSlotCount = 4`, `FrameStore::kSlotBytes = 32 * 1024`, `kMaxViewers = 2`. Camera task: 4096-byte stack, core 1, priority 2. Stream task: 4096-byte stack, core 1, priority 1. Stats line every 5 s.
- The stats line format, verbatim from the spec: `[stream] up=Ss captured=N sent=N dropped_no_slot=N oversize=N corrupt=N viewers=N copy_us_avg=N heap=N heap_min=N psram=N psram_min=N`
- Run PlatformIO from `ESP32/` as `~/.platformio/penv/Scripts/pio.exe`. **Never pipe `pio` into a command that exits early** (`grep -m1`, `head`): it hangs on Windows. Redirect to a log file and grep the file.
- Native tests need GCC 14 first on PATH: `export PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT.LLVM*/mingw64/bin | head -1):$PATH"`
- Never read, edit, copy or print `ESP32/include/secrets.h`. A hook blocks shell commands containing the substring `secret`. Files that mention it are written with the Write tool, and commit messages that mention it use `git commit -F <file>`.
- A `PostToolUse` hook runs clang-format 19 on edited firmware C/C++ files. Don't fight its output.
- No em dashes in any text. Use " - ".
- Every commit message ends with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Branch `feature/stage1-camera-stream`. Only Task 7 pushes.

## Review Focus

1. **A viewer closes the tab while a frame is being sent to it.** The send job must still release its lease, the viewer must be removed, and the slot must become reusable, with no slot leak and no crash. Pinned by `test_remove_busy_viewer_frees_capacity` (Task 2) and Task 7, Step 4 (close a tab mid-stream, then confirm `dropped_no_slot` doesn't climb).
2. **Every slot is held** (slow viewer plus controller). New frames must be dropped and counted, never written into a leased slot. Pinned by `test_drops_when_every_slot_is_held` and `test_leased_slot_is_never_rewritten` (Task 1).
3. **A lease is moved, then both handles go out of scope.** The slot must be released exactly once. Pinned by `test_lease_move_releases_exactly_once` and `test_lease_move_assignment_releases_old_slot` (Task 1).
4. **A third browser tab connects.** It must be rejected cleanly while the first two keep streaming. Pinned by `test_add_until_full` (Task 2) and Task 7, Step 4 (open 3 tabs).
5. **The camera driver pads JPEG data after the end marker.** If it does, every frame fails `is_complete_jpeg`, the `corrupt` counter equals `captured`, and the HUD gets no video. Pinned by Task 7, Step 3, which reads `corrupt` on the real board. If it's non-zero, stop and report it so the controller can rule on a tolerant check.

---

## File structure

| File | Status | Responsibility |
|---|---|---|
| `ESP32/lib/frames/include/jpeg_check.h` | Create | `is_complete_jpeg` (pure) |
| `ESP32/lib/frames/include/frame_slots.h` | Create | `FrameMeta`, `AbortReason`, `SlotReleaser`, `FrameLease`, `FrameSlots` (pure) |
| `ESP32/lib/frames/include/viewer_set.h` | Create | `ReadyList`, `ViewerSet` (pure) |
| `ESP32/test/native/test_frames/test_frames.cpp` | Create | Tests for `jpeg_check` and `frame_slots` |
| `ESP32/test/native/test_viewer_set/test_viewer_set.cpp` | Create | Tests for `ViewerSet` |
| `ESP32/scripts/sanitize_link.py` | Create | Adds sanitizer flags at link time for `native_sanitize` |
| `ESP32/platformio.ini` | Modify | Add `[env:native_sanitize]` |
| `.github/workflows/ci.yml` | Modify | Add `native-sanitize` job and a `workflow_dispatch` trigger |
| `ESP32/lib/frame_store/include/frame_store.h`, `src/frame_store.cpp` | Create | `FrameStore`: mutex + PSRAM buffers around `FrameSlots` |
| `ESP32/lib/frame_store/include/camera_task.h`, `src/camera_task.cpp` | Create | Camera FreeRTOS task + `CameraTaskStats` |
| `ESP32/lib/detection/include/camera.h`, `src/camera.cpp` | Modify | Remove `capture()`, `_buffer` and the per-frame malloc; keep setup and the `camera_buffer_t` type |
| `ESP32/lib/controller/include/controller.h`, `src/controller.cpp` | Modify | Read frames through a `FrameStore` lease instead of `Camera` |
| `ESP32/lib/network/include/stream_broadcaster.h`, `src/stream_broadcaster.cpp` | Create | `/stream` WebSocket push with drop-when-busy |
| `ESP32/lib/network/include/websockets.h` | Modify | Delete `StreamWebsocketHandler` (the commands handler stays) |
| `ESP32/lib/network/include/turret_server.h`, `src/turret_server.cpp` | Modify | Start the broadcaster, set `close_fn`, expose stream stats |
| `ESP32/src/main.cpp` | Modify | Wire everything together, drop `capture()` from `loop()`, add the stats line |
| `web/web_ui.html` | Modify | Blob display, fps, decode-error counter, `ws://` URL fix |
| `CLAUDE.md` | Modify | Architecture notes for the new frame path |

---

### Task 1: `FrameSlots`, `FrameLease`, `is_complete_jpeg` (model: Haiku)

**Files:**
- Create: `ESP32/lib/frames/include/jpeg_check.h`, `ESP32/lib/frames/include/frame_slots.h`
- Test: `ESP32/test/native/test_frames/test_frames.cpp`

**Interfaces:**
- Produces (namespace `frames`): `kSlotCount`, `FrameMeta{length,width,height,timestamp_ms,sequence}`, `enum class AbortReason{Oversize,Corrupt,CaptureFailed}`, `class SlotReleaser{virtual void release_slot(size_t)=0;}`, `class FrameLease` (move-only, `slot()`, `meta()`, `valid()`, `release()`), `class FrameSlots : SlotReleaser` (`begin_write()`, `publish(slot, meta)`, `abort_write(slot, reason)`, `acquire_latest()`, `acquire_latest(SlotReleaser&)`, `release_slot(slot)`, `frames_published()`, `frames_dropped_no_slot()`, `frames_dropped_oversize()`, `frames_dropped_corrupt()`), and `bool is_complete_jpeg(const uint8_t*, size_t)`. Task 4 consumes all of them.

- [ ] **Step 1: Write the failing tests**

Create `ESP32/test/native/test_frames/test_frames.cpp`:

```cpp
/**
 * @file test_frames.cpp
 * @brief Host-side tests for the frame slot bookkeeping, leases and the JPEG completeness check.
 */

#include <unity.h>
#include <utility>

#include "frame_slots.h"
#include "jpeg_check.h"

using namespace frames;

void setUp(void) {}
void tearDown(void) {}

namespace
{
FrameMeta meta_of(size_t length)
{
    FrameMeta m;
    m.length = length;
    m.width = 320;
    m.height = 240;
    return m;
}

// Publishes one frame and returns the slot it went into.
size_t publish_one(FrameSlots& slots, size_t length = 100)
{
    auto slot = slots.begin_write();
    TEST_ASSERT_TRUE(slot.has_value());
    slots.publish(*slot, meta_of(length));
    return *slot;
}

struct CountingReleaser : SlotReleaser
{
    int calls = 0;
    size_t last_slot = 99;
    void release_slot(size_t slot) override
    {
        ++calls;
        last_slot = slot;
    }
};
} // namespace

// --- is_complete_jpeg ---

void test_jpeg_accepts_soi_and_eoi(void)
{
    const uint8_t frame[] = {0xFF, 0xD8, 0x00, 0x11, 0xFF, 0xD9};
    const uint8_t minimal[] = {0xFF, 0xD8, 0xFF, 0xD9};
    TEST_ASSERT_TRUE(is_complete_jpeg(frame, sizeof(frame)));
    TEST_ASSERT_TRUE(is_complete_jpeg(minimal, sizeof(minimal)));
}

void test_jpeg_rejects_null_and_short(void)
{
    const uint8_t data[] = {0xFF, 0xD8, 0xFF};
    TEST_ASSERT_FALSE(is_complete_jpeg(nullptr, 10));
    TEST_ASSERT_FALSE(is_complete_jpeg(data, 0));
    TEST_ASSERT_FALSE(is_complete_jpeg(data, 3));
}

void test_jpeg_rejects_missing_soi(void)
{
    const uint8_t data[] = {0x00, 0xD8, 0x11, 0xFF, 0xD9};
    TEST_ASSERT_FALSE(is_complete_jpeg(data, sizeof(data)));
}

void test_jpeg_rejects_truncated_frame(void)
{
    const uint8_t data[] = {0xFF, 0xD8, 0x11, 0x22, 0x33};
    TEST_ASSERT_FALSE(is_complete_jpeg(data, sizeof(data)));
}

// --- FrameSlots ---

void test_acquire_before_publish_is_empty(void)
{
    FrameSlots slots;
    TEST_ASSERT_FALSE(slots.acquire_latest().has_value());
}

void test_acquire_returns_latest_frame(void)
{
    FrameSlots slots;
    publish_one(slots, 100);
    size_t newest = publish_one(slots, 200);
    auto lease = slots.acquire_latest();
    TEST_ASSERT_TRUE(lease.has_value());
    TEST_ASSERT_EQUAL_UINT(newest, lease->slot());
    TEST_ASSERT_EQUAL_UINT(200, lease->meta().length);
    TEST_ASSERT_EQUAL_UINT32(2, lease->meta().sequence);
}

void test_sequences_increase(void)
{
    FrameSlots slots;
    for (uint32_t i = 1; i <= 10; ++i)
    {
        publish_one(slots);
        auto lease = slots.acquire_latest();
        TEST_ASSERT_EQUAL_UINT32(i, lease->meta().sequence);
    }
    TEST_ASSERT_EQUAL_UINT32(10, slots.frames_published());
}

void test_leased_slot_is_never_rewritten(void)
{
    FrameSlots slots;
    size_t held_slot = publish_one(slots);
    auto held = slots.acquire_latest();
    for (int i = 0; i < 20; ++i)
    {
        auto slot = slots.begin_write();
        TEST_ASSERT_TRUE(slot.has_value());
        TEST_ASSERT_NOT_EQUAL(held_slot, *slot);
        slots.publish(*slot, meta_of(50));
    }
    TEST_ASSERT_EQUAL_UINT(held_slot, held->slot());
}

void test_drops_when_every_slot_is_held(void)
{
    FrameSlots slots;
    publish_one(slots);
    auto a = slots.acquire_latest();
    publish_one(slots);
    auto b = slots.acquire_latest();
    publish_one(slots);
    auto c = slots.acquire_latest();
    auto writing = slots.begin_write();
    TEST_ASSERT_TRUE(writing.has_value());
    TEST_ASSERT_FALSE(slots.begin_write().has_value());
    TEST_ASSERT_EQUAL_UINT32(1, slots.frames_dropped_no_slot());
}

void test_released_stale_slot_becomes_free(void)
{
    FrameSlots slots;
    size_t first = publish_one(slots);
    auto held = slots.acquire_latest();
    auto w1 = slots.begin_write();
    auto w2 = slots.begin_write();
    auto w3 = slots.begin_write();
    TEST_ASSERT_TRUE(w1.has_value() && w2.has_value() && w3.has_value());
    slots.publish(*w1, meta_of(10)); // `first` is now stale but still leased
    TEST_ASSERT_FALSE(slots.begin_write().has_value());
    held->release();
    auto next = slots.begin_write();
    TEST_ASSERT_TRUE(next.has_value());
    TEST_ASSERT_EQUAL_UINT(first, *next);
}

void test_lease_move_releases_exactly_once(void)
{
    CountingReleaser releaser;
    {
        FrameLease a(releaser, 2, meta_of(10));
        FrameLease b(std::move(a));
        TEST_ASSERT_FALSE(a.valid());
        TEST_ASSERT_TRUE(b.valid());
    }
    TEST_ASSERT_EQUAL_INT(1, releaser.calls);
    TEST_ASSERT_EQUAL_UINT(2, releaser.last_slot);
}

void test_lease_move_assignment_releases_old_slot(void)
{
    CountingReleaser releaser;
    FrameLease a(releaser, 1, meta_of(10));
    FrameLease b(releaser, 3, meta_of(10));
    b = std::move(a);
    TEST_ASSERT_EQUAL_INT(1, releaser.calls);
    TEST_ASSERT_EQUAL_UINT(3, releaser.last_slot);
    b.release();
    TEST_ASSERT_EQUAL_INT(2, releaser.calls);
    TEST_ASSERT_EQUAL_UINT(1, releaser.last_slot);
    b.release();
    TEST_ASSERT_EQUAL_INT(2, releaser.calls);
}

void test_abort_frees_slot_and_counts_reason(void)
{
    FrameSlots slots;
    auto w = slots.begin_write();
    slots.abort_write(*w, AbortReason::Oversize);
    w = slots.begin_write();
    slots.abort_write(*w, AbortReason::Corrupt);
    w = slots.begin_write();
    slots.abort_write(*w, AbortReason::CaptureFailed);
    TEST_ASSERT_EQUAL_UINT32(1, slots.frames_dropped_oversize());
    TEST_ASSERT_EQUAL_UINT32(1, slots.frames_dropped_corrupt());
    for (size_t i = 0; i < kSlotCount; ++i)
    {
        TEST_ASSERT_TRUE(slots.begin_write().has_value());
    }
    TEST_ASSERT_EQUAL_UINT32(0, slots.frames_dropped_no_slot());
    TEST_ASSERT_FALSE(slots.acquire_latest().has_value());
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_jpeg_accepts_soi_and_eoi);
    RUN_TEST(test_jpeg_rejects_null_and_short);
    RUN_TEST(test_jpeg_rejects_missing_soi);
    RUN_TEST(test_jpeg_rejects_truncated_frame);
    RUN_TEST(test_acquire_before_publish_is_empty);
    RUN_TEST(test_acquire_returns_latest_frame);
    RUN_TEST(test_sequences_increase);
    RUN_TEST(test_leased_slot_is_never_rewritten);
    RUN_TEST(test_drops_when_every_slot_is_held);
    RUN_TEST(test_released_stale_slot_becomes_free);
    RUN_TEST(test_lease_move_releases_exactly_once);
    RUN_TEST(test_lease_move_assignment_releases_old_slot);
    RUN_TEST(test_abort_frees_slot_and_counts_reason);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

From `ESP32/`, with the GCC 14 PATH prefix:

`~/.platformio/penv/Scripts/pio.exe test -e native -f native/test_frames > ../t.log 2>&1; grep -E "error|FAILED|PASSED" ../t.log`

Expected: a compile error, `frame_slots.h: No such file or directory`.

- [ ] **Step 3: Implement `jpeg_check.h`**

```cpp
/**
 * @file jpeg_check.h
 * @brief Cheap integrity check for camera JPEG frames (pure C++, no platform headers).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace frames
{
/**
 * @brief True if the buffer starts with the JPEG SOI marker (FF D8) and ends with the EOI marker (FF D9).
 * @details A frame cut off mid-transfer has no EOI and fails this check.
 */
inline bool is_complete_jpeg(const uint8_t* data, size_t length)
{
    if (data == nullptr || length < 4)
    {
        return false;
    }
    return data[0] == 0xFF && data[1] == 0xD8 && data[length - 2] == 0xFF && data[length - 1] == 0xD9;
}
} // namespace frames
```

- [ ] **Step 4: Implement `frame_slots.h`**

```cpp
/**
 * @file frame_slots.h
 * @brief Slot bookkeeping for camera frames: who may write which slot, and RAII leases for readers.
 * @details Pure C++17, no platform headers, so it is unit tested natively (with ASan/UBSan in CI).
 * NOT thread-safe on its own: on the device, FrameStore serializes every call with a mutex.
 *
 * A slot is Free, Writing or Published. One Published slot is the latest. A Published slot that is no longer
 * the latest becomes Free when its last reader releases it, so a slot with readers is never rewritten.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace frames
{
constexpr size_t kSlotCount = 4; // one writing + one latest + up to two readers holding older frames

struct FrameMeta
{
    size_t length = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t timestamp_ms = 0;
    uint32_t sequence = 0; // assigned by FrameSlots::publish()
};

enum class AbortReason : uint8_t
{
    Oversize,
    Corrupt,
    CaptureFailed
};

/** @brief Whatever a FrameLease must notify when it is released (FrameSlots natively, FrameStore on device). */
class SlotReleaser
{
public:
    virtual void release_slot(size_t slot) = 0;

protected:
    ~SlotReleaser() = default;
};

/** @brief Move-only handle on a published frame. Releases its slot exactly once, on release() or destruction. */
class FrameLease
{
public:
    FrameLease(SlotReleaser& owner, size_t slot, const FrameMeta& meta) : owner_(&owner), slot_(slot), meta_(meta) {}

    FrameLease(const FrameLease&) = delete;
    FrameLease& operator=(const FrameLease&) = delete;

    FrameLease(FrameLease&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)), slot_(other.slot_), meta_(other.meta_)
    {
    }

    FrameLease& operator=(FrameLease&& other) noexcept
    {
        if (this != &other)
        {
            release();
            owner_ = std::exchange(other.owner_, nullptr);
            slot_ = other.slot_;
            meta_ = other.meta_;
        }
        return *this;
    }

    ~FrameLease() { release(); }

    void release()
    {
        if (owner_ != nullptr)
        {
            owner_->release_slot(slot_);
            owner_ = nullptr;
        }
    }

    size_t slot() const { return slot_; }
    const FrameMeta& meta() const { return meta_; }
    bool valid() const { return owner_ != nullptr; }

private:
    SlotReleaser* owner_;
    size_t slot_;
    FrameMeta meta_;
};

class FrameSlots : public SlotReleaser
{
public:
    /** @brief A slot that is not latest, not leased and not being written; nullopt (and a counted drop) if none. */
    std::optional<size_t> begin_write()
    {
        for (size_t i = 0; i < kSlotCount; ++i)
        {
            if (slots_[i].state == State::Free)
            {
                slots_[i].state = State::Writing;
                return i;
            }
        }
        ++dropped_no_slot_;
        return std::nullopt;
    }

    /** @brief Make a written slot the latest frame. Assigns the next sequence number. */
    void publish(size_t slot, FrameMeta meta)
    {
        meta.sequence = ++sequence_;
        slots_[slot].state = State::Published;
        slots_[slot].meta = meta;
        if (latest_ && *latest_ != slot && slots_[*latest_].readers == 0)
        {
            slots_[*latest_].state = State::Free;
        }
        latest_ = slot;
        ++published_;
    }

    /** @brief Give up a slot taken by begin_write(). Oversize and Corrupt are counted. */
    void abort_write(size_t slot, AbortReason reason)
    {
        slots_[slot].state = State::Free;
        if (reason == AbortReason::Oversize)
        {
            ++dropped_oversize_;
        } else if (reason == AbortReason::Corrupt)
        {
            ++dropped_corrupt_;
        }
    }

    std::optional<FrameLease> acquire_latest() { return acquire_latest(*this); }

    /** @brief Lease the latest frame; the lease will call releaser.release_slot() when done. */
    std::optional<FrameLease> acquire_latest(SlotReleaser& releaser)
    {
        if (!latest_)
        {
            return std::nullopt;
        }
        Slot& slot = slots_[*latest_];
        ++slot.readers;
        return FrameLease(releaser, *latest_, slot.meta);
    }

    void release_slot(size_t slot) override
    {
        Slot& s = slots_[slot];
        if (s.readers > 0)
        {
            --s.readers;
        }
        if (s.readers == 0 && s.state == State::Published && latest_ != slot)
        {
            s.state = State::Free;
        }
    }

    uint32_t frames_published() const { return published_; }
    uint32_t frames_dropped_no_slot() const { return dropped_no_slot_; }
    uint32_t frames_dropped_oversize() const { return dropped_oversize_; }
    uint32_t frames_dropped_corrupt() const { return dropped_corrupt_; }

private:
    enum class State : uint8_t
    {
        Free,
        Writing,
        Published
    };

    struct Slot
    {
        State state = State::Free;
        uint8_t readers = 0;
        FrameMeta meta;
    };

    std::array<Slot, kSlotCount> slots_{};
    std::optional<size_t> latest_;
    uint32_t sequence_ = 0;
    uint32_t published_ = 0;
    uint32_t dropped_no_slot_ = 0;
    uint32_t dropped_oversize_ = 0;
    uint32_t dropped_corrupt_ = 0;
};
} // namespace frames
```

- [ ] **Step 5: Run to verify it passes**

`~/.platformio/penv/Scripts/pio.exe test -e native > ../t.log 2>&1; grep -E "test cases|FAIL" ../t.log; rm ../t.log`

Expected: `22 test cases: 22 succeeded`, which is 13 new plus the 9 existing in `test_types`.

- [ ] **Step 6: Commit**

```bash
git add ESP32/lib/frames ESP32/test/native/test_frames
git commit -m "Add FrameSlots, FrameLease and JPEG completeness check" -m "Pure C++17 slot bookkeeping for camera frames with RAII leases; natively tested.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: `ViewerSet` (model: Haiku)

**Files:**
- Create: `ESP32/lib/frames/include/viewer_set.h`
- Test: `ESP32/test/native/test_viewer_set/test_viewer_set.cpp`

**Interfaces:**
- Produces (namespace `frames`): `kMaxViewers = 2`, `struct ReadyList{std::array<int,kMaxViewers> fds; size_t count;}`, `class ViewerSet` with `add(int fd) -> bool`, `remove(int fd)`, `ready_for(uint32_t sequence) const -> ReadyList`, `mark_sending(int fd, uint32_t sequence)`, `mark_done(int fd)`, `index_of(int fd) const -> std::optional<size_t>`, `size() const -> size_t`. Task 5 consumes all of them.

- [ ] **Step 1: Write the failing tests**

Create `ESP32/test/native/test_viewer_set/test_viewer_set.cpp`:

```cpp
/**
 * @file test_viewer_set.cpp
 * @brief Host-side tests for stream viewer bookkeeping (capacity, busy skipping, no duplicate frames).
 */

#include <unity.h>

#include "viewer_set.h"

using namespace frames;

void setUp(void) {}
void tearDown(void) {}

void test_add_until_full(void)
{
    ViewerSet viewers;
    TEST_ASSERT_TRUE(viewers.add(10));
    TEST_ASSERT_TRUE(viewers.add(11));
    TEST_ASSERT_FALSE(viewers.add(12));
    TEST_ASSERT_EQUAL_UINT(2, viewers.size());
}

void test_add_twice_is_idempotent(void)
{
    ViewerSet viewers;
    TEST_ASSERT_TRUE(viewers.add(10));
    TEST_ASSERT_TRUE(viewers.add(10));
    TEST_ASSERT_EQUAL_UINT(1, viewers.size());
}

void test_ready_for_skips_busy_viewer(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.add(11);
    viewers.mark_sending(10, 5);
    ReadyList ready = viewers.ready_for(6);
    TEST_ASSERT_EQUAL_UINT(1, ready.count);
    TEST_ASSERT_EQUAL_INT(11, ready.fds[0]);
}

void test_never_sends_same_sequence_twice(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.mark_sending(10, 5);
    viewers.mark_done(10);
    TEST_ASSERT_EQUAL_UINT(0, viewers.ready_for(5).count);
    TEST_ASSERT_EQUAL_UINT(1, viewers.ready_for(6).count);
}

void test_remove_busy_viewer_frees_capacity(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.add(11);
    viewers.mark_sending(10, 3);
    viewers.remove(10);
    TEST_ASSERT_EQUAL_UINT(1, viewers.size());
    TEST_ASSERT_FALSE(viewers.index_of(10).has_value());
    TEST_ASSERT_TRUE(viewers.add(12));
}

void test_index_of(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.add(11);
    TEST_ASSERT_EQUAL_UINT(1, *viewers.index_of(11));
    TEST_ASSERT_FALSE(viewers.index_of(99).has_value());
}

void test_marks_on_unknown_fd_are_ignored(void)
{
    ViewerSet viewers;
    viewers.mark_sending(99, 1);
    viewers.mark_done(99);
    viewers.remove(99);
    TEST_ASSERT_EQUAL_UINT(0, viewers.size());
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_add_until_full);
    RUN_TEST(test_add_twice_is_idempotent);
    RUN_TEST(test_ready_for_skips_busy_viewer);
    RUN_TEST(test_never_sends_same_sequence_twice);
    RUN_TEST(test_remove_busy_viewer_frees_capacity);
    RUN_TEST(test_index_of);
    RUN_TEST(test_marks_on_unknown_fd_are_ignored);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

`~/.platformio/penv/Scripts/pio.exe test -e native -f native/test_viewer_set > ../t.log 2>&1; grep -E "error|FAILED|PASSED" ../t.log`

Expected: a compile error, `viewer_set.h: No such file or directory`.

- [ ] **Step 3: Implement `viewer_set.h`**

```cpp
/**
 * @file viewer_set.h
 * @brief Which stream viewers should receive which frame (pure C++17, natively tested).
 * @details A viewer that is still receiving (busy) is skipped, so it never builds a backlog and always gets the
 * newest frame next - this is what keeps stream latency low. NOT thread-safe; StreamBroadcaster locks around it.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace frames
{
constexpr size_t kMaxViewers = 2;

/** @brief Fixed-capacity result so the stream path never allocates per frame. */
struct ReadyList
{
    std::array<int, kMaxViewers> fds{};
    size_t count = 0;
};

class ViewerSet
{
public:
    /** @brief Add a viewer by socket fd. Adding an existing fd is a no-op. False when full. */
    bool add(int fd)
    {
        if (index_of(fd))
        {
            return true;
        }
        for (Viewer& v : viewers_)
        {
            if (!v.used)
            {
                v = Viewer{true, fd, false, 0};
                return true;
            }
        }
        return false;
    }

    void remove(int fd)
    {
        if (auto i = index_of(fd))
        {
            viewers_[*i] = Viewer{};
        }
    }

    /** @brief Viewers that are idle and have not yet been sent this sequence (or newer). */
    ReadyList ready_for(uint32_t sequence) const
    {
        ReadyList ready;
        for (const Viewer& v : viewers_)
        {
            if (v.used && !v.busy && v.last_sequence_sent < sequence)
            {
                ready.fds[ready.count++] = v.fd;
            }
        }
        return ready;
    }

    void mark_sending(int fd, uint32_t sequence)
    {
        if (auto i = index_of(fd))
        {
            viewers_[*i].busy = true;
            viewers_[*i].last_sequence_sent = sequence;
        }
    }

    void mark_done(int fd)
    {
        if (auto i = index_of(fd))
        {
            viewers_[*i].busy = false;
        }
    }

    std::optional<size_t> index_of(int fd) const
    {
        for (size_t i = 0; i < kMaxViewers; ++i)
        {
            if (viewers_[i].used && viewers_[i].fd == fd)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    size_t size() const
    {
        size_t n = 0;
        for (const Viewer& v : viewers_)
        {
            n += v.used ? 1 : 0;
        }
        return n;
    }

private:
    struct Viewer
    {
        bool used = false;
        int fd = -1;
        bool busy = false;
        uint32_t last_sequence_sent = 0;
    };

    std::array<Viewer, kMaxViewers> viewers_{};
};
} // namespace frames
```

- [ ] **Step 4: Run to verify it passes**

`~/.platformio/penv/Scripts/pio.exe test -e native > ../t.log 2>&1; grep -E "test cases|FAIL" ../t.log; rm ../t.log`

Expected: `29 test cases: 29 succeeded`.

- [ ] **Step 5: Commit**

```bash
git add ESP32/lib/frames/include/viewer_set.h ESP32/test/native/test_viewer_set
git commit -m "Add ViewerSet for drop-when-busy stream bookkeeping" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: `native_sanitize` env + CI job (model: Haiku)

**Files:**
- Create: `ESP32/scripts/sanitize_link.py`
- Modify: `ESP32/platformio.ini`, `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: the native tests from Tasks 1-2.
- Produces: a CI check named `native-sanitize`, and a `workflow_dispatch` trigger on `ci.yml`. Task 7 uses the trigger to prove the sanitizer actually catches bugs.

- [ ] **Step 1: Create `ESP32/scripts/sanitize_link.py`**

```python
"""PlatformIO extra script for [env:native_sanitize]: link with the sanitizer runtimes.

build_flags only reach the compiler; the sanitizer flags must also be passed to the linker.
"""

Import("env")  # type: ignore  # noqa: F821 - provided by PlatformIO/SCons

env.Append(LINKFLAGS=["-fsanitize=address,undefined", "-fno-sanitize-recover=all"])  # type: ignore  # noqa: F821
```

- [ ] **Step 2: Add the env to `ESP32/platformio.ini`** (append at the end of the file)

```ini

; Native tests under AddressSanitizer + UndefinedBehaviorSanitizer. Linux only (runs in CI);
; the Windows MinGW toolchains ship no libasan/libubsan.
[env:native_sanitize]
extends = env:native
build_flags =
	${env:native.build_flags}
	-fsanitize=address,undefined
	-fno-sanitize-recover=all
	-fno-omit-frame-pointer
	-g
extra_scripts = post:scripts/sanitize_link.py
```

- [ ] **Step 3: Add the CI job and manual trigger to `.github/workflows/ci.yml`**

Change the `on:` block to:

```yaml
on:
    push:
        branches: [main]
    pull_request:
    workflow_dispatch:
```

Append this job at the end of `jobs:`, with the same indentation as `native-tests`:

```yaml
    native-sanitize:
        runs-on: ubuntu-latest
        steps:
            - uses: actions/checkout@v6
            - uses: actions/setup-python@v5
              with:
                  python-version: "3.12"
            - uses: actions/cache@v4
              with:
                  path: ~/.platformio
                  key: pio-native-sanitize-${{ runner.os }}-${{ hashFiles('ESP32/platformio.ini') }}
            - name: Install PlatformIO
              run: pip install -U platformio
            - name: Run host unit tests under ASan + UBSan
              working-directory: ESP32
              run: pio test -e native_sanitize
```

- [ ] **Step 4: Verify locally what can be verified**

```bash
cd ESP32
~/.platformio/penv/Scripts/pio.exe project config > ../cfg.log 2>&1; grep -E "env:native_sanitize|sanitize_link" ../cfg.log; rm ../cfg.log
~/.platformio/penv/Scripts/python.exe -I -c "print('script compiles:', compile(open('scripts/sanitize_link.py').read(), 'sanitize_link.py', 'exec') is not None)"
```

Expected: both `env:native_sanitize` and `sanitize_link` lines appear, then `script compiles: True`. **Don't run `pio test -e native_sanitize` locally.** It's expected to fail to link on Windows (`cannot find -lasan`), and CI runs it in Task 7.

- [ ] **Step 5: Commit**

```bash
git add ESP32/scripts/sanitize_link.py ESP32/platformio.ini .github/workflows/ci.yml
git commit -m "Run native tests under ASan + UBSan in CI" -m "Adds native_sanitize env and a native-sanitize CI job; adds workflow_dispatch so CI can run on demand.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: `FrameStore`, camera task, `Camera` and `Controller` rework (model: Sonnet)

**Files:**
- Create: `ESP32/lib/frame_store/include/frame_store.h`, `ESP32/lib/frame_store/src/frame_store.cpp`, `ESP32/lib/frame_store/include/camera_task.h`, `ESP32/lib/frame_store/src/camera_task.cpp`
- Modify: `ESP32/lib/detection/include/camera.h`, `ESP32/lib/detection/src/camera.cpp`, `ESP32/lib/controller/include/controller.h`, `ESP32/lib/controller/src/controller.cpp`, `ESP32/src/main.cpp`

**Interfaces:**
- Consumes: Task 1's `frames::FrameSlots`, `FrameLease`, `FrameMeta`, `AbortReason`, `SlotReleaser`, `is_complete_jpeg`.
- Produces: `class FrameStore final : public frames::SlotReleaser` with `static constexpr size_t kSlotBytes`, `begin()`, `begin_write()`, `write_buffer(slot)`, `publish(slot, meta)`, `abort_write(slot, reason)`, `acquire_latest() -> std::optional<frames::FrameLease>`, `data(lease) -> const uint8_t*`, `set_subscriber(TaskHandle_t)`, `stats() -> FrameStoreStats`, `release_slot(slot)`. Also `struct CameraTaskStats{std::atomic<uint32_t> captured, copy_us_total, copies;}` and `bool start_camera_task(FrameStore&, CameraTaskStats&)`. Task 5 consumes `FrameStore`. The `main.cpp` globals `frame_store` and `camera_stats` are used by Task 5's stats line.

- [ ] **Step 1: Create `frame_store.h`**

```cpp
/**
 * @file frame_store.h
 * @brief Thread-safe home for camera frames: FrameSlots bookkeeping + fixed PSRAM buffers, allocated once.
 * @details The mutex guards bookkeeping only - it is never held while copying pixels or sending on the network.
 * Leases release through release_slot(), which takes the mutex, so never destroy a lease while holding it.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "frame_slots.h"

struct FrameStoreStats
{
    uint32_t published = 0;
    uint32_t dropped_no_slot = 0;
    uint32_t dropped_oversize = 0;
    uint32_t dropped_corrupt = 0;
};

class FrameStore final : public frames::SlotReleaser
{
public:
    static constexpr size_t kSlotBytes = 32 * 1024;

    /** @brief Create the mutex and allocate all slot buffers in PSRAM. False if any allocation fails. */
    bool begin();

    // Writer side (camera task only)
    std::optional<size_t> begin_write();
    uint8_t* write_buffer(size_t slot) { return buffers_[slot]; }
    void publish(size_t slot, const frames::FrameMeta& meta);
    void abort_write(size_t slot, frames::AbortReason reason);

    // Reader side (any task)
    std::optional<frames::FrameLease> acquire_latest();
    const uint8_t* data(const frames::FrameLease& lease) const { return buffers_[lease.slot()]; }

    /** @brief Task to wake (xTaskNotifyGive) on every publish. Set once, before frames flow. */
    void set_subscriber(TaskHandle_t task) { subscriber_ = task; }

    FrameStoreStats stats();

    void release_slot(size_t slot) override;

private:
    frames::FrameSlots slots_;
    std::array<uint8_t*, frames::kSlotCount> buffers_{};
    SemaphoreHandle_t mutex_ = nullptr;
    TaskHandle_t subscriber_ = nullptr;
};
```

- [ ] **Step 2: Create `frame_store.cpp`**

```cpp
#include "frame_store.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

static const char* TAG_STORE = "FRAME_STORE";

namespace
{
/** @brief Scoped FreeRTOS mutex lock. */
class Lock
{
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    SemaphoreHandle_t mutex_;
};
} // namespace

bool FrameStore::begin()
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        ESP_LOGE(TAG_STORE, "Failed to create mutex");
        return false;
    }
    for (uint8_t*& buffer : buffers_)
    {
        buffer = static_cast<uint8_t*>(heap_caps_malloc(kSlotBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (buffer == nullptr)
        {
            ESP_LOGE(TAG_STORE, "Failed to allocate %u bytes of PSRAM for a frame slot", (unsigned)kSlotBytes);
            return false;
        }
    }
    ESP_LOGI(TAG_STORE, "%u slots x %u bytes allocated in PSRAM", (unsigned)frames::kSlotCount, (unsigned)kSlotBytes);
    return true;
}

std::optional<size_t> FrameStore::begin_write()
{
    Lock lock(mutex_);
    return slots_.begin_write();
}

void FrameStore::publish(size_t slot, const frames::FrameMeta& meta)
{
    {
        Lock lock(mutex_);
        slots_.publish(slot, meta);
    }
    if (subscriber_ != nullptr)
    {
        xTaskNotifyGive(subscriber_);
    }
}

void FrameStore::abort_write(size_t slot, frames::AbortReason reason)
{
    Lock lock(mutex_);
    slots_.abort_write(slot, reason);
}

std::optional<frames::FrameLease> FrameStore::acquire_latest()
{
    Lock lock(mutex_);
    return slots_.acquire_latest(*this);
}

void FrameStore::release_slot(size_t slot)
{
    Lock lock(mutex_);
    slots_.release_slot(slot);
}

FrameStoreStats FrameStore::stats()
{
    Lock lock(mutex_);
    FrameStoreStats s;
    s.published = slots_.frames_published();
    s.dropped_no_slot = slots_.frames_dropped_no_slot();
    s.dropped_oversize = slots_.frames_dropped_oversize();
    s.dropped_corrupt = slots_.frames_dropped_corrupt();
    return s;
}
```

- [ ] **Step 3: Create `camera_task.h` and `camera_task.cpp`**

`camera_task.h`:

```cpp
/**
 * @file camera_task.h
 * @brief FreeRTOS task that owns frame capture: driver frame -> checks -> copy into a FrameStore slot -> publish.
 */

#pragma once

#include <atomic>
#include <cstdint>

#include "frame_store.h"

struct CameraTaskStats
{
    std::atomic<uint32_t> captured{0};      // frames received from the driver
    std::atomic<uint32_t> copy_us_total{0}; // total time spent copying into slots
    std::atomic<uint32_t> copies{0};        // frames copied (published)
};

/** @brief Start the camera task (core 1, priority 2, 4096-byte stack). Call after esp_camera init succeeded. */
bool start_camera_task(FrameStore& store, CameraTaskStats& stats);
```

`camera_task.cpp`:

```cpp
#include "camera_task.h"

#include <cstring>
#include <esp_camera.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "jpeg_check.h"

static const char* TAG_CAMERA_TASK = "CAMERA_TASK";

namespace
{
struct CameraTaskContext
{
    FrameStore* store = nullptr;
    CameraTaskStats* stats = nullptr;
};

CameraTaskContext g_context;

void camera_task(void*)
{
    for (;;)
    {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb == nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        g_context.stats->captured++;

        std::optional<size_t> slot = g_context.store->begin_write();
        if (slot)
        {
            if (fb->len > FrameStore::kSlotBytes)
            {
                g_context.store->abort_write(*slot, frames::AbortReason::Oversize);
            } else if (!frames::is_complete_jpeg(fb->buf, fb->len))
            {
                g_context.store->abort_write(*slot, frames::AbortReason::Corrupt);
            } else
            {
                const int64_t start_us = esp_timer_get_time();
                std::memcpy(g_context.store->write_buffer(*slot), fb->buf, fb->len);
                g_context.stats->copy_us_total += static_cast<uint32_t>(esp_timer_get_time() - start_us);
                g_context.stats->copies++;

                frames::FrameMeta meta;
                meta.length = fb->len;
                meta.width = static_cast<uint16_t>(fb->width);
                meta.height = static_cast<uint16_t>(fb->height);
                meta.timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
                g_context.store->publish(*slot, meta);
            }
        }
        esp_camera_fb_return(fb); // hand the driver buffer back right away
    }
}
} // namespace

bool start_camera_task(FrameStore& store, CameraTaskStats& stats)
{
    g_context.store = &store;
    g_context.stats = &stats;
    if (xTaskCreatePinnedToCore(camera_task, "camera", 4096, nullptr, 2, nullptr, 1) != pdPASS)
    {
        ESP_LOGE(TAG_CAMERA_TASK, "Failed to create camera task");
        return false;
    }
    return true;
}
```

- [ ] **Step 4: Remove per-frame capture from `Camera`**

In `ESP32/lib/detection/include/camera.h`:
- delete the `camera_buffer_t _buffer;` member, the `capture()` declaration and its doc comment, and `get_frame_buffer()`;
- change the constructor initializer `Camera() : _buffer()` to `Camera()`;
- delete the destructor body that frees `_buffer.buffer`, leaving no user-declared destructor;
- **keep** the `camera_buffer_t` typedef, because the detection interface uses it.

In `ESP32/lib/detection/src/camera.cpp`, delete the whole `Camera::capture()` function.

Then search for any remaining uses:

```bash
grep -rn "capture()\|get_frame_buffer" ESP32/src ESP32/lib
```

Expected: no matches in `src/` or `lib/`. `lib/network/include/websockets.h` still uses `get_frame_buffer()` inside `StreamWebsocketHandler`. Delete the whole `StreamWebsocketHandler` class from `websockets.h` now, since Task 5 replaces it. The `HttpServer` code that creates it in `turret_server.cpp` must go too: remove the `_stream_handler` member and its use in `turret_server.h`/`.cpp`, keeping everything else. Task 5 adds the replacement.

- [ ] **Step 5: Controller reads frames through a lease**

In `ESP32/lib/controller/include/controller.h`:
- replace the forward declaration `class Camera;` with `class FrameStore;`;
- replace the member `Camera& _camera;` with `FrameStore& _frame_store;` (update its doc comment);
- change the constructor's last parameter from `Camera& camera` to `FrameStore& frame_store`, and the initializer `_camera(camera)` to `_frame_store(frame_store)`.

In `ESP32/lib/controller/src/controller.cpp`:
- add `#include "frame_store.h"`, keeping `#include "camera.h"` for `camera_buffer_t`;
- replace the frame fetch at the top of `run()` (from `const camera_buffer_t& fb = this->_camera.get_frame_buffer();` through the `if (fb.length == 0) { ... return; }` block) with:

```cpp
    std::optional<frames::FrameLease> lease = this->_frame_store.acquire_latest();
    if (!lease)
    {
        ESP_LOGV(TAG, "No frame published yet");
        return;
    }
    camera_buffer_t fb{const_cast<uint8_t*>(this->_frame_store.data(*lease)), lease->meta().length,
                       lease->meta().width, lease->meta().height};
```

The rest of `run()` is unchanged; it uses `fb`. The lease is released automatically when `run()` returns.

- [ ] **Step 6: Wire it up in `ESP32/src/main.cpp`**

- Add includes: `#include "frame_store.h"` and `#include "camera_task.h"`.
- Add globals before `Controller controller(...)`: `FrameStore frame_store;` and `CameraTaskStats camera_stats;`
- Change the controller construction to `Controller controller(movement_manager, detection_manager, joystick, frame_store);`
- In `setup()`, replace the camera block (from `if (camera.begin())` through its `else { ... return; }`) with:

```cpp
    if (!camera.begin())
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Camera initialization failed - continuing without video");
    } else if (!frame_store.begin() || !start_camera_task(frame_store, camera_stats))
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Frame pipeline failed to start - continuing without video");
    } else
    {
        ESP_LOGI(TAG_MAIN, "Camera and frame pipeline running");
    }
```

- In `loop()`, delete the line `camera.capture();`.
- **Leave `http_server.start(...)` as it is for now.** Task 5 changes its signature. If removing `StreamWebsocketHandler` in Step 4 left `HttpServer::start` still taking `Camera*`, keep passing `&camera` so the firmware builds at the end of this task.

- [ ] **Step 7: Verify**

```bash
cd ESP32
~/.platformio/penv/Scripts/pio.exe run -e freenove_esp32_wrover > ../b.log 2>&1; grep -E "error|^freenove_esp32_wrover +(SUCCESS|FAILED)" ../b.log
~/.platformio/penv/Scripts/pio.exe run -e pin_check > ../b.log 2>&1; grep -E "^pin_check +(SUCCESS|FAILED)" ../b.log
~/.platformio/penv/Scripts/pio.exe test -e native > ../b.log 2>&1; grep -E "test cases" ../b.log
~/.platformio/penv/Scripts/pio.exe test -e freenove_esp32_wrover -f test_controller --without-uploading --without-testing > ../b.log 2>&1; grep -E "error|SUCCESS|FAILED|PASSED" ../b.log
rm ../b.log
```

Expected:
- Both firmware envs report `SUCCESS` with no `error` lines.
- `29 test cases: 29 succeeded`.
- The on-device `test_controller` suite builds without errors. It's built only, not run.

- [ ] **Step 8: Commit**

```bash
git add ESP32/lib/frame_store ESP32/lib/detection ESP32/lib/controller ESP32/lib/network ESP32/src/main.cpp
git commit -m "Give camera frames one owner: FrameStore slots and a camera task" -m "Removes the per-frame malloc and the stream/capture use-after-free: frames live in fixed PSRAM slots, readers hold RAII leases, and a dedicated camera task copies and publishes. The old pull-based stream handler is removed; its push replacement follows.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: `StreamBroadcaster` + stats line (model: Sonnet)

**Files:**
- Create: `ESP32/lib/network/include/stream_broadcaster.h`, `ESP32/lib/network/src/stream_broadcaster.cpp`
- Modify: `ESP32/lib/network/include/turret_server.h`, `ESP32/lib/network/src/turret_server.cpp`, `ESP32/src/main.cpp`, `CLAUDE.md`

**Interfaces:**
- Consumes: Task 2's `frames::ViewerSet`, `ReadyList`, `kMaxViewers`; Task 4's `FrameStore`, `FrameStoreStats`, `CameraTaskStats`, and the `main.cpp` globals `frame_store` and `camera_stats`.
- Produces: `bool HttpServer::start(FrameStore& store, BaseDetectionModule* detection)`, `uint32_t HttpServer::frames_sent()`, `size_t HttpServer::viewer_count()`.

- [ ] **Step 1: Create `stream_broadcaster.h`**

```cpp
/**
 * @file stream_broadcaster.h
 * @brief Pushes the latest camera frame to each idle /stream WebSocket viewer (drop-when-busy, low latency).
 * @details Sends run inside the HTTP server's own task via httpd_queue_work + httpd_ws_send_frame_async, the
 * thread-safe way to use esp_http_server from another task. Each viewer has one preallocated send job, so the
 * frame path allocates nothing. A viewer still receiving the previous frame is skipped, never queued.
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "frame_slots.h"
#include "frame_store.h"
#include "viewer_set.h"

class StreamBroadcaster
{
public:
    /** @brief Register the /stream WebSocket endpoint and start the stream task (core 1, priority 1). */
    bool start(httpd_handle_t server, FrameStore& store);

    /** @brief httpd close_fn: forget the viewer and close the socket (required when close_fn is set). */
    static void on_session_closed(httpd_handle_t server, int sockfd);

    uint32_t frames_sent() const { return sent_.load(); }
    size_t viewer_count();

private:
    struct SendJob
    {
        StreamBroadcaster* owner = nullptr;
        int fd = -1;
        bool in_flight = false;
        std::optional<frames::FrameLease> lease;
    };

    static esp_err_t ws_handler(httpd_req_t* req);
    static void stream_task(void* arg);
    static void send_job(void* arg);
    void run();
    void remove_viewer(int fd);

    httpd_handle_t server_ = nullptr;
    FrameStore* store_ = nullptr;
    frames::ViewerSet viewers_;
    std::array<SendJob, frames::kMaxViewers> jobs_{};
    SemaphoreHandle_t mutex_ = nullptr; // guards viewers_ and jobs_[i].in_flight
    std::atomic<uint32_t> sent_{0};

    static StreamBroadcaster* instance_; // close_fn has no user context
};
```

- [ ] **Step 2: Create `stream_broadcaster.cpp`**

```cpp
#include "stream_broadcaster.h"

#include <esp_log.h>
#include <unistd.h>

static const char* TAG_STREAM = "STREAM";

StreamBroadcaster* StreamBroadcaster::instance_ = nullptr;

namespace
{
class Lock
{
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    SemaphoreHandle_t mutex_;
};
} // namespace

bool StreamBroadcaster::start(httpd_handle_t server, FrameStore& store)
{
    server_ = server;
    store_ = &store;
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        ESP_LOGE(TAG_STREAM, "Failed to create mutex");
        return false;
    }
    instance_ = this;

    httpd_uri_t uri = {};
    uri.uri = "/stream";
    uri.method = HTTP_GET;
    uri.handler = ws_handler;
    uri.user_ctx = this;
    uri.is_websocket = true;
    if (httpd_register_uri_handler(server_, &uri) != ESP_OK)
    {
        ESP_LOGE(TAG_STREAM, "Failed to register /stream");
        return false;
    }

    TaskHandle_t task = nullptr;
    if (xTaskCreatePinnedToCore(stream_task, "stream", 4096, this, 1, &task, 1) != pdPASS)
    {
        ESP_LOGE(TAG_STREAM, "Failed to create stream task");
        return false;
    }
    store_->set_subscriber(task);
    return true;
}

size_t StreamBroadcaster::viewer_count()
{
    Lock lock(mutex_);
    return viewers_.size();
}

esp_err_t StreamBroadcaster::ws_handler(httpd_req_t* req)
{
    auto* self = static_cast<StreamBroadcaster*>(req->user_ctx);
    const int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) // WebSocket handshake completed
    {
        bool added = false;
        {
            Lock lock(self->mutex_);
            added = self->viewers_.add(fd);
        }
        if (!added)
        {
            ESP_LOGW(TAG_STREAM, "Viewer limit (%u) reached, closing fd %d", (unsigned)frames::kMaxViewers, fd);
            httpd_sess_trigger_close(req->handle, fd);
        } else
        {
            ESP_LOGI(TAG_STREAM, "Viewer connected (fd %d)", fd);
        }
        return ESP_OK;
    }

    // The board pushes; clients have nothing to say on /stream. Drain and ignore small frames.
    httpd_ws_frame_t pkt = {};
    esp_err_t err = httpd_ws_recv_frame(req, &pkt, 0);
    if (err != ESP_OK)
    {
        return err;
    }
    if (pkt.len > 0)
    {
        uint8_t scratch[64];
        if (pkt.len > sizeof(scratch))
        {
            return ESP_FAIL; // unexpected large client message: let the server close the session
        }
        pkt.payload = scratch;
        err = httpd_ws_recv_frame(req, &pkt, pkt.len);
    }
    return err;
}

void StreamBroadcaster::on_session_closed(httpd_handle_t, int sockfd)
{
    if (instance_ != nullptr)
    {
        instance_->remove_viewer(sockfd);
    }
    close(sockfd);
}

void StreamBroadcaster::remove_viewer(int fd)
{
    Lock lock(mutex_);
    if (viewers_.index_of(fd))
    {
        viewers_.remove(fd);
        ESP_LOGI(TAG_STREAM, "Viewer disconnected (fd %d)", fd);
    }
}

void StreamBroadcaster::stream_task(void* arg) { static_cast<StreamBroadcaster*>(arg)->run(); }

void StreamBroadcaster::run()
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)); // woken by FrameStore::publish

        frames::ReadyList ready;
        {
            std::optional<frames::FrameLease> probe = store_->acquire_latest();
            if (!probe)
            {
                continue;
            }
            Lock lock(mutex_);
            ready = viewers_.ready_for(probe->meta().sequence);
        } // probe released here, outside the broadcaster mutex

        for (size_t i = 0; i < ready.count; ++i)
        {
            const int fd = ready.fds[i];
            std::optional<frames::FrameLease> lease = store_->acquire_latest();
            if (!lease)
            {
                break;
            }

            SendJob* job = nullptr;
            {
                Lock lock(mutex_);
                std::optional<size_t> index = viewers_.index_of(fd);
                if (!index || jobs_[*index].in_flight)
                {
                    continue; // viewer left, or its previous send is still queued
                }
                job = &jobs_[*index];
                job->in_flight = true;
                viewers_.mark_sending(fd, lease->meta().sequence);
            }

            job->owner = this;
            job->fd = fd;
            job->lease = std::move(lease);
            if (httpd_queue_work(server_, send_job, job) != ESP_OK)
            {
                job->lease.reset();
                Lock lock(mutex_);
                viewers_.mark_done(fd);
                job->in_flight = false;
            }
        }
    }
}

void StreamBroadcaster::send_job(void* arg)
{
    auto* job = static_cast<SendJob*>(arg);
    StreamBroadcaster* self = job->owner;

    httpd_ws_frame_t frame = {};
    frame.final = true;
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = const_cast<uint8_t*>(self->store_->data(*job->lease));
    frame.len = job->lease->meta().length;

    const esp_err_t err = httpd_ws_send_frame_async(self->server_, job->fd, &frame);
    job->lease.reset(); // release the slot before touching the broadcaster mutex

    const int fd = job->fd;
    {
        Lock lock(self->mutex_);
        if (err == ESP_OK)
        {
            self->viewers_.mark_done(fd);
        } else
        {
            self->viewers_.remove(fd);
        }
        job->in_flight = false;
    }

    if (err == ESP_OK)
    {
        self->sent_++;
    } else
    {
        ESP_LOGW(TAG_STREAM, "Send to fd %d failed (%s), dropping viewer", fd, esp_err_to_name(err));
        httpd_sess_trigger_close(self->server_, fd);
    }
}
```

- [ ] **Step 3: Hook it into `HttpServer`**

In `turret_server.h`:
- `#include "frame_store.h"` and `#include "stream_broadcaster.h"`;
- remove `static Camera* _camera_instance;` and any leftover stream-handler member;
- add the member `StreamBroadcaster _broadcaster;`;
- change `bool start(Camera* camera, BaseDetectionModule* detection = nullptr);` to `bool start(FrameStore& store, BaseDetectionModule* detection = nullptr);` and update its doc comment;
- add the public methods `uint32_t frames_sent() const { return _broadcaster.frames_sent(); }` and `size_t viewer_count() { return _broadcaster.viewer_count(); }`.

In `turret_server.cpp`:
- remove the `_camera_instance` definition and assignment;
- in `start()`, set `config.close_fn = StreamBroadcaster::on_session_closed;` before `httpd_start`;
- after the index URI is registered, call `this->_broadcaster.start(this->_server_handle, store)`, and on failure log it and return `false`;
- keep the commands WebSocket registration as it is.

In `main.cpp`, change the server start call to `http_server.start(frame_store, &detection_manager)`.

- [ ] **Step 4: Add the stats line in `main.cpp`**

Add this function above `setup()`:

```cpp
// One benchmark line every 5 s (format fixed by the Stage 1 spec). Never prints credentials.
static void log_stream_stats()
{
    static uint32_t last_ms = 0;
    const uint32_t now = millis();
    if (now - last_ms < 5000)
    {
        return;
    }
    last_ms = now;

    const FrameStoreStats store = frame_store.stats();
    const uint32_t copies = camera_stats.copies.load();
    const uint32_t copy_us_avg = copies ? camera_stats.copy_us_total.load() / copies : 0;
    ESP_LOGI(TAG_MAIN,
             "[stream] up=%us captured=%u sent=%u dropped_no_slot=%u oversize=%u corrupt=%u viewers=%u "
             "copy_us_avg=%u heap=%u heap_min=%u psram=%u psram_min=%u",
             (unsigned)(now / 1000), (unsigned)camera_stats.captured.load(), (unsigned)http_server.frames_sent(),
             (unsigned)store.dropped_no_slot, (unsigned)store.dropped_oversize, (unsigned)store.dropped_corrupt,
             (unsigned)http_server.viewer_count(), (unsigned)copy_us_avg, (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getMinFreePsram());
}
```

Then call `log_stream_stats();` at the end of `loop()`.

- [ ] **Step 5: Update `CLAUDE.md` architecture notes**

Replace the bullet that starts `- **Camera frame ownership**:` with:

```markdown
- **Camera frame ownership**: a camera FreeRTOS task (`lib/frame_store/camera_task.cpp`, core 1, prio 2) copies each driver frame into one of 4 fixed 32 KB PSRAM slots in `FrameStore` and returns the driver buffer immediately. Bookkeeping is the pure `frames::FrameSlots` (natively tested); readers hold move-only RAII `frames::FrameLease`s, and a leased slot is never rewritten (a new frame is dropped and counted instead). No per-frame allocation anywhere on the frame path. Frames are **JPEG** (needed for streaming), QVGA, `CAMERA_GRAB_LATEST`; incomplete JPEGs (no FF D8 ... FF D9) are dropped as `corrupt`. PSRAM is mandatory.
```

In the bullet that starts `- **Network**`, replace the sentence about `/stream` with:

```markdown
`/stream` is a push WebSocket: `StreamBroadcaster` wakes on each published frame and, for each idle viewer (max 2, `frames::ViewerSet`), queues one send onto the httpd task (`httpd_queue_work` + `httpd_ws_send_frame_async`); busy viewers skip frames, which keeps latency low. `close_fn` removes viewers and must close the socket.
```

Also remove the phrase `websockets.h` is header-only (TODO to split)`, along with the reference to `StreamWebsocketHandler`, if either is present.

Finally, add this bullet at the end of the Architecture section:

```markdown
- **Benchmark stats**: `main.cpp` logs `[stream] up= captured= sent= dropped_no_slot= oversize= corrupt= viewers= copy_us_avg= heap= heap_min= psram= psram_min=` every 5 s; `heap_min`/`psram_min` drifting down means a leak.
```

- [ ] **Step 6: Verify**

```bash
cd ESP32
~/.platformio/penv/Scripts/pio.exe run -e freenove_esp32_wrover > ../b.log 2>&1; grep -E "error|warning: .*stream|^freenove_esp32_wrover +(SUCCESS|FAILED)" ../b.log
~/.platformio/penv/Scripts/pio.exe test -e native > ../b.log 2>&1; grep -E "test cases" ../b.log
~/.platformio/penv/Scripts/pio.exe test -e freenove_esp32_wrover -f test_turret_server --without-uploading --without-testing > ../b.log 2>&1; grep -E "error|SUCCESS|FAILED|PASSED" ../b.log
rm ../b.log
grep -rn "StreamWebsocketHandler\|_camera_instance\|get_frame_buffer" ESP32/src ESP32/lib ESP32/test
```

Expected:
- The firmware builds with `SUCCESS`, no errors, and no warnings from the stream code.
- 29/29 native tests pass.
- If the `test_turret_server` build fails because that test calls the old `HttpServer::start(&camera, ...)`, update the call to the new signature (`http_server.start(frame_store, nullptr)` with a local `FrameStore`) until the suite builds. It's built only, not run.
- The final grep returns nothing.

- [ ] **Step 7: Commit**

```bash
git add ESP32/lib/network ESP32/src/main.cpp ESP32/test CLAUDE.md
git commit -m "Push frames to /stream viewers with drop-when-busy" -m "StreamBroadcaster sends the latest frame to each idle viewer from the httpd task (httpd_queue_work + httpd_ws_send_frame_async) with one preallocated job per viewer. Adds the 5 s benchmark stats line.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: HUD push client (model: Haiku)

**Files:**
- Modify: `web/web_ui.html`

**Interfaces:**
- Consumes: Task 5's `/stream` binary pushes (one JPEG per WebSocket message, with no client requests needed).
- Produces: the HUD elements `metric-fps` (already present) and `metric-decode-errors` (new).

- [ ] **Step 1: Add the decode-error stat line**

In the `ADVANCED` `<details>` block, directly after the line containing `id="metric-fps"`, add:

```html
                <div class="stat-line">DECODE ERR <span class="stat-val" id="metric-decode-errors">0</span></div>
```

- [ ] **Step 2: Replace the stream client**

Delete everything from the comment block `/** * Converts a Blob object to a Base64 data URL string.` (and the `blobToBase64` function) through the end of the `ws_stream.onerror = ... };` handler. Replace it with:

```javascript
            const imgElement = document.getElementById('streamImg');
            const StreamStats = { frames: 0, windowStart: performance.now(), decodeErrors: 0 };
            let currentFrameUrl = null;

            imgElement.addEventListener('error', () => {
                StreamStats.decodeErrors += 1;
                document.getElementById('metric-decode-errors').textContent = StreamStats.decodeErrors;
            });

            // The board pushes one JPEG per message; nothing is requested from here.
            const ws_stream = new WebSocket(`ws://${window.location.host}/stream`);
            ws_stream.binaryType = 'blob';
            App.state.stream_socket = ws_stream;

            ws_stream.onopen = () => {
                App.updateStatus("ONLINE", "#00ff41");
            };

            ws_stream.onmessage = (event) => {
                const previousUrl = currentFrameUrl;
                currentFrameUrl = URL.createObjectURL(event.data);
                imgElement.src = currentFrameUrl;
                if (previousUrl) {
                    URL.revokeObjectURL(previousUrl);
                }

                StreamStats.frames += 1;
                const now = performance.now();
                if (now - StreamStats.windowStart >= 1000) {
                    const fps = Math.round((StreamStats.frames * 1000) / (now - StreamStats.windowStart));
                    document.getElementById('metric-fps').textContent = fps;
                    StreamStats.frames = 0;
                    StreamStats.windowStart = now;
                }
            };

            ws_stream.onclose = () => {
                App.updateStatus("DISCONNECTED", "#ff0000");
            };

            ws_stream.onerror = (error) => {
                console.error('WebSocket Error: ', error);
                App.updateStatus("RETRYING", "orange");
            };
```

Leave the commands WebSocket code below it unchanged.

- [ ] **Step 3: Verify**

```bash
cd /c/Users/yuval/void-sentry-esp32
grep -c "blobToBase64\|ws:/\${" web/web_ui.html
python -I -c "import re,sys; h=open('web/web_ui.html',encoding='utf-8').read(); s=re.findall(r'<script>(.*?)</script>', h, re.S); open('ui_check.js','w',encoding='utf-8').write('\n'.join(s)); print('scripts:', len(s))"
node --check ui_check.js && echo "JS syntax OK"; rm ui_check.js
python -I -m unittest discover -s web -p "test_*.py" 2>&1 | tail -1
cd ESP32 && ~/.platformio/penv/Scripts/pio.exe run -e freenove_esp32_wrover > ../b.log 2>&1; grep -E "\[web-ui\]|^freenove_esp32_wrover +(SUCCESS|FAILED)" ../b.log; rm ../b.log
```

Expected:
- The first grep prints `0`, meaning no Base64 helper and no single-slash URL remain.
- `JS syntax OK`.
- The generator tests print `OK`.
- The firmware build shows `[web-ui] Generated ...` and `SUCCESS`.

- [ ] **Step 4: Commit**

```bash
git add web/web_ui.html
git commit -m "HUD: display pushed frames as blobs, show fps and decode errors" -m "Removes the request-per-frame loop and Base64 conversion; fixes the ws:// URL.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Hardware benchmarks, sanitizer proof, PR (main session, with the user)

**Files:** none, unless a problem turns up. Fixes go to an agent and are re-reviewed.

**Interfaces:**
- Consumes: everything above, the board on COM3, and **real WiFi credentials, which the user puts in `secrets.h` themselves**. It currently holds the template placeholders.
- Produces: an open PR with benchmark numbers and all CI checks green.

- [ ] **Step 1: The user sets real WiFi credentials** in `ESP32/include/secrets.h`. The controller never reads that file.
- [ ] **Step 2: Flash** with `pio run -e freenove_esp32_wrover -t upload --upload-port COM3`, then read serial without resetting the board (DTR/RTS low) for about 30 s to get the IP address and the first `[stream]` lines.
- [ ] **Step 3: Corrupt-frame check (Review Focus 5).** With the HUD open, the `corrupt` counter must stay at 0. If it equals `captured`, stop: the driver pads its JPEG data, and the controller rules on a tolerant end-marker search.
- [ ] **Step 4: Viewer behaviour.**
  - Two tabs at once both stream.
  - A third tab is rejected while the other two keep streaming.
  - Closing a tab mid-stream gives a `Viewer disconnected` log line, and `dropped_no_slot` stops climbing once only one viewer remains.
- [ ] **Step 5: Benchmarks, recorded for the PR.**
  - HUD fps, plus the board's `captured` and `sent` rates.
  - Latency: the user takes 5 photos with the stopwatch method, and the median is recorded.
  - A 10-minute soak: `heap_min` and `psram_min` stay flat, `corrupt` and the HUD decode errors stay at 0, and `up=` shows no reboot.
- [ ] **Step 6: Prove the sanitizer catches a bug.**
  1. Create a throwaway branch `ci/sanitizer-canary`.
  2. Add a native test that reads one element past the end of a `std::array` through `.data()`.
  3. Push it and run `gh workflow run CI --ref ci/sanitizer-canary`.
  4. Confirm that `native-sanitize` **fails** with an AddressSanitizer report while the plain `native-tests` job passes.
  5. Delete the branch, locally and on the remote.
- [ ] **Step 7: PR.** Push `feature/stage1-camera-stream` and open the PR with a body file that includes the benchmark numbers. Then `gh pr checks --watch` until `format`, `web-ui-generator`, `build`, `native-tests`, `native-sanitize` and `cppcheck` are all green. The merge stays the user's decision.
