/**
 * @file step_planner.h
 * @brief Trapezoidal velocity planner for the pan stepper, ticked at 1 kHz (pure C++17, natively tested).
 * @details Target mode moves to an absolute step count: accelerate, cruise, brake so it stops exactly on the target
 * (no overshoot); a new target mid-move is reached by braking first, never by an instant reversal. Jog mode follows a
 * commanded velocity with the same acceleration limit. Both modes brake into the soft limits and never step past
 * them. tick() returns the step to emit this tick (-1, 0, +1); at most one per tick (max 400 steps/s < 1 kHz).
 * NOT thread-safe: the motor task owns it.
 */

#pragma once

#include <cstdint>

namespace aiming
{
struct PlannerConfig {
    float max_speed = 400.0f; ///< steps/s
    float accel = 800.0f;     ///< steps/s^2
    float min_speed = 20.0f;  ///< creep speed: start/finish at this without ramping
    int32_t min_pos = -512;   ///< soft limits (+-90 deg at 2048 steps/rev)
    int32_t max_pos = 512;
    float tick_s = 0.001f;
};

class StepPlanner
{
public:
    explicit StepPlanner(const PlannerConfig& config = PlannerConfig{});

    void set_target(int32_t steps); ///< absolute, clamped to the soft limits; switches to target mode
    void set_velocity(float steps_per_s); ///< jog; clamped to +-max_speed; switches to jog mode
    int tick();

    int32_t position() const { return position_; }
    int32_t target() const { return target_; }
    float speed() const { return speed_; }
    bool is_idle() const;
    void reset_position(int32_t steps = 0); ///< redefine the current position (zero_here); stops motion
    const PlannerConfig& config() const { return config_; }

private:
    enum class Mode : uint8_t {
        Target,
        Jog
    };

    float goal_speed();

    PlannerConfig config_;
    Mode mode_ = Mode::Target;
    int32_t position_ = 0;
    int32_t target_ = 0;
    float jog_speed_ = 0.0f;
    float speed_ = 0.0f;
    float frac_ = 0.0f;
};
} // namespace aiming
