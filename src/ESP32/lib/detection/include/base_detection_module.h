/**
 * @file BaseDetectionModule.h
 * @brief Abstract interface for object detection logic on ESP32-CAM.
 */

#pragma once

#include "../../../include/move_types.h"
#include <esp_camera.h>
#include <tuple>

/**
 * @class BaseDetectionModule
 * @brief An abstract base class (Interface) that defines the contract for
 * detection algorithms. This class allows the Controller to remain agnostic of
 * the specific detection method (e.g., Color, AI, or Motion) by providing a
 * consistent interface.
 */
class BaseDetectionModule
{
public:
    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~BaseDetectionModule() = default;

    /**
     * @brief Analyzes a camera frame to locate a specific object.
     * @param frame Pointer to the ESP32-CAM frame buffer (camera_fb_t).
     * @return std::tuple<uint8_t, uint8_t> Returns a tuple containing:
     * - Index 0 (MoveX): Left | Right | Stay
     * - Index 1 (MoveY): Up   | Down  | Stay
     */
    virtual std::tuple<MoveX, MoveY> detect_object(camera_fb_t* frame) = 0;

    /**
     * @brief Gets the RGB565 buffer with overlay (for streaming visualization).
     * @details Base implementation returns nullptr. Derived classes that support
     * RGB visualization should override this method.
     * @return Pointer to RGB565 buffer with overlay, or nullptr if not supported.
     */
    virtual uint16_t* get_rgb_buffer() const { return nullptr; }

    /**
     * @brief Gets the dimensions of the RGB buffer.
     * @param[out] width Width of the buffer in pixels
     * @param[out] height Height of the buffer in pixels
     * @return true if buffer is allocated and dimensions are valid, false otherwise.
     * Default implementation returns false.
     */
    virtual bool get_rgb_buffer_dimensions(int& width, int& height) const
    {
        width = 0;
        height = 0;
        return false;
    }

    /**
     * @brief Encodes the RGB565 overlay buffer to JPEG format for HTTP streaming.
     * @details Base implementation returns false. Derived classes that support
     * JPEG overlay encoding should override this method.
     * @param[out] jpeg_buf Pointer to the output JPEG buffer (allocated internally)
     * @param[out] jpeg_len Size of the JPEG buffer
     * @param quality JPEG quality (1-100)
     * @return true if encoding succeeded, false otherwise (or not supported)
     */
    virtual bool get_jpeg_with_overlay(uint8_t** jpeg_buf, size_t* jpeg_len, uint8_t quality = 80) { return false; }
};