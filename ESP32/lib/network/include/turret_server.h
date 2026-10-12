/**
 * @file HttpServer.h
 * @brief Web server interface for MJPEG streaming and remote command handling.
 */

#pragma once

#include "base_detection_module.h"
#include "frame_store.h"
#include "motion_data.h"
#include "stream_broadcaster.h"
#include "websockets.h"
#include <esp_http_server.h>

/**
 * @class HttpServer
 * @brief Manages the ESP32-CAM web interface and control API.
 * @details This class wraps the ESP-IDF HTTP server component. It provides
 * a video stream endpoint and a command endpoint for remote movement.
 */
class HttpServer
{
private:
    static httpd_handle_t _server_handle; ///< Internal handle for the ESP-IDF server instance.

    /**
     * @brief Static reference to motion detection module for drawing targets.
     */
    static BaseDetectionModule* _detection_instance;

    CommandsWebSocketHandler* _commands_handler; // Used both for commands to the motors & sending metrics
    StreamBroadcaster _broadcaster;              // Pushes frames from the FrameStore to /stream viewers

public:
    HttpServer();
    ~HttpServer();

    /**
     * @brief Configures and launches the web server on port 80.
     * @param store FrameStore the /stream endpoint reads frames from. Must outlive the server.
     * @param detection Optional pointer to detection module for motion visualization.
     * @return true if the server was created and handlers were registered.
     */
    bool start(FrameStore& store, BaseDetectionModule* detection = nullptr);

    /** @brief Frames successfully sent to /stream viewers since boot. */
    uint32_t frames_sent() const { return _broadcaster.frames_sent(); }

    /** @brief Number of currently connected /stream viewers. */
    size_t viewer_count() { return _broadcaster.viewer_count(); }

    /**
     * @brief Shut down the server and unregister all URI handlers.
     */
    void stop();

    /**
     * @brief HTTP handler for the root ("/") URI.
     * This method is called by the ESP HTTP server whenever a client accesses the
     * root IP address. It serves the primary web interface stored in Flash
     * memory.
     * @param req Pointer to the HTTP request object containing session data.
     * @return esp_err_t Returns ESP_OK if the response was sent successfully,
     * otherwise returns an error code from the underlying HTTP server.
     * @see index_html.h
     */
    static esp_err_t index_handler(httpd_req_t* req);
};
