#pragma once

// TODO: Change name to the header file and split to cpp file as well
#include "base_detection_module.h"
#include "camera.h"
#include "control_status.h"
#include "remote_command.h"
#include "track_snapshot.h"
#include "tracking_types.h"
#include <cstdio>
#include <cstring>
#include <esp_http_server.h>
#include <esp_log.h>

static const char* TAG_WEBSOCKETS = "WEBSOCKETS";

/**
 * @class CommandsWebSocketHandler
 * @brief The /commands WebSocket: parses each HUD message, hands it to the remote sink and replies with JSON metrics.
 * @details Request/reply: every received text frame gets exactly one JSON reply (the HUD's poll loop depends on it).
 * Receive and reply buffers live on the httpd task's stack (64 B and 384 B), so there is no heap use per message and
 * nothing to leak when a send fails.
 */
class CommandsWebSocketHandler
{
public:
    /**
     * @param server_handle Running httpd server to register the endpoint on.
     * @param uri Endpoint path (must outlive the handler; a string literal).
     * @param detection Optional detection module for the tracker fields.
     * @param status Optional controller status for the mode/calibration fields.
     * @param remote Optional sink that receives the parsed commands (the Controller).
     */
    CommandsWebSocketHandler(httpd_handle_t server_handle, const char* uri, BaseDetectionModule* detection = nullptr,
                             const BaseStatusProvider* status = nullptr, remote::BaseRemoteSink* remote = nullptr)
        : _server_handle(server_handle), _uri(uri), _detection_instance(detection), _status_provider(status),
          _remote(remote)
    {
    }

    ~CommandsWebSocketHandler() { this->unregister_endpoint(); }

    /** @brief Registers the WebSocket endpoint on the server. */
    void register_endpoint()
    {
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Register endpoint %s", this->_uri);
        httpd_uri_t uri_obj = {0};
        uri_obj.uri = this->_uri;
        uri_obj.method = HTTP_GET;
        uri_obj.user_ctx = this;
        uri_obj.handler = CommandsWebSocketHandler::websocket_handler;
        uri_obj.is_websocket = true;

        httpd_register_uri_handler(this->_server_handle, &uri_obj);
    }

    /** @brief Removes the endpoint from the server. */
    void unregister_endpoint()
    {
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Unregister endpoint %s", this->_uri);
        httpd_unregister_uri_handler(this->_server_handle, this->_uri, HTTP_GET);
    }

    /** @brief Longest accepted message in bytes; the longest real command ("move:down-right") is 15. */
    static constexpr size_t kMaxMessage = 64;

    /** @brief Size of the JSON reply buffer on the stack. */
    static constexpr size_t kReplySize = 384;

    static esp_err_t websocket_handler(httpd_req_t* req)
    {
        CommandsWebSocketHandler* _this = reinterpret_cast<CommandsWebSocketHandler*>(req->user_ctx);

        if (req->method == HTTP_GET)
        {
            // Initial WebSocket upgrade handshake
            ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: New connection established to %s", _this->_uri);
            return ESP_OK;
        }

        // Get the frame header (type and length) without reading the payload.
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG_WEBSOCKETS, "WebSocket: Frame receive error: %s", esp_err_to_name(ret));
            return ret;
        }

        // The server answers PING/CLOSE itself (handle_ws_control_frames is off); do not reject one that slips through.
        if (ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_PONG ||
            ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
        {
            return ESP_OK;
        }
        if (ws_pkt.type != HTTPD_WS_TYPE_TEXT || ws_pkt.len > kMaxMessage)
        {
            ESP_LOGW(TAG_WEBSOCKETS, "WebSocket: rejecting frame type=%d len=%u", (int)ws_pkt.type,
                     (unsigned)ws_pkt.len);
            return ESP_FAIL; // the server closes the session
        }
        if (ws_pkt.len == 0)
        {
            return ESP_OK;
        }

        uint8_t buf[kMaxMessage];
        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            return ret;
        }

        if (_this->_remote != nullptr)
        {
            _this->_remote->submit(remote::parse(reinterpret_cast<const char*>(buf), ws_pkt.len));
        }

        char json[kReplySize];
        const size_t json_len = _this->build_reply(json, sizeof(json));

        httpd_ws_frame_t response_pkt = {0};
        response_pkt.type = HTTPD_WS_TYPE_TEXT;
        response_pkt.payload = reinterpret_cast<uint8_t*>(json);
        response_pkt.len = json_len;
        return httpd_ws_send_frame(req, &response_pkt);
    }

private:
    /** @brief Writes the NUL-terminated JSON metrics into out (size bytes) and returns its length. */
    size_t build_reply(char* out, size_t size) const
    {
        const int cap = (int)size;
        if (_detection_instance == nullptr)
        {
            // Fallback if no detection module
            ESP_LOGW(TAG_WEBSOCKETS, "No detection module connected, sending fallback metrics");
            return (size_t)snprintf(out, size,
                                    "{\"detected\":0,\"x\":0,\"y\":0,\"width\":320,\"height\":240,\"pixels\":0}");
        }

        const TrackSnapshot t = _detection_instance->get_track_snapshot();
        // Existing keys first (older HUDs keep working), then the tracker fields.
        int len =
            snprintf(out, size,
                     "{\"detected\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"pixels\":%d,"
                     "\"state\":\"%s\",\"id\":%u,\"bx\":%d,\"by\":%d,\"bw\":%d,\"bh\":%d,"
                     "\"vx\":%.1f,\"vy\":%.1f",
                     t.detected() ? 1 : 0, t.x, t.y, t.frame_width, t.frame_height, t.area_px,
                     tracking::to_string(t.state), (unsigned)t.id, t.bx, t.by, t.bw, t.bh, (double)t.vx, (double)t.vy);
        if (len > 0 && len < cap && _status_provider != nullptr)
        {
            const ControlStatus c = _status_provider->control_status();
            const int extra =
                snprintf(out + len, size - len,
                         ",\"mode\":\"%s\",\"cal\":%u,\"cal_state\":\"%s\",\"calibrated\":%d,"
                         "\"pan\":%.1f,\"tilt\":%.1f,\"cal_error\":\"%s\"",
                         to_string(c.mode), (unsigned)c.cal_points, cal_state_name(c.cal_state), c.calibrated ? 1 : 0,
                         (double)c.angles.pan, (double)c.angles.tilt, c.cal_failure ? c.cal_failure : "");
            len = (extra < 0) ? -1 : len + extra;
        }
        if (len > 0 && len < cap - 1)
        {
            out[len++] = '}';
            out[len] = '\0';
        } else
        {
            len = -1;
        }
        if (len < 0 || len >= cap)
        {
            len = snprintf(out, size, "{\"detected\":0}");
        }
        return (size_t)len;
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

    httpd_handle_t _server_handle;
    const char* _uri;
    BaseDetectionModule* _detection_instance;
    const BaseStatusProvider* _status_provider;
    remote::BaseRemoteSink* _remote;
};