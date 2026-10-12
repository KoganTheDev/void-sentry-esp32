#include "camera_diff_detection.h"
#include "constants.h"
#include "controller.h"
#include "joystick.h"
#include "movement_manager.h"
#include "pins.h"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_system.h>

#include "secrets.h" // Not committed: copy include/secrets.h.example to include/secrets.h and set your WiFi
#include "wifi_manager.h"

#include "camera.h"
#include "camera_task.h"
#include "frame_store.h"
#include "turret_server.h"

static const char* TAG_MAIN = "MAIN";

Servo servo;
// The 28BYJ-48 coil order needs IN1, IN3, IN2, IN4 for the Arduino Stepper library's sequence
Stepper stepper(STEPPER_NUMBER_OF_STEPS, pins::STEPPER_IN1, pins::STEPPER_IN3, pins::STEPPER_IN2, pins::STEPPER_IN4);

MovementManager movement_manager(stepper, servo);
CameraDiffDetection detection_manager;
Joystick joystick(pins::JOYSTICK_X, pins::JOYSTICK_Y, pins::JOYSTICK_SW);
HttpServer http_server;
Camera camera;
FrameStore frame_store;
CameraTaskStats camera_stats;

Controller controller(movement_manager, detection_manager, joystick, frame_store);

// One benchmark line every 5 s (format fixed by the Stage 1 spec). Never prints credentials.
static void log_stream_stats()
{
    static uint32_t last_ms = 0;
    const uint32_t now = millis();
    if (now - last_ms < 5000)
    {
        return;
    }
    last_ms = now;

    const FrameStoreStats store = frame_store.stats();
    const DetectionStats det = detection_manager.get_stats();
    const TrackSnapshot track = detection_manager.get_track_snapshot();
    const uint32_t copies = camera_stats.copies.load();
    const uint32_t copy_us_avg = copies ? camera_stats.copy_us_total.load() / copies : 0;
    ESP_LOGI(TAG_MAIN,
             "[stream] up=%us captured=%u sent=%u dropped_no_slot=%u oversize=%u corrupt=%u viewers=%u "
             "copy_us_avg=%u heap=%u heap_min=%u psram=%u psram_min=%u "
             "decode_us_avg=%u track_us_avg=%u scene_changes=%u decode_fail=%u track=%s id=%u",
             (unsigned)(now / 1000), (unsigned)camera_stats.captured.load(), (unsigned)http_server.frames_sent(),
             (unsigned)store.dropped_no_slot, (unsigned)store.dropped_oversize, (unsigned)store.dropped_corrupt,
             (unsigned)http_server.viewer_count(), (unsigned)copy_us_avg, (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getMinFreePsram(),
             (unsigned)det.decode_us_avg, (unsigned)det.track_us_avg, (unsigned)det.scene_changes,
             (unsigned)det.decode_failures, tracking::to_string(track.state), (unsigned)track.id);
}

void setup()
{
    Serial.begin(BAUDRATE);
    delay(100);

    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "==== SMART CAMERA TURRET SYSTEM ====");
    ESP_LOGI(TAG_MAIN, "====  Initialization Sequence   ====");
    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "Reset reason: %d", (int)esp_reset_reason()); // lets a soak test confirm no reboots
    ESP_LOGI(TAG_MAIN, "Initializing components...");
    ESP_LOGI(TAG_MAIN, "[SYSTEM] Initializing components...");

    // Initialize Joystick
    ESP_LOGI(TAG_MAIN, "Initializing Joystick...");
    joystick.begin();
    ESP_LOGI(TAG_MAIN, "Joystick initialized successfully");

    // Initialize Camera
    ESP_LOGI(TAG_MAIN, "Initializing Camera...");
    if (!camera.begin())
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Camera initialization failed - continuing without video");
    } else if (!frame_store.begin() || !start_camera_task(frame_store, camera_stats))
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Frame pipeline failed to start - continuing without video");
    } else
    {
        ESP_LOGI(TAG_MAIN, "Camera and frame pipeline running");
    }

    if (!detection_manager.begin())
    {
        ESP_LOGE(TAG_MAIN, "Tracking pipeline allocation failed - continuing without detection");
    }

    // Connect WiFi
    ESP_LOGI(TAG_MAIN, "Connecting to WiFi...");
    WifiManager::connect(WIFI_SSID, WIFI_PASSWORD);
    ESP_LOGI(TAG_MAIN, "WiFi connection sequence initiated");

    // Start HTTP Server
    ESP_LOGI(TAG_MAIN, "Starting HTTP Server...");
    if (http_server.start(frame_store, &detection_manager))
    {
        ESP_LOGI(TAG_MAIN, "HTTP Server started successfully");
    } else
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: HTTP Server failed to start!");
        return;
    }

    // Attach Servo
    ESP_LOGD(TAG_MAIN, "Attaching Servo on pin %d", pins::SERVO);
    servo.attach(pins::SERVO);
    ESP_LOGI(TAG_MAIN, "Servo attached successfully");

    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "Setup Complete - System Ready!");
    ESP_LOGI(TAG_MAIN, "AI_MODE active, waiting for frames");
    ESP_LOGI(TAG_MAIN, "====================================");
}

void loop()
{
    controller.run();        // Run main control loop
    WifiManager::maintain(); // Maintain WiFi connection
    log_stream_stats();

    // capture() used to pace loop(); now it spins unpaced, so always yield one tick to idle/other tasks.
    vTaskDelay(pdMS_TO_TICKS(1));
}
