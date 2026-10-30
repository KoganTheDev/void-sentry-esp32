/**
 * @file control_logic.h
 * @brief The controller's decisions: button gestures, HUD requests, modes, calibration and jog arbitration (pure
 * C++20, natively tested).
 * @details Controller (lib/controller) is the device glue around this class: it owns the clock, the HUD inbox and its
 * spinlock, NVS, logging and the published status, and implements @ref control::ControlPorts over the turret, the
 * detection module and the joystick. ControlLogic makes every read and command through the ports at the point where
 * the decision needs it, so the joystick ADC is read only on a jog pass, the HUD jog's clock is read inside the inbox
 * lock, and the turret's latest-wins mailbox receives its writes in decision order. Single-threaded: run() belongs to
 * the loop task.
 */

#pragma once

#include "aim_policy.h"
#include "button_classifier.h"
#include "calibration_session.h"
#include "control_status.h"
#include "remote_inbox.h"
#include "system_control_types.h"
#include "track_snapshot.h"

#include <cstdint>
#include <optional>

namespace control
{
/** @brief Minimum time between jog passes in User and Calibrate (25 Hz). */
constexpr uint32_t kJogPeriodMs = 40;

/** @brief Longest wait for the pan to stop before a HUD `cal:point` records the current pose anyway. */
constexpr uint32_t kHudRecordTimeoutMs = 1000;

/** @brief Joystick deflection per axis, -255..255 (Joystick::get_speed_x / get_speed_y); y positive = up. */
struct StickDeflection {
    int x = 0;
    int y = 0;
};

/** @brief The HUD's one-shot requests, taken from the inbox together once per pass. */
struct HudRequests {
    std::optional<remote::RemoteMode> mode;
    std::optional<remote::CalAction> cal;
    std::optional<remote::AimPixel> aim;
};

/** @brief Something worth a log line; the glue picks the level and the text. */
enum class LogEvent : uint8_t {
    ModeChanged,       ///< info; LogRecord::mode is the new mode
    CalStarted,        ///< info; LogRecord::mode is the mode calibration was started from
    CalCancelled,      ///< info; LogRecord::mode is the restored mode
    CalSaveFailed,     ///< error: the NVS write failed, the fit stays active until reboot
    CalComplete,       ///< info: the fit is active, back to Ai
    CalFailed,         ///< warning; LogRecord::failure is the session's reason (may be null)
    CalRestarted,      ///< info: a record after a failed fit restarted at point 1
    CalMiss,           ///< warning: record ignored, no target visible
    CalPointRecorded,  ///< info; LogRecord::points is the number of points now recorded
    HudPoseTimeout,    ///< warning: the pan still moved kHudRecordTimeoutMs after `cal:point`, current pose used
    PressPoseFallback, ///< warning: no stationary pose was captured during the press, current pose used
};

/** @brief One log event and its arguments; fields other than the event's own are left at their defaults. */
struct LogRecord {
    LogEvent event = LogEvent::ModeChanged;
    ControlMode mode = ControlMode::Ai;
    int points = 0;
    const char* failure = nullptr;
};

/**
 * @class ControlPorts
 * @brief Everything ControlLogic reads from or commands on the device, called from the loop task.
 * @details The Controller implements it over the real hardware; tests use a recording fake. None of them throws
 * (mailbox writes, lock-guarded copies, an NVS write), hence noexcept.
 */
class ControlPorts
{
public:
    virtual ~ControlPorts() = default;

    /** @brief Current joystick deflection. Read only on a jog pass when the jog is not forced to stop. */
    virtual StickDeflection read_stick() noexcept = 0;

    /** @brief The HUD's jog with the clock read at this call, under the inbox lock (dead man; see Controller). */
    virtual remote::Jog read_hud_jog() noexcept = 0;

    /** @brief Takes (and clears) the HUD's pending mode, calibration and aim requests. */
    virtual HudRequests take_hud_requests() noexcept = 0;

    /** @brief Latest tracker output from the detection module. */
    virtual TrackSnapshot read_track() noexcept = 0;

    /** @brief Current turret pose (BaseTurret::position_deg). */
    virtual aiming::Angles turret_pose() noexcept = 0;

    /** @brief Current pan stepper speed, 0 at rest (BaseTurret::pan_speed). */
    virtual float turret_pan_speed() noexcept = 0;

    /** @brief Manual velocity per axis, -1..1 (BaseTurret::jog). */
    virtual void turret_jog(float pan, float tilt) noexcept = 0;

    /** @brief Absolute move (BaseTurret::set_target_deg). */
    virtual void turret_set_target(const aiming::Angles& target) noexcept = 0;

    /** @brief Return home (BaseTurret::home). */
    virtual void turret_home() noexcept = 0;

    /** @brief Asks the detection task to drop its track and start fresh (BaseDetectionModule::reset). */
    virtual void reset_detection() noexcept = 0;

    /** @brief Persists a fresh calibration; false if the write failed. */
    virtual bool save_calibration(const aiming::AimMap& map) noexcept = 0;

    /** @brief Emits one log line. */
    virtual void log(const LogRecord& record) noexcept = 0;
};

/**
 * @class ControlLogic
 * @brief The control loop's decisions, one pass per run() call: button gestures, HUD requests, the three modes and
 * the status counters.
 * @details Owns the button classifier, the aim policy, the calibration session, the mode and the counters published
 * in ControlStatus. Starts in Ai mode with the default (uncalibrated) aim map.
 *
 * **Modes** (@ref ControlMode):
 * - **Ai**: every pass reads the track snapshot that the detection task publishes and feeds it to the
 * @ref aiming::AimPolicy, which yields absolute pan/tilt targets (or Home after the target is lost).
 * - **User**: no frames needed; the jog (a deflected stick, else an active HUD jog) goes to the turret at most once per
 * kJogPeriodMs. A HUD aim point moves the turret to an absolute pose.
 * - **Calibrate**: jogs and aims as in User mode (the jog is zeroed while the button is held or a HUD record is
 * pending); detection keeps running in its own task so the HUD box shows. A short press (or `cal:point`) records a
 * calibration point: the turret's resting angles paired with the visible target's pixel position. A successful fit
 * is saved through ControlPorts::save_calibration, made active, and switches to Ai.
 *
 * **Button gestures:** short press toggles Ai/User (or records a point in Calibrate); a long press enters
 * Calibrate, or cancels it and restores the previous mode.
 *
 * **HUD requests** (taken from the inbox once per pass, so a request that does not apply is dropped, never
 * deferred): `mode:*` outside Calibrate only, and not on a pass that started in Calibrate; `cal:start` /
 * `cal:cancel` act like a long press when not calibrating / while calibrating (ignored otherwise); `cal:point` acts
 * like a short press in Calibrate (ignored otherwise): the jog is zeroed and the pose is recorded on the first later
 * pass where the pan is at rest, or after kHudRecordTimeoutMs with the current pose. Any `cal:*` is dropped on a pass
 * where the button produced an event, and a knob record clears a pending HUD one. `aim:x,y` in User and Calibrate
 * only, and not while a calibration record is in progress: the pixel goes through the active aim map to an absolute
 * target, and the aim holds (the idle (0,0) jog is not sent) until a jog source becomes active.
 */
class ControlLogic
{
public:
    /** @param io The device ports; must outlive this object. */
    explicit ControlLogic(ControlPorts& io) : io_(io), policy_(aiming::default_map()) {}

    /** @brief Makes a saved calibration active (Controller::begin, after loading it from NVS). */
    void load_calibration(const aiming::AimMap& map);

    /**
     * @brief One pass: classifies the button, services a pending HUD record, applies the HUD's mode, calibration and
     * aim requests, then runs the per-mode behaviour (jog arbitration, or the aim policy in Ai).
     * @param now_ms millis() at the start of the pass.
     * @param held Raw joystick button level read at the start of the pass.
     */
    void run(uint32_t now_ms, bool held);

    /**
     * @brief The status to publish after a pass.
     * @param now_ms The pass's millis() (for the AI home countdown).
     * @param pose Current turret pose.
     * @param speed The HUD jog speed setting from the inbox.
     */
    ControlStatus status(uint32_t now_ms, const aiming::Angles& pose, remote::JogSpeed speed) const;

    /** @brief The active operating mode. */
    ControlMode mode() const { return mode_; }

private:
    void jog_from_inputs(uint32_t now_ms, bool force_stop);
    void stop_jog();
    void set_mode(ControlMode mode);
    void enter_calibration();
    void cancel_calibration();
    void record_cal_point(const aiming::Angles& pose);
    void apply_cal_request(remote::CalAction action, uint32_t now_ms);
    void service_hud_record(uint32_t now_ms);
    void on_long_press();
    void on_short_press();

    ControlPorts& io_;
    aiming::ButtonClassifier button_;
    aiming::AimPolicy policy_;
    aiming::CalibrationSession session_;
    ControlMode mode_ = ControlMode::Ai;
    ControlMode mode_before_cal_ = ControlMode::Ai; ///< restored when a calibration is cancelled
    bool calibrated_ = false;                       ///< a saved or fresh calibration is active
    uint32_t cal_saves_ = 0;                        ///< successful calibrations since boot (button or HUD)
    uint32_t cal_miss_ = 0;                         ///< records ignored because no target was visible
    bool hud_record_pending_ = false;               ///< a HUD `cal:point` waits for the pan to stop
    uint32_t hud_record_since_ms_ = 0;              ///< when that `cal:point` arrived
    bool aim_hold_ = false;  ///< a HUD aim is travelling: the idle arbiter must not send (0,0), which would stop it
    bool stick_ = false;     ///< the last jog pass used the deflected joystick
    uint32_t aims_ = 0;      ///< Ai aim commands since boot
    bool prev_held_ = false; ///< previous raw button level, to detect the start of each press
    aiming::Angles cal_pose_at_press_; ///< pose captured while the button was held and the pan was at rest
    bool cal_pose_captured_ = false;   ///< cal_pose_at_press_ holds a pose for the current press
    uint32_t last_jog_ms_ = 0;         ///< start of the last jog pass
};
} // namespace control
