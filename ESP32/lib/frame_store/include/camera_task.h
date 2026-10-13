/**
 * @file camera_task.h
 * @brief FreeRTOS task that owns frame capture: driver frame -> checks -> copy into a FrameStore slot -> publish.
 */

#pragma once

#include <atomic>
#include <cstdint>

#include "frame_store.h"

struct CameraTaskStats {
    std::atomic<uint32_t> captured{0};      // frames received from the driver
    std::atomic<uint32_t> copy_us_total{0}; // total time spent copying into slots
    std::atomic<uint32_t> copies{0};        // frames copied (published)
    std::atomic<uint32_t> bytes_total{0};   // JPEG bytes copied (wraps after ~4 GB; use per-window deltas)
};

/** @brief Start the camera task (core 1, priority 2, 4096-byte stack). Call after esp_camera init succeeded. */
bool start_camera_task(FrameStore& store, CameraTaskStats& stats);
