/**
 * @file Controller.h
 * @brief Header file for the Controller class.
 * @details This class serves as the **Central Intelligence Unit** of the turret system.
 * It coordinates a high-speed control loop that integrates:
 * 1. **Input**: Manual Joystick signals or AI Detection frames, plus the push-button gestures.
 * 2. **Logic**: Arbitration between user overrides, autonomous tracking and calibration (control::ControlLogic).
 * 3. **Output**: Absolute aim targets and jog commands sent to the Turret.
 */

#pragma once

#include "base_detection_module.h"
#include "base_turret.h"
#include "control_logic.h"
#include "control_status.h"
#include "joystick.h"
#include "remote_inbox.h"
#include "sync.h"
#include "system_control_types.h"
#include <cstdint>
#include <stdlib.h>

/**
 * @class Controller
 * @brief Runs the control loop on the device: the clock, the HUD inbox, NVS, logging and the published status around
 * the pure @ref control::ControlLogic.
 * @details Every decision (modes, button gestures, HUD requests, calibration, jog arbitration) is made by
 * control::ControlLogic, which documents the rules and is natively tested. The Controller implements its
 * @ref control::ControlPorts over the @ref BaseTurret, the @ref BaseDetectionModule and the @ref Joystick, turns its
 * log events into ESP_LOG lines and saves a fresh calibration with cal_store.
 *
 * **Threads:** submit() runs on the httpd task and only stores into the inbox under its spinlock; run() and every
 * port call run on the loop task. The status is published under its own spinlock for any task to read.
 *
 * The camera is fixed on the base and the turret aims absolutely, so a calibration is a fixed two-point mapping
 * from image pixels to pan/tilt angles, saved in NVS.
 */
class Controller : public BaseStatusProvider, public remote::BaseRemoteSink, private control::ControlPorts
{
private:
    /** @brief Reference to the turret (non-blocking; executed by its motor task). */
    BaseTurret& _turret;

    /** @brief Reference to the AI/Computer Vision module for target acquisition. */
    BaseDetectionModule& _detection_module;

    /** @brief Reference to the input handler for manual control. */
    Joystick& _joystick;

    /** @brief The decisions; calls back into this object's ControlPorts. Loop task only. */
    control::ControlLogic _logic;

    /** @brief Status published at the end of each run() pass; read from the httpd task. */
    vsync::SpinGuarded<ControlStatus> _status;

    /** @brief Latest HUD command state (dead-man jog, speed, pending mode, calibration and aim requests): submit() runs
     * on the httpd task, run() on the loop task. */
    vsync::SpinGuarded<remote::RemoteInbox> _remote_inbox;

    /** @brief Publishes the current state under the lock; @p now is this pass's millis(). */
    void publish_status(uint32_t now);

    /** @name control::ControlPorts (loop task)
     * @{
     */
    control::StickDeflection read_stick() noexcept override;
    /** @brief The HUD jog, judged against millis() read inside the inbox lock (see controller.cpp). */
    remote::Jog read_hud_jog() noexcept override;
    control::HudRequests take_hud_requests() noexcept override;
    TrackSnapshot read_track() noexcept override;
    aiming::Angles turret_pose() noexcept override;
    float turret_pan_speed() noexcept override;
    void turret_jog(float pan, float tilt) noexcept override;
    void turret_set_target(const aiming::Angles& target) noexcept override;
    void turret_home() noexcept override;
    void reset_detection() noexcept override;
    bool save_calibration(const aiming::AimMap& map) noexcept override;
    void log(const control::LogRecord& record) noexcept override;
    /** @} */

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
        : _turret(turret), _detection_module(detection_module), _joystick(joystick), _logic(*this)
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
     * @details Reads the clock and the button, runs one control::ControlLogic pass and publishes the status.
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
