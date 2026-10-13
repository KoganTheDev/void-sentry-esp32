/**
 * @file test_aim_policy.cpp
 * @brief Host-side tests for the aim policy, the button classifier, the calibration session, and a
 * policy-to-planner pipeline check.
 */

#include <unity.h>

#include "aim_policy.h"
#include "button_classifier.h"
#include "calibration_session.h"
#include "step_planner.h"

#include <cmath>

using namespace aiming;
using tracking::TrackPhase;

void setUp(void) {}
void tearDown(void) {}

// ---- AimPolicy ----

void test_locked_aims_at_mapped_angles(void)
{
    AimPolicy p(default_map());
    const AimCommand c = p.update({TrackPhase::Locked, 320.0f, 120.0f}, 0);
    TEST_ASSERT_TRUE(c.kind == AimKind::Aim);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.0f, c.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, c.angles.tilt);
}

void test_deadband_suppresses_small_changes(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 160.0f, 120.0f}, 0);
    // 4 px = 0.825 deg pan: inside the 1 deg deadband
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 164.0f, 120.0f}, 60).kind == AimKind::Hold);
    // 6 px = 1.24 deg: outside
    const AimCommand c = p.update({TrackPhase::Locked, 166.0f, 120.0f}, 120);
    TEST_ASSERT_TRUE(c.kind == AimKind::Aim);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2375f, c.angles.pan);
}

void test_coasting_aims_and_tentative_holds(void)
{
    AimPolicy p(default_map());
    TEST_ASSERT_TRUE(p.update({TrackPhase::Tentative, 300.0f, 120.0f}, 0).kind == AimKind::Hold);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Coasting, 300.0f, 120.0f}, 60).kind == AimKind::Aim);
}

void test_idle_before_any_target_holds(void)
{
    AimPolicy p(default_map());
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0.0f, 0.0f}, 10000).kind == AimKind::Hold);
}

void test_idle_holds_then_homes_once(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 1000);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 2999).kind == AimKind::Hold);
    const AimCommand home = p.update({TrackPhase::Idle, 0, 0}, 3000); // exactly 2000 ms after last seen
    TEST_ASSERT_TRUE(home.kind == AimKind::Home);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, home.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, home.angles.tilt);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 5000).kind == AimKind::Hold); // only once
}

void test_relock_after_home_aims(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.update({TrackPhase::Idle, 0, 0}, 2500);
    // Same spot as before homing: must aim again even though it equals the old setpoint.
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 300.0f, 60.0f}, 3000).kind == AimKind::Aim);
}

void test_millis_wraparound(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0xFFFFFF00u);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 0x00000100u).kind == AimKind::Hold); // 512 ms later
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 0x000007D0u).kind == AimKind::Home); // 2256 ms later
}

void test_reset_forgets_setpoint(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.reset();
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 300.0f, 60.0f}, 10).kind == AimKind::Aim);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 20).kind == AimKind::Hold);
}

// ---- ButtonClassifier ----

/** Feed a constant raw level for [from, to) in 1 ms ticks; returns the last non-None event. */
static ButtonEvent feed(ButtonClassifier& b, bool pressed, uint32_t from, uint32_t to, int* count = nullptr)
{
    ButtonEvent last = ButtonEvent::None;
    for (uint32_t t = from; t < to; ++t)
    {
        const ButtonEvent e = b.update(pressed, t);
        if (e != ButtonEvent::None)
        {
            last = e;
            if (count)
                ++*count;
        }
    }
    return last;
}

void test_short_press(void)
{
    ButtonClassifier b;
    TEST_ASSERT_TRUE(feed(b, true, 0, 300) == ButtonEvent::None);
    TEST_ASSERT_TRUE(feed(b, false, 300, 400) == ButtonEvent::Short);
}

void test_long_press_fires_once_at_threshold(void)
{
    ButtonClassifier b;
    // The press is confirmed at t = 50 (debounce); long fires 1500 ms after that.
    TEST_ASSERT_TRUE(feed(b, true, 0, 1549) == ButtonEvent::None);
    int count = 0;
    TEST_ASSERT_TRUE(feed(b, true, 1549, 3000, &count) == ButtonEvent::Long);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_TRUE(feed(b, false, 3000, 3100) == ButtonEvent::None); // release after long: no short
}

void test_bounce_is_ignored(void)
{
    ButtonClassifier b;
    feed(b, true, 0, 20); // 20 ms glitch < 50 ms debounce
    TEST_ASSERT_TRUE(feed(b, false, 20, 500) == ButtonEvent::None);
}

// ---- CalibrationSession ----

void test_calibration_happy_path(void)
{
    CalibrationSession s;
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    s.start();
    TEST_ASSERT_TRUE(s.state() == CalState::WaitPoint1);
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, true) == CalState::WaitPoint2);
    TEST_ASSERT_EQUAL_INT(1, s.points());
    TEST_ASSERT_TRUE(s.record(280, 200, Angles{25, 70}, true) == CalState::Done);
    TEST_ASSERT_EQUAL_INT(2, s.points());
    TEST_ASSERT_TRUE(s.result().has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, s.result()->aim(280, 200).pan);
}

void test_record_without_target_is_ignored(void)
{
    CalibrationSession s;
    s.start();
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, false) == CalState::WaitPoint1);
    TEST_ASSERT_EQUAL_INT(0, s.points());
}

void test_bad_fit_fails_and_next_press_restarts(void)
{
    CalibrationSession s;
    s.start();
    s.record(100, 100, Angles{0, 90}, true);
    TEST_ASSERT_TRUE(s.record(120, 110, Angles{5, 85}, true) == CalState::Failed);
    TEST_ASSERT_NOT_NULL(s.failure());
    TEST_ASSERT_FALSE(s.result().has_value());
    // In Failed, the next press restarts at point 1 without recording.
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, true) == CalState::WaitPoint1);
    TEST_ASSERT_EQUAL_INT(0, s.points());
}

void test_cancel_from_any_state(void)
{
    CalibrationSession s;
    s.start();
    s.record(40, 50, Angles{-30, 110}, true);
    s.cancel();
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    TEST_ASSERT_EQUAL_INT(0, s.points());
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, true) == CalState::Idle); // ignored when Idle
}

// ---- Pipeline: policy -> planner ----

void test_policy_to_planner_reaches_aim(void)
{
    AimPolicy p(default_map());
    StepPlanner planner;
    const AimCommand c = p.update({TrackPhase::Locked, 280.0f, 120.0f}, 0); // pan 24.75 deg
    TEST_ASSERT_TRUE(c.kind == AimKind::Aim);
    planner.set_target(pan_deg_to_steps(c.angles.pan));
    int ticks = 0;
    while (!planner.is_idle() && ticks < 5000)
    {
        planner.tick();
        ++ticks;
    }
    TEST_ASSERT_EQUAL_INT32(141, planner.position()); // 24.75 * 2048 / 360 = 140.8
    TEST_ASSERT_TRUE(ticks < 1100);                   // triangular move of 141 steps: ~0.84 s plus creep
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_locked_aims_at_mapped_angles);
    RUN_TEST(test_deadband_suppresses_small_changes);
    RUN_TEST(test_coasting_aims_and_tentative_holds);
    RUN_TEST(test_idle_before_any_target_holds);
    RUN_TEST(test_idle_holds_then_homes_once);
    RUN_TEST(test_relock_after_home_aims);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_reset_forgets_setpoint);
    RUN_TEST(test_short_press);
    RUN_TEST(test_long_press_fires_once_at_threshold);
    RUN_TEST(test_bounce_is_ignored);
    RUN_TEST(test_calibration_happy_path);
    RUN_TEST(test_record_without_target_is_ignored);
    RUN_TEST(test_bad_fit_fails_and_next_press_restarts);
    RUN_TEST(test_cancel_from_any_state);
    RUN_TEST(test_policy_to_planner_reaches_aim);
    return UNITY_END();
}
