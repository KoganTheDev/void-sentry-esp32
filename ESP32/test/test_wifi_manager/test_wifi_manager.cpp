#include "wifi_manager.h"
#include <Arduino.h>
#include <unity.h>

// begin() must return at once; the plan must then reach Setup (no saved network in range) or Connected.
#define TEST_BEGIN_MAX_MS 1000
#define TEST_PLAN_TIMEOUT_MS 40000

static WifiManager wifi;

void setUp(void) {}

void tearDown(void) {}

void test_begin_returns_at_once(void)
{
    const uint32_t start = millis();
    wifi.begin();
    TEST_ASSERT_LESS_THAN_UINT32(TEST_BEGIN_MAX_MS, millis() - start);
}

void test_plan_reaches_setup_or_connected(void)
{
    const uint32_t start = millis();
    bool settled = false;
    while (millis() - start < TEST_PLAN_TIMEOUT_MS)
    {
        wifi.maintain();
        const prov::PlanState st = wifi.plan_state();
        if (st == prov::PlanState::Setup || st == prov::PlanState::Connected)
        {
            settled = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    TEST_ASSERT_TRUE_MESSAGE(settled, "plan did not reach Setup or Connected within 40 s");
}

void test_status_matches_mode(void)
{
    const WifiManager::Status s = wifi.status();
    Serial.printf("[TEST] mode=%s ssid=%s ip=%s pin_set=%d\n", s.mode, s.ssid, s.ip, (int)s.pin_set);
    if (wifi.plan_state() == prov::PlanState::Connected)
    {
        TEST_ASSERT_EQUAL_STRING("sta", s.mode);
        TEST_ASSERT_TRUE(strcmp("0.0.0.0", s.ip) != 0);
    } else
    {
        TEST_ASSERT_EQUAL_STRING("setup", s.mode);
        TEST_ASSERT_TRUE(wifi.setup_mode());
    }
}

void setup()
{
    delay(2000);
    Serial.begin(115200);

    UNITY_BEGIN();
    RUN_TEST(test_begin_returns_at_once);
    RUN_TEST(test_plan_reaches_setup_or_connected);
    RUN_TEST(test_status_matches_mode);
    UNITY_END();
}

void loop() {}
