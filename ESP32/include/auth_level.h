/**
 * @file auth_level.h
 * @brief What a /commands client may do: watch only, or control the turret (shared by the firmware and native tests).
 */

#pragma once

#include <stdint.h>

/**
 * @enum AuthLevel
 * @brief Access of one /commands session; sent to the HUD as the reply key "auth" (the numeric value).
 */
enum class AuthLevel : uint8_t {
    Viewer = 0, ///< A PIN is set and the session has no valid login: status only, control commands are dropped.
    Authed = 1, ///< The session carries a valid login cookie: full control.
    Open = 2    ///< No PIN has been set yet, so there is nothing to log in with: control is open to everyone.
};

/**
 * @brief Access level from the two facts that decide it.
 * @param pin_set A PIN exists (WiFi setup has been done at least once).
 * @param session_valid The session's login token is in the session table.
 */
constexpr AuthLevel auth_level(bool pin_set, bool session_valid)
{
    if (!pin_set)
    {
        return AuthLevel::Open;
    }
    return session_valid ? AuthLevel::Authed : AuthLevel::Viewer;
}

/** @brief True if the level allows control commands (everything except Viewer). */
constexpr bool may_control(AuthLevel level) { return level != AuthLevel::Viewer; }
