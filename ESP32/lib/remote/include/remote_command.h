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
enum class CommandKind : uint8_t { Poll, Move, Stop, Mode, Fire };

/** @brief Operating mode requested by the HUD. */
enum class RemoteMode : uint8_t { Ai, User };

/** @brief One parsed message. Only the fields matching kind are meaningful. */
struct Command {
    CommandKind kind = CommandKind::Poll;
    int8_t pan = 0;                   ///< Move only: -1 left, 0, +1 right
    int8_t tilt = 0;                  ///< Move only: -1 down, 0, +1 up (up = larger tilt angle)
    RemoteMode mode = RemoteMode::Ai; ///< Mode only
    bool fire_on = false;             ///< Fire only
};

/**
 * @brief Parses one message.
 * @details Exact, case-sensitive match on the whole view (it need not be NUL-terminated, and the receive buffer is
 * not). Anything unrecognised (empty, unknown text, trailing bytes) yields a Poll command.
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
