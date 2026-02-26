#include "turret_server.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

#include "camera_diff_detection.h"
#include "esp_camera.h"
#include "motion_data.h"
#include <index_html.h>

#define _STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=123456789000000000000987654321"

static const char* TAG_HTTP = "HTTP_SERVER";
static const char* _STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t HttpServer::_server_handle = nullptr;

Camera* HttpServer::_camera_instance = nullptr;
BaseDetectionModule* HttpServer::_detection_instance = nullptr;

bool HttpServer::start(Camera* camera, BaseDetectionModule* detection)
{
    this->_camera_instance = camera;
    this->_detection_instance = detection;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

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

        Serial.println("[HTTP] Server started successfully");
        return true;
    }

    Serial.println("[HTTP] Failed to start server");
    return false;
}

void HttpServer::stop()
{
    ESP_LOGI(TAG_HTTP, "Stopping HTTP Server...");
    if (this->_server_handle)
    {
        ESP_LOGD(TAG_HTTP, "Calling httpd_stop()...");
        httpd_stop(this->_server_handle);
        this->_server_handle = NULL;
        ESP_LOGI(TAG_HTTP, "HTTP Server stopped successfully");
    } else
    {
        ESP_LOGW(TAG_HTTP, "HTTP Server was not running");
    }
}

esp_err_t HttpServer::index_handler(httpd_req_t* req) { return httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN); }

esp_err_t HttpServer::stream_handler(httpd_req_t* req)
{
    camera_fb_t* fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t* _jpg_buf = NULL;
    char part_buf[64];
    bool first_frame = true;

    // 1. Safety Check: Ensure camera and request are valid
    if (req == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (HttpServer::_camera_instance == nullptr)
    {
        Serial.println("Stream Error: Camera instance is null");
        return httpd_resp_send_500(req);
    }

    // 2. Set the HTTP Response Header to Multipart
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }

    Serial.println("[STREAM] Client connected to stream");

    // 3. Start the Infinite Streaming Loop
    uint32_t last_frame_time = millis();
    const uint32_t MIN_FRAME_INTERVAL = 33; // ~30 FPS max (33ms per frame)

    while (true)
    {
        // Limit frame rate to prevent blocking motion detection
        uint32_t now = millis();
        uint32_t elapsed = now - last_frame_time;
        if (elapsed < MIN_FRAME_INTERVAL)
        {
            vTaskDelay(1); // Yield to other tasks
            continue;
        }
        last_frame_time = now;

        // Grab frame - don't block, use GRAB_LATEST to skip frames if needed
        fb = HttpServer::_camera_instance->capture();
        if (!fb)
        {
            vTaskDelay(5);
            continue;
        }

        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;

        // Verify frame data is valid
        if (!_jpg_buf || _jpg_buf_len == 0)
        {
            HttpServer::_camera_instance->release(fb);
            fb = NULL;
            vTaskDelay(1);
            continue;
        }

        // Send the boundary separator
        // First frame boundary should not have leading \r\n
        if (first_frame)
        {
            res = httpd_resp_send_chunk(req, "--123456789000000000000987654321\r\n", 36);
            first_frame = false;
        } else
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }

        // Send the part header (Content-Type and Content-Length)
        if (res == ESP_OK)
        {
            size_t hlen = snprintf(part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char*)part_buf, hlen);
        }

        // Send the actual JPEG binary data (raw, no overlay)
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char*)_jpg_buf, _jpg_buf_len);
        }

        // 4. Release the frame buffer back to the camera driver
        if (fb)
        {
            HttpServer::_camera_instance->release(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }

        if (res != ESP_OK)
        {
            Serial.printf("[STREAM] Stream error: %s\n", esp_err_to_name(res));
            Serial.println("[STREAM] Client disconnected");
            break;
        }

        // Small delay to yield to other RTOS tasks (especially motion detection in main loop)
        vTaskDelay(1);
    }

    return res;
}

esp_err_t HttpServer::cmd_handler(httpd_req_t* req)
{
    ESP_LOGD(TAG_HTTP, "Command handler: Processing request");

    char buf[128];

    if (req == nullptr)
    {
        ESP_LOGE(TAG_HTTP, "Command handler: Request pointer is NULL!");
        return ESP_ERR_INVALID_ARG;
    }

    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    ESP_LOGD(TAG_HTTP, "Command handler: Query string length = %u", buf_len - 1);

    if (buf_len > 1)
    {
        esp_err_t err = httpd_req_get_url_query_str(req, buf, buf_len);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG_HTTP, "Command handler: Failed to get URL query string! Error: %s", esp_err_to_name(err));
            return httpd_resp_send(req, "ERROR: Invalid query", HTTPD_RESP_USE_STRLEN);
        }

        ESP_LOGD(TAG_HTTP, "Command handler: Query string = %s", buf);
        char param[32];

        // Extract "pan" value
        if (httpd_query_key_value(buf, "pan", param, sizeof(param)) == ESP_OK)
        {
            int pan_val = atoi(param);
            ESP_LOGI(TAG_HTTP, "COMMAND: Pan servo to angle %d", pan_val);
            // TODO: Call Servo/Motor function here: turret.movePan(pan_val);
        }

        // Extract "tilt" value
        if (httpd_query_key_value(buf, "tilt", param, sizeof(param)) == ESP_OK)
        {
            int tilt_val = atoi(param);
            ESP_LOGI(TAG_HTTP, "COMMAND: Tilt servo to angle %d", tilt_val);
            // TODO: Call Servo/Motor function here: turret.moveTilt(tilt_val);
        }
    }

    // Send a simple "OK" response back to the browser
    const char* resp_str = "Command Processed";
    esp_err_t resp_err = httpd_resp_send(req, resp_str, strlen(resp_str));
    if (resp_err != ESP_OK)
    {
        ESP_LOGW(TAG_HTTP, "Command handler: Failed to send response! Error: %s", esp_err_to_name(resp_err));
        return resp_err;
    }

    ESP_LOGD(TAG_HTTP, "Command handler: Response sent successfully");
    return ESP_OK;
}

esp_err_t HttpServer::websocket_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET)
    {
        // Initial WebSocket upgrade handshake
        ESP_LOGI(TAG_HTTP, "WebSocket: New connection established");
        return ESP_OK;
    }

    // WebSocket frame handler - keep connection alive
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // Use a timeout to wait for frames (1 second)
    // This keeps the handler alive instead of returning immediately
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 1000);

    // Handle timeout gracefully - just return ESP_OK to keep connection alive
    if (ret == ESP_ERR_TIMEOUT)
    {
        return ESP_OK;
    }

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG_HTTP, "WebSocket: Frame receive error: %s", esp_err_to_name(ret));
        return ret;
    }

    // Check for control frames (CLOSE, PING, PONG)
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        ESP_LOGD(TAG_HTTP, "WebSocket: Client requested close frame");
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_PING)
    {
        // Send PONG response
        httpd_ws_frame_t pong_frame = {.final = true,
                                       .fragmented = false,
                                       .type = HTTPD_WS_TYPE_PONG,
                                       .payload = ws_pkt.payload,
                                       .len = ws_pkt.len};
        httpd_ws_send_frame(req, &pong_frame);
        return ESP_OK;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_PONG)
    {
        // Ignore PONG frames
        return ESP_OK;
    }

    // TEXT frames - handle application data
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        if (ws_pkt.len > 0)
        {
            uint8_t* buf = (uint8_t*)malloc(ws_pkt.len + 1);
            if (buf)
            {
                ws_pkt.payload = buf;
                ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
                if (ret == ESP_OK)
                {
                    buf[ws_pkt.len] = '\0';
                    ESP_LOGD(TAG_HTTP, "WebSocket: Command received: %s", (const char*)buf);
                    // TODO: Parse and handle control commands
                }
                free(buf);
            }
        }
        return ESP_OK;
    }

    // Unknown frame type - just continue
    return ESP_OK;
}

void HttpServer::broadcast_ws_data(const char* json_str)
{
    if (json_str == nullptr || _server_handle == nullptr)
        return;

    size_t max_clients = 10;
    int client_fds[10];
    size_t actual_clients = max_clients;

    // Get the list of all open file descriptors
    esp_err_t err = httpd_get_client_list(_server_handle, &actual_clients, client_fds);
    if (err != ESP_OK)
        return;

    for (size_t i = 0; i < actual_clients; ++i)
    {
        // Double-check if this specific FD is actually a WebSocket
        if (httpd_ws_get_fd_info(_server_handle, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
        {
            httpd_ws_frame_t ws_pkt = {.final = true,
                                       .fragmented = false,
                                       .type = HTTPD_WS_TYPE_TEXT,
                                       .payload = (uint8_t*)json_str,
                                       .len = strlen(json_str)};
            // Async send is vital to keep the stream_handler from lagging
            httpd_ws_send_frame_async(_server_handle, client_fds[i], &ws_pkt);
        }
    }
}

void HttpServer::broadcastDetectionResults(const MotionData& motion, const DetectionMetrics& metrics)
{
    char json_buffer[512];

    int len = snprintf(json_buffer, sizeof(json_buffer),
                       "{"
                       "\"x\":%d,"
                       "\"y\":%d,"
                       "\"lock\":%s,"
                       "\"w\":%d,"
                       "\"h\":%d,"
                       "\"pixels\":%d,"
                       "\"fps\":%u,"
                       "\"conf\":%.2f,"
                       "\"time\":%u"
                       "}",
                       motion.get_centroid_x(), motion.get_centroid_y(), motion.is_detected() ? "true" : "false",
                       motion.get_frame_width(), motion.get_frame_height(), motion.get_pixel_count(),
                       metrics.get_average_fps(), metrics.get_motion_confidence(), metrics.get_detection_time_ms());

    if (len > 0 && len < (int)sizeof(json_buffer))
    {
        Serial.printf("[DETECTION] Broadcasting: %s\n", json_buffer);
        HttpServer::broadcast_ws_data(json_buffer);
    } else
    {
        Serial.printf("[DETECTION] ERROR: JSON buffer overflow or error (len=%d)\n", len);
    }
}
