#include "camera_diff_detection.h"
#include "constants.h"
#include "controller.h"
#include "joystick.h"
#include "pins.h"
#include "telemetry.h"
#include "turret.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>

#include "wifi_manager.h"

#include "camera.h"
#include "camera_task.h"
#include "frame_store.h"
#include "turret_server.h"

static const char* TAG_MAIN = "MAIN";

Servo servo;
Turret turret(servo);
CameraDiffDetection detection_manager;
Joystick joystick(pins::JOYSTICK_X, pins::JOYSTICK_Y, pins::JOYSTICK_SW);
HttpServer http_server;
WifiManager wifi;
Camera camera;
FrameStore frame_store;
CameraTaskStats camera_stats;

Controller controller(turret, detection_manager, joystick);

static const telemetry::Sources telemetry_sources{frame_store, camera_stats, detection_manager, http_server, turret,
                                                  controller,  wifi};

void setup()
{
    // First: IDF 5.x zeroes the minimum-free counter of any heap added after startup, and Arduino core 3 adds PSRAM to
    // the heap after startup (psramAddToHeap in initArduino), so ESP.getMinFreePsram() would stay 0 forever. This
    // restarts every heap's minimum from its current free size, so heap_min and psram_min count from setup().
    // Never stopped: stop would restore the zeroed minimum.
    heap_caps_monitor_local_minimum_free_size_start();
    Serial.begin(BAUDRATE);
    delay(100);

    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "==== SMART CAMERA TURRET SYSTEM ====");
    ESP_LOGI(TAG_MAIN, "====  Initialization Sequence   ====");
    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "Reset reason: %d", (int)esp_reset_reason()); // lets a soak test confirm no reboots
    ESP_LOGI(TAG_MAIN, "Initializing components...");

    ESP_LOGI(TAG_MAIN, "Initializing Joystick...");
    joystick.begin();
    ESP_LOGI(TAG_MAIN, "Joystick initialized successfully");

    // Turret first: drives the ULN2003 inputs LOW and homes the servo before anything can fail below.
    ESP_LOGI(TAG_MAIN, "Starting turret motor task...");
    if (!turret.begin())
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Turret motor task failed to start");
    }

    controller.begin(); // load the saved calibration from NVS (the turret has started)

    ESP_LOGI(TAG_MAIN, "Initializing Camera...");
    if (!camera.begin())
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Camera initialization failed - continuing without video");
    } else if (!frame_store.begin())
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Frame store not ready (PSRAM slots) - continuing without video");
    } else if (!start_camera_task(frame_store, camera_stats))
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: start_camera_task failed - continuing without video");
    } else
    {
        ESP_LOGI(TAG_MAIN, "Camera and frame pipeline running");
    }

    if (!detection_manager.begin())
    {
        ESP_LOGE(TAG_MAIN, "Tracking pipeline allocation failed - continuing without detection");
    } else if (!frame_store.ready() || !detection_manager.start_task(frame_store))
    {
        ESP_LOGE(TAG_MAIN, "Detection task failed to start - continuing without detection");
    }

    // WiFi: loads the saved networks and starts joining or the setup hotspot; never blocks (maintain() drives it)
    ESP_LOGI(TAG_MAIN, "Starting WiFi...");
    wifi.begin();

    ESP_LOGI(TAG_MAIN, "Starting HTTP Server...");
    if (http_server.start(frame_store, &detection_manager, &controller, &controller, &wifi))
    {
        ESP_LOGI(TAG_MAIN, "HTTP Server started successfully");
    } else
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: HTTP Server failed to start!");
        return;
    }

    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "Setup Complete - System Ready!");
    ESP_LOGI(TAG_MAIN, "Ai mode active, waiting for frames");
    ESP_LOGI(TAG_MAIN, "====================================");
}

void loop()
{
    controller.run();
    wifi.maintain(); // WiFi plan, scans, setup hotspot DNS, serial wifi-reset
    telemetry::log_stats(telemetry_sources);

    // capture() used to pace loop(); now it spins unpaced, so always yield one tick to idle/other tasks.
    vTaskDelay(pdMS_TO_TICKS(1));
}
