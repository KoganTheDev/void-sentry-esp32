#pragma once

// TODO: Change name to the header file and split to cpp file as well
#include "base_detection_module.h"
#include "camera.h"
#include "control_status.h"
#include "handshake_guard.h"
#include "remote_command.h"
#include "reply_json.h"
#include "stream_broadcaster.h"
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
 * Receive and reply buffers live on the httpd task's stack (64 B and hud::kReplySize), so there is no heap use per
 * message and nothing to leak when a send fails.
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
     * @param stream Optional /stream broadcaster whose viewer count is sent as "vw" (0 when null). Must outlive the
     * handler; StreamBroadcaster::viewer_count() is safe to call from the httpd task (but not concurrently with
     * StreamBroadcaster::stop(), so httpd must be stopped first).
     */
    CommandsWebSocketHandler(httpd_handle_t server_handle, const char* uri, BaseDetectionModule* detection = nullptr,
                             const BaseStatusProvider* status = nullptr, remote::BaseRemoteSink* remote = nullptr,
                             const StreamBroadcaster* stream = nullptr)
        : _server_handle(server_handle), _uri(uri), _detection_instance(detection), _status_provider(status),
          _remote(remote), _stream(stream)
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
        uri_obj.ws_pre_handshake_cb = CommandsWebSocketHandler::pre_handshake;
        uri_obj.ws_post_handshake_cb = CommandsWebSocketHandler::post_handshake;

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

    /** @brief ws_pre_handshake_cb: refuses pages from other origins before the 101 upgrade reply (ESP_FAIL refuses). */
    static esp_err_t pre_handshake(httpd_req_t* req)
    {
        return handshake_origin_ok(req, TAG_WEBSOCKETS) ? ESP_OK : ESP_FAIL;
    }

    /** @brief ws_post_handshake_cb: logs the new session once it is a WebSocket. */
    static esp_err_t post_handshake(httpd_req_t* req)
    {
        const auto* _this = reinterpret_cast<const CommandsWebSocketHandler*>(req->user_ctx);
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: New connection established to %s", _this->_uri);
        return ESP_OK;
    }

    /** @brief httpd callback for /commands: parses one text frame and sends the JSON reply (the handshake goes to the
     * pre/post hooks, never here). */
    static esp_err_t websocket_handler(httpd_req_t* req)
    {
        CommandsWebSocketHandler* _this = reinterpret_cast<CommandsWebSocketHandler*>(req->user_ctx);

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

        // A zero-length text frame has no payload to read but still gets its one reply (parse yields Poll).
        uint8_t buf[kMaxMessage];
        if (ws_pkt.len > 0)
        {
            ws_pkt.payload = buf;
            ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
            if (ret != ESP_OK)
            {
                return ret;
            }
        }

        if (_this->_remote != nullptr)
        {
            _this->_remote->submit(remote::parse({reinterpret_cast<const char*>(buf), ws_pkt.len}));
        }

        char json[hud::kReplySize];
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
        hud::ReplyInput in;
        TrackSnapshot track;
        ControlStatus status;
        if (this->_detection_instance != nullptr)
        {
            track = this->_detection_instance->get_track_snapshot();
            in.track = &track;
            if (this->_status_provider != nullptr)
            {
                status = this->_status_provider->control_status();
                in.status = &status;
            }
        } else
        {
            ESP_LOGW(TAG_WEBSOCKETS, "No detection module connected, sending fallback metrics");
        }
        in.viewers = this->_stream != nullptr ? static_cast<uint32_t>(this->_stream->viewer_count()) : 0;
        return hud::format_reply(in, out, size);
    }

    httpd_handle_t _server_handle;
    const char* _uri;
    BaseDetectionModule* _detection_instance;
    const BaseStatusProvider* _status_provider;
    remote::BaseRemoteSink* _remote;
    const StreamBroadcaster* _stream;
};