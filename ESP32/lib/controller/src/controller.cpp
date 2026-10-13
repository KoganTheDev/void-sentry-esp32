#include "controller.h"
#include "cal_store.h"
#include "camera.h"
#include "frame_store.h"
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

void Controller::process_new_frame()
{
    std::optional<frames::FrameLease> lease = this->_frame_store.acquire_latest();
    if (!lease || lease->meta().sequence == this->_last_sequence)
    {
        return;
    }
    this->_last_sequence = lease->meta().sequence;

    camera_buffer_t fb{const_cast<uint8_t*>(this->_frame_store.data(*lease)), lease->meta().length, lease->meta().width,
                       lease->meta().height};
    this->_detection_module.detect_object(fb); // the result is read back as a track snapshot

    lease.reset();
}

void Controller::jog_from_joystick(uint32_t now)
{
    if (now - this->_last_user_move_ms >= kUserModePeriodMs)
    {
        this->_last_user_move_ms = now;
        // Joystick y positive = Up = larger tilt angle.
        this->_turret.jog(this->_joystick.get_speed_x() / 255.0f, this->_joystick.get_speed_y() / 255.0f);
    }
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
        const aiming::CalState st =
            this->_session.record((float)s.x, (float)s.y, this->_turret.position_deg(), s.detected());
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
        this->_mode = (this->_mode == ControlMode::Ai) ? ControlMode::User : ControlMode::Ai;
        this->_detection_module.reset(); // no stale lock in User mode, fresh start when Ai resumes
        this->_policy.reset();
        this->_turret.jog(0.0f, 0.0f);
        ESP_LOGI(TAG, "Control mode changed to: %s", to_string(this->_mode));
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

    portENTER_CRITICAL(&this->_status_lock);
    this->_status = st;
    portEXIT_CRITICAL(&this->_status_lock);
}

ControlStatus Controller::control_status() const
{
    portENTER_CRITICAL(&this->_status_lock);
    const ControlStatus st = this->_status;
    portEXIT_CRITICAL(&this->_status_lock);
    return st;
}

void Controller::run()
{
    ESP_LOGV(TAG, "Controller::run() called");

    const uint32_t now = millis();
    const aiming::ButtonEvent ev = this->_button.update(this->_joystick.is_z_held(), now);
    if (ev == aiming::ButtonEvent::Long)
    {
        this->on_long_press();
    } else if (ev == aiming::ButtonEvent::Short)
    {
        this->on_short_press();
    }

    switch (this->_mode)
    {
    case ControlMode::User:
        // User mode needs no frame. Jog commands are cheap mailbox writes; keep the 25 Hz cadence.
        this->jog_from_joystick(now);
        break;

    case ControlMode::Calibrate:
        this->jog_from_joystick(now);
        this->process_new_frame(); // so the HUD box shows; no aim policy while calibrating
        break;

    case ControlMode::Ai: {
        this->process_new_frame();
        const TrackSnapshot s = this->_detection_module.get_track_snapshot();
        const aiming::AimCommand cmd = this->_policy.update(aiming::TrackView{s.state, (float)s.x, (float)s.y}, now);
        if (cmd.kind == aiming::AimKind::Aim)
        {
            this->_turret.set_target_deg(cmd.angles);
        } else if (cmd.kind == aiming::AimKind::Home)
        {
            this->_turret.home();
        }
        break;
    }
    }

    this->publish_status();
}
