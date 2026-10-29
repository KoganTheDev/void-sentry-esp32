#include "controller.h"
#include "cal_store.h"
#include "camera.h"
#include "status_fields.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "CONTROLLER";

// ControlStatus::speed is published as the JogSpeed value (0 slow, 1 normal, 2 fast).
static_assert(static_cast<uint8_t>(remote::JogSpeed::Slow) == 0 &&
                  static_cast<uint8_t>(remote::JogSpeed::Normal) == 1 &&
                  static_cast<uint8_t>(remote::JogSpeed::Fast) == 2,
              "ControlStatus::speed relies on the JogSpeed values");

void Controller::begin()
{
    aiming::AimMap map;
    if (cal_store::load(map))
    {
        this->_policy.set_map(map);
        this->_calibrated = true;
        ESP_LOGI(TAG, "Loaded saved calibration from NVS");
    } else
    {
        ESP_LOGW(TAG, "No valid saved calibration, using the default map (hold the button 1.5 s to calibrate)");
    }
    this->publish_status(millis());
}

void Controller::submit(const remote::Command& cmd)
{
    const uint32_t now = millis(); // outside the lock
    this->_remote_inbox.update([&](remote::RemoteInbox& inbox) { inbox.submit(cmd, now); });
}

void Controller::jog_from_inputs(uint32_t now, bool force_stop)
{
    if (now - this->_last_user_move_ms < kUserModePeriodMs)
    {
        return;
    }
    this->_last_user_move_ms = now;

    if (force_stop)
    {
        this->_stick = false; // the knob press deflects the stick, but it is not jogging
        this->stop_jog();
        return;
    }
    const int sx = this->_joystick.get_speed_x();
    const int sy = this->_joystick.get_speed_y();
    this->_stick = (sx != 0 || sy != 0);
    if (this->_stick)
    {
        // Joystick y positive = Up = larger tilt angle. The stick overrides any HUD jog and ends a HUD aim.
        this->_aim_hold = false;
        this->_turret.jog(sx / 255.0f, sy / 255.0f);
        return;
    }
    const remote::Jog jog =
        this->_remote_inbox.inspect([now](const remote::RemoteInbox& inbox) { return inbox.jog(now); });
    if (jog.pan != 0.0f || jog.tilt != 0.0f)
    {
        this->_aim_hold = false;
        this->_turret.jog(jog.pan, jog.tilt);
        return;
    }
    // No jog source: (0,0) once the dead-man window has passed, unless a HUD aim is still travelling (a zero jog
    // would stop it where it is).
    if (!this->_aim_hold)
    {
        this->_turret.jog(0.0f, 0.0f);
    }
}

void Controller::stop_jog()
{
    this->_aim_hold = false;
    this->_turret.jog(0.0f, 0.0f);
}

void Controller::set_mode(ControlMode mode)
{
    this->_mode = mode;
    this->_detection_module.reset(); // a request the detection task applies: no stale lock, fresh start
    this->_policy.reset();
    this->stop_jog();
    ESP_LOGI(TAG, "Control mode changed to: %s", to_string(this->_mode));
}

void Controller::enter_calibration()
{
    this->_mode_before_cal = this->_mode;
    this->_mode = ControlMode::Calibrate;
    this->_session.start();
    this->_policy.reset();
    this->stop_jog();
    this->_hud_record_pending = false;
    ESP_LOGI(TAG, "Calibration started (from %s)", to_string(this->_mode_before_cal));
}

void Controller::cancel_calibration()
{
    this->_session.cancel();
    this->_mode = this->_mode_before_cal;
    this->_policy.reset(); // the turret was jogged by hand; forget the old setpoint
    this->stop_jog();
    this->_hud_record_pending = false;
    ESP_LOGI(TAG, "Calibration cancelled, back to %s", to_string(this->_mode));
}

void Controller::record_cal_point(const aiming::Angles& pose)
{
    const TrackSnapshot s = this->_detection_module.get_track_snapshot();
    const aiming::CalState before_state = this->_session.state();
    const int before_points = this->_session.points();
    const aiming::CalState st = this->_session.record((float)s.x, (float)s.y, pose, s.detected());
    if (st == aiming::CalState::Done)
    {
        const aiming::AimMap map = *this->_session.result();
        if (!cal_store::save(map))
        {
            ESP_LOGE(TAG, "Failed to save the calibration to NVS (it stays active until reboot)");
        }
        this->_policy.set_map(map);
        this->_calibrated = true;
        ++this->_cal_saves; // counts the active fit even if the NVS write failed: the HUD reports the calibration
        this->_session.cancel();
        this->stop_jog(); // the stick may still be deflected; do not leave the turret running
        this->_hud_record_pending = false;
        this->_mode = ControlMode::Ai;
        this->_policy.reset();
        ESP_LOGI(TAG, "Calibration complete, AI mode");
    } else if (st == aiming::CalState::Failed)
    {
        const char* why = this->_session.failure();
        ESP_LOGW(TAG, "Calibration failed: %s", why ? why : "unknown");
    } else if (before_state == aiming::CalState::Failed)
    {
        ESP_LOGI(TAG, "Calibration restarted at point 1");
    } else if (st == before_state && this->_session.points() == before_points)
    {
        ++this->_cal_miss;
        ESP_LOGW(TAG, "Calibration record ignored: no target visible - wave at the spot");
    } else
    {
        ESP_LOGI(TAG, "Calibration points recorded: %d", this->_session.points());
    }
}

void Controller::apply_cal_request(remote::CalAction action, uint32_t now)
{
    const bool calibrating = this->_mode == ControlMode::Calibrate;
    switch (action)
    {
    case remote::CalAction::Start:
        if (!calibrating)
        {
            this->enter_calibration();
        }
        break;

    case remote::CalAction::Cancel:
        if (calibrating)
        {
            this->cancel_calibration();
        }
        break;

    case remote::CalAction::Point:
        // A second cal:point while one waits for the pan to stop is the same request; keep the first one's timer.
        if (calibrating && !this->_hud_record_pending)
        {
            this->stop_jog();
            this->_hud_record_pending = true;
            this->_hud_record_since_ms = now;
        }
        break;
    }
}

void Controller::service_hud_record(uint32_t now)
{
    if (!this->_hud_record_pending)
    {
        return;
    }
    if (this->_mode != ControlMode::Calibrate)
    {
        this->_hud_record_pending = false; // the button ended calibration meanwhile
        return;
    }
    const bool at_rest = this->_turret.pan_speed() == 0.0f;
    if (!at_rest && now - this->_hud_record_since_ms < kHudRecordTimeoutMs)
    {
        return;
    }
    if (!at_rest)
    {
        ESP_LOGW(TAG, "Calibration pose: pan still moving after %u ms, using the current pose",
                 (unsigned)kHudRecordTimeoutMs);
    }
    this->_hud_record_pending = false;
    this->record_cal_point(this->_turret.position_deg());
}

void Controller::on_long_press()
{
    if (this->_mode == ControlMode::Calibrate)
    {
        this->cancel_calibration();
    } else
    {
        this->enter_calibration();
    }
}

void Controller::on_short_press()
{
    if (this->_mode == ControlMode::Calibrate)
    {
        // Stop at the pose being recorded; the stick resumes jogging on the next 25 Hz pass if still deflected.
        this->stop_jog();
        // The knob press is the record: a HUD cal:point still waiting must not add a second point at this pose.
        this->_hud_record_pending = false;
        // Record the pose captured while the knob was held and the pan was at rest, not the pose now: pressing the
        // knob deflects the stick, and the release is only classified ~50 ms later, so position_deg() may have
        // drifted away from where the user aimed. Fallback (released before the pan stopped): the current pose.
        aiming::Angles pose = this->_cal_pose_at_press;
        if (!this->_cal_pose_captured)
        {
            pose = this->_turret.position_deg();
            ESP_LOGW(TAG, "Calibration pose: no stationary pose captured during the press, using the current pose");
        }
        this->_cal_pose_captured = false;
        this->record_cal_point(pose);
    } else
    {
        this->set_mode(this->_mode == ControlMode::Ai ? ControlMode::User : ControlMode::Ai);
    }
}

void Controller::publish_status(uint32_t now)
{
    const bool ai = this->_mode == ControlMode::Ai;
    const remote::JogSpeed speed =
        this->_remote_inbox.inspect([](const remote::RemoteInbox& inbox) { return inbox.speed(); });

    ControlStatus st;
    st.mode = this->_mode;
    st.cal_points = (uint8_t)this->_session.points();
    st.cal_state = this->_session.state();
    st.calibrated = this->_calibrated;
    st.angles = this->_turret.position_deg();
    st.cal_failure = this->_session.failure();
    st.aims = this->_aims;
    st.cal_saves = this->_cal_saves;
    st.cal_miss = this->_cal_miss;
    st.stick = !ai && this->_stick; // the stick only jogs in User and Calibrate
    st.speed = static_cast<uint8_t>(speed);
    hud::fill_pose_fields(st, this->_policy.map(),
                          hud::HomeState{.ai_mode = ai,
                                         .homed = this->_policy.homed(),
                                         .home_remaining_ms = ai ? this->_policy.home_remaining_ms(now) : 0});

    this->_status.store(st);
}

ControlStatus Controller::control_status() const { return this->_status.load(); }

void Controller::run()
{
    ESP_LOGV(TAG, "Controller::run() called");

    const uint32_t now = millis();
    const bool held = this->_joystick.is_z_held();
    if (held && !this->_prev_held)
    {
        this->_cal_pose_captured = false; // a new press starts: forget any pose from an earlier one
    }
    this->_prev_held = held;
    // While the knob is held in Calibrate the stick is deflected by the press itself, so the jog is zeroed and the
    // pose is captured on the first pass where the pan has actually stopped (pan_speed() == 0).
    if (this->_mode == ControlMode::Calibrate && held && !this->_cal_pose_captured && this->_turret.pan_speed() == 0.0f)
    {
        this->_cal_pose_at_press = this->_turret.position_deg();
        this->_cal_pose_captured = true;
    }
    // A HUD mode request is dropped if calibration was active at the start of this pass, even if the button ends it.
    // A HUD calibration request is dropped on any pass where the button produced an event: a queued cal:start after a
    // long press that cancelled would silently restart, and cal:cancel or cal:point after a press would double up.
    const bool was_calibrating = this->_mode == ControlMode::Calibrate;
    const aiming::ButtonEvent ev = this->_button.update(held, now);
    if (ev == aiming::ButtonEvent::Long)
    {
        this->on_long_press();
    } else if (ev == aiming::ButtonEvent::Short)
    {
        this->on_short_press();
    }

    // A HUD cal:point taken on an earlier pass records once the pan is at rest (or after the timeout).
    this->service_hud_record(now);

    // Take each HUD request every pass so one that does not apply now is dropped, not applied later.
    struct Requests {
        std::optional<remote::RemoteMode> mode;
        std::optional<remote::CalAction> cal;
        std::optional<remote::AimPixel> aim;
    };
    const Requests req = this->_remote_inbox.update([](remote::RemoteInbox& inbox) {
        return Requests{inbox.take_mode_request(), inbox.take_cal_request(), inbox.take_aim_request()};
    });
    if (req.mode.has_value() && !was_calibrating && this->_mode != ControlMode::Calibrate)
    {
        const ControlMode wanted = (*req.mode == remote::RemoteMode::Ai) ? ControlMode::Ai : ControlMode::User;
        if (wanted != this->_mode)
        {
            this->set_mode(wanted);
        }
    }
    if (req.cal.has_value() && ev == aiming::ButtonEvent::None)
    {
        this->apply_cal_request(*req.cal, now);
    }

    // The jog is forced to (0,0) while the knob is held in Calibrate or a HUD record waits for the pan to stop; an
    // aim then would move the pose being recorded, so it is dropped too.
    const bool cal_stop = this->_mode == ControlMode::Calibrate && (held || this->_hud_record_pending);
    if (req.aim.has_value() && (this->_mode == ControlMode::User || this->_mode == ControlMode::Calibrate) && !cal_stop)
    {
        // aim() clamps to the soft limits. The hold keeps the idle jog arbiter from cancelling the move.
        this->_turret.set_target_deg(this->_policy.map().aim(req.aim->x, req.aim->y));
        this->_aim_hold = true;
    }

    switch (this->_mode)
    {
    case ControlMode::User:
        // User mode needs no frame. Jog commands are cheap mailbox writes; keep the 25 Hz cadence.
        this->jog_from_inputs(now, false);
        break;

    case ControlMode::Calibrate:
        this->jog_from_inputs(now, cal_stop);
        // Detection runs in its own task, so the HUD box shows; no aim policy while calibrating.
        break;

    case ControlMode::Ai: {
        const TrackSnapshot s = this->_detection_module.get_track_snapshot();
        const aiming::AimCommand cmd =
            this->_policy.update(aiming::TrackView{.state = s.state, .x = (float)s.x, .y = (float)s.y}, now);
        if (cmd.kind == aiming::AimKind::Aim)
        {
            this->_turret.set_target_deg(cmd.angles);
            ++this->_aims;
        } else if (cmd.kind == aiming::AimKind::Home)
        {
            this->_turret.home();
        }
        break;
    }
    }

    this->publish_status(now);
}
