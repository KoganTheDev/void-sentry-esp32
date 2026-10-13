#include "aim_policy.h"

#include <cmath>

namespace aiming
{
AimPolicy::AimPolicy(const AimMap& map, const PolicyConfig& config) : map_(map), config_(config) {}

void AimPolicy::reset()
{
    have_setpoint_ = false;
    seen_ = false;
    homed_ = false;
}

AimCommand AimPolicy::update(const TrackView& track, uint32_t now_ms)
{
    using tracking::TrackPhase;
    if (track.state == TrackPhase::Locked || track.state == TrackPhase::Coasting)
    {
        seen_ = true;
        homed_ = false;
        last_seen_ms_ = now_ms;
        const Angles a = map_.aim(track.x, track.y);
        if (!have_setpoint_ || std::fabs(a.pan - setpoint_.pan) > config_.deadband_deg ||
            std::fabs(a.tilt - setpoint_.tilt) > config_.deadband_deg)
        {
            setpoint_ = a;
            have_setpoint_ = true;
            return AimCommand{AimKind::Aim, a};
        }
        return AimCommand{AimKind::Hold, setpoint_};
    }
    if (track.state == TrackPhase::Idle && seen_ && !homed_ &&
        static_cast<uint32_t>(now_ms - last_seen_ms_) >= config_.home_delay_ms)
    {
        homed_ = true;
        have_setpoint_ = false;
        setpoint_ = Angles{0.0f, kTiltHomeDeg};
        return AimCommand{AimKind::Home, setpoint_};
    }
    return AimCommand{AimKind::Hold, setpoint_};
}
} // namespace aiming
