/**
 * @file HttpServer.h
 * @brief Web server interface for MJPEG streaming and remote command handling.
 */

#pragma once

#include "base_detection_module.h"
#include "camera.h"
#include "motion_data.h"
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
     * @brief Static reference to the camera for use in C-style callbacks.
     */
    static Camera* _camera_instance;

    /**
     * @brief Static reference to motion detection module for drawing targets.
     */
    static BaseDetectionModule* _detection_instance;

public:
    /**
     * @brief Configures and launches the web server on port 80.
     * @param camera Pointer to the initialized Camera object for streaming.
     * @param detection Optional pointer to detection module for motion visualization.
     * @return true if the server was created and handlers were registered.
     */
    bool start(Camera* camera, BaseDetectionModule* detection = nullptr);

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

    /**
     * @brief HTTP GET Handler for the MJPEG stream.
     * @details Continuously streams RGB565 frames from the detection module
     * with motion detection overlay visualization. Each frame is sent with
     * multipart boundary markers.
     * @param req Pointer to the HTTP request structure.
     * @return esp_err_t ESP_OK on success.
     */
    static esp_err_t stream_handler(httpd_req_t* req);

    /**
     * @brief HTTP GET Handler for control commands.
     * @details Parses URL parameters (e.g., ?pan=90&tilt=45) to drive motors.
     * @param req Pointer to the HTTP request structure.
     * @return esp_err_t ESP_OK on success.
     */
    static esp_err_t cmd_handler(httpd_req_t* req);

    /**
     * @brief HTTP GET Handler for motion detection data as JSON.
     * @details Returns detection coordinates, motion metrics, and algorithm statistics
     * that can be used to render overlay and display algorithm performance.
     * @param req Pointer to the HTTP request structure.
     * @return esp_err_t ESP_OK on success.
     */
    static esp_err_t detection_handler(httpd_req_t* req); //TODO: Replace this function with the websockets

    static esp_err_t websocket_handler(httpd_req_t* req);

    static void broadcast_ws_data(const char* json_str);

    static void broadcastDetectionResults(const MotionData& motion, const DetectionMetrics& metrics);
};
