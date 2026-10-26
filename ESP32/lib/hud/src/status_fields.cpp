#include "status_fields.h"

#include <cmath>

namespace hud
{
int16_t round_pixel(float v)
{
    if (!std::isfinite(v))
    {
        return 0;
    }
    if (v <= static_cast<float>(INT16_MIN))
    {
        return INT16_MIN;
    }
    if (v >= static_cast<float>(INT16_MAX))
    {
        return INT16_MAX;
    }
    return static_cast<int16_t>(std::lround(v));
}

void fill_pose_fields(ControlStatus& st, const aiming::AimMap& map, const HomeState& home)
{
    const aiming::Angles pose = st.angles;
    const aiming::Pixel px = map.to_pixel(pose);
    st.aim_x = round_pixel(px.x);
    st.aim_y = round_pixel(px.y);
    st.lim = aiming::limit_bits(pose);

    const aiming::ReachBox box = aiming::reach_box(map);
    st.rx0 = box.x0;
    st.ry0 = box.y0;
    st.rx1 = box.x1;
    st.ry1 = box.y1;

    st.home_ms = home.ai_mode ? home.home_remaining_ms : 0;
    // A non-finite pose compares false both ways, so it never reads as away from home.
    const bool away =
        std::fabs(pose.pan) > kReturningMarginDeg || std::fabs(pose.tilt - aiming::kTiltHomeDeg) > kReturningMarginDeg;
    st.returning = home.ai_mode && home.homed && away;
}
} // namespace hud
