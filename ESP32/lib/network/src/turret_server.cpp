#include "turret_server.h"
#include <Arduino.h>
#include <cstdio>
#include <esp_log.h>

#include "camera_diff_detection.h"
#include "commands_handler.h"
#include "esp_camera.h"
#include "index_html.h"
#include "wifi_http.h"
#include "wifi_manager.h"

static const char* TAG_HTTP = "HTTP_SERVER";

httpd_handle_t HttpServer::_server_handle = nullptr;

httpd_handle_t HttpServer::_stream_handle = nullptr;

BaseDetectionModule* HttpServer::_detection_instance = nullptr;

const BaseStatusProvider* HttpServer::_status_instance = nullptr;

HttpServer::HttpServer() : _commands_handler(NULL) {}

HttpServer::~HttpServer()
{
    if (this->_commands_handler != NULL)
    {
        delete this->_commands_handler;
    }
}

bool HttpServer::start(FrameStore& store, BaseDetectionModule* detection, const BaseStatusProvider* status,
                       remote::BaseRemoteSink* remote, WifiManager* wifi)
{
    this->_detection_instance = detection;
    this->_status_instance = status;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Keep network work on core 0 so it cannot preempt detection on core 1.
    config.core_id = 0;
    // 8 KB, double the default: handlers keep their buffers on this stack (scan list and escaped SSIDs in
    // wifi_http.cpp, the hud::kReplySize JSON reply in commands_handler.cpp, form bodies).
    config.stack_size = 8192;
    config.max_uri_handlers = 16; // "/" and /commands, 6 WiFi and 2 HUD login routes, the catch-all; headroom
    config.uri_match_fn = httpd_uri_match_wildcard; // the WiFi catch-all "/*"; exact routes still match exactly
    config.send_wait_timeout = 2;                   // seconds; stalled viewers would block httpd 5 s
    config.lru_purge_enable = true;
    config.max_open_sockets = 6; // /stream lives on its own server; lwIP has 16 sockets for both servers

    if (httpd_start(&this->_server_handle, &config) == ESP_OK)
    {
        httpd_uri_t index_uri = {.uri = "/", .method = HTTP_GET, .handler = index_handler, .user_ctx = wifi};
        httpd_register_uri_handler(this->_server_handle, &index_uri);

        // /stream gets its own server and task: a video send that waits for a TCP retransmit (notes s13) then only
        // freezes the video, not the /commands replies. The 4 s send timeout lets a double retransmit finish the
        // frame instead of cutting it mid-send and dropping the viewer.
        httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
        stream_config.server_port = 81;
        stream_config.ctrl_port = 32769; // the main instance uses the default 32768
        stream_config.core_id = 0;
        stream_config.stack_size = 4096;
        stream_config.max_open_sockets = 3; // 2 viewers + 1 so a third can be accepted and refused
        stream_config.max_uri_handlers = 2;
        stream_config.lru_purge_enable = true;
        stream_config.send_wait_timeout = 4;
        stream_config.close_fn = StreamBroadcaster::on_session_closed; // Forget /stream viewers; must close the socket

        if (httpd_start(&this->_stream_handle, &stream_config) != ESP_OK)
        {
            ESP_LOGE(TAG_HTTP, "Failed to start the /stream server");
            this->_stream_handle = NULL;
            httpd_stop(this->_server_handle); // no half-started server
            this->_server_handle = NULL;
            return false;
        }
        if (!this->_broadcaster.start(this->_stream_handle, store))
        {
            ESP_LOGE(TAG_HTTP, "Failed to start the /stream broadcaster");
            this->stop();
            return false;
        }

        this->_commands_handler = new CommandsWebSocketHandler(this->_server_handle, "/commands", detection, status,
                                                               remote, &this->_broadcaster, wifi);
        this->_commands_handler->register_endpoint();

        if (wifi != nullptr)
        {
            register_wifi_routes(this->_server_handle, *wifi); // last: it registers the catch-all
        }

        ESP_LOGI(TAG_HTTP, "Server started successfully");
        return true;
    }

    ESP_LOGE(TAG_HTTP, "Failed to start server");
    return false;
}

void HttpServer::stop()
{
    ESP_LOGI(TAG_HTTP, "Stopping HTTP Server...");
    this->_broadcaster.stop(); // before httpd_stop: the stream task must not touch a dead server
    if (this->_stream_handle)
    {
        httpd_stop(this->_stream_handle);
        this->_stream_handle = NULL;
    }
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

/**
 * @brief HTTP GET handler for the root ("/") URI.
 * This function serves the primary web interface of the turret. To optimize
 * memory usage and transmission speed, it sends a Gzip-compressed HTML payload.
 * @param[in] req Pointer to the HTTP request context provided by the ESP-IDF server.
 * @return
 * - ESP_OK: The index page was sent successfully.
 * - ESP_FAIL: There was an error sending the response.
 * - ESP_ERR_HTTPD_RESP_SEND: Specifically indicates a failure in the response send call.
 */
esp_err_t HttpServer::index_handler(httpd_req_t* req)
{
    const WifiManager* wifi = static_cast<const WifiManager*>(req->user_ctx);
    if (wifi != nullptr && wifi->setup_mode())
    {
        // Setup mode: whatever hostname a phone opened, the useful page is the WiFi setup page, not the HUD.
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/wifi");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        return httpd_resp_send(req, nullptr, 0);
    }
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    // Forbid framing by any site (clickjacking: a hidden HUD under a decoy page could be clicked to move the turret).
    // No script-src: the page uses inline scripts.
    httpd_resp_set_hdr(req, "Content-Security-Policy", "frame-ancestors 'none'");
    httpd_resp_set_hdr(req, "X-Frame-Options", "DENY");

    return httpd_resp_send(req, (const char*)HTML_PAGE_GZ, HTML_PAGE_GZ_LEN);
}