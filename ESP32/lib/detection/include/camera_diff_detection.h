#pragma once

#include "base_detection_module.h"
#include "motion_detector.h"
#include "tracker.h"

#include <freertos/FreeRTOS.h>

/**
 * @class CameraDiffDetection
 * @brief Motion detection and persistent single-target tracking on camera frames.
 * @details Each frame: JPEG -> 80x60 gray (jpeg_gray), MotionDetector, Tracker. The pipeline state (~50 KB) is
 * allocated once in PSRAM by begin(); nothing is allocated per frame. detect_object() runs in loop(); the snapshot
 * and stats getters may be called from other tasks.
 */
class CameraDiffDetection : public BaseDetectionModule
{
public:
    CameraDiffDetection() = default;
    ~CameraDiffDetection() override;
    CameraDiffDetection(const CameraDiffDetection&) = delete;
    CameraDiffDetection& operator=(const CameraDiffDetection&) = delete;

    /** @brief Allocate the pipeline in PSRAM. Without it detect_object() reports no motion. */
    bool begin();

    /** @brief Analyse one frame; returns the direction toward the tracked target (None when there is none). */
    std::tuple<MoveX, MoveY> detect_object(camera_buffer_t frame) override;

    TrackSnapshot get_track_snapshot() const override;
    DetectionStats get_stats() const override;

private:
    struct Pipeline {
        tracking::GrayFrame gray;
        tracking::MotionDetector detector;
        tracking::Tracker tracker;
    };

    Pipeline* _pipeline = nullptr;
    mutable portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
    TrackSnapshot _snapshot;
    DetectionStats _stats;
    uint64_t _decode_us_total = 0;
    uint64_t _track_us_total = 0;
};
