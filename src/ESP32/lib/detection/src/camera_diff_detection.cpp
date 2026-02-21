#include "camera_diff_detection.h"
#include "motion_data.h"

#include <cstring>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <img_converters.h>

CameraDiffDetection::CameraDiffDetection()
    : _prev_frame(nullptr), _curr_frame(nullptr), _diff_buffer(nullptr), _rgb_buffer(nullptr), _jpeg_cache(nullptr),
      _jpeg_cache_len(0), _buffers_allocated(false), _first_frame(true), _rgb_buffer_width(0), _rgb_buffer_height(0),
      _last_centroid_x(0), _last_centroid_y(0), _has_valid_position(false), _consecutive_motion_count(0),
      _consecutive_static_count(0)
{
}

CameraDiffDetection::~CameraDiffDetection()
{
    if (this->_buffers_allocated)
    {
        if (this->_prev_frame)
            heap_caps_free(this->_prev_frame);
        if (this->_curr_frame)
            heap_caps_free(this->_curr_frame);
        if (this->_diff_buffer)
            heap_caps_free(this->_diff_buffer);
        if (this->_rgb_buffer)
            heap_caps_free(this->_rgb_buffer);
    }
    // Free cached JPEG
    if (this->_jpeg_cache)
    {
        free(this->_jpeg_cache);
        this->_jpeg_cache = nullptr;
        this->_jpeg_cache_len = 0;
    }
}

std::tuple<MoveDirectionX, MoveDirectionY> CameraDiffDetection::detect_object(camera_fb_t* frame)
{
    if (!frame)
    {
        return std::make_tuple(MoveDirectionX::None, MoveDirectionY::None);
    }

    // Track metrics: record start time and frame count
    uint32_t frame_start_time = millis();
    this->_current_metrics.set_total_frames(this->_current_metrics.get_total_frames() + 1);
    this->_current_metrics.set_last_frame_timestamp(frame_start_time);

    // === STEP 1: Allocate buffers on first frame ===
    if (!this->_buffers_allocated)
    {
        size_t buffer_size = frame->width * frame->height;

        this->_prev_frame = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        this->_curr_frame = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        this->_diff_buffer = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!this->_prev_frame || !this->_curr_frame || !this->_diff_buffer)
        {
            Serial.println("[DETECTION] Failed to allocate frame buffers");
            return std::make_tuple(MoveDirectionX::None, MoveDirectionY::None);
        }

        this->_buffers_allocated = true;
    }

    // === STEP 2: Decompress JPEG to greyscale for analysis AND RGB565 for overlay ===
    if (!jpeg_to_greyscale(frame, this->_curr_frame))
    {
        Serial.printf("[DETECTION] ERROR: Frame decompression failed (Free heap: %u bytes)\n",
                      esp_get_free_heap_size());
        return std::make_tuple(MoveDirectionX::None, MoveDirectionY::None);
    }

    // === STEP 3: Skip first frame (need previous frame to compare) ===
    if (this->_first_frame)
    {
        memcpy(this->_prev_frame, this->_curr_frame, frame->width * frame->height);
        this->_first_frame = false;
        return std::make_tuple(MoveDirectionX::None, MoveDirectionY::None);
    }

    // === STEP 4: Detect motion by comparing frames ===
    int motion_centroid_x = 0, motion_centroid_y = 0;
    int motion_pixel_count = 0;

    bool motion_found = find_motion(this->_prev_frame, this->_curr_frame, frame->width, frame->height,
                                    motion_centroid_x, motion_centroid_y, motion_pixel_count);

    // === STEP 5: Save current frame as previous for next iteration ===
    memcpy(this->_prev_frame, this->_curr_frame, frame->width * frame->height);

    if (motion_found)
    {
        this->_last_motion_data =
            MotionData(motion_centroid_x, motion_centroid_y, frame->width, frame->height, motion_pixel_count);

        // Update last known position
        this->_last_centroid_x = motion_centroid_x;
        this->_last_centroid_y = motion_centroid_y;
        this->_has_valid_position = true;

        // Update metrics: motion detected
        this->_consecutive_motion_count++;
        this->_consecutive_static_count = 0;
        this->_current_metrics.set_total_detections(this->_current_metrics.get_total_detections() + 1);
        this->_current_metrics.set_consecutive_motion_frames(this->_consecutive_motion_count);
        this->_current_metrics.set_consecutive_static_frames(0);

        // Calculate confidence (0.0-1.0 based on pixels changed relative to threshold)
        int motion_threshold = MOTION_THRESHOLD;
        float confidence = (float)motion_pixel_count / (float)motion_threshold;
        if (confidence > 1.0f)
            confidence = 1.0f;
        this->_current_metrics.set_motion_confidence(confidence);
        this->_current_metrics.set_pixels_changed(motion_pixel_count);
    } else
    {
        this->_last_motion_data = MotionData(); // Empty motion data (no motion)
        this->_consecutive_motion_count = 0;
        this->_consecutive_static_count++;
        this->_current_metrics.set_consecutive_motion_frames(0);
        this->_current_metrics.set_consecutive_static_frames(this->_consecutive_static_count);
        this->_current_metrics.set_motion_confidence(0.0f);
        this->_current_metrics.set_pixels_changed(0);
    }

    // Update detection time metric
    uint32_t detection_time = millis() - frame_start_time;
    this->_current_metrics.set_detection_time_ms(detection_time);

    // Calculate average FPS every 30 frames
    static uint32_t frame_count = 0;
    static uint32_t fps_timer = 0;
    frame_count++;
    if ((millis() - fps_timer) >= 1000)
    {
        this->_current_metrics.set_average_fps(frame_count);
        frame_count = 0;
        fps_timer = millis();
    }

    if (!motion_found)
    {
        return std::make_tuple(MoveDirectionX::None, MoveDirectionY::None);
    }

    // === STEP 8: Convert motion centroid to movement direction ===
    int center_x = frame->width / 2;
    int center_y = frame->height / 2;

    MoveDirectionX x_dir = MoveDirectionX::None;
    MoveDirectionY y_dir = MoveDirectionY::None;

    // X-axis: determine if motion is left or right of center (with deadzone)
    if (motion_centroid_x < center_x - CENTER_DEADZONE)
    {
        x_dir = MoveDirectionX::Left;
    } else if (motion_centroid_x > center_x + CENTER_DEADZONE)
    {
        x_dir = MoveDirectionX::Right;
    }

    // Y-axis: determine if motion is up or down from center (with deadzone)
    if (motion_centroid_y < center_y - CENTER_DEADZONE)
    {
        y_dir = MoveDirectionY::Up;
    } else if (motion_centroid_y > center_y + CENTER_DEADZONE)
    {
        y_dir = MoveDirectionY::Down;
    }

    return std::make_tuple(x_dir, y_dir);
}

bool CameraDiffDetection::find_motion(uint8_t* prev, uint8_t* curr, int width, int height, int& center_x, int& center_y,
                                      int& pixel_count)
{
    // === Step 1: Analyze frame differences ===
    int total_diff = 0;    // Sum of all pixel differences
    int weighted_x = 0;    // Sum of (x * pixel_difference)
    int weighted_y = 0;    // Sum of (y * pixel_difference)
    int motion_pixels = 0; // Count of pixels above threshold

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int idx = y * width + x;
            int diff = abs((int)curr[idx] - (int)prev[idx]);

            // Store for visualization if needed
            _diff_buffer[idx] = (diff > 255) ? 255 : (uint8_t)diff;

            // Count pixels that changed significantly
            if (diff > DIFF_THRESHOLD)
            {
                weighted_x += x * diff;
                weighted_y += y * diff;
                total_diff += diff;
                motion_pixels++;
            }
        }
    }

    // === Step 2: Check if enough pixels changed ===
    if (motion_pixels < MOTION_THRESHOLD)
    {
        pixel_count = motion_pixels;
        return false; // Not enough motion
    }

    // === Step 3: Calculate weighted centroid ===
    center_x = weighted_x / total_diff;
    center_y = weighted_y / total_diff;

    // === Step 4: Filter sensor noise (reject motion at image edges) ===
    // These regions often have artifacts and false positives
    int bottom_edge_y = height - (height / 10); // Bottom 10%
    int top_edge_y = height / 20;               // Top 5%
    int left_edge_x = width / 20;               // Left 5%
    int right_edge_x = width - (width / 20);    // Right 5%

    // Only reject if motion is ONLY in edge region with insufficient pixels
    bool is_in_bottom_edge = (center_y > bottom_edge_y);
    bool is_in_top_edge = (center_y < top_edge_y);
    bool is_in_left_edge = (center_x < left_edge_x);
    bool is_in_right_edge = (center_x > right_edge_x);

    if ((is_in_bottom_edge || is_in_top_edge || is_in_left_edge || is_in_right_edge) &&
        motion_pixels < (MOTION_THRESHOLD * 2))
    {
        pixel_count = motion_pixels;
        return false; // Reject edge noise
    }

    // === Step 5: Motion confirmed ===
    pixel_count = motion_pixels;
    Serial.printf("[CAMERA] Motion detected - Centroid: (%d, %d), Pixels: %d\n", center_x, center_y, motion_pixels);
    return true;
}
//*??
bool CameraDiffDetection::jpeg_to_greyscale(camera_fb_t* frame, uint8_t* output)
{
    if (!frame || !output)
        return false;

    // === Step 1: Allocate RGB buffer for JPEG decompression ===
    int w = frame->width;
    int h = frame->height;
    size_t rgb_size = (size_t)w * h * 2;

    // Check available memory before allocation
    uint32_t free_heap = esp_get_free_heap_size();
    if (free_heap < (rgb_size + 10000)) // Need buffer + 10KB margin
    {
        Serial.printf("[CAMERA] WARNING: Low memory (Free: %u bytes, Need: %u)\n", free_heap, (unsigned int)rgb_size);
        return false;
    }

    uint16_t* rgb_buf = (uint16_t*)heap_caps_malloc(rgb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!rgb_buf)
    {
        Serial.printf("[CAMERA] ERROR: Failed to allocate RGB buffer (Free: %u bytes, Need: %u)\n", free_heap,
                      (unsigned int)rgb_size);
        return false;
    }

    // === Step 2: Decompress JPEG to RGB565 ===
    bool decompress_ok = jpg2rgb565(frame->buf, frame->len, (uint8_t*)rgb_buf, JPG_SCALE_NONE);
    if (!decompress_ok)
    {
        Serial.println("[CAMERA] ERROR: JPEG decompression failed");
        heap_caps_free(rgb_buf);
        return false;
    }

    // === Step 4: Free temporary buffer immediately ===
    heap_caps_free(rgb_buf);
    rgb_buf = nullptr;
    return true;
}

MotionData CameraDiffDetection::get_motion_data() const { return this->_last_motion_data; }

DetectionMetrics CameraDiffDetection::get_detection_metrics() const { return this->_current_metrics; }
