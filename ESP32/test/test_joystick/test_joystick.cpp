#include "Joystick.h"
#include <unity.h>
#include <Arduino.h>

// Define test pins (use pins that aren't used by your camera)
#define TEST_PIN_X 32
#define TEST_PIN_Y 33

Joystick joystick(TEST_PIN_X, TEST_PIN_Y, 150);

void setUp(void)
{
    joystick.begin();
}

void tearDown(void) {}

void test_joystick_initialization(void) {
    // At rest, the joystick should not be active
    // Note: This assumes the stick is physically centered during the test
    TEST_ASSERT_FALSE(joystick.is_active());
}

void test_deadzone_logic(void) {
    // Since we can't physically move the stick, we verify 
    // that get_x/y returns 0 when the delta is small.
    // In a real mock environment, we would override analogRead.
    int x = joystick.get_x();
    TEST_ASSERT_EQUAL_INT(0, x);
}

void test_speed_mapping_limits(void) {
    // We test the boundary logic of the get_speed functions
    // A speed of 0 should always return 0
    TEST_ASSERT_EQUAL_INT(0, joystick.get_speed_x(-255, 255));
}

void setup()
{
    // Wait for hardware to stabilize
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_joystick_initialization);
    RUN_TEST(test_deadzone_logic);
    RUN_TEST(test_speed_mapping_limits);
    UNITY_END();
}

void loop() {}