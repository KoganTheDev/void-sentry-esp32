/**
 * @file motion_data.h
 * * @details This header defines the data structures used to pass motion information 
 * between the Computer Vision pipeline and the Kinetic Engine/Web UI. It ensures 
 * that the tracking logic remains decoupled from the specific sensor hardware.
 */

#pragma once

#include <Arduino.h>

/**
 * @class MotionData
 * @brief Clean abstraction for motion detection results.
 * @details Separates motion detection logic from visualization. 
 * This class acts as a "Snapshot" of a single frame's motion analysis, 
 * providing the geometric data required to draw AR overlays or move servos.
 */
class MotionData
{
private:
    bool _detected;    ///< Internal flag indicating if the motion threshold was met.
    int _centroid_x;   ///< Calculated X coordinate of target center of mass.
    int _centroid_y;   ///< Calculated Y coordinate of target center of mass.
    int _frame_width;  ///< Horizontal resolution of the source frame buffer.
    int _frame_height; ///< Vertical resolution of the source frame buffer.
    int _pixel_count;  ///< Raw count of "hot" pixels identified in frame differencing.

public:
    /**
     * @name Initialization
     * @{
     */

    /**
     * @brief Default Constructor: Create empty motion data.
     * @note Used to initialize state when no motion has been processed yet.
     */
    MotionData() : _detected(false), _centroid_x(0), _centroid_y(0), _frame_width(0), _frame_height(0), _pixel_count(0)
    {
    }

    /**
     * @brief Parameterized Constructor: Create motion data from algorithm output.
     * @param centroid_x The X coordinate of the motion "blob" center.
     * @param centroid_y The Y coordinate of the motion "blob" center.
     * @param frame_width The resolution width used during analysis.
     * @param frame_height The resolution height used during analysis.
     * @param pixel_count The quantity of pixels that exceeded the sensitivity threshold.
     */
    MotionData(int centroid_x, int centroid_y, int frame_width, int frame_height, int pixel_count)
        : _detected(true), _centroid_x(centroid_x), _centroid_y(centroid_y), _frame_width(frame_width),
          _frame_height(frame_height), _pixel_count(pixel_count)
    {
    }
    /** @} */

    /**
     * @name Data Accessors (Getters)
     * @{
     */

    /**
     * @brief Check if valid motion was detected in this snapshot.
     * @return true if the pixel_count met the minimum algorithm threshold.
     */
    bool is_detected() const { return this->_detected; }

    /**
     * @brief Get motion centroid X coordinate.
     * @return X pixel coordinate (0 if no motion).
     */
    int get_centroid_x() const { return this->_centroid_x; }

    /**
     * @brief Get motion centroid Y coordinate.
     * @return Y pixel coordinate (0 if no motion).
     */
    int get_centroid_y() const { return this->_centroid_y; }

    /**
     * @brief Get the width of the frame processed.
     * @return Source width in pixels (e.g., 320 for QVGA).
     */
    int get_frame_width() const { return this->_frame_width; }

    /**
     * @brief Get the height of the frame processed.
     * @return Source height in pixels (e.g., 240 for QVGA).
     */
    int get_frame_height() const { return this->_frame_height; }

    /**
     * @brief Get the volume of detected motion.
     * @return Total count of pixels that changed between frames.
     */
    int get_pixel_count() const { return this->_pixel_count; }
    /** @} */
};

/**
 * @class DetectionMetrics
 * @brief Performance and health metrics for the Motion Detection engine.
 * @details Tracks computational overhead (latency) and statistical trends 
 * such as FPS and detection consistency. This data is primarily used for 
 * the Web Dashboard telemetry and debugging.
 */
class DetectionMetrics
{
private:
    uint32_t _detection_time_ms;    ///< Latency of the frame differencing loop.
    int _pixels_changed;            ///< Number of pixels that changed vs current threshold.
    float _motion_confidence;       ///< Normalised score (0.0 to 1.0) of detection reliability.
    int _consecutive_motion_frames; ///< Debounce counter for sustained motion.
    int _consecutive_static_frames; ///< Debounce counter for sustained stillness.
    uint32_t _total_detections;     ///< Historical counter of successful locks.
    uint32_t _total_frames;         ///< Life-time frame counter for uptime/FPS calculation.
    uint32_t _last_frame_timestamp; ///< Epoch/Millis of the most recent calculation.
    uint32_t _average_fps;          ///< Calculated throughput of the CV pipeline.

public:
    /**
     * @brief Constructor: Initializes all metrics to zero.
     */
    DetectionMetrics()
        : _detection_time_ms(0), _pixels_changed(0), _motion_confidence(0.0f), _consecutive_motion_frames(0),
          _consecutive_static_frames(0), _total_detections(0), _total_frames(0), _last_frame_timestamp(0),
          _average_fps(0)
    {
    }

    /**
     * @name Metric Mutators (Setters)
     * @{
     */
    void set_detection_time_ms(uint32_t time) { this->_detection_time_ms = time; }
    void set_pixels_changed(int count) { this->_pixels_changed = count; }
    
    /** 
     * @brief Sets confidence and clamps value between 0.0 and 1.0. 
     * @param conf The raw float confidence score.
     */
    void set_motion_confidence(float conf) { this->_motion_confidence = (conf < 0.0f) ? 0.0f : (conf > 1.0f) ? 1.0f : conf; }
    void set_consecutive_motion_frames(int count) { this->_consecutive_motion_frames = count; }
    void set_consecutive_static_frames(int count) { this->_consecutive_static_frames = count; }
    void set_total_detections(uint32_t count) { this->_total_detections = count; }
    void set_total_frames(uint32_t count) { this->_total_frames = count; }
    void set_last_frame_timestamp(uint32_t ts) { this->_last_frame_timestamp = ts; }
    void set_average_fps(uint32_t fps) { this->_average_fps = fps; }
    /** @} */

    /**
     * @name Metric Accessors (Getters)
     * @{
     */
    /** @return Processing time in milliseconds. */
    uint32_t get_detection_time_ms() const { return this->_detection_time_ms; }
    /** @return Count of changed pixels. */
    int get_pixels_changed() const { return this->_pixels_changed; }
    /** @return Confidence score as a percentage (0.0 - 1.0). */
    float get_motion_confidence() const { return this->_motion_confidence; }
    /** @return Number of frames motion has been continuously present. */
    int get_consecutive_motion_frames() const { return this->_consecutive_motion_frames; }
    /** @return Number of frames the scene has been continuously static. */
    int get_consecutive_static_frames() const { return this->_consecutive_static_frames; }
    /** @return Accumulated detection count. */
    uint32_t get_total_detections() const { return this->_total_detections; }
    /** @return Accumulated frame count. */
    uint32_t get_total_frames() const { return this->_total_frames; }
    /** @return Millis timestamp of last update. */
    uint32_t get_last_frame_timestamp() const { return this->_last_frame_timestamp; }
    /** @return Current performance in frames per second. */
    uint32_t get_average_fps() const { return this->_average_fps; }
    /** @} */
};