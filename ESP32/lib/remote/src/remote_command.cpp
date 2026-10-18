#include "remote_command.h"

namespace remote
{
namespace
{
struct Entry {
    std::string_view text;
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

Command parse(std::string_view message)
{
    if (message.empty())
    {
        return Command{};
    }
    const Entry table[] = {
        {.text = "move:up", .cmd = move(0, 1)},
        {.text = "move:down", .cmd = move(0, -1)},
        {.text = "move:left", .cmd = move(-1, 0)},
        {.text = "move:right", .cmd = move(1, 0)},
        {.text = "move:up-left", .cmd = move(-1, 1)},
        {.text = "move:up-right", .cmd = move(1, 1)},
        {.text = "move:down-left", .cmd = move(-1, -1)},
        {.text = "move:down-right", .cmd = move(1, -1)},
        {.text = "move:stop", .cmd = simple(CommandKind::Stop)},
        {.text = "mode:ai", .cmd = mode(RemoteMode::Ai)},
        {.text = "mode:user", .cmd = mode(RemoteMode::User)},
        {.text = "fire:on", .cmd = fire(true)},
        {.text = "fire:off", .cmd = fire(false)},
    };
    for (const Entry& e : table)
    {
        if (message == e.text)
        {
            return e.cmd;
        }
    }
    return Command{};
}
} // namespace remote
