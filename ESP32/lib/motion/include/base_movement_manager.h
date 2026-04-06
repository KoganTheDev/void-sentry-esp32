/**
 * @file base_movement_manager.h
 * @brief Header file for the BaseMovementManager interface.
 * @details Defines the abstract interface for turret locomotion.
 */
#pragma once

#include "move_types.h"
#include "tuple"

/**
 * @class BaseMovementManager
 * @brief Abstract base class for turret movement hardware abstraction.
 * @details This interface defines the contract for any class responsible for
 * physical movement.
 */
class BaseMovementManager
{
public:
    /**
     * @brief Virtual destructor for safe polymorphic cleanup.
     */
    virtual ~BaseMovementManager() = default;

    /**
     * @brief Moves the turret relative to its current orientation.
     */
    virtual void move_relative(std::tuple<MoveX, MoveY> move_directions) = 0;
};