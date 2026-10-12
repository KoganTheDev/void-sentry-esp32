#include "camera.h" // Your Camera class
#include "camera_task.h"
#include "frame_store.h"
#include "secrets.h" // For WIFI_SSID, WIFI_PASSWORD
#include "turret_server.h"
#include "wifi_manager.h" // Your Network class
#include <Arduino.h>
#include <unity.h>

HttpServer* test_server;
Camera* test_camera;
FrameStore* test_store;
CameraTaskStats test_camera_stats;

// Runs before every test
void setUp(void)
{
    test_server = new HttpServer();
    test_camera = new Camera();
    test_store = new FrameStore();
}

// Runs after every test
void tearDown(void)
{
    test_server->stop();
    delete test_server;
    delete test_camera;
    delete test_store;
}

// 1. Test Lifecycle: Start and Stop without crashing
void test_server_lifecycle(void)
{
    // start() takes a FrameStore; it does not need to be ready (viewers simply receive nothing)
    TEST_ASSERT_TRUE_MESSAGE(test_server->start(*test_store), "Server failed to start");
    test_server->stop();

    // Ensure it can restart cleanly
    TEST_ASSERT_TRUE_MESSAGE(test_server->start(*test_store), "Server failed to restart");
}

// 3. Test Integration: Live Stream and Command Access
void test_live_interaction(void)
{
    // Initialize Hardware
    bool cam_ok = test_camera->begin();
    TEST_ASSERT_TRUE_MESSAGE(cam_ok, "Camera Hardware failed to initialize");
    TEST_ASSERT_TRUE_MESSAGE(test_store->begin(), "FrameStore failed to initialize");
    TEST_ASSERT_TRUE_MESSAGE(start_camera_task(*test_store, test_camera_stats), "Camera task failed to start");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    // Start Server
    test_server->start(*test_store);

    Serial.println("\n==========================================");
    Serial.println("LIVE TEST ACTIVE FOR 60 SECONDS");
    Serial.printf("STREAM: http://%s/stream\n", WifiManager::get_ip().c_str());
    Serial.printf("MOVE:   http://%s/move?pan=90&tilt=45\n", WifiManager::get_ip().c_str());
    Serial.println("==========================================\n");

    // Keep the test alive for 1 minute to allow manual browser check
    delay(60000);
}

void setup()
{
    delay(2000);
    Serial.begin(115200);

    // This sets up the LwIP stack (the "mbox") so the server doesn't crash
    WifiManager::connect(WIFI_SSID, WIFI_PASSWORD);

    UNITY_BEGIN();

    // Now these tests will find a valid TCP/IP stack
    RUN_TEST(test_server_lifecycle);
    RUN_TEST(test_live_interaction);

    UNITY_END();
}

void loop() {}