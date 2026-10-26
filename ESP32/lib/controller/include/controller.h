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
 * @brief Runs the control loop: button gestures, HUD requests, the three modes and the published status.
 * @details **Modes** (@ref ControlMode):
 * - **Ai**: every pass the controller reads the track snapshot that the @ref BaseDetectionModule publishes (the
 * detection runs in its own task) and feeds it to the @ref aiming::AimPolicy, which yields absolute pan/tilt
 * targets for @ref BaseTurret::set_target_deg (or Home after the target is lost).
 * - **User**: no frames needed; the jog (a deflected @ref Joystick, else an active HUD jog) goes to
 * @ref BaseTurret::jog at most once per kUserModePeriodMs. A HUD aim point moves the turret to an absolute pose.
 * - **Calibrate**: jogs and aims as in User mode (the jog is zeroed while the button is held or a HUD record is
 * pending); detection keeps running in its own task so the HUD box shows. A short press (or `cal:point`) records a
 * calibration point: the turret's resting angles paired with the visible target's pixel position.
 *
 * **Button gestures:** short press toggles Ai/User (or records a point in Calibrate); a long press enters
 * Calibrate, or cancels it and restores the previous mode.
 *
 * **HUD requests** (taken from the inbox once per pass, so a request that does not apply is dropped, never
 * deferred): `mode:*` outside Calibrate only; `cal:start` / `cal:cancel` act like a long press when not
 * calibrating / while calibrating (ignored otherwise); `cal:point` acts like a short press in Calibrate (ignored
 * otherwise): the jog is zeroed and the pose is recorded on the first later pass where the pan is at rest, or after
 * kHudRecordTimeoutMs with the current pose. `aim:x,y` in User and Calibrate only, and not while a calibration record
 * is in progress: the pixel goes through the active aim map to @ref BaseTurret::set_target_deg, and the aim holds
 * until a jog source becomes active.
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

    /** @brief The active aim map, always equal to the one in _policy; HUD aims and the status (aim pixel, reach box)
     * use it. The default map until begin() loads a saved one or a calibration succeeds. */
    aiming::AimMap _map;

    /** @brief Successful calibrations since boot (button or HUD). */
    uint32_t _cal_saves = 0;

    /** @brief Calibration records ignored because no target was visible (button or HUD). */
    uint32_t _cal_miss = 0;

    /** @brief true while a HUD `cal:point` waits for the pan to stop; the jog is forced to (0,0) meanwhile. */
    bool _hud_record_pending = false;

    /** @brief Timestamp (millis) of the pending HUD `cal:point`. */
    uint32_t _hud_record_since_ms = 0;

    /** @brief Longest wait for the pan to stop before a HUD `cal:point` records the current pose anyway. */
    static constexpr uint32_t kHudRecordTimeoutMs = 1000;

    /** @brief true after a HUD aim until a jog source becomes active: the idle jog arbiter must not send (0,0), which
     * would stop the aim move. */
    bool _aim_hold = false;

    /** @brief true when the last jog pass used the deflected joystick (published as ControlStatus::stick). */
    bool _stick = false;

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

    /** @brief Latest HUD command state (dead-man jog, speed, pending mode, calibration and aim requests): submit() runs
     * on the httpd task, run() on the loop task. */
    vsync::SpinGuarded<remote::RemoteInbox> _remote_inbox;

    /**
     * @brief Jog arbiter for User/Calibrate, at most once per kUserModePeriodMs (25 Hz).
     * @details @p force_stop (Calibrate with the button held, or a HUD record pending) sends (0,0); a deflected stick
     * wins; else an active HUD jog; else (0,0). While a HUD aim holds, an idle (0,0) is not sent, so the aim move
     * completes; the first non-zero jog ends the hold.
     */
    void jog_from_inputs(uint32_t now, bool force_stop);

    /** @brief Sends a (0,0) jog and ends any HUD aim hold. */
    void stop_jog();

    /** @brief Switches to a non-Calibrate mode: detection reset, policy reset, jog stop, log. */
    void set_mode(ControlMode mode);

    /** @brief Makes @p map the active aim map in both the controller and the policy. */
    void set_active_map(const aiming::AimMap& map);

    /** @brief Enters Calibrate at point 1, remembering the mode to restore (long press, or HUD `cal:start`). */
    void enter_calibration();

    /** @brief Leaves Calibrate, restores the previous mode and keeps the old map (long press, or HUD `cal:cancel`). */
    void cancel_calibration();

    /**
     * @brief Records one calibration point at @p pose with the current track (short press, or HUD `cal:point`).
     * @details A successful fit is saved, made active and switches to Ai (counted in _cal_saves); a record ignored
     * because no target is visible is counted in _cal_miss.
     */
    void record_cal_point(const aiming::Angles& pose);

    /** @brief Applies a HUD calibration request (Start, Point or Cancel). */
    void apply_cal_request(remote::CalAction action, uint32_t now);

    /** @brief Records the pending HUD calibration point once the pan is at rest or the wait timed out. */
    void service_hud_record(uint32_t now);

    /** @brief Handles a Long press (enter or leave calibration). */
    void on_long_press();

    /** @brief Handles a Short press (record a point, or toggle Ai/User). */
    void on_short_press();

    /** @brief Publishes the current state under the lock; @p now is this pass's millis(). */
    void publish_status(uint32_t now);

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
        : _turret(turret), _detection_module(detection_module), _joystick(joystick), _policy(aiming::default_map()),
          _map(aiming::default_map())
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
     * @details Classifies the button, applies the HUD's mode, calibration and aim requests, runs the per-mode
     * behaviour and publishes the status.
     * @warning Not paced by the camera: returns immediately when there is nothing to do, so the caller's loop
     * must yield.
     */
    void run();

    /**
     * @brief Receives a HUD command. Safe to call from the httpd task while run() executes on another core.
     * @details Only stores the command in the inbox under a spinlock; run() applies it. Moves expire after
     * remote::kJogTimeoutMs without a refresh (dead man).
     */
    void submit(const remote::Command& cmd) override;

    /** @brief Latest published status. Safe to call from any task. */
    ControlStatus control_status() const override;
    /** @} */
};
