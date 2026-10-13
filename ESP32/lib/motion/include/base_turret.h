/**
 * @file base_turret.h
 * @brief What the controller can ask of the pan/tilt turret. Thread-safe: called from loop(), executed by the motor
 * task.
 */
#pragma once

#include "aim_map.h"

class BaseTurret
{
public:
    virtual ~BaseTurret() = default;
    /** @brief Move to absolute angles (clamped to the soft limits). */
    virtual void set_target_deg(const aiming::Angles& target) = 0;
    /** @brief Manual velocity, each axis -1..1 of its max speed; 0,0 stops (with deceleration). */
    virtual void jog(float pan, float tilt) = 0;
    /** @brief Same as set_target_deg({0, home tilt}). */
    virtual void home() = 0;
    /** @brief Current angles (pan from the step counter, tilt as last written to the servo). */
    virtual aiming::Angles position_deg() const = 0;
    /** @brief Current stepper speed, steps/s (signed). */
    virtual float pan_speed() const = 0;
};
