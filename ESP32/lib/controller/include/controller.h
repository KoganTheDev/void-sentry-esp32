/**
 * @file Controller.h
 * @brief Header file for the Controller class.
 * @details This class serves as the **Central Intelligence Unit** of the turret system.
 * It coordinates a high-speed control loop that integrates:
 * 1. **Input**: Manual Joystick signals or AI Detection frames.
 * 2. **Logic**: Arbitration between user overrides and autonomous tracking.
 * 3. **Output**: Kinematic commands sent to the Movement Manager.
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

class Camera; // Forward declaration to minimize header inter-dependency

/**
 * @class Controller
 * @brief Handles logic for object detection and movement calculations.
 * @details The Controller implements a **State-Driven architecture**. It acts as the
 * bridge between the Computer Vision pipeline and the physical servos.
 * * **Control Arbitration Logic:**
 * - **AI_MODE**: The controller requests a frame from the @ref Camera, passes it to the
 * @ref BaseDetectionModule, and calculates the error vector to center the target.
 * - **USER_MODE**: The controller bypasses the CV pipeline and maps @ref Joystick
 * input directly to the @ref BaseMovementManager.
 */
class Controller
{
private:
    /** @brief The active operation state (Manual vs Autonomous). */
    SystemControl _system_control_state;

    /** @brief Reference to the movement Manager (e.g., Servo/PWM control). */
    BaseMovementManager& _movement_manager;

    /** @brief Reference to the AI/Computer Vision module for target acquisition. */
    BaseDetectionModule& _detection_module;

    /** @brief Reference to the input handler for manual control. */
    Joystick& _joystick;

    /** @brief Reference to the camera sensor interface. */
    Camera& _camera;

public:
    /**
     * @name Lifecycle Management
     * @{
     */

    /**
     * @brief Construct a new Controller object.
     * @param movement_manager Reference to the movement implementation.
     * @param detection_module Reference to the detection implementation.
     * @param joystick Reference to the Joystick input handler.
     * @param camera Reference to the Camera object.
     * @note Defaults to `AI_MODE` upon initialization.
     */
    Controller(BaseMovementManager& movement_manager, BaseDetectionModule& detection_module, Joystick& joystick,
               Camera& camera)
        : _system_control_state(SystemControl::AI_MODE), _movement_manager(movement_manager),
          _detection_module(detection_module), _joystick(joystick), _camera(camera)
    {
    }

    /**
     * @brief Destroy the Controller object.
     * @details Ensures the turret is safely parked and motor signals are neutralized.
     */
    ~Controller() {}
    /** @} */

    /**
     * @name Core Execution
     * @{
     */

    /**
     * @brief Main processing cycle for the turret system.
     * @details This method should be called inside the primary `loop()` of the application.
     * * **Workflow:**
     * 1. Check current `SystemControl` state.
     * 2. If **USER_MODE**:
     * - Poll `_joystick` for X/Y offsets.
     * - Call `_movement_manager.move()`.
     * 3. If **AI_MODE**:
     * - Capture frame from `_camera`.
     * - Perform inference via `_detection_module`.
     * - Translate target coordinates to motor pulses.
     * * @warning This function is blocking relative to the frame-rate of the camera.
     */
    void run();

    const std::tuple<MoveX, MoveY> user_mode() const;

    //! TODO: maybe can be deleted
    // const std::tuple<MoveX, MoveY> ai_mode() const;
    /** @} */
};
