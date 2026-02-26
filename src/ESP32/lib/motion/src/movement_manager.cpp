#include "movement_manager.h"

void MovementManager::move_relative(const std::tuple<MoveX, MoveY> move_directions)
{

    // Unpack
    MoveX x = std::get<0>(move_directions);
    MoveY y = std::get<1>(move_directions);

    move_stepper(x);
    move_servo(y);
}

void MovementManager::move_stepper(const MoveX yaw_direction)
{
    if (yaw_direction != MoveX::None)
    {
        if (yaw_direction == MoveX::Left) // Rotate Left
        {
            this->_stepper.step(-this->_STEP_INCREMENT);
        } else if (yaw_direction == MoveX::Right) // Rotate Right
        {
            this->_stepper.step(this->_STEP_INCREMENT);
        }
    }
}

void MovementManager::move_servo(const MoveY pitch_direction)
{
    if (pitch_direction != MoveY::None)
    {
        int current_angle = this->_servo.read();
        int target_angle = current_angle;

        if (pitch_direction == MoveY::Up) // Angle Up
        {
            target_angle += this->_SERVO_INCREMENT;
        } else if (pitch_direction == MoveY::Down) // Angle Down
        {
            target_angle -= this->_SERVO_INCREMENT;
        }

        target_angle = constrain(target_angle, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
        this->_servo.write(target_angle);
    }
}
