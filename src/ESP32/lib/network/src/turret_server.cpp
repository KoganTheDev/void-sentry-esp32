#include "turret_server.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

#include "camera_diff_detection.h"
#include "esp_camera.h"
#include "motion_data.h"
#include "websockets.h"
#include <index_html.h>

#define _STREAM_CONTENT_TYPE "multipart/x-mixed-replace;boundary=123456789000000000000987654321"

static const char* TAG_HTTP = "HTTP_SERVER";
static const char* _STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t HttpServer::_server_handle = nullptr;

Camera* HttpServer::_camera_instance = nullptr;
BaseDetectionModule* HttpServer::_detection_instance = nullptr;

HttpServer::HttpServer() : _stream_handler(NULL) {}

HttpServer::~HttpServer()
{
    if (this->_stream_handler != NULL)
    {
        delete this->_stream_handler;
    }
}

bool HttpServer::start(Camera* camera, BaseDetectionModule* detection)
{
    this->_camera_instance = camera;
    this->_detection_instance = detection;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;       // Increase stack for JSON/String handling
    config.max_uri_handlers = 12;   // Just to be safe
    config.lru_purge_enable = true; // Purge oldest connection if full

    if (httpd_start(&this->_server_handle, &config) == ESP_OK)
    {
        // Define Root Route
        httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = NULL};
        httpd_register_uri_handler(this->_server_handle, &index_uri);

        this->_stream_handler = new StreamWebsocketHandler(this->_server_handle, "/stream", *camera);
        this->_stream_handler->register_endpoint();

        /*
        // Define the Video Stream Route
        httpd_uri_t stream_uri = {.uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL};
        httpd_register_uri_handler(this->_server_handle, &stream_uri);

        // Define the Motor Command Route
        httpd_uri_t cmd_uri = {.uri = "/move", .method = HTTP_GET, .handler = cmd_handler, .user_ctx = NULL};
        httpd_register_uri_handler(this->_server_handle, &cmd_uri);

        // Define WS route
        httpd_uri_t ws_uri = {
            .uri = "/ws", .method = HTTP_GET, .handler = websocket_handler, .user_ctx = NULL, .is_websocket = true};
        httpd_register_uri_handler(this->_server_handle, &ws_uri);
        */
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
    if (req->method == HTTP_GET)
    {
        // Initial WebSocket upgrade handshake
        ESP_LOGI(TAG_HTTP, "WebSocket: New connection established");
        return ESP_OK;
    }

    // WebSocket frame handler - keep connection alive
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // get packet length
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG_HTTP, "WebSocket: Frame receive error: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len <= 0)
    {
        return ESP_OK;
    }

    uint8_t* buf = (uint8_t*)malloc(ws_pkt.len + 1);
    if (!buf)
    {
        return ESP_OK;
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
    {
        free(buf);
        return ret;
    }

    buf[ws_pkt.len] = '\0';
    ESP_LOGD(TAG_HTTP, "WebSocket: Command received: %s", (const char*)buf);
    // TODO: Parse and handle control commands
    httpd_ws_frame_t response_pkt = {0};

    camera_fb_t* fb = HttpServer::_camera_instance->capture();

    if (!fb)
    {
        free(buf);
        ESP_LOGE(TAG_HTTP, "Frame buffer is NULL");
        return ESP_OK; // TODO: maybe later change to ESP_FAIL
    }

    response_pkt.payload = (uint8_t*)fb->buf;
    response_pkt.len = fb->len;
    response_pkt.type = HTTPD_WS_TYPE_BINARY;

    ret = httpd_ws_send_frame(req, &response_pkt);
    if (ret != ESP_OK)
    {
        free(buf);
        return ret;
    }

    HttpServer::_camera_instance->release(fb);

    free(buf);
    return ESP_OK;
}
