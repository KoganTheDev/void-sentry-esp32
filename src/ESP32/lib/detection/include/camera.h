/**
 * @file Camera.h
 * @brief Hardware abstraction layer for the ESP32-Wrover Camera module.
 */

#pragma once

#include "esp_camera.h"

/**
 * @namespace WroverPins
 * @brief GPIO mapping specifically for the ESP32-Wrover-kit.
 */
namespace WroverPins
{
/** @brief Power down pin; set to -1 if tied to ground/unused. */
#define PWDN_GPIO_NUM -1
/** @brief Hardware reset pin; set to -1 if managed by internal circuitry. */
#define RESET_GPIO_NUM -1
/** @brief External Clock (XCLK) provided by ESP32 to drive the sensor. */
#define XCLK_GPIO_NUM 21
/** @brief Serial Data line for SCCB (I2C-compatible) configuration bus. */
#define SIOD_GPIO_NUM 26
/** @brief Serial Clock line for SCCB configuration bus. */
#define SIOC_GPIO_NUM 27

/* Data Bus Pins (Y2-Y9 represent the 8-bit parallel interface) */
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4

/** @brief Vertical Sync: Signals the start of a new frame. */
#define VSYNC_GPIO_NUM 25
/** @brief Horizontal Reference: Signals the start of a valid pixel row. */
#define HREF_GPIO_NUM 23
/** @brief Pixel Clock: Signals when the data on the Y pins is valid for
 * capture. */
#define PCLK_GPIO_NUM 22
} // namespace WroverPins

/**
 * @class Camera
 * @brief Manages camera initialization, frame capture, and memory lifecycle.
 */
class Camera
{
private:
    camera_config_t _config; ///< Internal structure holding ESP-Camera driver settings.

public:
    /**
     * @brief Constructor: Initializes the default hardware configuration.
     * @details Sets default values for QVGA resolution, JPEG format, and PSRAM
     * usage.
     */
    Camera()
    {
        _config.ledc_channel = LEDC_CHANNEL_0;
        _config.ledc_timer = LEDC_TIMER_0;
        _config.pin_d0 = Y2_GPIO_NUM;
        _config.pin_d1 = Y3_GPIO_NUM;
        _config.pin_d2 = Y4_GPIO_NUM;
        _config.pin_d3 = Y5_GPIO_NUM;
        _config.pin_d4 = Y6_GPIO_NUM;
        _config.pin_d5 = Y7_GPIO_NUM;
        _config.pin_d6 = Y8_GPIO_NUM;
        _config.pin_d7 = Y9_GPIO_NUM;
        _config.pin_xclk = XCLK_GPIO_NUM;
        _config.pin_pclk = PCLK_GPIO_NUM;
        _config.pin_vsync = VSYNC_GPIO_NUM;
        _config.pin_href = HREF_GPIO_NUM;
        _config.pin_sccb_sda = SIOD_GPIO_NUM;
        _config.pin_sccb_scl = SIOC_GPIO_NUM;
        _config.pin_pwdn = PWDN_GPIO_NUM;
        _config.pin_reset = RESET_GPIO_NUM;

        _config.xclk_freq_hz = 20000000;       // 20MHz for OV3660 stability
        _config.pixel_format = PIXFORMAT_JPEG; // Compression for network throughput
        _config.frame_size = FRAMESIZE_QVGA;   // 320x240: Optimal for real-time overlay encoding
        _config.jpeg_quality = 70;             // 0-63 (70 = good quality, much faster encoding than VGA)
        _config.fb_count = 2; // Double buffering
        _config.fb_location = CAMERA_FB_IN_PSRAM;
        _config.grab_mode = CAMERA_GRAB_LATEST; // Drops old frames to reduce latency
    }

    /**
     * @brief Initializes the sensor and allocates memory for the frame buffer.
     * @return true if initialization succeeded, false if sensor not found or
     * PSRAM failed.
     */
    bool begin();

    /**
     * @brief Captures a new image frame.
     * @return camera_fb_t* Pointer to the frame buffer.
     * @warning Must call release() after processing to avoid memory leaks.
     */
    camera_fb_t* capture();

    /**
     * @brief Returns the frame buffer to the driver to be reused.
     * @param fb Pointer to the captured frame buffer.
     */
    void release(camera_fb_t* fb);
};
