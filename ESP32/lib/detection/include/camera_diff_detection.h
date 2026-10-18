#pragma once

#include "base_detection_module.h"
#include "motion_detector.h"
#include "sync.h"
#include "tracker.h"

#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class FrameStore;

/**
 * @class CameraDiffDetection
 * @brief Motion detection and persistent single-target tracking on camera frames.
 * @details Each frame: JPEG -> 40x30 gray (jpeg_gray), MotionDetector, Tracker. The pipeline state is
 * allocated once in internal RAM by begin() (~12.6 KB at 40x30; tracking measured 0.6 ms there versus 1.1 ms in PSRAM);
 * nothing is allocated per frame. After start_task(), a dedicated "detect" task (core 1, priority 3) owns the pipeline
 * and calls detect_object() on every new published frame; the snapshot and stats getters and reset() may be called
 * from any task.
 */
class CameraDiffDetection : public BaseDetectionModule
{
public:
    CameraDiffDetection() = default;
    ~CameraDiffDetection() override;
    CameraDiffDetection(const CameraDiffDetection&) = delete;
    CameraDiffDetection& operator=(const CameraDiffDetection&) = delete;

    /** @brief Allocate the pipeline in internal RAM. Without it detect_object() reports no motion. */
    bool begin();

    /** @brief Analyse one frame; returns the direction toward the tracked target (None when there is none). */
    std::tuple<MoveX, MoveY> detect_object(camera_buffer_t frame) override;

    /**
     * @brief Create the "detect" task (core 1, priority 3, 4096 B stack) and subscribe it to the store.
     * @details The task sleeps until FrameStore::publish wakes it, then analyses the newest unseen frame. Idempotent.
     * Call after begin() and after the frame pipeline is running; @p store must outlive this object.
     * @return false if begin() has not succeeded, the task cannot be created or the store has no free subscriber slot.
     */
    bool start_task(FrameStore& store);

    /**
     * @brief Request a reset of detector and tracker plus an Idle snapshot.
     * @details Sets a flag and wakes the detection task. Safe from any task. Called from loop() (core 1, priority 1)
     * the higher-priority detect task preempts the caller, applies the reset and publishes Idle before reset()
     * returns, so it is synchronous in practice. The task's 200 ms wait timeout remains a backstop.
     */
    void reset() override;

    TrackSnapshot get_track_snapshot() const override;
    DetectionStats get_stats() const override;

private:
    struct Pipeline {
        tracking::GrayFrame gray;
        tracking::MotionDetector detector;
        tracking::Tracker tracker;
    };

    static void task_entry(void* arg);
    void task_loop();
    /** @brief Detection task only: reset the pipeline and publish an Idle snapshot. */
    void apply_reset();

    Pipeline* _pipeline = nullptr;
    FrameStore* _store = nullptr;
    TaskHandle_t _task = nullptr;
    std::atomic<bool> _reset_requested{false};
    uint32_t _last_sequence = 0; ///< detection task only
    /** @brief Snapshot and counters published together under one lock by the detection task. */
    struct Published {
        TrackSnapshot snapshot;
        DetectionStats stats;
    };
    vsync::SpinGuarded<Published> _published;
};
