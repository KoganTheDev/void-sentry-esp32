#include "reply_json.h"

#include <cstdarg>
#include <cstdio>

namespace hud
{
namespace
{
constexpr char kNoDetection[] = "{\"detected\":0,\"x\":0,\"y\":0,\"width\":320,\"height\":240,\"pixels\":0}";
constexpr char kOverflow[] = "{\"detected\":0}";

/**
 * @brief snprintf appended at out + len. Returns false if the text does not fit with its NUL or the format fails;
 * otherwise advances len.
 */
bool put(char* out, size_t size, size_t& len, const char* fmt, ...) __attribute__((format(printf, 4, 5)));

bool put(char* out, size_t size, size_t& len, const char* fmt, ...)
{
    if (len >= size)
    {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    const int n = vsnprintf(out + len, size - len, fmt, args);
    va_end(args);
    if (n < 0 || static_cast<size_t>(n) >= size - len)
    {
        return false;
    }
    len += static_cast<size_t>(n);
    return true;
}

/** @brief Copies text (with its NUL) into out if it fits; otherwise writes an empty string. Returns the length. */
size_t copy_text(const char* text, size_t text_len, char* out, size_t size)
{
    if (size == 0)
    {
        return 0;
    }
    if (text_len + 1 > size)
    {
        out[0] = '\0';
        return 0;
    }
    for (size_t i = 0; i <= text_len; ++i)
    {
        out[i] = text[i];
    }
    return text_len;
}

bool put_tracker(const TrackSnapshot& t, char* out, size_t size, size_t& len)
{
    return put(out, size, len,
               "{\"detected\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"pixels\":%d,"
               "\"state\":\"%s\",\"id\":%u,\"bx\":%d,\"by\":%d,\"bw\":%d,\"bh\":%d,"
               "\"vx\":%.1f,\"vy\":%.1f",
               t.detected() ? 1 : 0, t.x, t.y, t.frame_width, t.frame_height, t.area_px, tracking::to_string(t.state),
               static_cast<unsigned>(t.id), t.bx, t.by, t.bw, t.bh, static_cast<double>(t.vx),
               static_cast<double>(t.vy));
}

bool put_status(const ControlStatus& c, uint32_t viewers, AuthLevel auth, char* out, size_t size, size_t& len)
{
    return put(out, size, len,
               ",\"mode\":\"%s\",\"cal\":%u,\"cal_state\":\"%s\",\"calibrated\":%d,"
               "\"pan\":%.1f,\"tilt\":%.1f,\"cal_error\":\"%s\"",
               to_string(c.mode), static_cast<unsigned>(c.cal_points), cal_state_name(c.cal_state),
               c.calibrated ? 1 : 0, static_cast<double>(c.angles.pan), static_cast<double>(c.angles.tilt),
               c.cal_failure ? c.cal_failure : "") &&
           put(out, size, len,
               ",\"cal_saves\":%u,\"cal_miss\":%u,\"stick\":%d,\"ax\":%d,\"ay\":%d,\"lim\":%u,"
               "\"rx0\":%d,\"ry0\":%d,\"rx1\":%d,\"ry1\":%d,\"home\":%u,\"ret\":%d,\"spd\":\"%s\",\"vw\":%u,\"auth\":%"
               "u",
               static_cast<unsigned>(c.cal_saves), static_cast<unsigned>(c.cal_miss), c.stick ? 1 : 0,
               static_cast<int>(c.aim_x), static_cast<int>(c.aim_y), static_cast<unsigned>(c.lim),
               static_cast<int>(c.rx0), static_cast<int>(c.ry0), static_cast<int>(c.rx1), static_cast<int>(c.ry1),
               static_cast<unsigned>(c.home_ms), c.returning ? 1 : 0, speed_name(c.speed),
               static_cast<unsigned>(viewers), static_cast<unsigned>(auth));
}
} // namespace

size_t format_reply(const ReplyInput& in, char* out, size_t size)
{
    if (in.track == nullptr)
    {
        return copy_text(kNoDetection, sizeof(kNoDetection) - 1, out, size);
    }
    size_t len = 0;
    bool ok = put_tracker(*in.track, out, size, len);
    if (ok && in.status != nullptr)
    {
        ok = put_status(*in.status, in.viewers, in.auth, out, size, len);
    }
    // The closing brace needs one more byte besides the NUL.
    if (ok && len + 2 <= size)
    {
        out[len++] = '}';
        out[len] = '\0';
        return len;
    }
    return copy_text(kOverflow, sizeof(kOverflow) - 1, out, size);
}

const char* cal_state_name(aiming::CalState st)
{
    switch (st)
    {
    case aiming::CalState::WaitPoint1:
        return "point1";
    case aiming::CalState::WaitPoint2:
        return "point2";
    case aiming::CalState::Done:
        return "done";
    case aiming::CalState::Failed:
        return "failed";
    case aiming::CalState::Idle:
        break;
    }
    return "idle";
}

const char* speed_name(uint8_t speed)
{
    switch (speed)
    {
    case 0:
        return "slow";
    case 2:
        return "fast";
    default:
        break;
    }
    return "normal";
}
} // namespace hud
