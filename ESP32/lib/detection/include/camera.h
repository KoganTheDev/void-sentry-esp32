/**
 * @file Camera.h
 * @brief Hardware Abstraction Layer (HAL) for the ESP32-Wrover Camera module.
 * @details This class handles the low-level handshake between the ESP32-S3/S2/WROVER
 * and the OV-series camera sensors. It manages I2C (SCCB) configuration and
 * high-speed parallel data capture into PSRAM.
 */

#pragma once

#include "esp_camera.h"

/**
 * @namespace WroverPins
 * @brief GPIO mapping specifically for the ESP32-Wrover-kit.
 * @note If using a different board (like AI-Thinker), these pins must be remapped.
 */
namespace WroverPins
{
/** @brief Power down pin; set to -1 if hardware is always on. */
#define PWDN_GPIO_NUM -1
/** @brief Hardware reset pin; set to -1 if managed by external RC circuit. */
#define RESET_GPIO_NUM -1
/** @brief External Clock (XCLK). Drives the internal timing of the sensor. */
#define XCLK_GPIO_NUM 21
/** @brief Serial Data line for SCCB (I2C) bus. Requires external pull-up. */
#define SIOD_GPIO_NUM 26
/** @brief Serial Clock line for SCCB (I2C) bus. */
#define SIOC_GPIO_NUM 27

/* Parallel Data Bus (Y2-Y9) */
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 19
#define Y4_GPIO_NUM 18
#define Y3_GPIO_NUM 5
#define Y2_GPIO_NUM 4

/** @brief Vertical Sync: High pulse indicates a new frame start. */
#define VSYNC_GPIO_NUM 25
/** @brief Horizontal Reference: High pulse indicates valid pixel data on the row. */
#define HREF_GPIO_NUM 23
/** @brief Pixel Clock: The ESP32 samples data on the rising edge of this clock. */
#define PCLK_GPIO_NUM 22
} // namespace WroverPins

typedef struct {
    void* buffer;
    size_t length;
    size_t width;
    size_t height;
} camera_buffer_t;

/**
 * @class Camera
 * @brief Singleton-style manager for camera lifecycle and frame acquisition.
 * @details Encapsulates the `esp_camera` driver. It is configured by default for
 * **Void Sentry's** real-time requirements: Low latency over high resolution.
 */
class Camera
{
private:
    camera_config_t _config; ///< Internal configuration struct for the ESP-Camera driver.
    camera_buffer_t _buffer;

public:
    /**
     * @name Lifecycle Management
     * @{
     */

    /**
     * @brief Constructor: Pre-configures the sensor for optimal CV performance.
     * @details
     * - **Resolution**: QVGA (320x240) to maintain high FPS.
     * - **Buffering**: Double-buffering enabled to allow simultaneous capture and processing.
     * - **Grab Mode**: `CAMERA_GRAB_LATEST` ensures we never process "stale" motion data.
     */
    Camera() : _buffer()
    {
        this->_config.ledc_channel = LEDC_CHANNEL_0;
        this->_config.ledc_timer = LEDC_TIMER_0;
        this->_config.pin_d0 = Y2_GPIO_NUM;
        this->_config.pin_d1 = Y3_GPIO_NUM;
        this->_config.pin_d2 = Y4_GPIO_NUM;
        this->_config.pin_d3 = Y5_GPIO_NUM;
        this->_config.pin_d4 = Y6_GPIO_NUM;
        this->_config.pin_d5 = Y7_GPIO_NUM;
        this->_config.pin_d6 = Y8_GPIO_NUM;
        this->_config.pin_d7 = Y9_GPIO_NUM;
        this->_config.pin_xclk = XCLK_GPIO_NUM;
        this->_config.pin_pclk = PCLK_GPIO_NUM;
        this->_config.pin_vsync = VSYNC_GPIO_NUM;
        this->_config.pin_href = HREF_GPIO_NUM;
        this->_config.pin_sccb_sda = SIOD_GPIO_NUM;
        this->_config.pin_sccb_scl = SIOC_GPIO_NUM;
        this->_config.pin_pwdn = PWDN_GPIO_NUM;
        this->_config.pin_reset = RESET_GPIO_NUM;

        this->_config.xclk_freq_hz = 20000000;       // 20MHz: Balance between speed and signal noise
        this->_config.pixel_format = PIXFORMAT_JPEG; // Required for MJPEG streaming
        this->_config.frame_size = FRAMESIZE_QVGA;   // 320x240
        this->_config.jpeg_quality = 12;             // 0-63 (lower is higher quality, 12 is optimal for Sentry)
        this->_config.fb_count = 2;                  // Double buffering in PSRAM
        this->_config.fb_location = CAMERA_FB_IN_PSRAM;
        this->_config.grab_mode = CAMERA_GRAB_LATEST;
    }

    ~Camera()
    {
        if (this->_buffer.buffer != NULL)
        {
            free(this->_buffer.buffer);
        }
    }

    /**
     * @brief Initializes the hardware and mounts the sensor.
     * @return true if the sensor was identified and PSRAM was allocated.
     * @return false if the SCCB bus failed or the camera is not powered.
     */
    bool begin();
    /** @} */

    /**
     * @name Image Acquisition
     * @{
     */

    /**
     * @brief Synchronously captures a frame from the sensor.
     * @return camera_fb_t* Pointer to the ESP-Camera frame buffer structure.
     * @note This function blocks until a full frame is clocked into memory.
     * @warning **Memory Safety**: You MUST call @ref release() with this pointer
     * to return the buffer to the DMA pool.
     */
    void capture();
    /** @} */

    const camera_buffer_t& get_frame_buffer() { return this->_buffer; }
};