/**
 * @file SystemControl.h
 * @brief Defines the operational states for the turret system.
 */

#pragma once

#include <Arduino.h>

// TODO: refactor some more, i want it to work like this: state.to_string() instead of to_string(state)

/**
 * @enum SystemControl
 * @brief Represents the primary control authority of the turret.
 */
enum class SystemControl : uint8_t {
    USER_MODE = 0, /**< Manual override: Joystick inputs are mapped to motors. */
    AI_MODE = 1    /**< Autonomous: Detection logic and Computer Vision drive motors. */
};

/**
 * @brief Helper utility to convert the SystemControl enum to a human-readable
 * string.
 * @param mode The current system mode.
 * @return A descriptive string for logging.
 */
inline const char* to_string(const SystemControl& mode)
{
    return (mode == SystemControl::USER_MODE) ? "USER_MODE" : "AI_MODE";
}

/**
 * @brief Toggles the system state between User and AI control.
 * @param state The reference to the current system state.
 */
static inline SystemControl toggle_mode(const SystemControl& state)
{
    return (state == SystemControl::USER_MODE) ? SystemControl::AI_MODE : SystemControl::USER_MODE;
}
