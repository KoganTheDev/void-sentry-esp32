/**
 * @file direction.h
 * @brief Maps a tracked target to a pan/tilt direction relative to the grid centre (pure C++20, header-only).
 * @details Only Locked and Coasting targets produce a direction. It is kept for the detect_object() return value;
 * aiming uses AimPolicy/AimMap instead.
 */

#pragma once

#include "move_types.h"
#include "tracking_types.h"

#include <tuple>

namespace tracking
{
inline std::tuple<MoveX, MoveY> direction_for(const TrackState& track, float deadzone_cells = 2.5f)
{
    if (track.state != TrackPhase::Locked && track.state != TrackPhase::Coasting)
    {
        return std::make_tuple(MoveX(MoveX::None), MoveY(MoveY::None));
    }
    const float dx = track.cx - (kGridW - 1) / 2.0f;
    const float dy = track.cy - (kGridH - 1) / 2.0f;
    MoveX x(MoveX::None);
    MoveY y(MoveY::None);
    if (dx < -deadzone_cells)
    {
        x = MoveX(MoveX::Left);
    } else if (dx > deadzone_cells)
    {
        x = MoveX(MoveX::Right);
    }
    if (dy < -deadzone_cells)
    {
        y = MoveY(MoveY::Up);
    } else if (dy > deadzone_cells)
    {
        y = MoveY(MoveY::Down);
    }
    return std::make_tuple(x, y);
}
} // namespace tracking
