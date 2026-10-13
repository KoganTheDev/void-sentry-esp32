#include "step_planner.h"

#include <algorithm>
#include <cmath>

namespace aiming
{
namespace
{
int sign(float v) { return (v > 0.0f) - (v < 0.0f); }
} // namespace

StepPlanner::StepPlanner(const PlannerConfig& config) : config_(config) {}

void StepPlanner::set_target(int32_t steps)
{
    mode_ = Mode::Target;
    target_ = std::clamp(steps, config_.min_pos, config_.max_pos);
}

void StepPlanner::set_velocity(float steps_per_s)
{
    mode_ = Mode::Jog;
    jog_speed_ = std::clamp(steps_per_s, -config_.max_speed, config_.max_speed);
}

bool StepPlanner::is_idle() const
{
    if (speed_ != 0.0f)
    {
        return false;
    }
    return mode_ == Mode::Target ? position_ == target_ : jog_speed_ == 0.0f;
}

void StepPlanner::reset_position(int32_t steps)
{
    position_ = steps;
    target_ = steps;
    mode_ = Mode::Target;
    jog_speed_ = 0.0f;
    speed_ = 0.0f;
    frac_ = 0.0f;
}

float StepPlanner::goal_speed()
{
    const float brake_dist = speed_ * speed_ / (2.0f * config_.accel);
    if (mode_ == Mode::Jog)
    {
        const int dir = sign(jog_speed_);
        if (dir == 0)
        {
            return 0.0f;
        }
        const int32_t room = dir > 0 ? config_.max_pos - position_ : position_ - config_.min_pos;
        if (static_cast<float>(room) <= brake_dist + 1.0f)
        {
            return 0.0f; // brake into the soft limit
        }
        return jog_speed_;
    }

    const int32_t r = target_ - position_;
    const int dir = r != 0 ? (r > 0 ? 1 : -1) : -sign(speed_);
    if (speed_ * static_cast<float>(dir) < 0.0f)
    {
        return 0.0f; // moving away from the target: brake first
    }
    if (static_cast<float>(std::abs(r)) <= brake_dist + 1.0f)
    {
        return 0.0f; // close enough that we must brake now
    }
    return static_cast<float>(dir) * config_.max_speed;
}

int StepPlanner::tick()
{
    const int32_t r = target_ - position_;
    if (mode_ == Mode::Target && r == 0 && std::fabs(speed_) <= config_.min_speed)
    {
        speed_ = 0.0f;
        frac_ = 0.0f;
        return 0;
    }

    const float dv = config_.accel * config_.tick_s;
    const float goal = goal_speed();
    if (mode_ == Mode::Jog && goal == 0.0f && std::fabs(speed_) <= config_.min_speed)
    {
        speed_ = 0.0f; // jog finished braking: drop from creep speed to rest (the allowed creep jump)
        frac_ = 0.0f;
        return 0;
    }
    if (speed_ < goal)
    {
        speed_ = std::min(speed_ + dv, goal);
    } else if (speed_ > goal)
    {
        speed_ = std::max(speed_ - dv, goal);
    }

    if (mode_ == Mode::Target && r != 0)
    {
        const float dir = r > 0 ? 1.0f : -1.0f;
        // Creep at min_speed toward the target: start without ramping, and never stall short of it while braking.
        if (speed_ * dir >= 0.0f && std::fabs(speed_) < config_.min_speed)
        {
            speed_ = dir * config_.min_speed;
        }
    } else if (mode_ == Mode::Jog && goal != 0.0f && std::fabs(speed_) < config_.min_speed && speed_ * goal >= 0.0f)
    {
        speed_ = static_cast<float>(sign(goal)) * config_.min_speed;
    }

    frac_ += speed_ * config_.tick_s;
    int step = 0;
    if (frac_ >= 1.0f)
    {
        step = 1;
        frac_ -= 1.0f;
    } else if (frac_ <= -1.0f)
    {
        step = -1;
        frac_ += 1.0f;
    }
    if (step != 0)
    {
        const int32_t next = position_ + step;
        if (next < config_.min_pos || next > config_.max_pos)
        {
            speed_ = 0.0f; // hard stop at a soft limit (only reachable if braking was cut short)
            frac_ = 0.0f;
            return 0;
        }
        position_ = next;
    }
    return step;
}
} // namespace aiming
