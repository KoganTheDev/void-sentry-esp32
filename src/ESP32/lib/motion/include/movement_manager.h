/**
 * @file movement_manager.h
 * @brief implementation of the turret movement system.
 */
#pragma once

#include "base_movement_manager.h"
#include <ESP32Servo.h>
#include <Stepper.h>

const int SERVO_MIN_ANGLE = 0;
const int SERVO_MAX_ANGLE = 180;

/**
 * @class MovementManager
 * @brief Manages physical actuators for turret positioning.
 */
class MovementManager : public BaseMovementManager
{
private:
    /** @brief Private Stepper instance for horizontal (X) rotation. */
    Stepper& _stepper;

    /** @brief Private Servo instance for vertical (Y) tilting. */
    Servo& _servo;

    static constexpr int _SERVO_INCREMENT = 5;
    static constexpr int _STEP_INCREMENT = 5;
    static constexpr int _STEPPER_SPEED = 10;

public:
    /**
     * @brief Construct a new Movement Manager object.
     * @param stepper Reference to the initialized Stepper motor object.
     * @param servo Reference to the initialized Servo motor object.
     */
    MovementManager(Stepper& stepper, Servo& servo) : _stepper(stepper), _servo(servo)
    {
        this->_stepper.setSpeed(this->_STEPPER_SPEED);
    }

    /**
     * @brief Implements relative movement using Stepper and Servo hardware.
     * @details Translates MoveX and MoveY into actuator
     * actions.
     * @param x Directional command for the stepper motor.
     * @param y Directional command for the servo motor.
     */
    virtual void move_relative(const std::tuple<MoveX, MoveY> move_directions);

    /**
     * @brief Executes horizontal rotation (Yaw) using the stepper motor.
     * @details Checks the MoveX enum; if Left or Right, the stepper
     * moves by the predefined _STEP_INCREMENT. Negative steps rotate in one
     * direction, positive in the other.
     * @param yaw_direction The direction to rotate (Left, Right, or None).
     */
    void move_stepper(const MoveX yaw_direction);

    /**
     * @brief Executes vertical tilting (Pitch) using the servo motor.
     * @details Reads current servo position and adjusts it by _SERVO_INCREMENT.
     * The final position is constrained between SERVO_MIN_ANGLE and
     * SERVO_MAX_ANGLE to prevent mechanical stall or gear damage.
     * @param pitch_direction The direction to tilt (Up, Down, or None).
     */
    void move_servo(const MoveY pitch_direction);
};