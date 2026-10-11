/**
 * @file Camera.h
 * @brief Hardware Abstraction Layer (HAL) for the ESP32-Wrover Camera module.
 * @details This class handles the low-level handshake between the ESP32-S3/S2/WROVER
 * and the OV-series camera sensors. It manages I2C (SCCB) configuration and
 * high-speed parallel data capture into PSRAM.
 */

#pragma once

#include "esp_camera.h"
#include "pins.h" // Camera bus GPIOs (pins::CAM_*)

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
        this->_config.pin_d0 = pins::CAM_Y2;
        this->_config.pin_d1 = pins::CAM_Y3;
        this->_config.pin_d2 = pins::CAM_Y4;
        this->_config.pin_d3 = pins::CAM_Y5;
        this->_config.pin_d4 = pins::CAM_Y6;
        this->_config.pin_d5 = pins::CAM_Y7;
        this->_config.pin_d6 = pins::CAM_Y8;
        this->_config.pin_d7 = pins::CAM_Y9;
        this->_config.pin_xclk = pins::CAM_XCLK;
        this->_config.pin_pclk = pins::CAM_PCLK;
        this->_config.pin_vsync = pins::CAM_VSYNC;
        this->_config.pin_href = pins::CAM_HREF;
        this->_config.pin_sccb_sda = pins::CAM_SIOD;
        this->_config.pin_sccb_scl = pins::CAM_SIOC;
        this->_config.pin_pwdn = pins::CAM_PWDN;
        this->_config.pin_reset = pins::CAM_RESET;

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