#include "tracker.h"

#include <algorithm>
#include <cmath>

namespace tracking
{
Tracker::Tracker(const TrackerConfig& config) : config_(config) {}

void Tracker::reset() { to_idle(); }

const TrackState& Tracker::update(const DetectionResult& detection)
{
    if (detection.scene_change)
    {
        if (state_.state == TrackPhase::Tentative)
        {
            to_idle();
        } else if (state_.state != TrackPhase::Idle)
        {
            miss(); // Locked or Coasting: keep predicting, the target may still be there after the warm-up
        }
        return state_;
    }

    Blob measurement;
    switch (state_.state)
    {
    case TrackPhase::Idle:
        if (detection.blob_count > 0)
        {
            start_tentative(detection.blobs[0]); // blobs are sorted: [0] is the largest
        }
        break;
    case TrackPhase::Tentative:
        if (measure(detection, gate(), measurement))
        {
            correct(measurement);
            if (++hits_ >= config_.confirm_frames)
            {
                state_.state = TrackPhase::Locked;
                state_.id = ++last_id_;
            }
        } else
        {
            to_idle();
        }
        break;
    case TrackPhase::Locked:
    case TrackPhase::Coasting:
        if (measure(detection, gate(), measurement))
        {
            correct(measurement);
            state_.state = TrackPhase::Locked;
        } else
        {
            miss();
        }
        break;
    }
    return state_;
}

Box Tracker::gate() const
{
    const float px = state_.cx + state_.vx;
    const float py = state_.cy + state_.vy;
    const float grow = static_cast<float>(config_.coast_gate_growth * state_.frames_since_seen);
    const float mx = std::max(static_cast<float>(config_.gate_margin_min), config_.gate_margin_frac * width_) + grow;
    const float my = std::max(static_cast<float>(config_.gate_margin_min), config_.gate_margin_frac * height_) + grow;
    Box g;
    g.x0 = static_cast<int>(std::floor(px - (width_ - 1.0f) / 2.0f - mx));
    g.x1 = static_cast<int>(std::ceil(px + (width_ - 1.0f) / 2.0f + mx));
    g.y0 = static_cast<int>(std::floor(py - (height_ - 1.0f) / 2.0f - my));
    g.y1 = static_cast<int>(std::ceil(py + (height_ - 1.0f) / 2.0f + my));
    return g;
}

bool Tracker::measure(const DetectionResult& detection, const Box& gate, Blob& out) const
{
    Blob m;
    float wx = 0.0f;
    float wy = 0.0f;
    for (int k = 0; k < detection.blob_count; ++k)
    {
        const Blob& b = detection.blobs[k];
        if (!b.box.overlaps(gate))
        {
            continue;
        }
        m.box.merge(b.box);
        m.area += b.area;
        wx += b.cx * static_cast<float>(b.area);
        wy += b.cy * static_cast<float>(b.area);
    }
    if (m.area == 0)
    {
        return false;
    }
    m.cx = wx / static_cast<float>(m.area);
    m.cy = wy / static_cast<float>(m.area);
    out = m;
    return true;
}

void Tracker::start_tentative(const Blob& blob)
{
    state_ = TrackState{};
    state_.state = TrackPhase::Tentative;
    state_.cx = blob.cx;
    state_.cy = blob.cy;
    state_.area = blob.area;
    width_ = static_cast<float>(blob.box.width());
    height_ = static_cast<float>(blob.box.height());
    hits_ = 1;
    refresh_box();
}

void Tracker::correct(const Blob& measurement)
{
    const float px = state_.cx + state_.vx;
    const float py = state_.cy + state_.vy;
    const float rx = measurement.cx - px;
    const float ry = measurement.cy - py;
    state_.cx = px + config_.alpha * rx;
    state_.cy = py + config_.alpha * ry;
    state_.vx += config_.beta * rx;
    state_.vy += config_.beta * ry;
    width_ += config_.alpha * (static_cast<float>(measurement.box.width()) - width_);
    height_ += config_.alpha * (static_cast<float>(measurement.box.height()) - height_);
    state_.area = measurement.area;
    state_.frames_since_seen = 0;
    refresh_box();
}

void Tracker::miss()
{
    ++state_.frames_since_seen;
    if (state_.frames_since_seen > config_.coast_frames)
    {
        to_idle();
        return;
    }
    state_.state = TrackPhase::Coasting;
    state_.cx += state_.vx;
    state_.cy += state_.vy;
    state_.cx = std::clamp(state_.cx, 0.0f, static_cast<float>(kGridW - 1)); // never coast off the grid
    state_.cy = std::clamp(state_.cy, 0.0f, static_cast<float>(kGridH - 1));
    state_.vx *= config_.velocity_decay;
    state_.vy *= config_.velocity_decay;
    refresh_box();
}

void Tracker::to_idle()
{
    state_ = TrackState{};
    hits_ = 0;
    width_ = 0.0f;
    height_ = 0.0f;
}

void Tracker::refresh_box()
{
    // Box of the smoothed size centred on the filtered position. Not clamped: callers clamp to the frame.
    const int w = std::max(1, static_cast<int>(std::lround(width_)));
    const int h = std::max(1, static_cast<int>(std::lround(height_)));
    state_.box.x0 = static_cast<int>(std::lround(state_.cx - (static_cast<float>(w) - 1.0f) / 2.0f));
    state_.box.y0 = static_cast<int>(std::lround(state_.cy - (static_cast<float>(h) - 1.0f) / 2.0f));
    state_.box.x1 = state_.box.x0 + w - 1;
    state_.box.y1 = state_.box.y0 + h - 1;
}
} // namespace tracking
