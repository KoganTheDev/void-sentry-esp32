#include <Arduino.h>
#include <unity.h>
#include "turret_server.h"
#include "camera.h"   // Your Camera class
#include "wifi_manager.h"  // Your Network class
#include "secrets.h"  // For WIFI_SSID, WIFI_PASSWORD

HttpServer* test_server;
Camera* test_camera;

// Runs before every test
void setUp(void) {
    test_server = new HttpServer();
    test_camera = new Camera();
}

// Runs after every test
void tearDown(void) {
    test_server->stop();
    delete test_server;
    delete test_camera;
}

// 1. Test Lifecycle: Start and Stop without crashing
void test_server_lifecycle(void) {
    // Note: start() requires a camera pointer now
    TEST_ASSERT_TRUE_MESSAGE(test_server->start(test_camera), "Server failed to start");
    test_server->stop();
    
    // Ensure it can restart cleanly
    TEST_ASSERT_TRUE_MESSAGE(test_server->start(test_camera), "Server failed to restart");
}

// 2. Test Safety: Handlers should not crash with NULL requests
void test_handlers_null_safety(void) {
    // Testing the guard clause: if (req == NULL) { return ESP_ERR_INVALID_ARG; }
    esp_err_t res_stream = test_server->stream_handler(NULL);
    esp_err_t res_cmd = test_server->cmd_handler(NULL);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, res_stream);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, res_cmd);
}

// 3. Test Integration: Live Stream and Command Access
void test_live_interaction(void) {
    // Initialize Hardware
    bool cam_ok = test_camera->begin();
    TEST_ASSERT_TRUE_MESSAGE(cam_ok, "Camera Hardware failed to initialize");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500); 
        Serial.print(".");
    }

    // Start Server
    test_server->start(test_camera);

    Serial.println("\n==========================================");
    Serial.println("LIVE TEST ACTIVE FOR 60 SECONDS");
    Serial.printf("STREAM: http://%s/stream\n", WifiManager::get_ip().c_str());
    Serial.printf("MOVE:   http://%s/move?pan=90&tilt=45\n", WifiManager::get_ip().c_str());
    Serial.println("==========================================\n");

    // Keep the test alive for 1 minute to allow manual browser check
    delay(60000); 
}

void setup() {
    delay(2000); 
    Serial.begin(115200);

    // This sets up the LwIP stack (the "mbox") so the server doesn't crash
    WifiManager::connect(WIFI_SSID, WIFI_PASSWORD); 

    UNITY_BEGIN();

    // Now these tests will find a valid TCP/IP stack
    RUN_TEST(test_server_lifecycle);
    RUN_TEST(test_handlers_null_safety);
    RUN_TEST(test_live_interaction); 

    UNITY_END();
}

void loop() {}