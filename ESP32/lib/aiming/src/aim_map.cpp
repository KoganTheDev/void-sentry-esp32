#include "aim_map.h"

#include <algorithm>
#include <cmath>

namespace aiming
{
namespace
{
constexpr float kHFovDeg = 66.0f;
constexpr float kVFovDeg = 50.0f;

bool finite(float v) { return std::isfinite(v); }
} // namespace

Angles clamp_angles(Angles a)
{
    if (!finite(a.pan))
    {
        a.pan = 0.0f; // NaN/inf has no meaningful angle: fall back to home
    }
    if (!finite(a.tilt))
    {
        a.tilt = kTiltHomeDeg;
    }
    a.pan = std::clamp(a.pan, kPanMinDeg, kPanMaxDeg);
    a.tilt = std::clamp(a.tilt, kTiltMinDeg, kTiltMaxDeg);
    return a;
}

Angles AimMap::aim(float x, float y) const { return clamp_angles(Angles{pan_a * x + pan_b, tilt_a * y + tilt_b}); }

AimMap default_map()
{
    AimMap m;
    m.pan_a = kHFovDeg / kFrameW; // image right -> pan right
    m.pan_b = -m.pan_a * (kFrameW / 2.0f);
    m.tilt_a = -kVFovDeg / kFrameH; // image down -> tilt down (smaller servo angle)
    m.tilt_b = kTiltHomeDeg - m.tilt_a * (kFrameH / 2.0f);
    return m;
}

std::optional<AimMap> fit(const CalPoint& p1, const CalPoint& p2)
{
    for (float v : {p1.x, p1.y, p1.pan, p1.tilt, p2.x, p2.y, p2.pan, p2.tilt})
    {
        if (!finite(v))
        {
            return std::nullopt;
        }
    }
    const float dx = p2.x - p1.x;
    const float dy = p2.y - p1.y;
    if (!finite(dx) || !finite(dy) || std::fabs(dx) < kMinCalDx || std::fabs(dy) < kMinCalDy)
    {
        return std::nullopt;
    }
    AimMap m;
    m.pan_a = (p2.pan - p1.pan) / dx;
    m.pan_b = p1.pan - m.pan_a * p1.x;
    m.tilt_a = (p2.tilt - p1.tilt) / dy;
    m.tilt_b = p1.tilt - m.tilt_a * p1.y;
    // Finite inputs can still overflow here (e.g. pan values near +/-FLT_MAX), so check the result too.
    if (!finite(m.pan_a) || !finite(m.pan_b) || !finite(m.tilt_a) || !finite(m.tilt_b))
    {
        return std::nullopt;
    }
    return m;
}

int32_t pan_deg_to_steps(float deg)
{
    if (!finite(deg))
    {
        return 0;
    }
    deg = std::clamp(deg, kPanMinDeg, kPanMaxDeg);
    return static_cast<int32_t>(std::lround(deg * static_cast<float>(kStepsPerRev) / 360.0f));
}

float pan_steps_to_deg(int32_t steps) { return static_cast<float>(steps) * 360.0f / static_cast<float>(kStepsPerRev); }
} // namespace aiming
