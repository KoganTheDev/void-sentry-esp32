/**
 * @file system_control_types.h
 * @brief Operating modes of the turret system.
 */

#pragma once

#include <stdint.h>

/**
 * @enum ControlMode
 * @brief Who drives the turret right now.
 */
enum class ControlMode : uint8_t {
    Ai = 0,       /**< Autonomous: the tracker drives the turret through the aim policy. */
    User = 1,     /**< Manual override: joystick inputs jog the turret. */
    Calibrate = 2 /**< Two-point calibration: joystick jogs, short press records a point. */
};

/**
 * @brief Converts a mode to its lowercase wire name.
 * @return "ai", "user" or "calibrate".
 */
inline const char* to_string(ControlMode mode)
{
    switch (mode)
    {
    case ControlMode::Ai:
        return "ai";
    case ControlMode::User:
        return "user";
    case ControlMode::Calibrate:
        return "calibrate";
    }
    return "ai";
}
