#include "camera_diff_detection.h"
#include "constants.h"
#include "controller.h"
#include "joystick.h"
#include "movement_manager.h"
#include <Arduino.h>
#include <esp_log.h>

#include "secrets.h"
#include "wifi_manager.h"

#include "camera.h"
#include "turret_server.h"

static const char* TAG_MAIN = "MAIN";

Servo servo;
Stepper stepper(STEPPER_NUMBER_OF_STEPS, STEPPER_PIN1, STEPPER_PIN3, STEPPER_PIN2, STEPPER_PIN4);

MovementManager movement_manager(stepper, servo);
CameraDiffDetection detection_manager;
Joystick joystick(JOYSTICK_PIN_X, JOYSTICK_PIN_Y, JOYSTICK_PIN_Z);
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
    ESP_LOGD(TAG_MAIN, "Attaching Servo on pin %d", SERVO_PIN);
    servo.attach(SERVO_PIN);
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
