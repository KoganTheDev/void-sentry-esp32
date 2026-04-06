/**
 * @file BaseDetectionModule.h
 * @brief Abstract interface for object detection logic on ESP32-CAM.
 */

#pragma once

#include "camera.h"
#include "motion_data.h"
#include "move_types.h"
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
    virtual std::tuple<MoveX, MoveY> detect_object(camera_buffer_t frame) = 0;

    /**
     * @brief Get the latest motion data from the detection module.
     * @details This method provides access to the motion metrics detected.
     * Derived classes should override this to return their current metrics.
     * @return MotionData containing the latest detection results.
     */
    virtual MotionData get_motion_data() const
    {
        // Default implementation returns empty motion data
        return MotionData();
    }
};