#include "remote_command.h"

namespace remote
{
namespace
{
constexpr int kAimW = 320; ///< aim:<x> accepts 0..kAimW-1 (QVGA width)
constexpr int kAimH = 240; ///< aim:<y> accepts 0..kAimH-1

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

Command cal(CalAction a)
{
    Command c;
    c.kind = CommandKind::Calibrate;
    c.cal = a;
    return c;
}

Command speed(JogSpeed s)
{
    Command c;
    c.kind = CommandKind::Speed;
    c.speed = s;
    return c;
}

/** @brief Reads 1 to 3 decimal digits from the front of s into out and removes them; false if there are none. */
bool take_number(std::string_view& s, int& out)
{
    size_t n = 0;
    int value = 0;
    while (n < s.size() && s[n] >= '0' && s[n] <= '9')
    {
        if (n == 3)
        {
            return false; // a fourth digit
        }
        value = value * 10 + (s[n] - '0');
        ++n;
    }
    if (n == 0)
    {
        return false;
    }
    out = value;
    s.remove_prefix(n);
    return true;
}

/** @brief Parses the part after "aim:"; yields a Poll unless it is exactly "<x>,<y>" in range. */
Command parse_aim(std::string_view args)
{
    int x = 0;
    int y = 0;
    if (!take_number(args, x) || args.empty() || args.front() != ',')
    {
        return Command{};
    }
    args.remove_prefix(1);
    if (!take_number(args, y) || !args.empty() || x >= kAimW || y >= kAimH)
    {
        return Command{};
    }
    Command c;
    c.kind = CommandKind::Aim;
    c.aim_x = static_cast<int16_t>(x);
    c.aim_y = static_cast<int16_t>(y);
    return c;
}
} // namespace

Command parse(std::string_view message)
{
    if (message.empty())
    {
        return Command{};
    }
    constexpr std::string_view kAimPrefix = "aim:";
    if (message.starts_with(kAimPrefix))
    {
        message.remove_prefix(kAimPrefix.size());
        return parse_aim(message);
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
        {.text = "cal:start", .cmd = cal(CalAction::Start)},
        {.text = "cal:point", .cmd = cal(CalAction::Point)},
        {.text = "cal:cancel", .cmd = cal(CalAction::Cancel)},
        {.text = "speed:slow", .cmd = speed(JogSpeed::Slow)},
        {.text = "speed:normal", .cmd = speed(JogSpeed::Normal)},
        {.text = "speed:fast", .cmd = speed(JogSpeed::Fast)},
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
