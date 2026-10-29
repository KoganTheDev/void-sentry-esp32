#include "remote_inbox.h"

#include <utility>

namespace remote
{
void RemoteInbox::submit(const Command& cmd, uint32_t now_ms)
{
    using enum CommandKind;
    switch (cmd.kind)
    {
    case Move:
        has_move_ = true;
        pan_ = cmd.pan;
        tilt_ = cmd.tilt;
        move_ms_ = now_ms;
        break;
    case Stop:
        has_move_ = false;
        break;
    case Mode:
        mode_request_ = cmd.mode;
        break;
    case Calibrate:
        cal_request_ = cmd.cal;
        break;
    case Aim:
        has_move_ = false; // a new aim point replaces any running HUD jog
        aim_request_ = AimPixel{cmd.aim_x, cmd.aim_y};
        break;
    case Speed:
        speed_ = cmd.speed;
        break;
    case Poll:
    case Fire:
        break;
    }
}

bool RemoteInbox::jog_active(uint32_t now_ms) const
{
    return has_move_ && static_cast<uint32_t>(now_ms - move_ms_) < kJogTimeoutMs;
}

Jog RemoteInbox::jog(uint32_t now_ms) const
{
    if (!jog_active(now_ms))
    {
        return Jog{};
    }
    float mag = kJogSpeed;
    switch (speed_)
    {
    case JogSpeed::Slow:
        mag = kJogSpeedSlow;
        break;
    case JogSpeed::Fast:
        mag = kJogSpeedFast;
        break;
    case JogSpeed::Normal:
        break;
    }
    return Jog{mag * static_cast<float>(pan_), mag * static_cast<float>(tilt_)};
}

std::optional<RemoteMode> RemoteInbox::take_mode_request() { return std::exchange(mode_request_, std::nullopt); }

std::optional<CalAction> RemoteInbox::take_cal_request() { return std::exchange(cal_request_, std::nullopt); }

std::optional<AimPixel> RemoteInbox::take_aim_request() { return std::exchange(aim_request_, std::nullopt); }
} // namespace remote
