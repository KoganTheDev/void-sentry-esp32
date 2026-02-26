#pragma once

#include "base_detection_module.h"
#include "motion_data.h"
#include <Arduino.h>

/**
 * @class CameraDiffDetection
 * @brief Motion detection using frame differencing.
 * @details Analyzes consecutive frames to detect moving objects.
 * Returns clean MotionData structure with detection results.
 * Completely independent from visualization.
 */
class CameraDiffDetection : public BaseDetectionModule
{
private:
    // Sensitivity parameters (tune these for your environment)
    // Resolution: QVGA is 320x240 = 76,800 pixels (optimized for fast overlay encoding)
    static const int FRAME_WIDTH = 320;   ///< Frame width in pixels (QVGA resolution)
    static const int FRAME_HEIGHT = 240;  ///< Frame height in pixels
    static const int DIFF_THRESHOLD = 20; ///< Pixel difference threshold (0-255)
    static const int MOTION_THRESHOLD =
        150; ///< Minimum pixels that changed to consider motion (lowered from 800 for better sensitivity)
    static const int CENTER_DEADZONE = 20; ///< Pixels around center for stable tracking

    uint8_t* _prev_frame;    ///< Previous frame greyscale buffer
    uint8_t* _curr_frame;    ///< Current frame greyscale buffer
    uint8_t* _diff_buffer;   ///< Difference map buffer
    bool _buffers_allocated; ///< Flag to track buffer allocation
    bool _first_frame;       ///< Flag for first frame (no previous frame)

    // Motion tracking data
    MotionData _last_motion_data;      ///< Last detected motion (or empty if none)
    int _last_centroid_x;              ///< Last detected motion X coordinate (persists)
    int _last_centroid_y;              ///< Last detected motion Y coordinate (persists)
    DetectionMetrics _current_metrics; ///< Performance metrics for algorithm evaluation

public:
    CameraDiffDetection();
    ~CameraDiffDetection();

    /**
     * @brief Detects motion by comparing frames and returns direction to track.
     * @param frame Camera frame buffer
     * @return Tuple of (MoveX, MoveY) indicating motion direction
     */
    std::tuple<MoveX, MoveY> detect_object(camera_fb_t* frame) override;

    MotionData get_motion_data() const { return this->_last_motion_data; }

    DetectionMetrics get_detection_metrics() const { return this->_current_metrics; }

private:
    bool jpeg_to_greyscale(camera_fb_t* frame, uint8_t* output);

    /**
     * @brief Computes frame difference and finds motion centroid.
     * @param prev Previous frame (greyscale)
     * @param curr Current frame (greyscale)
     * @param width Frame width
     * @param height Frame height
     * @param center_x Output center X coordinate
     * @param center_y Output center Y coordinate
     * @param pixel_count Output count of pixels that detected motion
     * @return true if significant motion was found
     */
    bool find_motion(uint8_t* prev, uint8_t* curr, int width, int height, int& center_x, int& center_y,
                     int& pixel_count);

    /** @brief calculates fps */
    void calculate_fps();
};
