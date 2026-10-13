/**
 * @file calibration_session.h
 * @brief Two-point joystick calibration as a state machine (pure C++17, natively tested).
 * @details start() -> WaitPoint1 -> WaitPoint2 -> Done (result() holds the fitted map) or Failed (failure() says
 * why). record() ignores presses while no target is visible (the point is meaningless) and while Idle; in Failed
 * the next record() restarts at point 1 without recording. cancel() returns to Idle from any state.
 */

#pragma once

#include "aim_map.h"

#include <optional>

namespace aiming
{
enum class CalState : uint8_t { Idle, WaitPoint1, WaitPoint2, Done, Failed };

class CalibrationSession
{
public:
    void start();
    void cancel();
    CalState record(float x, float y, Angles at, bool target_visible);

    CalState state() const { return state_; }
    int points() const { return points_; }
    std::optional<AimMap> result() const { return result_; }
    const char* failure() const { return failure_; }

private:
    CalState state_ = CalState::Idle;
    int points_ = 0;
    CalPoint first_;
    std::optional<AimMap> result_;
    const char* failure_ = nullptr;
};
} // namespace aiming
