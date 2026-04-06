/**
 * @file SystemControl.h
 * @brief Operational states for the turret system.
 */

#pragma once

#include <stdint.h>

/**
 * @class SystemControl
 * @brief Encapsulates the primary control authority of the turret.
 * @details Provides a wrapper around system modes to allow for method-based
 * state manipulation and string conversion.
 */
class SystemControl
{
public:
    /**
     * @enum Mode
     * @brief Internal mode values.
     */
    enum Mode : uint8_t {
        USER_MODE = 0, /**< Manual override: Joystick inputs drive motors. */
        AI_MODE = 1    /**< Autonomous: Computer Vision drives motors. */
    };

    /**
     * @brief Construct a new SystemControl object.
     * @param mode Initial mode (defaults to AI_MODE).
     */
    constexpr SystemControl(Mode mode = AI_MODE) : _mode(mode) {}

    /** @brief Checks if the system is in AI mode. @return true if AI_MODE. */
    bool is_ai() const { return this->_mode == AI_MODE; }

    /** @brief Checks if the system is in User mode. @return true if USER_MODE. */
    bool is_user() const { return this->_mode == USER_MODE; }

    /**
     * @brief Toggles the state between USER_MODE and AI_MODE.
     * @return The new Mode state after toggling.
     */
    Mode toggle_mode()
    {
        this->_mode = (this->_mode == USER_MODE) ? AI_MODE : USER_MODE;
        return this->_mode;
    }

    /**
     * @brief Converts the current mode to a human-readable string.
     * @return const char* "USER_MODE" or "AI_MODE".
     */
    const char* to_string() const { return (this->_mode == USER_MODE) ? "USER_MODE" : "AI_MODE"; }

    /** @brief Gets the underlying Mode enum. @return Mode. */
    Mode get_mode() const { return this->_mode; }

    /** @brief Implicit conversion to uint8_t for simple storage. */
    operator uint8_t() const { return static_cast<uint8_t>(this->_mode); }

    /** @brief Equality comparison with Mode enum. */
    bool operator==(Mode m) const { return this->_mode == m; }

    /** @brief Inequality comparison with Mode enum. */
    bool operator!=(Mode m) const { return this->_mode != m; }

private:
    Mode _mode; /**< Internal storage of the system control state. */
};