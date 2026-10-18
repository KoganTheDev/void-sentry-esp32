/**
 * @file aim_policy.h
 * @brief Decides where the turret should point from the tracker state (pure C++20, natively tested).
 * @details Locked/Coasting: aim at the mapped target position, changing the setpoint only when the new aim is more
 * than deadband_deg away on either axis (no jitter). Tentative: hold. Idle: hold, and home_delay_ms after the target
 * was last seen, return Home exactly once. Time is millis()-style uint32 and wraparound-safe.
 */

#pragma once

#include "aim_map.h"
#include "tracking_types.h"

#include <cstdint>

namespace aiming
{
struct TrackView {
    tracking::TrackPhase state = tracking::TrackPhase::Idle;
    float x = 0.0f; ///< target centroid, frame pixels
    float y = 0.0f;
};

enum class AimKind : uint8_t { Hold, Aim, Home };

/**
 * @brief Result of one policy update. For Aim and Home, angles is the new target pose. For Hold, angles is the
 * current commanded pose (the last Aim or Home, or the Home pose before any lock); consumers act only on Aim and Home.
 */
struct AimCommand {
    AimKind kind = AimKind::Hold;
    Angles angles;
};

struct PolicyConfig {
    float deadband_deg = 1.0f;
    uint32_t home_delay_ms = 2000;
};

class AimPolicy
{
public:
    explicit AimPolicy(const AimMap& map, const PolicyConfig& config = PolicyConfig{});

    void set_map(const AimMap& map) { map_ = map; }
    AimCommand update(const TrackView& track, uint32_t now_ms);
    void reset(); ///< forget the setpoint and the lost-target timer (mode changes)

private:
    AimMap map_;
    PolicyConfig config_;
    bool have_setpoint_ = false; ///< false = the next lock aims even if it equals setpoint_ (after Home or reset)
    Angles setpoint_{0.0f, kTiltHomeDeg}; ///< current commanded pose; starts at Home, follows Aim and Home
    bool seen_ = false;
    bool homed_ = false;
    uint32_t last_seen_ms_ = 0;
};
} // namespace aiming
