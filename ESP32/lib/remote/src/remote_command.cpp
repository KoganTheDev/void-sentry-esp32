#include "remote_command.h"

#include <cstring>

namespace remote
{
namespace
{
struct Entry {
    const char* text;
    Command cmd;
};

Command move(int8_t pan, int8_t tilt)
{
    Command c;
    c.kind = CommandKind::Move;
    c.pan = pan;
    c.tilt = tilt;
    return c;
}

Command simple(CommandKind kind)
{
    Command c;
    c.kind = kind;
    return c;
}

Command mode(RemoteMode m)
{
    Command c;
    c.kind = CommandKind::Mode;
    c.mode = m;
    return c;
}

Command fire(bool on)
{
    Command c;
    c.kind = CommandKind::Fire;
    c.fire_on = on;
    return c;
}
} // namespace

Command parse(const char* data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        return Command{};
    }
    const Entry table[] = {
        {"move:up", move(0, 1)},
        {"move:down", move(0, -1)},
        {"move:left", move(-1, 0)},
        {"move:right", move(1, 0)},
        {"move:up-left", move(-1, 1)},
        {"move:up-right", move(1, 1)},
        {"move:down-left", move(-1, -1)},
        {"move:down-right", move(1, -1)},
        {"move:stop", simple(CommandKind::Stop)},
        {"mode:ai", mode(RemoteMode::Ai)},
        {"mode:user", mode(RemoteMode::User)},
        {"fire:on", fire(true)},
        {"fire:off", fire(false)},
    };
    for (const Entry& e : table)
    {
        if (std::strlen(e.text) == len && std::memcmp(data, e.text, len) == 0)
        {
            return e.cmd;
        }
    }
    return Command{};
}
} // namespace remote
