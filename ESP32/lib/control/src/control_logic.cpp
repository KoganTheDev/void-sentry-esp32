#include "control_logic.h"

#include "status_fields.h"

namespace control
{
// ControlStatus::speed is published as the JogSpeed value (0 slow, 1 normal, 2 fast).
static_assert(static_cast<uint8_t>(remote::JogSpeed::Slow) == 0 &&
                  static_cast<uint8_t>(remote::JogSpeed::Normal) == 1 &&
                  static_cast<uint8_t>(remote::JogSpeed::Fast) == 2,
              "ControlStatus::speed relies on the JogSpeed values");

void ControlLogic::load_calibration(const aiming::AimMap& map)
{
    policy_.set_map(map);
    calibrated_ = true;
}

void ControlLogic::jog_from_inputs(uint32_t now_ms, bool force_stop)
{
    if (now_ms - last_jog_ms_ < kJogPeriodMs)
    {
        return;
    }
    last_jog_ms_ = now_ms;

    if (force_stop)
    {
        stick_ = false; // the knob press deflects the stick, but it is not jogging
        stop_jog();
        return;
    }
    const StickDeflection s = io_.read_stick();
    stick_ = (s.x != 0 || s.y != 0);
    if (stick_)
    {
        // Joystick y positive = Up = larger tilt angle. The stick overrides any HUD jog and ends a HUD aim.
        aim_hold_ = false;
        io_.turret_jog(s.x / 255.0f, s.y / 255.0f);
        return;
    }
    // The port reads the clock itself, inside the inbox lock, not this pass's now_ms (see Controller::read_hud_jog).
    const remote::Jog jog = io_.read_hud_jog();
    if (jog.pan != 0.0f || jog.tilt != 0.0f)
    {
        aim_hold_ = false;
        io_.turret_jog(jog.pan, jog.tilt);
        return;
    }
    // No jog source: (0,0) once the dead-man window has passed, unless a HUD aim is still travelling (a zero jog
    // would stop it where it is).
    if (!aim_hold_)
    {
        io_.turret_jog(0.0f, 0.0f);
    }
}

void ControlLogic::stop_jog()
{
    aim_hold_ = false;
    io_.turret_jog(0.0f, 0.0f);
}

void ControlLogic::set_mode(ControlMode mode)
{
    mode_ = mode;
    io_.reset_detection(); // a request the detection task applies: no stale lock, fresh start
    policy_.reset();
    stop_jog();
    io_.log(LogRecord{.event = LogEvent::ModeChanged, .mode = mode_});
}

void ControlLogic::enter_calibration()
{
    mode_before_cal_ = mode_;
    mode_ = ControlMode::Calibrate;
    session_.start();
    policy_.reset();
    stop_jog();
    hud_record_pending_ = false;
    io_.log(LogRecord{.event = LogEvent::CalStarted, .mode = mode_before_cal_});
}

void ControlLogic::cancel_calibration()
{
    session_.cancel();
    mode_ = mode_before_cal_;
    policy_.reset(); // the turret was jogged by hand; forget the old setpoint
    stop_jog();
    hud_record_pending_ = false;
    io_.log(LogRecord{.event = LogEvent::CalCancelled, .mode = mode_});
}

void ControlLogic::record_cal_point(const aiming::Angles& pose)
{
    const TrackSnapshot s = io_.read_track();
    const aiming::CalState before_state = session_.state();
    const int before_points = session_.points();
    const aiming::CalState st = session_.record((float)s.x, (float)s.y, pose, s.detected());
    if (st == aiming::CalState::Done)
    {
        const aiming::AimMap map = *session_.result();
        if (!io_.save_calibration(map))
        {
            io_.log(LogRecord{.event = LogEvent::CalSaveFailed});
        }
        policy_.set_map(map);
        calibrated_ = true;
        ++cal_saves_; // counts the active fit even if the NVS write failed: the HUD reports the calibration
        session_.cancel();
        stop_jog(); // the stick may still be deflected; do not leave the turret running
        hud_record_pending_ = false;
        mode_ = ControlMode::Ai;
        policy_.reset();
        io_.log(LogRecord{.event = LogEvent::CalComplete});
    } else if (st == aiming::CalState::Failed)
    {
        io_.log(LogRecord{.event = LogEvent::CalFailed, .failure = session_.failure()});
    } else if (before_state == aiming::CalState::Failed)
    {
        io_.log(LogRecord{.event = LogEvent::CalRestarted});
    } else if (st == before_state && session_.points() == before_points)
    {
        ++cal_miss_;
        io_.log(LogRecord{.event = LogEvent::CalMiss});
    } else
    {
        io_.log(LogRecord{.event = LogEvent::CalPointRecorded, .points = session_.points()});
    }
}

void ControlLogic::apply_cal_request(remote::CalAction action, uint32_t now_ms)
{
    const bool calibrating = mode_ == ControlMode::Calibrate;
    switch (action)
    {
    case remote::CalAction::Start:
        if (!calibrating)
        {
            enter_calibration();
        }
        break;

    case remote::CalAction::Cancel:
        if (calibrating)
        {
            cancel_calibration();
        }
        break;

    case remote::CalAction::Point:
        // A second cal:point while one waits for the pan to stop is the same request; keep the first one's timer.
        if (calibrating && !hud_record_pending_)
        {
            stop_jog();
            hud_record_pending_ = true;
            hud_record_since_ms_ = now_ms;
        }
        break;
    }
}

void ControlLogic::service_hud_record(uint32_t now_ms)
{
    // Every path out of Calibrate clears the pending record already; leaving it set outside Calibrate is harmless.
    if (!hud_record_pending_ || mode_ != ControlMode::Calibrate)
    {
        hud_record_pending_ = false;
        return;
    }
    const bool at_rest = io_.turret_pan_speed() == 0.0f;
    if (!at_rest && now_ms - hud_record_since_ms_ < kHudRecordTimeoutMs)
    {
        return;
    }
    if (!at_rest)
    {
        io_.log(LogRecord{.event = LogEvent::HudPoseTimeout});
    }
    hud_record_pending_ = false;
    record_cal_point(io_.turret_pose());
}

void ControlLogic::on_long_press()
{
    if (mode_ == ControlMode::Calibrate)
    {
        cancel_calibration();
    } else
    {
        enter_calibration();
    }
}

void ControlLogic::on_short_press()
{
    if (mode_ == ControlMode::Calibrate)
    {
        // Stop at the pose being recorded; the stick resumes jogging on the next 25 Hz pass if still deflected.
        stop_jog();
        // The knob press is the record: a HUD cal:point still waiting must not add a second point at this pose.
        hud_record_pending_ = false;
        // Record the pose captured while the knob was held and the pan was at rest, not the pose now: pressing the
        // knob deflects the stick, and the release is only classified ~50 ms later, so the pose may have drifted
        // away from where the user aimed. Fallback (released before the pan stopped): the current pose.
        aiming::Angles pose = cal_pose_at_press_;
        if (!cal_pose_captured_)
        {
            pose = io_.turret_pose();
            io_.log(LogRecord{.event = LogEvent::PressPoseFallback});
        }
        cal_pose_captured_ = false;
        record_cal_point(pose);
    } else
    {
        set_mode(mode_ == ControlMode::Ai ? ControlMode::User : ControlMode::Ai);
    }
}

ControlStatus ControlLogic::status(uint32_t now_ms, const aiming::Angles& pose, remote::JogSpeed speed) const
{
    const bool ai = mode_ == ControlMode::Ai;
    ControlStatus st;
    st.mode = mode_;
    st.cal_points = (uint8_t)session_.points();
    st.cal_state = session_.state();
    st.calibrated = calibrated_;
    st.angles = pose;
    st.cal_failure = session_.failure();
    st.aims = aims_;
    st.cal_saves = cal_saves_;
    st.cal_miss = cal_miss_;
    st.stick = !ai && stick_; // the stick only jogs in User and Calibrate
    st.speed = static_cast<uint8_t>(speed);
    hud::fill_pose_fields(st, policy_.map(),
                          hud::HomeState{.ai_mode = ai,
                                         .homed = policy_.homed(),
                                         .home_remaining_ms = ai ? policy_.home_remaining_ms(now_ms) : 0});
    return st;
}

void ControlLogic::run(uint32_t now_ms, bool held)
{
    if (held && !prev_held_)
    {
        cal_pose_captured_ = false; // a new press starts: forget any pose from an earlier one
    }
    prev_held_ = held;
    // While the knob is held in Calibrate the stick is deflected by the press itself, so the jog is zeroed and the
    // pose is captured on the first pass where the pan has actually stopped (pan speed 0).
    if (mode_ == ControlMode::Calibrate && held && !cal_pose_captured_ && io_.turret_pan_speed() == 0.0f)
    {
        cal_pose_at_press_ = io_.turret_pose();
        cal_pose_captured_ = true;
    }
    // A HUD mode request is dropped if calibration was active at the start of this pass, even if the button ends it.
    // A HUD calibration request is dropped on any pass where the button produced an event: a queued cal:start after a
    // long press that cancelled would silently restart, and cal:cancel or cal:point after a press would double up.
    const bool was_calibrating = mode_ == ControlMode::Calibrate;
    const aiming::ButtonEvent ev = button_.update(held, now_ms);
    if (ev == aiming::ButtonEvent::Long)
    {
        on_long_press();
    } else if (ev == aiming::ButtonEvent::Short)
    {
        on_short_press();
    }

    // A HUD cal:point taken on an earlier pass records once the pan is at rest (or after the timeout).
    service_hud_record(now_ms);

    // Take each HUD request every pass so one that does not apply now is dropped, not applied later.
    const HudRequests req = io_.take_hud_requests();
    if (req.mode.has_value() && !was_calibrating && mode_ != ControlMode::Calibrate)
    {
        const ControlMode wanted = (*req.mode == remote::RemoteMode::Ai) ? ControlMode::Ai : ControlMode::User;
        if (wanted != mode_)
        {
            set_mode(wanted);
        }
    }
    if (req.cal.has_value() && ev == aiming::ButtonEvent::None)
    {
        apply_cal_request(*req.cal, now_ms);
    }

    // The jog is forced to (0,0) while the knob is held in Calibrate or a HUD record waits for the pan to stop; an
    // aim then would move the pose being recorded, so it is dropped too.
    const bool cal_stop = mode_ == ControlMode::Calibrate && (held || hud_record_pending_);
    if (req.aim.has_value() && (mode_ == ControlMode::User || mode_ == ControlMode::Calibrate) && !cal_stop)
    {
        // aim() clamps to the soft limits. The hold keeps the idle jog arbiter from cancelling the move.
        io_.turret_set_target(policy_.map().aim(req.aim->x, req.aim->y));
        aim_hold_ = true;
    }

    switch (mode_)
    {
    case ControlMode::User:
        // User mode needs no frame. Jog commands are cheap mailbox writes; keep the 25 Hz cadence.
        jog_from_inputs(now_ms, false);
        break;

    case ControlMode::Calibrate:
        jog_from_inputs(now_ms, cal_stop);
        // Detection runs in its own task, so the HUD box shows; no aim policy while calibrating.
        break;

    case ControlMode::Ai: {
        const TrackSnapshot s = io_.read_track();
        const aiming::AimCommand cmd =
            policy_.update(aiming::TrackView{.state = s.state, .x = (float)s.x, .y = (float)s.y}, now_ms);
        if (cmd.kind == aiming::AimKind::Aim)
        {
            io_.turret_set_target(cmd.angles);
            ++aims_;
        } else if (cmd.kind == aiming::AimKind::Home)
        {
            io_.turret_home();
        }
        break;
    }
    }
}
} // namespace control
