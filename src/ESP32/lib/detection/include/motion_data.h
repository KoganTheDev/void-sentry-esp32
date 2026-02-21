/**
 * @file motion_data.h
 * @brief Encapsulates detected motion information in a clean, abstract way.
 */

#pragma once

#include <Arduino.h>

/**
 * @class MotionData
 * @brief Clean abstraction for motion detection results.
 * @details Separates motion detection logic from visualization.
 * Contains all information needed to draw a target overlay.
 */
class MotionData
{
private:
    bool _detected;    ///< Whether motion was detected
    int _centroid_x;   ///< X coordinate of motion center (0 if not detected)
    int _centroid_y;   ///< Y coordinate of motion center (0 if not detected)
    int _frame_width;  ///< Width of frame where motion was detected
    int _frame_height; ///< Height of frame where motion was detected
    int _pixel_count;  ///< Number of pixels that changed

public:
    /**
     * @brief Constructor: Create empty motion data (no motion detected).
     */
    MotionData() : _detected(false), _centroid_x(0), _centroid_y(0), _frame_width(0), _frame_height(0), _pixel_count(0)
    {
    }

    /**
     * @brief Constructor: Create motion data with values.
     * @param centroid_x X coordinate of detected motion
     * @param centroid_y Y coordinate of detected motion
     * @param frame_width Width of the analyzed frame
     * @param frame_height Height of the analyzed frame
     * @param pixel_count Number of pixels that changed
     */
    MotionData(int centroid_x, int centroid_y, int frame_width, int frame_height, int pixel_count)
        : _detected(true), _centroid_x(centroid_x), _centroid_y(centroid_y), _frame_width(frame_width),
          _frame_height(frame_height), _pixel_count(pixel_count)
    {
    }

    /**
     * @brief Check if motion was detected.
     * @return true if motion detected, false otherwise
     */
    bool is_detected() const { return _detected; }

    /**
     * @brief Get motion centroid X coordinate.
     * @return X pixel coordinate (0 if no motion)
     */
    int get_centroid_x() const { return _centroid_x; }

    /**
     * @brief Get motion centroid Y coordinate.
     * @return Y pixel coordinate (0 if no motion)
     */
    int get_centroid_y() const { return _centroid_y; }

    /**
     * @brief Get frame width.
     * @return Width in pixels
     */
    int get_frame_width() const { return _frame_width; }

    /**
     * @brief Get frame height.
     * @return Height in pixels
     */
    int get_frame_height() const { return _frame_height; }

    /**
     * @brief Get number of pixels that changed.
     * @return Pixel count
     */
    int get_pixel_count() const { return _pixel_count; }
};
/**
 * @class DetectionMetrics
 * @brief Metrics for evaluating motion detection algorithm performance.
 * @details Tracks timing, accuracy, and algorithm statistics for analysis.
 */
class DetectionMetrics
{
private:
    uint32_t _detection_time_ms;    ///< Time taken for motion detection (milliseconds)
    int _pixels_changed;            ///< Number of pixels that changed vs threshold
    float _motion_confidence;       ///< Confidence score (0.0-1.0) based on pixels changed
    int _consecutive_motion_frames; ///< Count of consecutive frames with motion
    int _consecutive_static_frames; ///< Count of consecutive frames without motion
    uint32_t _total_detections;     ///< Total detections since startup
    uint32_t _total_frames;         ///< Total frames processed
    uint32_t _last_frame_timestamp; ///< Timestamp of last frame processed
    uint32_t _average_fps;          ///< Average frames per second

public:
    DetectionMetrics()
        : _detection_time_ms(0), _pixels_changed(0), _motion_confidence(0.0f), _consecutive_motion_frames(0),
          _consecutive_static_frames(0), _total_detections(0), _total_frames(0), _last_frame_timestamp(0),
          _average_fps(0)
    {
    }

    // Setters for updating metrics
    void set_detection_time_ms(uint32_t time) { _detection_time_ms = time; }
    void set_pixels_changed(int count) { _pixels_changed = count; }
    void set_motion_confidence(float conf) { _motion_confidence = (conf < 0.0f) ? 0.0f : (conf > 1.0f) ? 1.0f : conf; }
    void set_consecutive_motion_frames(int count) { _consecutive_motion_frames = count; }
    void set_consecutive_static_frames(int count) { _consecutive_static_frames = count; }
    void set_total_detections(uint32_t count) { _total_detections = count; }
    void set_total_frames(uint32_t count) { _total_frames = count; }
    void set_last_frame_timestamp(uint32_t ts) { _last_frame_timestamp = ts; }
    void set_average_fps(uint32_t fps) { _average_fps = fps; }

    // Getters for retrieving metrics
    uint32_t get_detection_time_ms() const { return _detection_time_ms; }
    int get_pixels_changed() const { return _pixels_changed; }
    float get_motion_confidence() const { return _motion_confidence; }
    int get_consecutive_motion_frames() const { return _consecutive_motion_frames; }
    int get_consecutive_static_frames() const { return _consecutive_static_frames; }
    uint32_t get_total_detections() const { return _total_detections; }
    uint32_t get_total_frames() const { return _total_frames; }
    uint32_t get_last_frame_timestamp() const { return _last_frame_timestamp; }
    uint32_t get_average_fps() const { return _average_fps; }
};