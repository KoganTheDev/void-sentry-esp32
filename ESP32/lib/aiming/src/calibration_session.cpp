#include "calibration_session.h"

namespace aiming
{
void CalibrationSession::start()
{
    state_ = CalState::WaitPoint1;
    points_ = 0;
    result_.reset();
    failure_ = nullptr;
}

void CalibrationSession::cancel()
{
    state_ = CalState::Idle;
    points_ = 0;
    result_.reset();
    failure_ = nullptr;
}

CalState CalibrationSession::record(float x, float y, Angles at, bool target_visible)
{
    if (state_ == CalState::Failed)
    {
        start();
        return state_;
    }
    if ((state_ != CalState::WaitPoint1 && state_ != CalState::WaitPoint2) || !target_visible)
    {
        return state_;
    }
    const CalPoint p{x, y, at.pan, at.tilt};
    if (state_ == CalState::WaitPoint1)
    {
        first_ = p;
        points_ = 1;
        state_ = CalState::WaitPoint2;
        return state_;
    }
    points_ = 2;
    result_ = fit(first_, p);
    if (result_)
    {
        state_ = CalState::Done;
    } else
    {
        state_ = CalState::Failed;
        failure_ = "points too close (need 80 px apart in x and 60 px in y)";
    }
    return state_;
}
} // namespace aiming
