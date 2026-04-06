#include <Arduino.h>
#include <unity.h>
#include "controller.h"

// TODO: Add tests

// This runs BEFORE every test case
void setUp(void)
{
}

// This runs AFTER every test case
void tearDown(void) {
}

// Entry point for the test
void setup()
{
    delay(2000); // Wait for hardware to stabilize
    UNITY_BEGIN();

    TEST_ASSERT_TRUE(true);

    UNITY_END();
}

void loop() {}