#include <unity.h>
#include <Arduino.h>
#include "wifi_manager.h"
#include "secrets.h"

// Set a timeout for the connection test (20 seconds)
#define TEST_WIFI_TIMEOUT 20000

void setUp(void) {
    // Standard Unity setup
}

void tearDown(void) {
    // Standard Unity teardown
}

void test_wifi_initial_state_disconnected(void) {
    // Before we call connect, we shouldn't be connected
    // Note: This assumes the ESP32 wasn't already connected from a previous boot
    WiFi.disconnect(true);
    delay(1000);
    TEST_ASSERT_FALSE(WifiManager::is_connected());
}

void test_wifi_connect_logic(void) {
    WifiManager::connect(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long start_time = millis();
    bool connected = false;

    // We poll because connect() is non-blocking
    while (millis() - start_time < TEST_WIFI_TIMEOUT) {
        if (WifiManager::is_connected()) {
            connected = true;
            break;
        }
        delay(500);
    }

    TEST_ASSERT_TRUE_MESSAGE(connected, "WiFi failed to connect within timeout");
}

void test_wifi_get_ip_valid(void) {
    String ip = WifiManager::get_ip();
    Serial.printf("[TEST] Captured IP: %s\n", ip.c_str());
    
    // Check that IP is not the default 0.0.0.0
    TEST_ASSERT(strcmp("0.0.0.0", WifiManager::get_ip().c_str()) != 0);
}

void test_wifi_maintain_reconnect(void) {
    // 1. Ensure we are connected
    TEST_ASSERT_TRUE(WifiManager::is_connected());

    // 2. Force a disconnect
    Serial.println("[TEST] Forcing disconnect to test maintain()...");
    WiFi.disconnect();
    delay(1000);
    TEST_ASSERT_FALSE(WifiManager::is_connected());

    // 3. Call maintain() - It should trigger WiFi.begin()
    WifiManager::maintain();

    // 4. Wait for auto-recovery
    unsigned long start_time = millis();
    bool recovered = false;
    while (millis() - start_time < TEST_WIFI_TIMEOUT) {
        if (WifiManager::is_connected()) {
            recovered = true;
            break;
        }
        delay(500);
    }

    TEST_ASSERT_TRUE_MESSAGE(recovered, "WifiManager::maintain() failed to reconnect");
}

void setup() {
    // Wait for hardware to stabilize
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_wifi_initial_state_disconnected);
    RUN_TEST(test_wifi_connect_logic);
    RUN_TEST(test_wifi_get_ip_valid);
    RUN_TEST(test_wifi_maintain_reconnect);
    UNITY_END();
}

void loop() {}