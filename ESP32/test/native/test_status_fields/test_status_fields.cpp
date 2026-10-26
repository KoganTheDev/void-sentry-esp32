/**
 * @file test_status_fields.cpp
 * @brief Host-side tests for the pose-derived HUD status fields (hud::fill_pose_fields).
 */

#include <unity.h>

#include "status_fields.h"

#include <cmath>
#include <cstdint>
#include <limits>

using namespace hud;

void setUp(void) {}
void tearDown(void) {}

namespace
{
ControlStatus at(float pan, float tilt)
{
    ControlStatus st;
    st.angles = aiming::Angles{pan, tilt};
    return st;
}

/** @brief pan = x - 160 (x 70..250 inside the pan limits), tilt = 210 - y (y 50..190 inside the tilt limits). */
aiming::AimMap narrow_map()
{
    aiming::AimMap m;
    m.pan_a = 1.0f;
    m.pan_b = -160.0f;
    m.tilt_a = -1.0f;
    m.tilt_b = 210.0f;
    return m;
}
} // namespace

void test_round_pixel_rounds_half_away_from_zero(void)
{
    TEST_ASSERT_EQUAL_INT16(0, round_pixel(0.0f));
    TEST_ASSERT_EQUAL_INT16(3, round_pixel(2.5f));
    TEST_ASSERT_EQUAL_INT16(-3, round_pixel(-2.5f));
    TEST_ASSERT_EQUAL_INT16(208, round_pixel(208.49f));
    TEST_ASSERT_EQUAL_INT16(-1, round_pixel(-0.5f));
}

void test_round_pixel_saturates_and_rejects_non_finite(void)
{
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, round_pixel(1e9f));
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, round_pixel(-1e9f));
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, round_pixel(32767.0f));
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, round_pixel(-32768.0f));
    TEST_ASSERT_EQUAL_INT16(32766, round_pixel(32766.0f));
    TEST_ASSERT_EQUAL_INT16(0, round_pixel(std::numeric_limits<float>::quiet_NaN()));
    TEST_ASSERT_EQUAL_INT16(0, round_pixel(std::numeric_limits<float>::infinity()));
    TEST_ASSERT_EQUAL_INT16(0, round_pixel(-std::numeric_limits<float>::infinity()));
}

void test_home_pose_on_default_map_aims_at_the_centre(void)
{
    ControlStatus st = at(0.0f, aiming::kTiltHomeDeg);
    fill_pose_fields(st, aiming::default_map(), HomeState{});
    TEST_ASSERT_EQUAL_INT16(160, st.aim_x);
    TEST_ASSERT_EQUAL_INT16(120, st.aim_y);
    TEST_ASSERT_EQUAL_UINT8(0, st.lim);
    TEST_ASSERT_EQUAL_INT16(0, st.rx0);
    TEST_ASSERT_EQUAL_INT16(0, st.ry0);
    TEST_ASSERT_EQUAL_INT16(319, st.rx1);
    TEST_ASSERT_EQUAL_INT16(239, st.ry1);
    TEST_ASSERT_EQUAL_UINT32(0, st.home_ms);
    TEST_ASSERT_FALSE(st.returning);
}

void test_aim_pixel_is_the_inverse_of_the_map(void)
{
    // Default map: 66/320 deg per px in pan (right = larger x), 50/240 deg per px in tilt (up = smaller y).
    ControlStatus st = at(10.0f, 100.0f);
    fill_pose_fields(st, aiming::default_map(), HomeState{});
    TEST_ASSERT_EQUAL_INT16(208, st.aim_x); // 160 + 10 / 0.20625 = 208.48
    TEST_ASSERT_EQUAL_INT16(72, st.aim_y);  // 120 - 10 / 0.20833 = 72

    ControlStatus n = at(-20.0f, 150.0f);
    fill_pose_fields(n, narrow_map(), HomeState{});
    TEST_ASSERT_EQUAL_INT16(140, n.aim_x);
    TEST_ASSERT_EQUAL_INT16(60, n.aim_y);
}

void test_aim_pixel_is_not_clamped_to_the_frame(void)
{
    ControlStatus st = at(aiming::kPanMaxDeg, aiming::kTiltMinDeg);
    fill_pose_fields(st, aiming::default_map(), HomeState{});
    TEST_ASSERT_EQUAL_INT16(596, st.aim_x); // 160 + 90 / 0.20625
    TEST_ASSERT_EQUAL_INT16(456, st.aim_y); // 120 + 70 / 0.20833
    TEST_ASSERT_EQUAL_UINT8(2 | 4, st.lim);
}

void test_aim_pixel_saturates_for_a_nearly_flat_map(void)
{
    aiming::AimMap flat;
    flat.pan_a = 1e-6f;
    flat.pan_b = 0.0f;
    flat.tilt_a = -1e-6f;
    flat.tilt_b = aiming::kTiltHomeDeg;
    ControlStatus st = at(aiming::kPanMinDeg, aiming::kTiltMinDeg);
    fill_pose_fields(st, flat, HomeState{});
    TEST_ASSERT_EQUAL_INT16(INT16_MIN, st.aim_x);
    TEST_ASSERT_EQUAL_INT16(INT16_MAX, st.aim_y);
    TEST_ASSERT_EQUAL_UINT8(1 | 4, st.lim);
}

void test_limit_bits_follow_the_pose(void)
{
    ControlStatus st = at(aiming::kPanMinDeg + 0.4f, aiming::kTiltMaxDeg - 0.2f);
    fill_pose_fields(st, aiming::default_map(), HomeState{});
    TEST_ASSERT_EQUAL_UINT8(1 | 8, st.lim);
}

void test_reach_box_comes_from_the_map(void)
{
    ControlStatus st = at(0.0f, aiming::kTiltHomeDeg);
    fill_pose_fields(st, narrow_map(), HomeState{});
    TEST_ASSERT_EQUAL_INT16(70, st.rx0);
    TEST_ASSERT_EQUAL_INT16(250, st.rx1);
    TEST_ASSERT_EQUAL_INT16(50, st.ry0);
    TEST_ASSERT_EQUAL_INT16(190, st.ry1);
}

void test_home_countdown_only_in_ai_mode(void)
{
    ControlStatus ai = at(30.0f, 120.0f);
    fill_pose_fields(ai, aiming::default_map(), HomeState{.ai_mode = true, .homed = false, .home_remaining_ms = 1400});
    TEST_ASSERT_EQUAL_UINT32(1400, ai.home_ms);
    TEST_ASSERT_FALSE(ai.returning); // Home has not fired yet

    ControlStatus user = at(30.0f, 120.0f);
    user.home_ms = 99; // a stale value is overwritten
    fill_pose_fields(user, aiming::default_map(),
                     HomeState{.ai_mode = false, .homed = false, .home_remaining_ms = 1400});
    TEST_ASSERT_EQUAL_UINT32(0, user.home_ms);
}

void test_returning_needs_ai_mode_homed_and_distance(void)
{
    const HomeState homed{.ai_mode = true, .homed = true, .home_remaining_ms = 0};
    const aiming::AimMap map = aiming::default_map();

    ControlStatus far = at(30.0f, aiming::kTiltHomeDeg);
    fill_pose_fields(far, map, homed);
    TEST_ASSERT_TRUE(far.returning);

    ControlStatus far_tilt = at(0.0f, aiming::kTiltHomeDeg - 1.1f);
    fill_pose_fields(far_tilt, map, homed);
    TEST_ASSERT_TRUE(far_tilt.returning);

    ControlStatus far_pan_neg = at(-1.5f, aiming::kTiltHomeDeg);
    fill_pose_fields(far_pan_neg, map, homed);
    TEST_ASSERT_TRUE(far_pan_neg.returning);

    ControlStatus near = at(1.0f, aiming::kTiltHomeDeg + 1.0f); // exactly 1 degree is "home"
    fill_pose_fields(near, map, homed);
    TEST_ASSERT_FALSE(near.returning);

    ControlStatus not_ai = at(30.0f, aiming::kTiltHomeDeg);
    fill_pose_fields(not_ai, map, HomeState{.ai_mode = false, .homed = true, .home_remaining_ms = 0});
    TEST_ASSERT_FALSE(not_ai.returning);

    ControlStatus not_homed = at(30.0f, aiming::kTiltHomeDeg);
    fill_pose_fields(not_homed, map, HomeState{.ai_mode = true, .homed = false, .home_remaining_ms = 0});
    TEST_ASSERT_FALSE(not_homed.returning);
}

void test_non_finite_pose_is_harmless(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ControlStatus st = at(nan, nan);
    fill_pose_fields(st, aiming::default_map(), HomeState{.ai_mode = true, .homed = true, .home_remaining_ms = 0});
    TEST_ASSERT_EQUAL_INT16(160, st.aim_x); // to_pixel falls back to the frame centre
    TEST_ASSERT_EQUAL_INT16(120, st.aim_y);
    TEST_ASSERT_EQUAL_UINT8(0, st.lim);
    TEST_ASSERT_FALSE(st.returning);
}

void test_other_fields_are_untouched(void)
{
    ControlStatus st = at(5.0f, 95.0f);
    st.mode = ControlMode::Calibrate;
    st.cal_points = 1;
    st.calibrated = true;
    st.aims = 11;
    st.cal_saves = 3;
    st.cal_miss = 4;
    st.stick = true;
    st.speed = 2;
    fill_pose_fields(st, aiming::default_map(), HomeState{});
    TEST_ASSERT_EQUAL(ControlMode::Calibrate, st.mode);
    TEST_ASSERT_EQUAL_UINT8(1, st.cal_points);
    TEST_ASSERT_TRUE(st.calibrated);
    TEST_ASSERT_EQUAL_UINT32(11, st.aims);
    TEST_ASSERT_EQUAL_UINT32(3, st.cal_saves);
    TEST_ASSERT_EQUAL_UINT32(4, st.cal_miss);
    TEST_ASSERT_TRUE(st.stick);
    TEST_ASSERT_EQUAL_UINT8(2, st.speed);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, st.angles.pan);
    TEST_ASSERT_EQUAL_FLOAT(95.0f, st.angles.tilt);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_round_pixel_rounds_half_away_from_zero);
    RUN_TEST(test_round_pixel_saturates_and_rejects_non_finite);
    RUN_TEST(test_home_pose_on_default_map_aims_at_the_centre);
    RUN_TEST(test_aim_pixel_is_the_inverse_of_the_map);
    RUN_TEST(test_aim_pixel_is_not_clamped_to_the_frame);
    RUN_TEST(test_aim_pixel_saturates_for_a_nearly_flat_map);
    RUN_TEST(test_limit_bits_follow_the_pose);
    RUN_TEST(test_reach_box_comes_from_the_map);
    RUN_TEST(test_home_countdown_only_in_ai_mode);
    RUN_TEST(test_returning_needs_ai_mode_homed_and_distance);
    RUN_TEST(test_non_finite_pose_is_harmless);
    RUN_TEST(test_other_fields_are_untouched);
    return UNITY_END();
}
