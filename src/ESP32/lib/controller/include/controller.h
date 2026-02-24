/**
 * @file Controller.h
 * @brief Header file for the Controller class.
 * @details This class serves as the central brain of the turret system,
 * coordinating input from the joystick, data from the detection module,
 * and executing commands via the movement manager.
 */
#pragma once

#include "base_detection_module.h"
#include "base_movement_manager.h"
#include "joystick.h"
#include "move_types.h"
#include "system_control_types.h"
#include <cstdint>
#include <stdlib.h>
#include <tuple>

// Forward declaration
class Camera;

/**
 * @class Controller
 * @brief Handles logic for object detection and movement calculations.
 * details The Controller class implements the main control loop. It arbitrates
 * between manual joystick input and autonomous detection tracking, ensuring
 * that the BaseMovementManager receives the correct vector for the turret
 * motors.
 */
class Controller
{
private:
    /** @brief Holds the current state of the turret - either "User Mode" or "AI Mode" */
    SystemControl _system_control_state;

    /** @brief Reference to the hardware abstraction for motor control. */
    BaseMovementManager& _movement_manager;

    /** @brief Reference to the AI/Computer Vision module for target acquisition. */
    BaseDetectionModule& _detection_module;

    /** @brief Reference to the physical or virtual joystick input handler. */
    Joystick& _joystick;

    /** @brief Reference to the camera for frame capture. */
    Camera& _camera;

public:
    /**
     * @brief Construct a new Controller object.
     * @param movement_manager Reference to an implementation of BaseMovementManager.
     * @param detection_module Reference to an implementation of BaseDetectionModule.
     * @param joystick Reference to the Joystick input handler.
     * @param camera Reference to the Camera object for frame capture.
     */
    Controller(BaseMovementManager& movement_manager, BaseDetectionModule& detection_module, Joystick& joystick,
               Camera& camera)
        : _movement_manager(movement_manager), _detection_module(detection_module), _joystick(joystick), _camera(camera)
    {
        this->_system_control_state = SystemControl::AI_MODE;
    }

    /**
     * @brief Destroy the Controller object.
     * @details Ensures all movement is halted and resources are released safely.
     */
    ~Controller() {}

    /**
     * @brief Main execution loop for the turret system.
     * @details When called, this method:
     * - In USER_MODE: reads joystick and commands motors
     * - In AI_MODE: captures frame, runs motion detection, and commands motors
     */
    void run();
};
