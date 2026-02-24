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
        this->_system_control_state = toggle_mode(this->_system_control_state);
        ESP_LOGI(TAG, "Control mode changed to: %s", to_string(this->_system_control_state));
    }

    std::tuple<MoveDirectionX, MoveDirectionY> move_directions;

    if (this->_system_control_state == SystemControl::USER_MODE) // USER mode
    {
        ESP_LOGV(TAG, "USER_MODE: Reading joystick input");
        // USER_MODE: Read joystick and command motors
        int speed_x = this->_joystick.get_speed_x();
        int speed_y = this->_joystick.get_speed_y();

        MoveDirectionX user_yaw = (speed_x > 0)   ? MoveDirectionX::Right
                                  : (speed_x < 0) ? MoveDirectionX::Left
                                                  : MoveDirectionX::None;

        MoveDirectionY user_pitch = (speed_y > 0)   ? MoveDirectionY::Up
                                    : (speed_y < 0) ? MoveDirectionY::Down
                                                    : MoveDirectionY::None;

        move_directions = std::make_tuple(user_yaw, user_pitch);
    } else // AI mode
    {
        // TODO: Find a more consice log for this section instad ESP_LOGV(TAG, "AI_MODE: Attempting frame capture...");
        static unsigned long frame_count = 0;

        camera_fb_t* fb = this->_camera.capture();

        if (fb == NULL)
        {
            ESP_LOGW(TAG, "AI_MODE: Frame capture failed - returned NULL. Camera buffer unavailable.");
            unsigned long now = millis();
            return;
        }

        frame_count++;
        ESP_LOGD(TAG, "AI_MODE: Frame captured successfully (%u bytes)", fb->len);

        move_directions = this->_detection_module.detect_object(fb);
        MoveDirectionX move_x = std::get<0>(move_directions);
        MoveDirectionY move_y = std::get<1>(move_directions);

        // Log detection results only when motion is found
        if (move_x != MoveDirectionX::None || move_y != MoveDirectionY::None)
        {
            ESP_LOGI(TAG, "AI_MODE: Motion detected - Moving X:%s Y:%s", moveDirectionXToString(move_x),
                     moveDirectionYToString(move_y));
            Serial.printf("[AI_MODE] Motion detected - Moving X:%s Y:%s\n", moveDirectionXToString(move_x),
                          moveDirectionYToString(move_y));
        }

        ESP_LOGV(TAG, "AI_MODE: Releasing frame buffer");
        this->_camera.release(fb);
    }

    this->_movement_manager.move_relative(move_directions);
}
