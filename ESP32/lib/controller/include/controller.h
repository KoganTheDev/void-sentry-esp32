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
#include "remote_inbox.h"
#include "sync.h"
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
 * - **Calibrate**: joystick (and the HUD, except while the button is held) jogs as in User mode; detection keeps
 * running in its own task so the HUD box shows. A short press records a calibration point (the turret's current angles
 * paired with the visible target's pixel position).
 *
 * **Button gestures:** short press toggles Ai/User (or records a point in Calibrate); a long press enters
 * Calibrate, or cancels it and restores the previous mode.
 *
 * The camera is fixed on the base and the turret aims absolutely, so a calibration is a fixed two-point mapping
 * from image pixels to pan/tilt angles, saved in NVS.
 */
class Controller : public BaseStatusProvider, public remote::BaseRemoteSink
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
    vsync::SpinGuarded<ControlStatus> _status;

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

    /** @brief Latest HUD command state (dead-man jog, pending mode request): submit() runs on the httpd task, run() on
     * the loop task. */
    vsync::SpinGuarded<remote::RemoteInbox> _remote_inbox;

    /**
     * @brief Jog arbiter for User/Calibrate, at most once per kUserModePeriodMs (25 Hz).
     * @details Calibrate with the button held sends (0,0); a deflected stick wins; else an active HUD jog; else (0,0).
     */
    void jog_from_inputs(uint32_t now, bool calibrate_button_held);

    /** @brief Switches to a non-Calibrate mode: detection reset, policy reset, jog stop, log. */
    void set_mode(ControlMode mode);

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

    /**
     * @brief Receives a HUD command. Safe to call from the httpd task while run() executes on another core.
     * @details Stores the command in the inbox under a spinlock; run() applies it. Moves expire after
     * remote::kJogTimeoutMs without a refresh (dead man).
     */
    void submit(const remote::Command& cmd) override;

    /** @brief Latest published status. Safe to call from any task. */
    ControlStatus control_status() const override;
    /** @} */
};
