#include "turret_server.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

#include "camera_diff_detection.h"
#include "esp_camera.h"
#include "motion_data.h"
#include <index_html.h>

#define _STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=123456789000000000000987654321"

static const char* _STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

Camera* HttpServer::_camera_instance = nullptr;
BaseDetectionModule* HttpServer::_detection_instance = nullptr;

bool HttpServer::start(Camera* camera, BaseDetectionModule* detection)
{
    this->_camera_instance = camera;
    this->_detection_instance = detection;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // 1. Increase the number of allowed simultaneous connections
    config.max_open_sockets = 10; 
    
    // 2. IMPORTANT: Enable purging of old connections
    // This allows the server to close an idle connection to make room for your JSON request
    config.lru_purge_enable = true; 

    // 3. Optional: Increase stack size if you see crashes during JSON processing
    config.stack_size = 8192;


    if (httpd_start(&this->_server_handle, &config) == ESP_OK)
    {
        // Define Root Route
        httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
        httpd_register_uri_handler(this->_server_handle, &index_uri);

        // Define the Video Stream Route
        httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};
        httpd_register_uri_handler(this->_server_handle, &stream_uri);

        // Define the Motor Command Route
        httpd_uri_t cmd_uri = {.uri = "/move", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL};
        httpd_register_uri_handler(this->_server_handle, &cmd_uri);

        // Define the Detection Data Route (motion overlay coordinates + metrics)
        httpd_uri_t detection_uri = {
            .uri = "/detection", .method = HTTP_GET, .handler = detection_handler, .user_ctx = NULL};
        esp_err_t reg_res = httpd_register_uri_handler(this->_server_handle, &detection_uri);
        Serial.printf("[SERVER] Registered /detection endpoint - result: %d\n", reg_res);

        return true;
    }
    return false;
}

void HttpServer::stop()
{
    if (this->_server_handle)
    {
        httpd_stop(this->_server_handle);
        this->_server_handle = NULL;
    }
}

esp_err_t HttpServer::index_handler(httpd_req_t* req) { return httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN); }

esp_err_t HttpServer::stream_handler(httpd_req_t* req)
{
    esp_err_t res = ESP_OK;
    char* part_buf[64];

    // 1. Safety Check: Ensure detection module and request are valid
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (HttpServer::_camera_instance == nullptr)
    {
        Serial.println("[STREAM] Error: Camera instance is null");
        return httpd_resp_send_500(req);
    }

    Serial.println("[STREAM] Client connected - streaming clean JPEG from camera");

    // 2. Set the HTTP Response Header to Multipart JPEG (MJPEG)
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        Serial.printf("[STREAM] Error setting response type: %s\n", esp_err_to_name(res));
        return res;
    }

    // 3. Start the Infinite Streaming Loop
    uint32_t last_frame_time = millis();
    const uint32_t MIN_FRAME_INTERVAL = 50; // 20 FPS target (50ms per frame)
    int frames_sent = 0;
    int motion_frames = 0;

    while (true)
    {
        // Limit frame rate to prevent blocking motion detection
        uint32_t now = millis();
        uint32_t elapsed = now - last_frame_time;
        if (elapsed < MIN_FRAME_INTERVAL)
        {
            vTaskDelay(2);
            continue;
        }
        last_frame_time = now;

        // Capture frame from camera (JPEG format, ready to stream)
        camera_fb_t* fb = HttpServer::_camera_instance->capture();
        if (!fb)
        {
            // Frame capture failed, wait briefly and retry
            vTaskDelay(5);
            continue;
        }

        // Try to get overlay JPEG if available (non-blocking, low priority)
        uint8_t* jpeg_buf = (uint8_t*)fb->buf;
        size_t jpeg_len = fb->len;
        uint8_t* overlay_jpeg = nullptr;
        size_t overlay_len = 0;

        // Attempt to get overlay version without blocking
        if (HttpServer::_detection_instance &&
            HttpServer::_detection_instance->get_jpeg_with_overlay(&overlay_jpeg, &overlay_len, 0) && overlay_jpeg &&
            overlay_len > 0 && overlay_len < 200000) // Sanity check size
        {
            // Use overlay JPEG if it's smaller than camera+overhead and seems valid
            jpeg_buf = overlay_jpeg;
            jpeg_len = overlay_len;
            motion_frames++;
        }

        // Send the boundary separator
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK)
        {
            Serial.printf("[STREAM] Error sending boundary\n");
            HttpServer::_camera_instance->release(fb);
            break;
        }

        // Send the part header (Content-Type: JPEG, Content-Length)
        size_t hlen = snprintf((char*)part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                               (unsigned int)jpeg_len);
        res = httpd_resp_send_chunk(req, (const char*)part_buf, hlen);
        if (res != ESP_OK)
        {
            Serial.printf("[STREAM] Error sending header\n");
            HttpServer::_camera_instance->release(fb);
            break;
        }

        // Send the JPEG frame buffer
        res = httpd_resp_send_chunk(req, (const char*)jpeg_buf, jpeg_len);
        if (res != ESP_OK)
        {
            Serial.printf("[STREAM] Error sending frame data\n");
            HttpServer::_camera_instance->release(fb);
            break;
        }

        HttpServer::_camera_instance->release(fb);

        frames_sent++;
        if (frames_sent % 20 == 0)
        {
            Serial.printf("[STREAM] Sent %d frames (%d with overlay)\n", frames_sent, motion_frames);
        }

        vTaskDelay(1);
    }

    Serial.printf("[STREAM] Client disconnected after %d frames (%d with overlay)\n", frames_sent, motion_frames);
    return res;
}

esp_err_t HttpServer::cmd_handler(httpd_req_t* req)
{
    char buf[128];

    if (req == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    size_t buf_len = httpd_req_get_url_query_len(req) + 1;

    if (buf_len > 1)
    {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            char param[32];

            // Extract "pan" value
            if (httpd_query_key_value(buf, "pan", param, sizeof(param)) == ESP_OK)
            {
                int pan_val = atoi(param);
                Serial.printf("Command Received: Pan to %d\n", pan_val);
                // TODO: Call Servo/Motor function here: turret.movePan(pan_val);
            }

            // Extract "tilt" value
            if (httpd_query_key_value(buf, "tilt", param, sizeof(param)) == ESP_OK)
            {
                int tilt_val = atoi(param);
                Serial.printf("Command Received: Tilt to %d\n", tilt_val);
                // TODO: Call Servo/Motor function here: turret.moveTilt(tilt_val);
            }
        }
    }

    // Send a simple "OK" response back to the browser
    const char* resp_str = "Command Processed";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

esp_err_t HttpServer::detection_handler(httpd_req_t* req)
{
    static int call_count = 0;
    if (++call_count % 20 == 0)
    {
        Serial.println("[DETECTION_HANDLER] Called!");
    }
    esp_err_t res = ESP_OK;

    // Safety check
    if (HttpServer::_detection_instance == nullptr || HttpServer::_camera_instance == nullptr)
    {
        const char* error_json = "{\"error\": \"Detection module not initialized\"}";
        httpd_resp_set_type(req, "application/json");
        Serial.println("[DETECTION_HANDLER] Error: Detection module is NULL");
        return httpd_resp_send(req, error_json, strlen(error_json));
    }

    // Set response type to JSON
    httpd_resp_set_type(req, "application/json");

    // Get motion data from detection module
    CameraDiffDetection* detector = reinterpret_cast<CameraDiffDetection*>(HttpServer::_detection_instance);

    MotionData motion = detector->get_motion_data();
    DetectionMetrics metrics = detector->get_detection_metrics();

    // Use actual frame dimensions for overlay - QVGA is 320x240
    // These are required even when no motion is detected so canvas can properly scale overlay
    int frame_width = motion.get_frame_width();
    int frame_height = motion.get_frame_height();

    // If dimensions are 0 (no motion detected yet), use default QVGA dimensions
    if (frame_width == 0 || frame_height == 0)
    {
        frame_width = 320;
        frame_height = 240;
    }

    // Log detection data (only every 20 calls to reduce spam)
    if (call_count % 20 == 0)
    {
        Serial.printf("[DETECTION_HANDLER] Motion: %s, X: %d, Y: %d, Frame: %dx%d, Pixels: %d, Detections: %u\n",
                      motion.is_detected() ? "true" : "false", motion.get_centroid_x(), motion.get_centroid_y(),
                      frame_width, frame_height, metrics.get_pixels_changed(), metrics.get_total_detections());
    }

    // Build JSON response
    char buffer[512];
    int len = snprintf(buffer, sizeof(buffer),
                       "{"
                       "\"motion_detected\":%s,"
                       "\"centroid_x\":%d,"
                       "\"centroid_y\":%d,"
                       "\"frame_width\":%d,"
                       "\"frame_height\":%d,"
                       "\"pixels_changed\":%d,"
                       "\"confidence\":%.2f,"
                       "\"detection_time_ms\":%u,"
                       "\"consecutive_motion_frames\":%d,"
                       "\"consecutive_static_frames\":%d,"
                       "\"total_detections\":%u,"
                       "\"total_frames\":%u,"
                       "\"average_fps\":%u"
                       "}",
                       motion.is_detected() ? "true" : "false", motion.get_centroid_x(), motion.get_centroid_y(),
                       frame_width, frame_height, metrics.get_pixels_changed(), metrics.get_motion_confidence(),
                       metrics.get_detection_time_ms(), metrics.get_consecutive_motion_frames(),
                       metrics.get_consecutive_static_frames(), metrics.get_total_detections(),
                       metrics.get_total_frames(), metrics.get_average_fps());

    if (len > 0 && len < (int)sizeof(buffer))
    {
        res = httpd_resp_send(req, buffer, len);
    } else
    {
        const char* error = "{\"error\": \"JSON buffer overflow\"}";
        res = httpd_resp_send(req, error, strlen(error));
        Serial.println("[DETECTION_HANDLER] Buffer overflow!");
    }

    return res;
}
