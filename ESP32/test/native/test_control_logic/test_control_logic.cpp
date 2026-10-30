/**
 * @file test_control_logic.cpp
 * @brief Host-side characterization tests for control::ControlLogic: button gestures, HUD requests, mode switching,
 * button and HUD calibration, jog arbitration (stick, HUD jog, dead man, aim hold) and the published status.
 * @details Every expected value was derived from Controller before its decisions moved into ControlLogic. A recording
 * fake stands in for the device ports; the HUD's commands go through the real parser and RemoteInbox, so the wire
 * strings, speeds and dead man are the production ones.
 */

#include <unity.h>

#include "control_logic.h"
#include "remote_command.h"
#include "status_fields.h"

#include <vector>

using namespace control;
using aiming::Angles;
using tracking::TrackPhase;

void setUp(void) {}
void tearDown(void) {}

namespace
{
enum class Op : uint8_t { Jog, Target, Home, ResetDetection, Save };

struct Call {
    Op op = Op::Jog;
    float a = 0.0f; ///< Jog: pan, Target: pan
    float b = 0.0f; ///< Jog: tilt, Target: tilt
};

class FakePorts final : public ControlPorts
{
public:
    StickDeflection stick;
    remote::RemoteInbox inbox; ///< what Controller guards with its spinlock
    uint32_t clock_ms = 0;     ///< millis() as read inside the inbox lock
    TrackSnapshot track;
    Angles pose;
    float pan_speed = 0.0f;
    bool save_ok = true;

    std::vector<Call> calls;
    std::vector<LogRecord> logs;
    std::vector<aiming::AimMap> saved;
    int stick_reads = 0;
    int hud_jog_reads = 0;

    StickDeflection read_stick() noexcept override
    {
        ++stick_reads;
        return stick;
    }
    remote::Jog read_hud_jog() noexcept override
    {
        ++hud_jog_reads;
        return inbox.jog(clock_ms);
    }
    HudRequests take_hud_requests() noexcept override
    {
        return HudRequests{inbox.take_mode_request(), inbox.take_cal_request(), inbox.take_aim_request()};
    }
    TrackSnapshot read_track() noexcept override { return track; }
    Angles turret_pose() noexcept override { return pose; }
    float turret_pan_speed() noexcept override { return pan_speed; }
    void turret_jog(float pan, float tilt) noexcept override { calls.push_back({Op::Jog, pan, tilt}); }
    void turret_set_target(const Angles& target) noexcept override
    {
        calls.push_back({Op::Target, target.pan, target.tilt});
    }
    void turret_home() noexcept override { calls.push_back({Op::Home}); }
    void reset_detection() noexcept override { calls.push_back({Op::ResetDetection}); }
    bool save_calibration(const aiming::AimMap& map) noexcept override
    {
        saved.push_back(map);
        calls.push_back({Op::Save});
        return save_ok;
    }
    void log(const LogRecord& record) noexcept override { logs.push_back(record); }
};

struct Rig {
    FakePorts io;
    ControlLogic logic{io};
    uint32_t now = 1000;

    /** @brief One pass at `now`; the inbox clock reads the same time. */
    void pass(bool held = false)
    {
        io.clock_ms = now;
        logic.run(now, held);
    }
    void step(uint32_t dt, bool held = false)
    {
        now += dt;
        pass(held);
    }
    /** @brief A HUD message arriving now. */
    void send(const char* message) { io.inbox.submit(remote::parse(message), now); }
    ControlStatus status() const { return logic.status(now, io.pose, io.inbox.speed()); }
    void clear()
    {
        io.calls.clear();
        io.logs.clear();
    }
    int count(Op op) const
    {
        int n = 0;
        for (const Call& c : io.calls)
        {
            n += c.op == op ? 1 : 0;
        }
        return n;
    }
    int count(LogEvent event) const
    {
        int n = 0;
        for (const LogRecord& r : io.logs)
        {
            n += r.event == event ? 1 : 0;
        }
        return n;
    }
};

// ButtonClassifier defaults: 50 ms debounce, 1.5 s long press.

/** @brief Press and release; the next step(60) is the pass that reports Short. */
void arm_short(Rig& r)
{
    r.step(10, true);  // raw press
    r.step(60, true);  // debounced: the press starts
    r.step(40, false); // raw release
}

void short_press(Rig& r)
{
    arm_short(r);
    r.step(60); // debounced release: Short
}

/** @brief Press; the next step(1500, true) is the pass that reports Long. */
void arm_long(Rig& r)
{
    r.step(10, true);
    r.step(60, true); // the press starts here
}

void release(Rig& r)
{
    r.step(10);
    r.step(60); // a release after Long reports nothing
}

void long_press(Rig& r)
{
    arm_long(r);
    r.step(1500, true);
    release(r);
}

TrackSnapshot track(TrackPhase state, int x, int y)
{
    TrackSnapshot s;
    s.state = state;
    s.x = x;
    s.y = y;
    return s;
}

/** @brief A knob-recorded calibration point: target at (x, y), pan at rest at @p pose during the press. */
void knob_point(Rig& r, int x, int y, Angles pose)
{
    r.io.track = track(TrackPhase::Locked, x, y);
    r.io.pan_speed = 0.0f;
    r.io.pose = pose;
    short_press(r);
}

/** @brief Second point that completes a fit with a first point at x 60, y 50. */
constexpr int kX2 = 260;
constexpr int kY2 = 190;
const Angles kPose2{20.0f, 60.0f};

void assert_map(const aiming::AimMap& expected, const aiming::AimMap& actual)
{
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected.pan_a, actual.pan_a);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected.pan_b, actual.pan_b);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected.tilt_a, actual.tilt_a);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, expected.tilt_b, actual.tilt_b);
}

/** @brief Asserts the only saved map is the fit of a first point at (60, 50) recorded at @p first_pose. */
void assert_first_point_pose(const Rig& r, Angles first_pose)
{
    TEST_ASSERT_EQUAL(1, (int)r.io.saved.size());
    const auto expected = aiming::fit(aiming::CalPoint{60.0f, 50.0f, first_pose.pan, first_pose.tilt},
                                      aiming::CalPoint{(float)kX2, (float)kY2, kPose2.pan, kPose2.tilt});
    TEST_ASSERT_TRUE(expected.has_value());
    assert_map(*expected, r.io.saved[0]);
}

void assert_call(const Call& c, Op op, float a = 0.0f, float b = 0.0f)
{
    TEST_ASSERT_EQUAL((int)op, (int)c.op);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, a, c.a);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, b, c.b);
}

void assert_log(const LogRecord& r, LogEvent event) { TEST_ASSERT_EQUAL((int)event, (int)r.event); }

void assert_mode(ControlMode expected, const Rig& r) { TEST_ASSERT_EQUAL((int)expected, (int)r.logic.mode()); }

void enter_user(Rig& r)
{
    short_press(r);
    assert_mode(ControlMode::User, r);
}
} // namespace

// ---- Modes and the button ----

void test_starts_in_ai_uncalibrated_and_idle(void)
{
    Rig r;
    const ControlStatus st = r.status();
    TEST_ASSERT_EQUAL((int)ControlMode::Ai, (int)st.mode);
    TEST_ASSERT_FALSE(st.calibrated);
    TEST_ASSERT_EQUAL((int)aiming::CalState::Idle, (int)st.cal_state);
    TEST_ASSERT_EQUAL(0, st.cal_points);
    TEST_ASSERT_EQUAL(1, st.speed);
    r.pass();
    TEST_ASSERT_TRUE(r.io.calls.empty()); // Ai with no target: hold
    TEST_ASSERT_EQUAL(0, r.io.stick_reads);
    TEST_ASSERT_EQUAL(0, r.io.hud_jog_reads);
}

void test_short_press_toggles_ai_and_user(void)
{
    Rig r;
    arm_short(r);
    r.clear();
    r.step(60);
    assert_mode(ControlMode::User, r);
    // set_mode: detection reset, stop; then this pass's User jog pass sends the idle (0,0).
    TEST_ASSERT_EQUAL(3, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::ResetDetection);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 0.0f);
    assert_call(r.io.calls[2], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::ModeChanged);
    TEST_ASSERT_EQUAL((int)ControlMode::User, (int)r.io.logs[0].mode);

    arm_short(r);
    r.clear();
    r.step(60);
    assert_mode(ControlMode::Ai, r);
    TEST_ASSERT_EQUAL(2, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::ResetDetection);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL((int)ControlMode::Ai, (int)r.io.logs[0].mode);
}

void test_long_press_enters_calibration_and_cancel_restores_the_previous_mode(void)
{
    Rig r;
    enter_user(r);
    arm_long(r);
    r.clear();
    r.step(1500, true);
    assert_mode(ControlMode::Calibrate, r);
    // enter_calibration stops (no detection reset); the held button then forces the jog pass to (0,0).
    TEST_ASSERT_EQUAL(2, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalStarted);
    TEST_ASSERT_EQUAL((int)ControlMode::User, (int)r.io.logs[0].mode);
    ControlStatus st = r.status();
    TEST_ASSERT_EQUAL((int)aiming::CalState::WaitPoint1, (int)st.cal_state);
    TEST_ASSERT_EQUAL(0, st.cal_points);

    release(r);
    arm_long(r);
    r.clear();
    r.step(1500, true);
    assert_mode(ControlMode::User, r);
    TEST_ASSERT_EQUAL(0, r.count(Op::ResetDetection));
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    assert_log(r.io.logs[0], LogEvent::CalCancelled);
    TEST_ASSERT_EQUAL((int)ControlMode::User, (int)r.io.logs[0].mode);
    st = r.status();
    TEST_ASSERT_EQUAL((int)aiming::CalState::Idle, (int)st.cal_state);
}

void test_long_press_from_ai_cancels_back_to_ai(void)
{
    Rig r;
    long_press(r);
    assert_mode(ControlMode::Calibrate, r);
    TEST_ASSERT_EQUAL((int)ControlMode::Ai, (int)r.io.logs[0].mode);
    long_press(r);
    assert_mode(ControlMode::Ai, r);
}

// ---- Button calibration ----

void test_short_press_records_the_pose_captured_while_held(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 0.0f;
    r.io.pose = Angles{-20.0f, 120.0f};
    r.step(10, true);                   // press starts, pan at rest: this pose is captured
    r.io.pose = Angles{-25.0f, 118.0f}; // the knob press drifts the turret before the release is classified
    r.step(60, true);
    r.step(40, false);
    r.clear();
    r.step(60);
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalPointRecorded);
    TEST_ASSERT_EQUAL(1, r.io.logs[0].points);
    ControlStatus st = r.status();
    TEST_ASSERT_EQUAL(1, st.cal_points);
    TEST_ASSERT_EQUAL((int)aiming::CalState::WaitPoint2, (int)st.cal_state);

    // Second point completes the fit: save, stop, Ai, and this same pass aims at the target through the new map.
    r.io.track = track(TrackPhase::Locked, kX2, kY2);
    r.io.pose = kPose2;
    arm_short(r);
    r.io.pose = Angles{30.0f, 70.0f};
    r.clear();
    r.step(60);
    assert_first_point_pose(r, Angles{-20.0f, 120.0f});
    TEST_ASSERT_EQUAL(4, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    assert_call(r.io.calls[1], Op::Save);
    assert_call(r.io.calls[2], Op::Jog, 0.0f, 0.0f);
    assert_call(r.io.calls[3], Op::Target, kPose2.pan, kPose2.tilt);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalComplete);
    st = r.status();
    TEST_ASSERT_EQUAL((int)ControlMode::Ai, (int)st.mode);
    TEST_ASSERT_TRUE(st.calibrated);
    TEST_ASSERT_EQUAL(1, (int)st.cal_saves);
    TEST_ASSERT_EQUAL((int)aiming::CalState::Idle, (int)st.cal_state);
    TEST_ASSERT_EQUAL(0, st.cal_points);
    TEST_ASSERT_EQUAL(1, (int)st.aims);
}

void test_short_press_without_a_stationary_pose_records_the_current_pose(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f; // never at rest during the press
    arm_short(r);
    r.io.pose = Angles{7.0f, 99.0f};
    r.clear();
    r.step(60);
    TEST_ASSERT_EQUAL(2, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::PressPoseFallback);
    assert_log(r.io.logs[1], LogEvent::CalPointRecorded);
    knob_point(r, kX2, kY2, kPose2);
    assert_first_point_pose(r, Angles{7.0f, 99.0f});
}

void test_pose_capture_waits_for_the_pan_to_stop_and_keeps_the_first_rest_pose(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f;
    r.io.pose = Angles{-5.0f, 100.0f};
    r.step(10, true); // moving: nothing captured
    r.io.pan_speed = 0.0f;
    r.io.pose = Angles{-10.0f, 110.0f};
    r.step(30, true); // first pass at rest: captured
    r.io.pose = Angles{-15.0f, 115.0f};
    r.step(30, true); // already captured
    r.step(40, false);
    r.step(60); // Short
    knob_point(r, kX2, kY2, kPose2);
    assert_first_point_pose(r, Angles{-10.0f, 110.0f});
}

void test_a_new_press_forgets_the_pose_of_an_earlier_one(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 0.0f;
    r.io.pose = Angles{-30.0f, 130.0f};
    r.step(10, true);  // a bounce shorter than the debounce: captured, but no press
    r.step(20, false); // no event
    r.io.pan_speed = 100.0f;
    arm_short(r);
    r.io.pose = Angles{3.0f, 95.0f};
    r.clear();
    r.step(60);
    assert_log(r.io.logs[0], LogEvent::PressPoseFallback);
    knob_point(r, kX2, kY2, kPose2);
    assert_first_point_pose(r, Angles{3.0f, 95.0f});
}

void test_record_without_a_visible_target_counts_a_miss(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Idle, 60, 50);
    r.clear();
    short_press(r);
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalMiss));
    r.io.track = track(TrackPhase::Tentative, 60, 50); // not detected() either
    short_press(r);
    const ControlStatus st = r.status();
    TEST_ASSERT_EQUAL(2, (int)st.cal_miss);
    TEST_ASSERT_EQUAL(0, st.cal_points);
    TEST_ASSERT_EQUAL((int)aiming::CalState::WaitPoint1, (int)st.cal_state);
    assert_mode(ControlMode::Calibrate, r);
}

void test_coasting_target_is_visible_for_a_record(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Coasting, 60, 50);
    short_press(r);
    TEST_ASSERT_EQUAL(1, r.status().cal_points);
}

void test_points_too_close_fail_and_the_next_record_restarts(void)
{
    Rig r;
    long_press(r);
    knob_point(r, 60, 50, Angles{-20.0f, 120.0f});
    r.clear();
    knob_point(r, 100, 60, Angles{20.0f, 60.0f});
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalFailed));
    const LogRecord* failed = nullptr;
    for (const LogRecord& l : r.io.logs)
    {
        failed = l.event == LogEvent::CalFailed ? &l : failed;
    }
    TEST_ASSERT_NOT_NULL(failed->failure);
    TEST_ASSERT_EQUAL(0, r.count(Op::Save));
    ControlStatus st = r.status();
    TEST_ASSERT_EQUAL((int)ControlMode::Calibrate, (int)st.mode);
    TEST_ASSERT_EQUAL((int)aiming::CalState::Failed, (int)st.cal_state);
    TEST_ASSERT_EQUAL(2, st.cal_points);
    TEST_ASSERT_EQUAL_STRING(failed->failure, st.cal_failure);

    // The next record restarts at point 1 without recording, even with no target visible (not a miss).
    r.io.track = track(TrackPhase::Idle, 0, 0);
    r.clear();
    short_press(r);
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalRestarted));
    st = r.status();
    TEST_ASSERT_EQUAL((int)aiming::CalState::WaitPoint1, (int)st.cal_state);
    TEST_ASSERT_EQUAL(0, st.cal_points);
    TEST_ASSERT_EQUAL(0, (int)st.cal_miss);
    TEST_ASSERT_NULL(st.cal_failure);
}

void test_a_failed_nvs_write_still_activates_the_fit(void)
{
    Rig r;
    r.io.save_ok = false;
    long_press(r);
    knob_point(r, 60, 50, Angles{-20.0f, 120.0f});
    r.clear();
    knob_point(r, kX2, kY2, kPose2);
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalSaveFailed));
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalComplete));
    const ControlStatus st = r.status();
    TEST_ASSERT_TRUE(st.calibrated);
    TEST_ASSERT_EQUAL(1, (int)st.cal_saves);
    TEST_ASSERT_EQUAL((int)ControlMode::Ai, (int)st.mode);
}

void test_hud_mode_request_is_dropped_on_the_pass_calibration_completes(void)
{
    Rig r;
    enter_user(r);
    long_press(r);
    knob_point(r, 60, 50, Angles{-20.0f, 120.0f});
    r.io.track = track(TrackPhase::Locked, kX2, kY2);
    r.io.pose = kPose2;
    arm_short(r);
    r.send("mode:user");
    r.step(60); // Done: Ai; the mode request is dropped (calibrating at the start of the pass)
    assert_mode(ControlMode::Ai, r);
    r.step(40);
    assert_mode(ControlMode::Ai, r);
}

// ---- HUD calibration ----

void test_hud_cal_start_and_cancel_act_like_long_presses(void)
{
    Rig r;
    enter_user(r);
    r.send("cal:start");
    r.clear();
    r.step(40);
    assert_mode(ControlMode::Calibrate, r);
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalStarted);
    TEST_ASSERT_EQUAL((int)ControlMode::User, (int)r.io.logs[0].mode);

    r.send("cal:cancel");
    r.clear();
    r.step(40);
    assert_mode(ControlMode::User, r);
    assert_log(r.io.logs[0], LogEvent::CalCancelled);
    TEST_ASSERT_EQUAL((int)ControlMode::User, (int)r.io.logs[0].mode);
}

void test_cal_start_is_ignored_while_calibrating(void)
{
    Rig r;
    long_press(r);
    knob_point(r, 60, 50, Angles{-20.0f, 120.0f});
    r.send("cal:start");
    r.clear();
    r.step(40);
    TEST_ASSERT_TRUE(r.io.logs.empty());
    const ControlStatus st = r.status();
    TEST_ASSERT_EQUAL(1, st.cal_points);
    TEST_ASSERT_EQUAL((int)aiming::CalState::WaitPoint2, (int)st.cal_state);
}

void test_cal_point_and_cancel_are_ignored_outside_calibration(void)
{
    Rig r;
    enter_user(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.send("cal:point");
    r.step(40);
    r.send("cal:cancel");
    r.step(40);
    assert_mode(ControlMode::User, r);
    r.clear();
    r.send("cal:start");
    r.step(40);
    r.step(40);
    r.step(40); // pan at rest: no stale cal:point records
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalStarted);
    TEST_ASSERT_EQUAL(0, r.status().cal_points);
}

void test_hud_point_waits_for_the_pan_to_stop(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f;
    r.send("cal:point");
    const int stick_reads = r.io.stick_reads;
    r.clear();
    r.step(40);
    // stop_jog, then the pending record forces this jog pass to (0,0).
    TEST_ASSERT_EQUAL(2, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_TRUE(r.io.logs.empty());
    TEST_ASSERT_EQUAL(stick_reads, r.io.stick_reads); // forced stop: the stick is not read

    r.send("aim:10,10"); // dropped while the record waits
    r.step(40);
    TEST_ASSERT_EQUAL(0, r.count(Op::Target));
    TEST_ASSERT_TRUE(r.io.logs.empty());

    r.io.pan_speed = 0.0f;
    r.io.pose = Angles{-20.0f, 120.0f};
    r.step(40);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalPointRecorded);

    r.io.track = track(TrackPhase::Locked, kX2, kY2);
    r.io.pose = kPose2;
    r.send("cal:point");
    r.step(40); // pending
    r.step(40); // at rest: records, fit done
    assert_first_point_pose(r, Angles{-20.0f, 120.0f});
    assert_mode(ControlMode::Ai, r);
}

void test_hud_point_times_out_after_one_second_with_the_current_pose(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f;
    r.send("cal:point");
    r.step(40); // pending since this pass
    r.io.pose = Angles{-12.0f, 105.0f};
    r.clear();
    r.step(kHudRecordTimeoutMs - 1);
    TEST_ASSERT_TRUE(r.io.logs.empty());
    r.step(1);
    TEST_ASSERT_EQUAL(2, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::HudPoseTimeout);
    assert_log(r.io.logs[1], LogEvent::CalPointRecorded);
    knob_point(r, kX2, kY2, kPose2);
    assert_first_point_pose(r, Angles{-12.0f, 105.0f});
}

void test_a_second_cal_point_keeps_the_first_timer(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f;
    r.send("cal:point");
    r.step(40);
    r.now += 500;
    r.send("cal:point");
    r.pass();
    r.clear();
    r.step(499); // 999 ms after the first
    TEST_ASSERT_TRUE(r.io.logs.empty());
    r.step(1);
    assert_log(r.io.logs[0], LogEvent::HudPoseTimeout);
}

void test_hud_cal_request_is_dropped_on_a_pass_where_the_button_cancels(void)
{
    Rig r;
    enter_user(r);
    long_press(r);
    arm_long(r);
    r.send("cal:start");
    r.clear();
    r.step(1500, true); // Long cancels; the queued cal:start must not restart
    assert_mode(ControlMode::User, r);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::CalCancelled);
    release(r);
    r.step(40);
    assert_mode(ControlMode::User, r); // dropped, not deferred
}

void test_hud_cal_point_is_dropped_on_a_pass_where_the_knob_records(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 0.0f;
    arm_short(r);
    r.send("cal:point");
    r.clear();
    r.step(60); // Short records; the cal:point is dropped
    r.step(40);
    r.step(40);
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalPointRecorded));
    TEST_ASSERT_EQUAL(1, r.status().cal_points);
}

void test_a_knob_record_clears_a_pending_hud_record(void)
{
    Rig r;
    long_press(r);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f;
    r.send("cal:point");
    r.step(40); // pending
    r.clear();
    short_press(r); // pan still moving: current pose
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::PressPoseFallback));
    r.io.pan_speed = 0.0f;
    r.step(40);
    r.step(40);
    TEST_ASSERT_EQUAL(1, r.count(LogEvent::CalPointRecorded));
    TEST_ASSERT_EQUAL(1, r.status().cal_points);
}

void test_cancelling_drops_a_pending_hud_record(void)
{
    Rig r;
    enter_user(r);
    r.send("cal:start");
    r.step(40);
    r.io.track = track(TrackPhase::Locked, 60, 50);
    r.io.pan_speed = 100.0f;
    r.send("cal:point");
    r.step(40); // pending
    r.send("cal:cancel");
    r.step(40);
    assert_mode(ControlMode::User, r);
    r.io.pan_speed = 0.0f;
    r.send("cal:start");
    r.step(40);
    r.clear();
    r.step(40);
    r.step(40);
    TEST_ASSERT_TRUE(r.io.logs.empty());
    TEST_ASSERT_EQUAL(0, r.status().cal_points);
}

// ---- HUD mode requests ----

void test_hud_mode_request_switches_between_ai_and_user(void)
{
    Rig r;
    r.send("mode:user");
    r.clear();
    r.step(40);
    assert_mode(ControlMode::User, r);
    assert_call(r.io.calls[0], Op::ResetDetection);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.logs.size());
    assert_log(r.io.logs[0], LogEvent::ModeChanged);

    r.send("mode:user"); // already User: no reset, no log
    r.clear();
    r.step(40);
    TEST_ASSERT_EQUAL(0, r.count(Op::ResetDetection));
    TEST_ASSERT_TRUE(r.io.logs.empty());

    r.send("mode:ai");
    r.step(40);
    assert_mode(ControlMode::Ai, r);
}

void test_hud_mode_request_is_dropped_in_calibrate(void)
{
    Rig r;
    long_press(r);
    r.send("mode:user");
    r.step(40);
    assert_mode(ControlMode::Calibrate, r);
    long_press(r);
    r.step(40);
    assert_mode(ControlMode::Ai, r); // dropped, not deferred
}

void test_hud_mode_request_is_dropped_on_the_pass_the_button_ends_calibration(void)
{
    Rig r;
    enter_user(r);
    long_press(r);
    arm_long(r);
    r.send("mode:ai");
    r.step(1500, true); // cancel back to User; calibration was active at the start of the pass
    assert_mode(ControlMode::User, r);
    release(r);
    r.step(40);
    assert_mode(ControlMode::User, r);
}

void test_hud_mode_request_is_dropped_on_the_pass_a_long_press_enters_calibration(void)
{
    Rig r;
    arm_long(r);
    r.send("mode:user");
    r.clear();
    r.step(1500, true);
    assert_mode(ControlMode::Calibrate, r);
    TEST_ASSERT_EQUAL(0, r.count(LogEvent::ModeChanged));
}

void test_hud_mode_request_applies_after_a_short_press_on_the_same_pass(void)
{
    // Only cal:* is dropped on a button-event pass; a mode request outside calibration still applies after it.
    Rig r;
    arm_short(r);
    r.send("mode:ai");
    r.clear();
    r.step(60); // Short: Ai -> User, then mode:ai -> Ai
    assert_mode(ControlMode::Ai, r);
    TEST_ASSERT_EQUAL(2, r.count(LogEvent::ModeChanged));
    TEST_ASSERT_EQUAL((int)ControlMode::User, (int)r.io.logs[0].mode);
    TEST_ASSERT_EQUAL((int)ControlMode::Ai, (int)r.io.logs[1].mode);
}

// ---- Jog arbitration ----

void test_stick_wins_over_the_hud_jog(void)
{
    Rig r;
    enter_user(r);
    r.send("move:right");
    r.io.stick = StickDeflection{128, -255};
    const int hud_reads = r.io.hud_jog_reads;
    r.clear();
    r.step(40);
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Jog, 128 / 255.0f, -1.0f);
    TEST_ASSERT_EQUAL(hud_reads, r.io.hud_jog_reads); // the HUD jog is not even read
    TEST_ASSERT_TRUE(r.status().stick);
}

void test_stick_flag_is_published_only_outside_ai(void)
{
    Rig r;
    enter_user(r);
    r.io.stick = StickDeflection{50, 0};
    r.step(40);
    TEST_ASSERT_TRUE(r.status().stick);
    short_press(r); // back to Ai; the last jog pass saw the stick
    assert_mode(ControlMode::Ai, r);
    TEST_ASSERT_FALSE(r.status().stick);
    const int reads = r.io.stick_reads;
    r.clear();
    r.step(40);
    r.step(40);
    TEST_ASSERT_EQUAL(reads, r.io.stick_reads); // Ai never reads the stick
    TEST_ASSERT_TRUE(r.io.calls.empty());
}

void test_hud_jog_moves_in_user_at_the_speed_setting(void)
{
    Rig r;
    enter_user(r);
    r.send("move:left");
    r.clear();
    r.step(40);
    assert_call(r.io.calls.back(), Op::Jog, -remote::kJogSpeed, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.6f, remote::kJogSpeed);
    TEST_ASSERT_EQUAL(1, r.status().speed);

    r.send("speed:slow");
    r.send("move:up");
    r.step(40);
    assert_call(r.io.calls.back(), Op::Jog, 0.0f, 0.25f);
    TEST_ASSERT_EQUAL(0, r.status().speed);

    r.send("speed:fast");
    r.send("move:down-right");
    r.step(40);
    assert_call(r.io.calls.back(), Op::Jog, 1.0f, -1.0f);
    TEST_ASSERT_EQUAL(2, r.status().speed);
}

void test_hud_jog_is_ignored_in_ai(void)
{
    Rig r;
    r.send("move:right");
    r.step(40);
    r.step(40);
    TEST_ASSERT_TRUE(r.io.calls.empty());
    TEST_ASSERT_EQUAL(0, r.io.hud_jog_reads);
}

void test_jog_is_zero_while_the_button_is_held_in_calibrate(void)
{
    Rig r;
    long_press(r);
    r.io.stick = StickDeflection{200, 0};
    r.send("move:right");
    const int stick_reads = r.io.stick_reads;
    const int hud_reads = r.io.hud_jog_reads;
    r.clear();
    r.step(40, true);
    r.step(40, true);
    TEST_ASSERT_EQUAL(2, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL(stick_reads, r.io.stick_reads);
    TEST_ASSERT_EQUAL(hud_reads, r.io.hud_jog_reads);
    TEST_ASSERT_FALSE(r.status().stick);

    r.clear();
    r.step(40, false); // released: the stick jogs again
    assert_call(r.io.calls[0], Op::Jog, 200 / 255.0f, 0.0f);
    TEST_ASSERT_TRUE(r.status().stick);
}

void test_hud_jog_applies_in_calibrate(void)
{
    Rig r;
    long_press(r);
    r.send("move:left");
    r.clear();
    r.step(40);
    assert_call(r.io.calls.back(), Op::Jog, -remote::kJogSpeed, 0.0f);
}

void test_dead_man_keeps_the_jog_until_299_ms(void)
{
    Rig r;
    enter_user(r);
    const uint32_t t = r.now + 100;
    r.now = t;
    r.send("move:right");
    r.now = t + 259;
    r.pass();
    r.clear();
    r.now = t + 299;
    r.pass();
    assert_call(r.io.calls[0], Op::Jog, remote::kJogSpeed, 0.0f);
    r.clear();
    r.now = t + 339;
    r.pass();
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
}

void test_dead_man_stops_the_jog_at_300_ms(void)
{
    Rig r;
    enter_user(r);
    const uint32_t t = r.now + 100;
    r.now = t;
    r.send("move:right");
    r.now = t + 260;
    r.pass();
    assert_call(r.io.calls.back(), Op::Jog, remote::kJogSpeed, 0.0f);
    r.clear();
    r.now = t + remote::kJogTimeoutMs;
    r.pass();
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
}

void test_hud_jog_reads_the_clock_at_read_time_not_the_pass_start(void)
{
    // A Move stamped after the pass started (httpd preempted the loop task between millis() and the inbox read). The
    // jog is judged against the clock read under the inbox lock; judging it against the pass-start time would wrap
    // the unsigned age to ~4e9 and drop a live jog for one pass.
    Rig r;
    enter_user(r);
    const uint32_t pass_start = r.now + 100;
    r.io.inbox.submit(remote::parse("move:right"), pass_start + 1);
    r.io.clock_ms = pass_start + 1;
    r.clear();
    r.logic.run(pass_start, false);
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Jog, remote::kJogSpeed, 0.0f);
}

void test_jog_passes_run_at_most_every_40_ms(void)
{
    Rig r;
    enter_user(r);
    r.step(100);
    r.clear();
    r.step(10);
    r.step(29); // 39 ms since the last jog pass
    TEST_ASSERT_TRUE(r.io.calls.empty());
    r.step(1);
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
}

// ---- HUD aim ----

void test_aim_goes_through_the_map_and_survives_the_idle_jog(void)
{
    Rig r;
    enter_user(r);
    r.send("aim:100,50");
    r.clear();
    r.step(40);
    const Angles want = aiming::default_map().aim(100.0f, 50.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Target, want.pan, want.tilt);
    r.step(40);
    r.step(40);
    r.step(400); // long after any dead man: the idle (0,0) is still held back
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
}

void test_a_hud_jog_ends_the_aim_hold(void)
{
    Rig r;
    enter_user(r);
    r.send("aim:100,50");
    r.step(40);
    r.send("move:left");
    r.clear();
    r.step(40);
    assert_call(r.io.calls[0], Op::Jog, -remote::kJogSpeed, 0.0f);
    r.clear();
    r.step(400); // dead man passed, no hold: idle (0,0)
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
}

void test_the_stick_ends_the_aim_hold(void)
{
    Rig r;
    enter_user(r);
    r.io.stick = StickDeflection{0, 255};
    r.send("aim:100,50");
    r.clear();
    r.step(40); // the aim is sent, then the stick on the same pass overrides it
    TEST_ASSERT_EQUAL(2, (int)r.io.calls.size());
    TEST_ASSERT_EQUAL((int)Op::Target, (int)r.io.calls[0].op);
    assert_call(r.io.calls[1], Op::Jog, 0.0f, 1.0f);
    r.io.stick = StickDeflection{};
    r.clear();
    r.step(40);
    assert_call(r.io.calls[0], Op::Jog, 0.0f, 0.0f);
}

void test_aim_is_ignored_in_ai(void)
{
    Rig r;
    r.send("aim:100,50");
    r.step(40);
    TEST_ASSERT_TRUE(r.io.calls.empty());
}

void test_aim_in_calibrate_is_dropped_while_the_button_is_held(void)
{
    Rig r;
    long_press(r);
    r.send("aim:100,50");
    r.clear();
    r.step(40, true);
    r.step(20, false); // a bounce: no button event
    r.step(40);
    TEST_ASSERT_EQUAL(0, r.count(Op::Target)); // dropped, not deferred
    r.send("aim:100,50");
    r.step(40);
    TEST_ASSERT_EQUAL(1, r.count(Op::Target));
}

void test_loaded_calibration_is_active_and_reported(void)
{
    Rig r;
    aiming::AimMap map;
    map.pan_a = 0.5f;
    map.pan_b = -80.0f;
    map.tilt_a = -0.25f;
    map.tilt_b = 120.0f;
    r.logic.load_calibration(map);
    TEST_ASSERT_TRUE(r.status().calibrated);
    enter_user(r);
    r.send("aim:100,40");
    r.clear();
    r.step(40);
    assert_call(r.io.calls[0], Op::Target, -30.0f, 110.0f);
}

// ---- Ai mode and status ----

void test_ai_aims_at_the_track_and_counts_aims(void)
{
    Rig r;
    r.io.track = track(TrackPhase::Locked, 160, 120);
    r.step(40);
    const Angles want = aiming::default_map().aim(160.0f, 120.0f);
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Target, want.pan, want.tilt);
    r.step(40); // same spot: inside the deadband
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
    TEST_ASSERT_EQUAL(1, (int)r.status().aims);
}

void test_ai_returns_home_after_the_target_is_lost(void)
{
    Rig r;
    r.io.track = track(TrackPhase::Locked, 160, 120);
    r.step(40);
    r.io.track = track(TrackPhase::Idle, 0, 0);
    r.step(500);
    TEST_ASSERT_EQUAL(1500, (int)r.status().home_ms);
    r.clear();
    r.step(1500);
    TEST_ASSERT_EQUAL(1, (int)r.io.calls.size());
    assert_call(r.io.calls[0], Op::Home);
}

void test_home_countdown_is_zero_outside_ai(void)
{
    Rig r;
    r.io.track = track(TrackPhase::Locked, 160, 120);
    r.step(40);
    r.io.track = track(TrackPhase::Idle, 0, 0);
    r.step(500);
    enter_user(r);
    TEST_ASSERT_EQUAL(0, (int)r.status().home_ms);
}

void test_status_copies_the_pose_and_pose_fields(void)
{
    Rig r;
    r.io.pose = Angles{12.0f, 80.0f};
    const ControlStatus st = r.status();
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f, st.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 80.0f, st.angles.tilt);
    ControlStatus want;
    want.angles = r.io.pose;
    hud::fill_pose_fields(want, aiming::default_map(), hud::HomeState{.ai_mode = true});
    TEST_ASSERT_EQUAL(want.aim_x, st.aim_x);
    TEST_ASSERT_EQUAL(want.aim_y, st.aim_y);
    TEST_ASSERT_EQUAL(want.rx1, st.rx1);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_in_ai_uncalibrated_and_idle);
    RUN_TEST(test_short_press_toggles_ai_and_user);
    RUN_TEST(test_long_press_enters_calibration_and_cancel_restores_the_previous_mode);
    RUN_TEST(test_long_press_from_ai_cancels_back_to_ai);
    RUN_TEST(test_short_press_records_the_pose_captured_while_held);
    RUN_TEST(test_short_press_without_a_stationary_pose_records_the_current_pose);
    RUN_TEST(test_pose_capture_waits_for_the_pan_to_stop_and_keeps_the_first_rest_pose);
    RUN_TEST(test_a_new_press_forgets_the_pose_of_an_earlier_one);
    RUN_TEST(test_record_without_a_visible_target_counts_a_miss);
    RUN_TEST(test_coasting_target_is_visible_for_a_record);
    RUN_TEST(test_points_too_close_fail_and_the_next_record_restarts);
    RUN_TEST(test_a_failed_nvs_write_still_activates_the_fit);
    RUN_TEST(test_hud_mode_request_is_dropped_on_the_pass_calibration_completes);
    RUN_TEST(test_hud_cal_start_and_cancel_act_like_long_presses);
    RUN_TEST(test_cal_start_is_ignored_while_calibrating);
    RUN_TEST(test_cal_point_and_cancel_are_ignored_outside_calibration);
    RUN_TEST(test_hud_point_waits_for_the_pan_to_stop);
    RUN_TEST(test_hud_point_times_out_after_one_second_with_the_current_pose);
    RUN_TEST(test_a_second_cal_point_keeps_the_first_timer);
    RUN_TEST(test_hud_cal_request_is_dropped_on_a_pass_where_the_button_cancels);
    RUN_TEST(test_hud_cal_point_is_dropped_on_a_pass_where_the_knob_records);
    RUN_TEST(test_a_knob_record_clears_a_pending_hud_record);
    RUN_TEST(test_cancelling_drops_a_pending_hud_record);
    RUN_TEST(test_hud_mode_request_switches_between_ai_and_user);
    RUN_TEST(test_hud_mode_request_is_dropped_in_calibrate);
    RUN_TEST(test_hud_mode_request_is_dropped_on_the_pass_the_button_ends_calibration);
    RUN_TEST(test_hud_mode_request_is_dropped_on_the_pass_a_long_press_enters_calibration);
    RUN_TEST(test_hud_mode_request_applies_after_a_short_press_on_the_same_pass);
    RUN_TEST(test_stick_wins_over_the_hud_jog);
    RUN_TEST(test_stick_flag_is_published_only_outside_ai);
    RUN_TEST(test_hud_jog_moves_in_user_at_the_speed_setting);
    RUN_TEST(test_hud_jog_is_ignored_in_ai);
    RUN_TEST(test_jog_is_zero_while_the_button_is_held_in_calibrate);
    RUN_TEST(test_hud_jog_applies_in_calibrate);
    RUN_TEST(test_dead_man_keeps_the_jog_until_299_ms);
    RUN_TEST(test_dead_man_stops_the_jog_at_300_ms);
    RUN_TEST(test_hud_jog_reads_the_clock_at_read_time_not_the_pass_start);
    RUN_TEST(test_jog_passes_run_at_most_every_40_ms);
    RUN_TEST(test_aim_goes_through_the_map_and_survives_the_idle_jog);
    RUN_TEST(test_a_hud_jog_ends_the_aim_hold);
    RUN_TEST(test_the_stick_ends_the_aim_hold);
    RUN_TEST(test_aim_is_ignored_in_ai);
    RUN_TEST(test_aim_in_calibrate_is_dropped_while_the_button_is_held);
    RUN_TEST(test_loaded_calibration_is_active_and_reported);
    RUN_TEST(test_ai_aims_at_the_track_and_counts_aims);
    RUN_TEST(test_ai_returns_home_after_the_target_is_lost);
    RUN_TEST(test_home_countdown_is_zero_outside_ai);
    RUN_TEST(test_status_copies_the_pose_and_pose_fields);
    return UNITY_END();
}
