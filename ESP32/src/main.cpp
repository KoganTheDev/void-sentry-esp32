#include "camera_diff_detection.h"
#include "constants.h"
#include "controller.h"
#include "joystick.h"
#include "movement_manager.h"
#include "pins.h"
#include <Arduino.h>
#include <esp_log.h>

#include "secrets.h" // Not committed: copy include/secrets.h.example to include/secrets.h and set your WiFi
#include "wifi_manager.h"

#include "camera.h"
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

Controller controller(movement_manager, detection_manager, joystick, camera);

void setup()
{
    Serial.begin(BAUDRATE);
    delay(100);

    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "==== SMART CAMERA TURRET SYSTEM ====");
    ESP_LOGI(TAG_MAIN, "====  Initialization Sequence   ====");
    ESP_LOGI(TAG_MAIN, "====================================");
    ESP_LOGI(TAG_MAIN, "Initializing components...");
    ESP_LOGI(TAG_MAIN, "[SYSTEM] Initializing components...");

    // Initialize Joystick
    ESP_LOGI(TAG_MAIN, "Initializing Joystick...");
    joystick.begin();
    ESP_LOGI(TAG_MAIN, "Joystick initialized successfully");

    // Initialize Camera
    ESP_LOGI(TAG_MAIN, "Initializing Camera...");
    if (camera.begin())
    {
        ESP_LOGI(TAG_MAIN, "Camera initialized successfully");
    } else
    {
        ESP_LOGE(TAG_MAIN, "CRITICAL ERROR: Camera initialization failed!");
        return; // TODO: Decide if to stop running with a harsher stop
    }

    // Connect WiFi
    ESP_LOGI(TAG_MAIN, "Connecting to WiFi: %s", WIFI_SSID);
    WifiManager::connect(WIFI_SSID, WIFI_PASSWORD);
    ESP_LOGI(TAG_MAIN, "WiFi connection sequence initiated");

    // Start HTTP Server
    ESP_LOGI(TAG_MAIN, "Starting HTTP Server...");
    if (http_server.start(&camera, &detection_manager))
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
    camera.capture();
    controller.run();        // Run main control loop
    WifiManager::maintain(); // Maintain WiFi connection
}
