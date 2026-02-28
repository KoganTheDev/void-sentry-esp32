#include "turret_server.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

#include "camera_diff_detection.h"
#include "esp_camera.h"
#include "motion_data.h"
#include "websockets.h"
#include <index_html.h>

static const char* TAG_HTTP = "HTTP_SERVER";

httpd_handle_t HttpServer::_server_handle = nullptr;

Camera* HttpServer::_camera_instance = nullptr;
BaseDetectionModule* HttpServer::_detection_instance = nullptr;

HttpServer::HttpServer() : _stream_handler(NULL), _commands_handler(NULL) {}

HttpServer::~HttpServer()
{
    if (this->_stream_handler != NULL)
    {
        delete this->_stream_handler;
    }

    if (this->_commands_handler != NULL)
    {
        delete this->_commands_handler;
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

        this->_commands_handler = new CommandsWebSocketHandler(this->_server_handle, "/commands");
        this->_commands_handler->register_endpoint();

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
