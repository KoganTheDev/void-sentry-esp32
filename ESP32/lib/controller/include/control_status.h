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
