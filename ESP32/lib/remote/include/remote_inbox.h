/**
 * @file remote_inbox.h
 * @brief Holds the latest remote command state with a dead-man jog timeout (pure C++20, natively tested).
 * @details Not thread-safe by itself; the owner (Controller) guards it with a spinlock. Time is millis()-style
 * uint32 and wraparound-safe.
 */

#pragma once

#include "remote_command.h"

#include <optional>

namespace remote
{
constexpr uint32_t kJogTimeoutMs = 300; ///< a jog expires this long after the last Move
constexpr float kJogSpeed = 0.6f;       ///< jog magnitude per axis while active

/** @brief Jog request in the same units as Joystick/Turret::jog. */
struct Jog {
    float pan = 0.0f;
    float tilt = 0.0f;
};

/** @brief Stores the latest Move (with timeout) and the pending mode request. */
class RemoteInbox
{
public:
    /** @brief Applies a command. Move stores direction and time, Stop ends the jog, Mode stores a request. */
    void submit(const Command& cmd, uint32_t now_ms);

    /** @brief True if a Move arrived and less than kJogTimeoutMs have passed since. */
    bool jog_active(uint32_t now_ms) const;

    /** @brief kJogSpeed times the direction while active, else {0, 0}. */
    Jog jog(uint32_t now_ms) const;

    /** @brief The latest Mode command, returned once. */
    std::optional<RemoteMode> take_mode_request();

private:
    bool has_move_ = false;
    int8_t pan_ = 0;
    int8_t tilt_ = 0;
    uint32_t move_ms_ = 0;
    std::optional<RemoteMode> mode_request_;
};
} // namespace remote
