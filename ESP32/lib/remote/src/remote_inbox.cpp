#include "remote_inbox.h"

namespace remote
{
void RemoteInbox::submit(const Command& cmd, uint32_t now_ms)
{
    switch (cmd.kind)
    {
    case CommandKind::Move:
        has_move_ = true;
        pan_ = cmd.pan;
        tilt_ = cmd.tilt;
        move_ms_ = now_ms;
        break;
    case CommandKind::Stop:
        has_move_ = false;
        break;
    case CommandKind::Mode:
        mode_request_ = cmd.mode;
        break;
    case CommandKind::Poll:
    case CommandKind::Fire:
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
    return Jog{kJogSpeed * static_cast<float>(pan_), kJogSpeed * static_cast<float>(tilt_)};
}

std::optional<RemoteMode> RemoteInbox::take_mode_request()
{
    const std::optional<RemoteMode> out = mode_request_;
    mode_request_.reset();
    return out;
}
} // namespace remote
