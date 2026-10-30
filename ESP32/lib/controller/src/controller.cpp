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
        this->_logic.load_calibration(map);
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

void Controller::publish_status(uint32_t now)
{
    const remote::JogSpeed speed =
        this->_remote_inbox.inspect([](const remote::RemoteInbox& inbox) { return inbox.speed(); });
    this->_status.store(this->_logic.status(now, this->_turret.position_deg(), speed));
}

ControlStatus Controller::control_status() const { return this->_status.load(); }

void Controller::run()
{
    const uint32_t now = millis();
    this->_logic.run(now, this->_joystick.is_z_held());
    this->publish_status(now);
}

control::StickDeflection Controller::read_stick() noexcept
{
    const int sx = this->_joystick.get_speed_x();
    const int sy = this->_joystick.get_speed_y();
    return control::StickDeflection{.x = sx, .y = sy};
}

remote::Jog Controller::read_hud_jog() noexcept
{
    // Read the clock inside the lock: submit() stamps its move before taking the lock, so any stamp visible here is
    // <= this millis(). Using the pass-start `now` could be older than the stamp (preemption), and the unsigned age
    // would wrap to ~4e9 and drop a live jog for one pass.
    return this->_remote_inbox.inspect([](const remote::RemoteInbox& inbox) { return inbox.jog(millis()); });
}

control::HudRequests Controller::take_hud_requests() noexcept
{
    return this->_remote_inbox.update([](remote::RemoteInbox& inbox) {
        return control::HudRequests{inbox.take_mode_request(), inbox.take_cal_request(), inbox.take_aim_request()};
    });
}

TrackSnapshot Controller::read_track() noexcept { return this->_detection_module.get_track_snapshot(); }

aiming::Angles Controller::turret_pose() noexcept { return this->_turret.position_deg(); }

float Controller::turret_pan_speed() noexcept { return this->_turret.pan_speed(); }

void Controller::turret_jog(float pan, float tilt) noexcept { this->_turret.jog(pan, tilt); }

void Controller::turret_set_target(const aiming::Angles& target) noexcept { this->_turret.set_target_deg(target); }

void Controller::turret_home() noexcept { this->_turret.home(); }

void Controller::reset_detection() noexcept
{
    this->_detection_module.reset(); // a request the detection task applies: no stale lock, fresh start
}

bool Controller::save_calibration(const aiming::AimMap& map) noexcept { return cal_store::save(map); }

void Controller::log(const control::LogRecord& record) noexcept
{
    using enum control::LogEvent;
    switch (record.event)
    {
    case ModeChanged:
        ESP_LOGI(TAG, "Control mode changed to: %s", to_string(record.mode));
        break;
    case CalStarted:
        ESP_LOGI(TAG, "Calibration started (from %s)", to_string(record.mode));
        break;
    case CalCancelled:
        ESP_LOGI(TAG, "Calibration cancelled, back to %s", to_string(record.mode));
        break;
    case CalSaveFailed:
        ESP_LOGE(TAG, "Failed to save the calibration to NVS (it stays active until reboot)");
        break;
    case CalComplete:
        ESP_LOGI(TAG, "Calibration complete, AI mode");
        break;
    case CalFailed:
        ESP_LOGW(TAG, "Calibration failed: %s", record.failure ? record.failure : "unknown");
        break;
    case CalRestarted:
        ESP_LOGI(TAG, "Calibration restarted at point 1");
        break;
    case CalMiss:
        ESP_LOGW(TAG, "Calibration record ignored: no target visible - wave at the spot");
        break;
    case CalPointRecorded:
        ESP_LOGI(TAG, "Calibration points recorded: %d", record.points);
        break;
    case HudPoseTimeout:
        ESP_LOGW(TAG, "Calibration pose: pan still moving after %u ms, using the current pose",
                 (unsigned)control::kHudRecordTimeoutMs);
        break;
    case PressPoseFallback:
        ESP_LOGW(TAG, "Calibration pose: no stationary pose captured during the press, using the current pose");
        break;
    }
}
