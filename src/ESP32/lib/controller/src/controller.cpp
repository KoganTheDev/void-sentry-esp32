#include "controller.h"
#include "camera.h"
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

    camera_fb_t* fb = this->_camera.capture();

    if (fb == NULL)
    {
        ESP_LOGW(TAG, "AI_MODE: Frame capture failed - returned NULL. Camera buffer unavailable.");
        unsigned long now = millis();
        return;
    }
    // ESP_LOGD(TAG, "AI_MODE: Frame captured successfully (%u bytes)", fb->len);

    std::tuple<MoveX, MoveY> move_directions;

    // TODO: try refactoring, it's not that readable.
    // What happens here:
    // 1. move_directions assigns the move directions as if in AI mode
    // 2. If in USER_MODE: //* Overwrites the directions given by the detection module and uses the directions from the joystick
    
    move_directions = this->_detection_module.detect_object(fb);


    if (this->_system_control_state.is_user()) // USER mode
    {
        move_directions = user_mode();
    }
    
    this->_movement_manager.move_relative(move_directions); // Move stepper & motor
    this->_camera.release(fb); // Release the camera's buffer
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
