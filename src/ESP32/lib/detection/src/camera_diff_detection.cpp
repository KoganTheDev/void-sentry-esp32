#include "camera_diff_detection.h"
#include "motion_data.h"

#include <cstring>
#include <esp_camera.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <img_converters.h>

static const char* TAG = "DETECTION";

CameraDiffDetection::CameraDiffDetection()
    : _prev_frame(nullptr), _curr_frame(nullptr), _diff_buffer(nullptr),
      _buffers_allocated(false), _first_frame(true), _last_centroid_x(0), _last_centroid_y(0)
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
    }
}

std::tuple<MoveX, MoveY> CameraDiffDetection::detect_object(camera_fb_t* frame)
{
    if (!frame)
    {
        ESP_LOGW(TAG, "detect_object called with NULL frame");
        return std::make_tuple(MoveX::None, MoveY::None);
    }

    if (frame->buf == NULL)
    {
        ESP_LOGE(TAG, "ERROR: Frame buffer pointer is NULL!");
        return std::make_tuple(MoveX::None, MoveY::None);
    }

    // Log frame details for debugging
    // ESP_LOGD(TAG, "detect_object: Frame received - Size:%u bytes, Width:%d, Height:%d, Format:%d", frame->len,
    //        frame->width, frame->height, frame->format);

    // Track metrics: record start time and frame count
    uint32_t frame_start_time = millis();
    this->_current_metrics.set_total_frames(this->_current_metrics.get_total_frames() + 1);
    this->_current_metrics.set_last_frame_timestamp(frame_start_time);

    // === STEP 1: Allocate buffers on first frame ===
    if (!this->_buffers_allocated)
    {
        size_t buffer_size = frame->width * frame->height;
        ESP_LOGI(TAG, "First frame detected - Allocating buffers: %u bytes each", buffer_size);

        this->_prev_frame = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        this->_curr_frame = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        this->_diff_buffer = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!this->_prev_frame || !this->_curr_frame || !this->_diff_buffer)
        {
            ESP_LOGE(TAG, "CRITICAL: Failed to allocate frame buffers! prev:%p curr:%p diff:%p", this->_prev_frame,
                     this->_curr_frame, this->_diff_buffer);
            Serial.println("[DETECTION] Failed to allocate frame buffers");
            return std::make_tuple(MoveX::None, MoveY::None);
        }

        this->_buffers_allocated = true;
        ESP_LOGI(TAG, "Buffers allocated successfully");
    }

    // === STEP 2: Decompress JPEG to greyscale for analysis AND RGB565 for overlay ===
    // ESP_LOGD(TAG, "Starting JPEG decompression...");
    if (!jpeg_to_greyscale(frame, this->_curr_frame))
    {
        ESP_LOGE(TAG, "Frame decompression failed (Free INTERNAL heap: %u bytes, Free PSRAM: %u bytes)",
                 esp_get_free_heap_size(), ESP.getFreePsram());
        return std::make_tuple(MoveX::None, MoveY::None);
    }
    // ESP_LOGD(TAG, "JPEG decompression successful");

    // === STEP 3: Skip first frame (need previous frame to compare) ===
    if (this->_first_frame)
    {
        memcpy(this->_prev_frame, this->_curr_frame, frame->width * frame->height);
        this->_first_frame = false;
        return std::make_tuple(MoveX::None, MoveY::None);
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

        // Update metrics: motion detected

        this->_current_metrics.set_total_detections(this->_current_metrics.get_total_detections() + 1);
        this->_current_metrics.set_consecutive_static_frames(0);

        // Calculate confidence (0.0-1.0 based on pixels changed relative to threshold)
        float confidence = (float)motion_pixel_count / (float)MOTION_THRESHOLD;
        if (confidence > 1.0f)
        {
            confidence = 1.0f;
        }
        this->_current_metrics.set_motion_confidence(confidence);
        this->_current_metrics.set_pixels_changed(motion_pixel_count);
    } else
    {
        this->_last_motion_data = MotionData(); // Empty motion data (no motion)
        this->_current_metrics.set_consecutive_motion_frames(0);
        this->_current_metrics.set_motion_confidence(0.0f);
        this->_current_metrics.set_pixels_changed(0);
    }

    // Update detection time metric
    uint32_t detection_time = millis() - frame_start_time;
    this->_current_metrics.set_detection_time_ms(detection_time);

    calculate_fps();

    if (!motion_found)
    {
        return std::make_tuple(MoveX::None, MoveY::None);
    }

    // === STEP 8: Convert motion centroid to movement direction ===
    int center_x = frame->width / 2;
    int center_y = frame->height / 2;

    MoveX x_dir = MoveX::None;
    MoveY y_dir = MoveY::None;

    // X-axis: determine if motion is left or right of center (with deadzone)
    if (motion_centroid_x < center_x - CENTER_DEADZONE)
    {
        x_dir = MoveX::Left;
    } else if (motion_centroid_x > center_x + CENTER_DEADZONE)
    {
        x_dir = MoveX::Right;
    }

    // Y-axis: determine if motion is up or down from center (with deadzone)
    if (motion_centroid_y < center_y - CENTER_DEADZONE)
    {
        y_dir = MoveY::Up;
    } else if (motion_centroid_y > center_y + CENTER_DEADZONE)
    {
        y_dir = MoveY::Down;
    }

    ESP_LOGD(TAG, "moving: x: [%s] y: [%s]", x_dir.to_string(), y_dir.to_string());

    return std::make_tuple(x_dir, y_dir);
}

void CameraDiffDetection::calculate_fps()
{
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
}


bool CameraDiffDetection::find_motion(uint8_t* prev, uint8_t* curr, int width, int height, int& center_x, int& center_y,
                                      int& pixel_count)
{
    uint64_t total_diff = 0;
    uint64_t weighted_x = 0;
    uint64_t weighted_y = 0;
    int motion_pixels = 0;

    // PERFORMANCE GAIN: stride of 4 = 1/16th of the work.
    const int stride = 4;

    for (int y = 0; y < height; y += stride)
    {
        for (int x = 0; x < width; x += stride)
        {
            int idx = y * width + x;
            int diff = abs((int)curr[idx] - (int)prev[idx]);

            if (diff > DIFF_THRESHOLD)
            {
                weighted_x += (uint64_t)x * diff;
                weighted_y += (uint64_t)y * diff;
                total_diff += diff;
                motion_pixels++;
            }
        }
    }

    // Update pixel count (scaled back up to represent the full frame size roughly)
    pixel_count = motion_pixels * (stride * stride);

    // If motion_pixels is too low, stop here.
    if (pixel_count < MOTION_THRESHOLD)
    {
        return false;
    }

    if (total_diff > 0)
    {
        center_x = (int)(weighted_x / total_diff);
        center_y = (int)(weighted_y / total_diff);
    }

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
    // Serial.printf("[CAMERA] Motion detected - Centroid: (%d, %d), Pixels: %d\n", center_x, center_y, motion_pixels);
    return true;
}

bool CameraDiffDetection::jpeg_to_greyscale(camera_fb_t* frame, uint8_t* output)
{
    if (!frame || !output)
        return false;

    size_t pixel_count = frame->width * frame->height;
    size_t rgb_size = pixel_count * 3;

    // Allocate temp RGB buffer in PSRAM
    uint8_t* rgb_tmp = (uint8_t*)heap_caps_malloc(rgb_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rgb_tmp)
        return false;

    // Decompress JPEG directly to RGB888
    if (!fmt2rgb888(frame->buf, frame->len, PIXFORMAT_JPEG, rgb_tmp))
    {
        heap_caps_free(rgb_tmp);
        return false;
    }

    // SPEED FIX: Use the Green channel as a high-speed proxy for Grayscale
    // Pointer arithmetic is faster than index [i*3+1]
    uint8_t* src = rgb_tmp + 1; // Point to first Green byte
    uint8_t* dst = output;

    for (size_t i = 0; i < pixel_count; ++i)
    {
        *dst++ = *src;
        src += 3; // Move to next Green byte
    }

    heap_caps_free(rgb_tmp);
    return true;
}
