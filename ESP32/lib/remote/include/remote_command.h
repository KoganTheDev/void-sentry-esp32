/**
 * @file remote_command.h
 * @brief Parses the HUD's /commands text messages into typed commands (pure C++20, natively tested).
 * @details No Arduino or ESP-IDF headers and no heap use, so the parser runs on the host and in the httpd handler.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace remote
{
/** @brief What a /commands message asks for. Poll is the keepalive and anything unrecognised. */
enum class CommandKind : uint8_t { Poll, Move, Stop, Mode, Fire, Calibrate, Aim, Speed };

/** @brief Operating mode requested by the HUD. */
enum class RemoteMode : uint8_t { Ai, User };

/** @brief Calibration action requested by the HUD; same semantics as the physical button. */
enum class CalAction : uint8_t {
    Start,  ///< like a long press when not calibrating
    Point,  ///< like a short press while calibrating
    Cancel, ///< like a long press while calibrating
};

/** @brief HUD jog magnitude setting. */
enum class JogSpeed : uint8_t { Slow, Normal, Fast };

/** @brief One parsed message. Only the fields matching kind are meaningful. */
struct Command {
    CommandKind kind = CommandKind::Poll;
    int8_t pan = 0;                    ///< Move only: -1 left, 0, +1 right
    int8_t tilt = 0;                   ///< Move only: -1 down, 0, +1 up (up = larger tilt angle)
    RemoteMode mode = RemoteMode::Ai;  ///< Mode only
    bool fire_on = false;              ///< Fire only
    CalAction cal = CalAction::Start;  ///< Calibrate only
    int16_t aim_x = 0;                 ///< Aim only: image pixel, 0..319
    int16_t aim_y = 0;                 ///< Aim only: image pixel, 0..239
    JogSpeed speed = JogSpeed::Normal; ///< Speed only
};

/**
 * @brief Parses one message.
 * @details Exact, case-sensitive match on the whole view (it need not be NUL-terminated, and the receive buffer is
 * not). Anything unrecognised (empty, unknown text, trailing bytes) yields a Poll command. `aim:<x>,<y>` takes 1 to 3
 * plain decimal digits per number (no sign, spaces or extra bytes), x in 0..319 and y in 0..239; anything else is a
 * Poll.
 */
Command parse(std::string_view message);

/** @brief Receives parsed commands; implemented by the controller (device) and fakes (tests). */
class BaseRemoteSink
{
public:
    virtual ~BaseRemoteSink() = default;
    /** @brief Delivers one parsed command (called from the httpd task). */
    virtual void submit(const Command& cmd) = 0;
};
} // namespace remote
