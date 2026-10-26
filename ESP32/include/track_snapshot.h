/**
 * @file track_snapshot.h
 * @brief Plain copies of the tracker output and detection timings, safe to hand across tasks.
 * @details The detection module writes these under a lock in loop(); the /commands handler (httpd task) and the
 * stats line read copies. Coordinates are in frame pixels (QVGA), already scaled and clamped.
 */

#pragma once

#include "tracking_types.h"

#include <cstdint>

struct TrackSnapshot {
    tracking::TrackPhase state = tracking::TrackPhase::Idle;
    uint32_t id = 0;
    int x = 0; ///< centroid, pixels
    int y = 0;
    int bx = 0; ///< box, pixels, clamped to the frame
    int by = 0;
    int bw = 0;
    int bh = 0;
    float vx = 0.0f; ///< pixels per frame
    float vy = 0.0f;
    int area_px = 0;
    int frame_width = 320;
    int frame_height = 240;

    bool detected() const { return state == tracking::TrackPhase::Locked || state == tracking::TrackPhase::Coasting; }
};

struct DetectionStats {
    uint32_t frames = 0;          ///< frames analysed
    uint32_t decode_failures = 0; ///< corrupt JPEG or not QVGA
    uint32_t scene_changes = 0;
    uint32_t decode_us_avg = 0;   ///< average since boot
    uint32_t track_us_avg = 0;    ///< detector + tracker, average since boot
    uint64_t decode_us_total = 0; ///< sum over analysed frames; the stats line differences it per window
    uint64_t track_us_total = 0;  ///< sum of detector + tracker time over analysed frames
    uint32_t stack_free = 0;      ///< detect task stack high-water mark in bytes, sampled every 100 frames
};
