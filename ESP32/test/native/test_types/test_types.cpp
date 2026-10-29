/**
 * @file test_types.cpp
 * @brief Host-side tests for the hardware-free value types and the pin map.
 * @details Runs on the PC with `pio test -e native` - no board needed.
 */

#include <cstring>
#include <unity.h>

#include "move_types.h"
#include "pins.h"
#include "system_control_types.h"

void setUp(void) {}
void tearDown(void) {}

// --- MoveX / MoveY ---

void test_move_defaults_to_none(void)
{
    MoveX x;
    MoveY y;
    TEST_ASSERT_TRUE(x.is_none());
    TEST_ASSERT_TRUE(y.is_none());
}

void test_move_compares_with_enum(void)
{
    MoveX x(MoveX::Left);
    TEST_ASSERT_TRUE(x == MoveX::Left);
    TEST_ASSERT_TRUE(x != MoveX::Right);
    TEST_ASSERT_FALSE(x.is_none());
}

void test_move_to_string(void)
{
    TEST_ASSERT_EQUAL_STRING("Right", MoveX(MoveX::Right).to_string());
    TEST_ASSERT_EQUAL_STRING("Left", MoveX(MoveX::Left).to_string());
    TEST_ASSERT_EQUAL_STRING("None", MoveX().to_string());
    TEST_ASSERT_EQUAL_STRING("Up", MoveY(MoveY::Up).to_string());
    TEST_ASSERT_EQUAL_STRING("Down", MoveY(MoveY::Down).to_string());
    TEST_ASSERT_EQUAL_STRING("None", MoveY().to_string());
}

void test_move_converts_to_raw_value(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(MoveX()));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(MoveX(MoveX::Right)));
    TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(MoveY(MoveY::Down)));
}

// --- ControlMode ---

void test_control_mode_names(void)
{
    TEST_ASSERT_EQUAL_STRING("ai", to_string(ControlMode::Ai));
    TEST_ASSERT_EQUAL_STRING("user", to_string(ControlMode::User));
    TEST_ASSERT_EQUAL_STRING("calibrate", to_string(ControlMode::Calibrate));
}

// --- Pin map (the static_asserts already ran at compile time; these check the checker) ---

void test_pin_checker_detects_reserved_pins(void)
{
    TEST_ASSERT_TRUE(pins::check::is_reserved(16));
    TEST_ASSERT_TRUE(pins::check::is_reserved(17));
    TEST_ASSERT_TRUE(pins::check::is_reserved(6));
    TEST_ASSERT_FALSE(pins::check::is_reserved(12));
}

void test_pin_checker_knows_camera_and_adc1(void)
{
    TEST_ASSERT_TRUE(pins::check::on_camera(pins::CAM_XCLK));
    TEST_ASSERT_FALSE(pins::check::on_camera(pins::SERVO));
    TEST_ASSERT_TRUE(pins::check::is_adc1(32));
    TEST_ASSERT_FALSE(pins::check::is_adc1(14));
}

void test_turret_uses_all_eight_free_pins(void) { TEST_ASSERT_EQUAL_UINT(8, pins::check::TURRET_COUNT); }

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_move_defaults_to_none);
    RUN_TEST(test_move_compares_with_enum);
    RUN_TEST(test_move_to_string);
    RUN_TEST(test_move_converts_to_raw_value);
    RUN_TEST(test_control_mode_names);
    RUN_TEST(test_pin_checker_detects_reserved_pins);
    RUN_TEST(test_pin_checker_knows_camera_and_adc1);
    RUN_TEST(test_turret_uses_all_eight_free_pins);
    return UNITY_END();
}
