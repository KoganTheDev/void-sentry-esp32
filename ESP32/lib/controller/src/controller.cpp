#include "controller.h"
#include "camera.h"
#include "frame_store.h"
#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "CONTROLLER";

void Controller::run()
{
    ESP_LOGV(TAG, "Controller::run() called");

    if (this->_joystick.is_z_pressed()) // Flip control state
    {
        this->_system_control_state.toggle_mode();
        ESP_LOGI(TAG, "Control mode changed to: %s", this->_system_control_state.to_string());
    }

    if (this->_system_control_state.is_user())
    {
        // USER_MODE needs no frame. Throttle so the stepper is not driven faster than it can step.
        const uint32_t now = millis();
        if (now - this->_last_user_move_ms >= kUserModePeriodMs)
        {
            this->_last_user_move_ms = now;
            this->_movement_manager.move_relative(user_mode());
        }
        return;
    }

    // AI_MODE: act once per new frame sequence.
    std::optional<frames::FrameLease> lease = this->_frame_store.acquire_latest();
    if (!lease || lease->meta().sequence == this->_last_sequence)
    {
        return;
    }
    this->_last_sequence = lease->meta().sequence;

    camera_buffer_t fb{const_cast<uint8_t*>(this->_frame_store.data(*lease)), lease->meta().length, lease->meta().width,
                       lease->meta().height};
    const std::tuple<MoveX, MoveY> move_directions = this->_detection_module.detect_object(fb);

    // Release the slot before moving: the stepper blocks ~15 ms and must not pin a frame slot.
    lease.reset();

    this->_movement_manager.move_relative(move_directions); // Move stepper & motor
}

const std::tuple<MoveX, MoveY> Controller::user_mode() const
{
    // USER_MODE: Read joystick and command motors
    ESP_LOGV(TAG, "USER_MODE: Reading joystick input");
    int speed_x = this->_joystick.get_speed_x();
    int speed_y = this->_joystick.get_speed_y();

    MoveX user_yaw = (speed_x > 0) ? MoveX::Right : (speed_x < 0) ? MoveX::Left : MoveX::None;

    MoveY user_pitch = (speed_y > 0) ? MoveY::Up : (speed_y < 0) ? MoveY::Down : MoveY::None;

    return std::make_tuple(user_yaw, user_pitch);
}

// const std::tuple<MoveX, MoveY> Controller::ai_mode() const
// {
//     //MoveX move_x = std::get<0>(move_directions);
//     //MoveY move_y = std::get<1>(move_directions);

//     // Log detection results only when motion is found
//     // if (move_x != MoveX::None || move_y != MoveY::None)
//     // {
//     //     ESP_LOGI(TAG, "AI_MODE: Motion detected - Moving X:%s Y:%s", MoveXToString(move_x),
//     //              MoveYToString(move_y));
//     // }

//     ESP_LOGV(TAG, "AI_MODE: Releasing frame buffer");
//     // return move_directions;
//     return std::make_tuple(MoveX::None, MoveY::None);
// }
