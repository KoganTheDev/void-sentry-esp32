#pragma once

// TODO: Change name to the header file and split to cpp file as well
#include "base_detection_module.h"
#include "camera.h"
#include "control_status.h"
#include "track_snapshot.h"
#include "tracking_types.h"
#include <cstdio>
#include <cstring>
#include <esp_http_server.h>
#include <esp_log.h>

static const char* TAG_WEBSOCKETS = "WEBSOCKETS";

class WebsocketHandler
{
public:
    WebsocketHandler(httpd_handle_t server_handle, const char* uri)
        : _server_handle(server_handle), _uri(uri), _is_registered(false)
    {
    }

    virtual ~WebsocketHandler() { this->unregister_endpoint(); }

    void register_endpoint()
    {
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Register endpoint %s", this->_uri);
        httpd_uri_t uri_obj = {0};
        uri_obj.uri = this->_uri;
        uri_obj.method = HTTP_GET;
        uri_obj.user_ctx = this;
        uri_obj.handler = WebsocketHandler::websocket_handler;
        uri_obj.is_websocket = true;

        httpd_register_uri_handler(this->_server_handle, &uri_obj);
    }

    void unregister_endpoint()
    {
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Unregister endpoint %s", this->_uri);
        httpd_unregister_uri_handler(this->_server_handle, this->_uri, HTTP_GET);
    }

    static esp_err_t websocket_handler(httpd_req_t* req)
    {
        WebsocketHandler* _this = reinterpret_cast<WebsocketHandler*>(req->user_ctx);

        if (req->method == HTTP_GET)
        {
            // Initial WebSocket upgrade handshake
            ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: New connection established to %s", _this->_uri);
            return ESP_OK;
        }

        // WebSocket frame handler - keep connection alive
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

        // get packet length
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG_WEBSOCKETS, "WebSocket: Frame receive error: %s", esp_err_to_name(ret));
            return ret;
        }

        if (ws_pkt.len <= 0)
        {
            return ESP_OK;
        }

        uint8_t* buf = (uint8_t*)malloc(ws_pkt.len);
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

        httpd_ws_frame_t response_pkt = {0};
        response_pkt.type = _this->get_frame_type();

        void* resource = _this->handler(ws_pkt.payload, ws_pkt.len, response_pkt.payload, response_pkt.len);
        if (resource == NULL)
        {
            ESP_LOGE(TAG_WEBSOCKETS, "Handler returned NULL for %s", _this->_uri);
            return ESP_FAIL;
        }

        ESP_LOGD(TAG_WEBSOCKETS, "Sending response on %s, type=%d, len=%d", _this->_uri, response_pkt.type,
                 response_pkt.len);

        ret = httpd_ws_send_frame(req, &response_pkt);
        if (ret != ESP_OK)
        {
            free(buf);
            return ret;
        }

        _this->release_resource(resource);

        free(buf);
        return ESP_OK;
    }

    // return resource which needs to be released
    virtual void* handler(const uint8_t* recv_buf, size_t recv_len, uint8_t*& out_buf, size_t& out_len) = 0;

    virtual void release_resource(void* resource) {} // release resources if needed

    /**
     * @brief Get the WebSocket frame type for this handler
     * @return HTTPD_WS_TYPE_TEXT for text data (JSON), HTTPD_WS_TYPE_BINARY for binary data (JPEG)
     */
    virtual httpd_ws_type_t get_frame_type() const { return HTTPD_WS_TYPE_TEXT; }

private:
    httpd_handle_t _server_handle;
    const char* _uri;
    bool _is_registered;
};

class CommandsWebSocketHandler : public WebsocketHandler
{
public:
    CommandsWebSocketHandler(httpd_handle_t server_handle, const char* uri, BaseDetectionModule* detection = nullptr,
                             const BaseStatusProvider* status = nullptr)
        : WebsocketHandler(server_handle, uri), _detection_instance(detection), _status_provider(status)
    {
    }

    virtual void* handler(const uint8_t* recv_buf, size_t recv_len, uint8_t*& out_buf, size_t& out_len)
    {
        // Allocate buffer for JSON response (384 bytes should be sufficient)
        uint8_t* json_buf = (uint8_t*)malloc(384);
        if (!json_buf)
        {
            ESP_LOGE(TAG_WEBSOCKETS, "Failed to allocate JSON buffer");
            out_buf = (uint8_t*)"{}";
            out_len = 2;
            return NULL; // Return NULL to indicate error
        }

        // Serialize metrics to JSON format
        if (_detection_instance != nullptr)
        {
            const TrackSnapshot t = _detection_instance->get_track_snapshot();
            // Existing keys first (older HUDs keep working), then the tracker fields.
            int len = snprintf((char*)json_buf, 384,
                               "{\"detected\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"pixels\":%d,"
                               "\"state\":\"%s\",\"id\":%u,\"bx\":%d,\"by\":%d,\"bw\":%d,\"bh\":%d,"
                               "\"vx\":%.1f,\"vy\":%.1f",
                               t.detected() ? 1 : 0, t.x, t.y, t.frame_width, t.frame_height, t.area_px,
                               tracking::to_string(t.state), (unsigned)t.id, t.bx, t.by, t.bw, t.bh, (double)t.vx,
                               (double)t.vy);
            if (len > 0 && len < 384 && _status_provider != nullptr)
            {
                const ControlStatus c = _status_provider->control_status();
                const int extra = snprintf((char*)json_buf + len, 384 - len,
                                           ",\"mode\":\"%s\",\"cal\":%u,\"cal_state\":\"%s\",\"calibrated\":%d,"
                                           "\"pan\":%.1f,\"tilt\":%.1f,\"cal_error\":\"%s\"",
                                           to_string(c.mode), (unsigned)c.cal_points, cal_state_name(c.cal_state),
                                           c.calibrated ? 1 : 0, (double)c.angles.pan, (double)c.angles.tilt,
                                           c.cal_failure ? c.cal_failure : "");
                len = (extra < 0) ? -1 : len + extra;
            }
            if (len > 0 && len < 383)
            {
                json_buf[len++] = '}';
                json_buf[len] = '\0';
            } else
            {
                len = -1;
            }
            if (len < 0 || len >= 384)
            {
                len = snprintf((char*)json_buf, 384, "{\"detected\":0}");
            }
            out_buf = json_buf;
            out_len = len;
        } else
        {
            // Fallback if no detection module
            int len = snprintf((char*)json_buf, 384,
                               "{\"detected\":0,\"x\":0,\"y\":0,\"width\":320,\"height\":240,\"pixels\":0}");
            out_buf = json_buf;
            out_len = len;
            ESP_LOGW(TAG_WEBSOCKETS, "No detection module connected, sending fallback metrics");
        }

        return (void*)json_buf; // Return the allocated buffer for cleanup
    }

    virtual void release_resource(void* resource)
    {
        if (resource != nullptr)
        {
            free(resource);
        }
    }

private:
    static const char* cal_state_name(aiming::CalState st)
    {
        switch (st)
        {
        case aiming::CalState::WaitPoint1:
            return "point1";
        case aiming::CalState::WaitPoint2:
            return "point2";
        case aiming::CalState::Done:
            return "done";
        case aiming::CalState::Failed:
            return "failed";
        case aiming::CalState::Idle:
            break;
        }
        return "idle";
    }

    BaseDetectionModule* _detection_instance;
    const BaseStatusProvider* _status_provider;
};