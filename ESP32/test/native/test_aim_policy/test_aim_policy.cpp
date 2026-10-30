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

void test_home_remaining_counts_down_while_idle(void)
{
    AimPolicy p(default_map()); // home_delay_ms = 2000
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(0));
    TEST_ASSERT_FALSE(p.homed());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 1000);
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(1000)); // still tracking, nothing pending
    p.update({TrackPhase::Idle, 0, 0}, 1100);
    TEST_ASSERT_EQUAL_UINT32(1900, p.home_remaining_ms(1100));
    TEST_ASSERT_EQUAL_UINT32(1000, p.home_remaining_ms(2000));
    p.update({TrackPhase::Idle, 0, 0}, 2999);
    TEST_ASSERT_EQUAL_UINT32(1, p.home_remaining_ms(2999));
    TEST_ASSERT_FALSE(p.homed());
}

void test_home_remaining_zero_after_home_fired(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 1000);
    p.update({TrackPhase::Idle, 0, 0}, 2500);
    TEST_ASSERT_EQUAL_UINT32(500, p.home_remaining_ms(2500));
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 3000).kind == AimKind::Home);
    TEST_ASSERT_TRUE(p.homed());
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(3000));
    p.update({TrackPhase::Idle, 0, 0}, 3500);
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(3500));
    TEST_ASSERT_TRUE(p.homed());
}

void test_home_remaining_clamps_to_zero_when_overdue(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.update({TrackPhase::Idle, 0, 0}, 100);
    // Queried later than the home delay without a further update(): never negative or huge.
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(5000));
}

void test_home_remaining_zero_without_a_target_or_while_tracking(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Idle, 0, 0}, 500); // never saw a target
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(600));
    p.update({TrackPhase::Tentative, 0, 0}, 700); // a candidate before any target was seen
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(800));
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 1000);
    p.update({TrackPhase::Idle, 0, 0}, 1100);
    TEST_ASSERT_TRUE(p.home_remaining_ms(1200) > 0);
    p.update({TrackPhase::Tentative, 0, 0}, 1300); // a candidate appeared: the return-home timer keeps running
    TEST_ASSERT_EQUAL_UINT32(1700, p.home_remaining_ms(1300));
    p.update({TrackPhase::Coasting, 300.0f, 60.0f}, 1400);
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(1400));
}

void test_home_remaining_wraparound(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0xFFFFFF00u);
    p.update({TrackPhase::Idle, 0, 0}, 0xFFFFFF10u);
    TEST_ASSERT_EQUAL_UINT32(1744, p.home_remaining_ms(0x00000000u)); // 256 ms elapsed across the wrap
    TEST_ASSERT_EQUAL_UINT32(992, p.home_remaining_ms(0x000002F0u));  // 1008 ms elapsed
}

void test_homed_clears_on_next_lock_and_on_reset(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.update({TrackPhase::Idle, 0, 0}, 2500);
    TEST_ASSERT_TRUE(p.homed());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 3000);
    TEST_ASSERT_FALSE(p.homed());
    p.update({TrackPhase::Idle, 0, 0}, 6000);
    TEST_ASSERT_TRUE(p.homed());
    p.reset();
    TEST_ASSERT_FALSE(p.homed());
    TEST_ASSERT_EQUAL_UINT32(0, p.home_remaining_ms(6000));
}

void test_hold_carries_commanded_pose(void)
{
    AimPolicy p(default_map());
    // Before any lock, Hold reports the Home pose.
    const AimCommand idle = p.update({TrackPhase::Idle, 0, 0}, 0);
    TEST_ASSERT_TRUE(idle.kind == AimKind::Hold);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, idle.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, idle.angles.tilt);
    // After an Aim to X, a Tentative Hold carries X.
    const AimCommand aim = p.update({TrackPhase::Locked, 280.0f, 120.0f}, 10); // pan 24.75, tilt 90
    TEST_ASSERT_TRUE(aim.kind == AimKind::Aim);
    const AimCommand hold = p.update({TrackPhase::Tentative, 280.0f, 120.0f}, 20);
    TEST_ASSERT_TRUE(hold.kind == AimKind::Hold);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, aim.angles.pan, hold.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, aim.angles.tilt, hold.angles.tilt);
    // After Home, Hold carries the Home pose (not the stale pre-home aim).
    const AimCommand home = p.update({TrackPhase::Idle, 0, 0}, 2100);
    TEST_ASSERT_TRUE(home.kind == AimKind::Home);
    const AimCommand after = p.update({TrackPhase::Idle, 0, 0}, 2200);
    TEST_ASSERT_TRUE(after.kind == AimKind::Hold);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, after.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, after.angles.tilt);
}

void test_reset_forgets_setpoint(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.reset();
    // reset() cleared the lost-target timer: 5 s of Idle after the reset must not Home.
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 5000).kind == AimKind::Hold);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 300.0f, 60.0f}, 5010).kind == AimKind::Aim);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 5020).kind == AimKind::Hold);
}

void test_tilt_deadband(void)
{
    AimPolicy p(default_map()); // tilt: 50 deg over 240 px = 0.208 deg/px
    p.update({TrackPhase::Locked, 160.0f, 120.0f}, 0);
    // 3 px = 0.625 deg tilt: inside the 1 deg deadband
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 160.0f, 123.0f}, 60).kind == AimKind::Hold);
    // 6 px = 1.25 deg: outside
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 160.0f, 126.0f}, 120).kind == AimKind::Aim);
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
    TEST_ASSERT_TRUE(b.update(true, 1549) == ButtonEvent::None);
    TEST_ASSERT_TRUE(b.update(true, 1550) == ButtonEvent::Long); // exactly 1500 ms after confirmation at 50
    int count = 0;
    TEST_ASSERT_TRUE(feed(b, true, 1551, 3000, &count) == ButtonEvent::None);
    TEST_ASSERT_EQUAL_INT(0, count);                                   // no repeat while held
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

void test_cancel_from_idle_done_and_failed(void)
{
    CalibrationSession s;
    s.cancel(); // from Idle
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    TEST_ASSERT_EQUAL_INT(0, s.points());

    s.start();
    s.record(40, 50, Angles{-30, 110}, true);
    TEST_ASSERT_TRUE(s.record(280, 200, Angles{25, 70}, true) == CalState::Done);
    s.cancel(); // from Done
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    TEST_ASSERT_EQUAL_INT(0, s.points());
    TEST_ASSERT_FALSE(s.result().has_value());

    s.start();
    s.record(100, 100, Angles{0, 90}, true);
    TEST_ASSERT_TRUE(s.record(120, 110, Angles{5, 85}, true) == CalState::Failed);
    s.cancel(); // from Failed
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    TEST_ASSERT_EQUAL_INT(0, s.points());
    TEST_ASSERT_NULL(s.failure());
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

void test_scripted_track_through_policy_and_planner(void)
{
    AimPolicy policy(default_map());
    StepPlanner planner;
    const PlannerConfig& c = planner.config();
    const float dv = c.accel * c.tick_s;
    float prev = 0.0f;
    int32_t last_aim_steps = 0;
    int home_ms = -1;
    // Track: x 40 -> 280 over 6000 ms (Locked), then Idle. The planner is driven at 1 kHz, the policy every 59 ms.
    // The last Locked update is at 5959 ms (59 * 101). Aims are about 1 deg (5.7 steps) apart and the planner follows
    // at ~47 steps/s, so at 6000 ms it is within ~10 steps of the last target: the worst case is a creep finish at
    // 20 steps/s (10 / 20 = 0.5 s) plus a few ms of ramp, so 700 ms after the track stops it must sit on the target.
    // Home fires at the first update >= 5959 + 2000, i.e. 7965 ms. The pose is then at most 141 steps (24.75 deg):
    // triangular profile (141 < 400^2 / 800 = 200), 2 * sqrt(141 / 800) = 0.84 s plus the creep tail, so 1200 ms.
    for (int t = 0; t <= 10000; ++t)
    {
        planner.tick();
        const float v = planner.speed();
        TEST_ASSERT_TRUE(std::fabs(v) <= c.max_speed + 1e-3f);
        const bool creep = std::fabs(prev) <= c.min_speed + 1e-3f && std::fabs(v) <= c.min_speed + 1e-3f;
        TEST_ASSERT_TRUE(std::fabs(v - prev) <= dv + 1e-3f || creep);
        prev = v;
        if (t % 59 == 0)
        {
            const bool moving = t < 6000;
            const TrackView view{moving ? TrackPhase::Locked : TrackPhase::Idle, 40.0f + 240.0f * t / 6000.0f, 120.0f};
            const AimCommand cmd = policy.update(view, static_cast<uint32_t>(t));
            if (cmd.kind == AimKind::Aim)
            {
                last_aim_steps = pan_deg_to_steps(cmd.angles.pan);
                planner.set_target(last_aim_steps);
            } else if (cmd.kind == AimKind::Home)
            {
                home_ms = t;
                planner.set_target(0);
            }
        }
        if (t == 6700)
        {
            TEST_ASSERT_EQUAL_INT32(last_aim_steps, planner.position());
            TEST_ASSERT_TRUE(last_aim_steps > 100);
        }
        if (home_ms >= 0 && t == home_ms + 1200)
        {
            TEST_ASSERT_EQUAL_INT32(0, planner.position());
        }
    }
    TEST_ASSERT_TRUE(home_ms >= 7959 && home_ms < 7959 + 59);
    TEST_ASSERT_EQUAL_INT32(0, planner.position());
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
    RUN_TEST(test_home_remaining_counts_down_while_idle);
    RUN_TEST(test_home_remaining_zero_after_home_fired);
    RUN_TEST(test_home_remaining_clamps_to_zero_when_overdue);
    RUN_TEST(test_home_remaining_zero_without_a_target_or_while_tracking);
    RUN_TEST(test_home_remaining_wraparound);
    RUN_TEST(test_homed_clears_on_next_lock_and_on_reset);
    RUN_TEST(test_hold_carries_commanded_pose);
    RUN_TEST(test_reset_forgets_setpoint);
    RUN_TEST(test_tilt_deadband);
    RUN_TEST(test_short_press);
    RUN_TEST(test_long_press_fires_once_at_threshold);
    RUN_TEST(test_bounce_is_ignored);
    RUN_TEST(test_calibration_happy_path);
    RUN_TEST(test_record_without_target_is_ignored);
    RUN_TEST(test_bad_fit_fails_and_next_press_restarts);
    RUN_TEST(test_cancel_from_any_state);
    RUN_TEST(test_cancel_from_idle_done_and_failed);
    RUN_TEST(test_policy_to_planner_reaches_aim);
    RUN_TEST(test_scripted_track_through_policy_and_planner);
    return UNITY_END();
}
