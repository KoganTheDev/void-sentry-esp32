/**
 * @file tracker.h
 * @brief Single-target persistent tracker over detector blobs (pure C++20, natively tested).
 * @details States: Idle -> Tentative (candidate, must match confirm_frames in a row) -> Locked (id assigned) ->
 * Coasting (missed; predicted forward with decaying velocity) -> Locked again with the same id, or Idle after
 * coast_frames misses. Prediction is an alpha-beta filter (constant velocity, fixed gains). The measurement is the
 * union of every blob overlapping the gate around the prediction, so a target split into parts stays one box.
 * While Locked, blobs outside the gate are ignored even if larger. NOT thread-safe.
 */

#pragma once

#include "tracking_types.h"

namespace tracking
{
class Tracker
{
public:
    explicit Tracker(const TrackerConfig& config = TrackerConfig{});

    /** @brief Advance one frame. The returned reference stays valid until the next call. */
    const TrackState& update(const DetectionResult& detection);

    const TrackState& state() const { return state_; }

    /** @brief Drop the current target (Idle). Ids already handed out are never reused. */
    void reset();

private:
    Box gate() const;
    bool measure(const DetectionResult& detection, const Box& gate, Blob& out) const;
    void start_tentative(const Blob& blob);
    void correct(const Blob& measurement);
    void miss();
    void to_idle();
    void refresh_box();

    TrackerConfig config_;
    TrackState state_;
    int hits_ = 0;
    uint32_t last_id_ = 0;
    float width_ = 0.0f; ///< smoothed box size, cells
    float height_ = 0.0f;
};
} // namespace tracking
