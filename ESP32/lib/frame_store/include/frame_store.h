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

struct FrameStoreStats {
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
