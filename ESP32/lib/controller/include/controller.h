/**
 * @file Controller.h
 * @brief Header file for the Controller class.
 * @details This class serves as the **Central Intelligence Unit** of the turret system.
 * It coordinates a high-speed control loop that integrates:
 * 1. **Input**: Manual Joystick signals or AI Detection frames, plus the push-button gestures.
 * 2. **Logic**: Arbitration between user overrides, autonomous tracking and calibration.
 * 3. **Output**: Absolute aim targets and jog commands sent to the Turret.
 */

#pragma once

#include "aim_policy.h"
#include "base_detection_module.h"
#include "base_turret.h"
#include "button_classifier.h"
#include "calibration_session.h"
#include "control_status.h"
#include "joystick.h"
#include "system_control_types.h"
#include <cstdint>
#include <stdlib.h>

/**
 * @class Controller
 * @brief Runs the control loop: button gestures, the three modes and the published status.
 * @details **Modes** (@ref ControlMode):
 * - **Ai**: every pass the controller reads the track snapshot that the @ref BaseDetectionModule publishes (the
 * detection runs in its own task) and feeds it to the @ref aiming::AimPolicy, which yields absolute pan/tilt
 * targets for @ref BaseTurret::set_target_deg (or Home after the target is lost).
 * - **User**: no frames needed; @ref Joystick input is mapped to @ref BaseTurret::jog at most once per
 * kUserModePeriodMs.
 * - **Calibrate**: joystick jogs as in User mode; detection keeps running in its own task so the HUD box shows. A short
 * press records a calibration point (the turret's current angles paired with the visible target's pixel position).
 *
 * **Button gestures:** short press toggles Ai/User (or records a point in Calibrate); a long press enters
 * Calibrate, or cancels it and restores the previous mode.
 *
 * The camera is fixed on the base and the turret aims absolutely, so a calibration is a fixed two-point mapping
 * from image pixels to pan/tilt angles, saved in NVS.
 */
class Controller : public BaseStatusProvider
{
private:
    /** @brief Reference to the turret (non-blocking; executed by its motor task). */
    BaseTurret& _turret;

    /** @brief Reference to the AI/Computer Vision module for target acquisition. */
    BaseDetectionModule& _detection_module;

    /** @brief Reference to the input handler for manual control. */
    Joystick& _joystick;

    /** @brief Classifies the raw Z button level into Short / Long press events. */
    aiming::ButtonClassifier _button;

    /** @brief Converts the tracked pixel position into absolute turret angles. */
    aiming::AimPolicy _policy;

    /** @brief Two-point calibration state machine. */
    aiming::CalibrationSession _session;

    /** @brief The active operating mode. */
    ControlMode _mode = ControlMode::Ai;

    /** @brief Mode to restore when a calibration is cancelled. */
    ControlMode _mode_before_cal = ControlMode::Ai;

    /** @brief true once a saved or fresh calibration is active. */
    bool _calibrated = false;

    /** @brief Status published at the end of each run() pass; read from the httpd task. */
    ControlStatus _status;

    /** @brief Guards _status (copies only; no blocking calls inside). */
    mutable portMUX_TYPE _status_lock = portMUX_INITIALIZER_UNLOCKED;

    /** @brief Aim commands issued to the turret since boot (telemetry). */
    uint32_t _aims = 0;

    /** @brief Previous raw button level; detects the start of each press. */
    bool _prev_held = false;

    /** @brief Turret pose captured while the button was held and the pan was at rest. */
    aiming::Angles _cal_pose_at_press;

    /** @brief true once _cal_pose_at_press holds a stationary pose for the current press. */
    bool _cal_pose_captured = false;

    /** @brief Minimum time between joystick-driven moves in User/Calibrate mode (25 Hz). */
    static constexpr uint32_t kUserModePeriodMs = 40;

    /** @brief Timestamp (millis) of the last joystick move. */
    uint32_t _last_user_move_ms = 0;

    /** @brief Jogs the turret from the joystick at 25 Hz; stick_neutral sends (0,0) instead of the stick. */
    void jog_from_joystick(uint32_t now, bool stick_neutral);

    /** @brief Handles a Long press (enter or leave calibration). */
    void on_long_press();

    /** @brief Handles a Short press (record a point, or toggle Ai/User). */
    void on_short_press();

    /** @brief Publishes the current state under the lock. */
    void publish_status();

public:
    /**
     * @name Lifecycle Management
     * @{
     */

    /**
     * @brief Construct a new Controller object.
     * @param turret Reference to the turret implementation.
     * @param detection_module Reference to the detection implementation.
     * @param joystick Reference to the Joystick input handler.
     * @note Defaults to Ai mode with the default (uncalibrated) aim map.
     */
    Controller(BaseTurret& turret, BaseDetectionModule& detection_module, Joystick& joystick)
        : _turret(turret), _detection_module(detection_module), _joystick(joystick), _policy(aiming::default_map())
    {
    }

    /**
     * @brief Destroy the Controller object.
     */
    ~Controller() override {}

    /**
     * @brief Loads a saved calibration from NVS, if any. Call from setup() after the turret has started.
     */
    void begin();
    /** @} */

    /**
     * @name Core Execution
     * @{
     */

    /**
     * @brief One pass of the control loop; call from the primary `loop()`.
     * @details Classifies the button, applies mode changes, runs the per-mode behaviour and publishes the status.
     * @warning Not paced by the camera: returns immediately when there is nothing to do, so the caller's loop
     * must yield.
     */
    void run();

    /** @brief Latest published status. Safe to call from any task. */
    ControlStatus control_status() const override;
    /** @} */
};
