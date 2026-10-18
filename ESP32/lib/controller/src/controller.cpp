#include "controller.h"
#include "cal_store.h"
#include "camera.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "CONTROLLER";

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
    this->publish_status();
}

void Controller::submit(const remote::Command& cmd)
{
    const uint32_t now = millis(); // outside the lock
    this->_remote_inbox.update([&](remote::RemoteInbox& inbox) { inbox.submit(cmd, now); });
}

void Controller::jog_from_inputs(uint32_t now, bool calibrate_button_held)
{
    if (now - this->_last_user_move_ms < kUserModePeriodMs)
    {
        return;
    }
    this->_last_user_move_ms = now;

    if (calibrate_button_held)
    {
        this->_turret.jog(0.0f, 0.0f);
        return;
    }
    const int sx = this->_joystick.get_speed_x();
    const int sy = this->_joystick.get_speed_y();
    if (sx != 0 || sy != 0)
    {
        // Joystick y positive = Up = larger tilt angle. The stick overrides any HUD jog.
        this->_turret.jog(sx / 255.0f, sy / 255.0f);
        return;
    }
    const remote::Jog jog = this->_remote_inbox.update([now](remote::RemoteInbox& inbox) { return inbox.jog(now); });
    this->_turret.jog(jog.pan, jog.tilt); // (0,0) once the dead-man window has passed
}

void Controller::set_mode(ControlMode mode)
{
    this->_mode = mode;
    this->_detection_module.reset(); // a request the detection task applies: no stale lock, fresh start
    this->_policy.reset();
    this->_turret.jog(0.0f, 0.0f);
    ESP_LOGI(TAG, "Control mode changed to: %s", to_string(this->_mode));
}

void Controller::on_long_press()
{
    if (this->_mode == ControlMode::Calibrate)
    {
        this->_session.cancel();
        this->_mode = this->_mode_before_cal;
        this->_policy.reset(); // the turret was jogged by hand; forget the old setpoint
        this->_turret.jog(0.0f, 0.0f);
        ESP_LOGI(TAG, "Calibration cancelled, back to %s", to_string(this->_mode));
    } else
    {
        this->_mode_before_cal = this->_mode;
        this->_mode = ControlMode::Calibrate;
        this->_session.start();
        this->_policy.reset();
        this->_turret.jog(0.0f, 0.0f);
        ESP_LOGI(TAG, "Calibration started (from %s)", to_string(this->_mode_before_cal));
    }
}

void Controller::on_short_press()
{
    if (this->_mode == ControlMode::Calibrate)
    {
        // Stop at the pose being recorded; the stick resumes jogging on the next 25 Hz pass if still deflected.
        this->_turret.jog(0.0f, 0.0f);
        const TrackSnapshot s = this->_detection_module.get_track_snapshot();
        const aiming::CalState before_state = this->_session.state();
        const int before_points = this->_session.points();
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
            this->_session.cancel();
            this->_turret.jog(0.0f, 0.0f); // the stick may still be deflected; do not leave the turret running
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
            ESP_LOGW(TAG, "Calibration press ignored: no target visible - wave at the spot");
        } else
        {
            ESP_LOGI(TAG, "Calibration points recorded: %d", this->_session.points());
        }
    } else
    {
        this->set_mode(this->_mode == ControlMode::Ai ? ControlMode::User : ControlMode::Ai);
    }
}

void Controller::publish_status()
{
    ControlStatus st;
    st.mode = this->_mode;
    st.cal_points = (uint8_t)this->_session.points();
    st.cal_state = this->_session.state();
    st.calibrated = this->_calibrated;
    st.angles = this->_turret.position_deg();
    st.cal_failure = this->_session.failure();
    st.aims = this->_aims;

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
    const bool was_calibrating = this->_mode == ControlMode::Calibrate;
    const aiming::ButtonEvent ev = this->_button.update(held, now);
    if (ev == aiming::ButtonEvent::Long)
    {
        this->on_long_press();
    } else if (ev == aiming::ButtonEvent::Short)
    {
        this->on_short_press();
    }

    // Take the HUD's mode request every pass so one made during Calibrate is dropped, not applied later.
    const std::optional<remote::RemoteMode> mode_req =
        this->_remote_inbox.update([](remote::RemoteInbox& inbox) { return inbox.take_mode_request(); });
    if (mode_req.has_value() && !was_calibrating && this->_mode != ControlMode::Calibrate)
    {
        const ControlMode wanted = (*mode_req == remote::RemoteMode::Ai) ? ControlMode::Ai : ControlMode::User;
        if (wanted != this->_mode)
        {
            this->set_mode(wanted);
        }
    }

    switch (this->_mode)
    {
    case ControlMode::User:
        // User mode needs no frame. Jog commands are cheap mailbox writes; keep the 25 Hz cadence.
        this->jog_from_inputs(now, false);
        break;

    case ControlMode::Calibrate:
        this->jog_from_inputs(now, held);
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

    this->publish_status();
}
