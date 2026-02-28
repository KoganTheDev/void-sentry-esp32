#include <unity.h>
#include "Servo.h"

// We use a specific pin that isn't connected to critical hardware for testing
#define TEST_SERVO_PIN 2 

using namespace Turret;

Turret::Servo* test_servo;

// This runs BEFORE every test case
void setUp(void)
{
    // Initialize with a restricted range for testing: 45 to 135 degrees
    test_servo = new Turret::Servo(45, 135);
}

// This runs AFTER every test case
void tearDown(void) {
    delete test_servo;
}

void test_initialization_defaults(void)
{
    // By default, your constructor sets current_angle to 90
    TEST_ASSERT_EQUAL_UINT8(90, test_servo->get_current_angle());
}

void test_set_target_within_bounds(void)
{
    test_servo->set_target(100);
    // Note: current_angle shouldn't change yet because update() hasn't been called
    TEST_ASSERT_EQUAL_UINT8(90, test_servo->get_current_angle());
}

void test_constrain_target_upper_bound(void)
{
    // We set max to 135 in setUp. Try to exceed it.
    test_servo->set_target(180); 
    
    // We need to verify the internal _target_angle is 135.
    // Since _target_angle is private, we verify it by snapping to it raw.
    test_servo->set_position_raw(180); 
    TEST_ASSERT_EQUAL_UINT8(135, test_servo->get_current_angle());
}

void test_constrain_target_lower_bound(void)
{
    // We set min to 45 in setUp. Try to go below.
    test_servo->set_position_raw(10); 
    TEST_ASSERT_EQUAL_UINT8(45, test_servo->get_current_angle());
}

void test_incremental_movement_logic(void)
{
    test_servo->set_speed(0); // Set speed to 0 for instant time-check
    test_servo->set_target(92);
    
    // First update: 90 -> 91
    test_servo->update();
    TEST_ASSERT_EQUAL_UINT8(91, test_servo->get_current_angle());
    
    // Second update: 91 -> 92
    test_servo->update();
    TEST_ASSERT_EQUAL_UINT8(92, test_servo->get_current_angle());
    
    // Third update: should stay at 92
    test_servo->update();
    TEST_ASSERT_EQUAL_UINT8(92, test_servo->get_current_angle());
}

// Entry point for the test
void setup()
{
    delay(2000); // Wait for hardware to stabilize
    UNITY_BEGIN();

    RUN_TEST(test_initialization_defaults);
    RUN_TEST(test_set_target_within_bounds);
    RUN_TEST(test_constrain_target_upper_bound);
    RUN_TEST(test_constrain_target_lower_bound);
    RUN_TEST(test_incremental_movement_logic);

    UNITY_END();
}

void loop() {}