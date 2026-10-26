/**
 * @file control_status.h
 * @brief Read-only view of the controller state for the HUD and stats line.
 */

#pragma once

#include "aim_map.h"
#include "calibration_session.h"
#include "system_control_types.h"
#include <cstdint>

/**
 * @struct ControlStatus
 * @brief Snapshot of the controller, published once per control pass.
 */
struct ControlStatus {
    ControlMode mode = ControlMode::Ai;                  ///< Current operating mode.
    uint8_t cal_points = 0;                              ///< Calibration points recorded so far (0-2).
    aiming::CalState cal_state = aiming::CalState::Idle; ///< Calibration state machine state.
    bool calibrated = false;                             ///< true once a saved or fresh calibration is active.
    aiming::Angles angles;                               ///< Current turret pose in degrees.
    const char* cal_failure = nullptr;                   ///< Static failure text, or null.
    uint32_t aims = 0;                                   ///< Aim commands sent to the turret since boot.
    uint32_t cal_saves = 0; ///< Successful calibrations since boot (a counter, so a polling HUD never misses one).
    uint32_t cal_miss = 0;  ///< Calibration records ignored because no target was visible (button or HUD).
    bool stick = false;     ///< The physical joystick is deflected and overriding the HUD jog.
    int16_t aim_x = 0;      ///< Aim pixel: AimMap inverse of the current pose, rounded, not clamped (may be off-frame).
    int16_t aim_y = 0;
    uint8_t lim = 0; ///< Soft-limit bits from aiming::limit_bits(): 1 pan min, 2 pan max, 4 tilt min, 8 tilt max.
    int16_t rx0 = 0; ///< Reachable box in pixels (aiming::reach_box()): the region whose aim is inside the limits.
    int16_t ry0 = 0;
    int16_t rx1 = 0;
    int16_t ry1 = 0;
    uint32_t home_ms = 0;   ///< Milliseconds until the AI return-home fires; 0 when none is pending or not in Ai mode.
    bool returning = false; ///< The turret is travelling home after a Home command (more than 1 degree from home).
    uint8_t speed = 1;      ///< HUD jog speed setting: 0 slow, 1 normal, 2 fast.
};

/**
 * @class BaseStatusProvider
 * @brief Interface for anything that can report a ControlStatus (the Controller). Lets the network layer read state
 * without depending on the Controller.
 */
class BaseStatusProvider
{
public:
    virtual ~BaseStatusProvider() = default;

    /** @brief Returns the latest published status. Safe to call from any task. */
    virtual ControlStatus control_status() const = 0;
};
