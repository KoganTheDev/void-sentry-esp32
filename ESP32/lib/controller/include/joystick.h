/**
 * @file Joystick.h
 * @brief Driver for the HW-504 Analog 2-Axis Joystick + Integrated Button.
 * @details This module provides filtered, calibrated access to a standard
 * dual-potentiometer joystick. It handles noise reduction via multi-sampling
 * and eliminates "ghost" movement via a software deadzone.
 */

#pragma once

#include <Arduino.h>

/** @name Signal Processing Constants
 * @{ */
#define JOYSTICK_DEADZONE 150       ///< Minimum deflection required to trigger movement.
#define JOYSTICK_RESOLUTION_BITS 12 ///< 12-bit ADC (0-4095) for ESP32.
#define JOYSTICK_SAMPLES 5          ///< Number of samples to average for noise reduction.
#define BUTTON_DEBOUNCE_MS 50       ///< Software debounce interval for the Z-axis tactile switch.
/** @} */

/**
 * @class Joystick
 * @brief High-level interface for 12-bit analog joystick input.
 * @details Manages the translation of raw ADC voltage into meaningful movement vectors.
 * Includes auto-calibration routines to compensate for mechanical center-point drift.
 */
class Joystick
{
private:
    uint8_t _pin_x;           ///< Analog GPIO for Horizontal axis.
    uint8_t _pin_y;           ///< Analog GPIO for Vertical axis.
    uint8_t _pin_z;           ///< Digital GPIO for Integrated Button (Z-axis).
    int _deadzone;            ///< Current deadzone threshold.
    int _center_x, _center_y; ///< Captured "zero" point determined during @ref begin().

    // Button state for Z-axis
    bool _last_btn_state;              ///< Previous state for edge detection logic.
    unsigned long _last_debounce_time; ///< Last timestamp of state transition.

    /** * @brief Reads multiple ADC samples and returns the average.
     * @param pin The GPIO to sample.
     * @return Averaged 12-bit value (0-4095).
     */
    int _read_raw(uint8_t pin) const;

    /** * @brief Filters raw input against the deadzone.
     * @param raw_val The current ADC reading.
     * @param center The calibrated center for this axis.
     * @return Deflection relative to center (e.g., -2048 to 2047). Returns 0 if within deadzone.
     */
    int _process_axis(int raw_val, int center) const;

    /** * @brief Maps a raw deflection value to a specific output range.
     * @param val The relative deflection value.
     * @param min_out Minimum mapped value (e.g., -255).
     * @param max_out Maximum mapped value (e.g., 255).
     * @return Rescaled integer for motor speed or PWM duty cycle.
     */
    int _map_speed(int val, int min_out, int max_out) const;

public:
    /**
     * @name Initialization
     * @{
     */

    /**
     * @brief Construct a new Joystick object.
     * @param pin_x Analog Pin (VRX).
     * @param pin_y Analog Pin (VRY).
     * @param pin_z Digital Pin (SW).
     * @param deadzone The sensitivity threshold (Defaults to @ref JOYSTICK_DEADZONE).
     */
    Joystick(uint8_t pin_x, uint8_t pin_y, uint8_t pin_z, int deadzone = JOYSTICK_DEADZONE)
        : _pin_x(pin_x), _pin_y(pin_y), _pin_z(pin_z), _deadzone(deadzone), _center_x(0), _center_y(0),
          _last_btn_state(HIGH), _last_debounce_time(0){};

    /**
     * @brief Configures hardware and performs auto-calibration.
     * @warning **Important**: The joystick must be at the neutral (resting) position
     * during this call. It averages 10 samples to establish the baseline center.
     */
    void begin();
    /** @} */

    /**
     * @name Positional Data
     * @{
     */

    /** @return Relative horizontal deflection (-2048 to 2048). */
    int get_x() const { return _process_axis(this->_read_raw(this->_pin_x), this->_center_x); }

    /** @return Relative vertical deflection (-2048 to 2048). */
    int get_y() const { return _process_axis(this->_read_raw(this->_pin_y), this->_center_y); }

    /**
     * @brief Checks if the joystick is currently deflected beyond the deadzone.
     * @return true if either axis is active.
     */
    bool is_active() const;
    /** @} */

    /**
     * @name Button Input
     * @{
     */

    /** * @brief Detects a button press (Falling Edge).
     * @return true only on the frame the button is first depressed.
     */
    bool is_z_pressed();

    /** * @brief Check the current physical state of the button.
     * @return true if the button is currently held down.
     */
    bool is_z_held() const { return digitalRead(this->_pin_z) == LOW; }
    /** @} */

    /**
     * @name Motor Control Mapping
     * @{
     */

    /** * @brief Maps X deflection to a motor speed range.
     * @param min_out Minimum speed (default -255).
     * @param max_out Maximum speed (default 255).
     */
    int get_speed_x(int min_out = -255, int max_out = 255) { return this->_map_speed(get_x(), min_out, max_out); }

    /** * @brief Maps Y deflection to a motor speed range.
     * @param min_out Minimum speed (default -255).
     * @param max_out Maximum speed (default 255).
     */
    int get_speed_y(int min_out = -255, int max_out = 255) { return this->_map_speed(get_y(), min_out, max_out); }
    /** @} */
};