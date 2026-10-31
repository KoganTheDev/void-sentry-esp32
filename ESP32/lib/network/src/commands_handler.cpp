#include "commands_handler.h"

#include "camera.h"
#include "handshake_guard.h"
#include "reply_json.h"
#include "track_snapshot.h"
#include "tracking_types.h"
#include "wifi_manager.h"

#include <cstdio>
#include <cstring>
#include <esp32-hal-log.h> // route ESP_LOGx through Arduino's logger (otherwise silenced)
#include <esp_log.h>
#include <new>

static const char* TAG_WEBSOCKETS = "WEBSOCKETS";

namespace
{
/** @brief httpd session context of a /commands socket that sent a valid login cookie at its handshake. */
struct CommandsSession {
    prov::SessionToken token;
};

void free_session(void* ctx) { delete static_cast<CommandsSession*>(ctx); }
} // namespace

CommandsWebSocketHandler::CommandsWebSocketHandler(httpd_handle_t server_handle, const char* uri,
                                                   BaseDetectionModule* detection, const BaseStatusProvider* status,
                                                   remote::BaseRemoteSink* remote, const StreamBroadcaster* stream,
                                                   WifiManager* auth)
    : _server_handle(server_handle), _uri(uri), _detection_instance(detection), _status_provider(status),
      _remote(remote), _stream(stream), _auth(auth)
{
}

CommandsWebSocketHandler::~CommandsWebSocketHandler() { this->unregister_endpoint(); }

void CommandsWebSocketHandler::register_endpoint()
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

void CommandsWebSocketHandler::unregister_endpoint()
{
    ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Unregister endpoint %s", this->_uri);
    httpd_unregister_uri_handler(this->_server_handle, this->_uri, HTTP_GET);
}

esp_err_t CommandsWebSocketHandler::pre_handshake(httpd_req_t* req)
{
    return handshake_origin_ok(req, TAG_WEBSOCKETS) ? ESP_OK : ESP_FAIL;
}

esp_err_t CommandsWebSocketHandler::post_handshake(httpd_req_t* req)
{
    const auto* _this = reinterpret_cast<const CommandsWebSocketHandler*>(req->user_ctx);
    // Only a token that is live right now is kept (a stale cookie from before a reboot is not); it is checked again
    // on every message. httpd frees the context with free_session when this socket closes.
    prov::SessionToken token{};
    bool logged_in = false;
    if (_this->_auth != nullptr && read_session_cookie(req, token) &&
        _this->_auth->auth_level(&token) == AuthLevel::Authed)
    {
        auto* session = new (std::nothrow) CommandsSession{token};
        if (session != nullptr)
        {
            httpd_sess_set_ctx(req->handle, httpd_req_to_sockfd(req), session, free_session);
            logged_in = true;
        }
    }
    ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: New connection established to %s (%s)", _this->_uri,
             logged_in ? "logged in" : "no login");
    return ESP_OK;
}

AuthLevel CommandsWebSocketHandler::access(const void* sess_ctx) const
{
    if (this->_auth == nullptr)
    {
        return AuthLevel::Open; // no WiFi manager, so no PIN anyone could log in with
    }
    const auto* session = static_cast<const CommandsSession*>(sess_ctx);
    return this->_auth->auth_level(session != nullptr ? &session->token : nullptr);
}

esp_err_t CommandsWebSocketHandler::websocket_handler(httpd_req_t* req)
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
    if (ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_PONG || ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        return ESP_OK;
    }
    if (ws_pkt.type != HTTPD_WS_TYPE_TEXT || ws_pkt.len > kMaxMessage)
    {
        ESP_LOGW(TAG_WEBSOCKETS, "WebSocket: rejecting frame type=%d len=%u", (int)ws_pkt.type, (unsigned)ws_pkt.len);
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

    // A viewer's control command reaches the sink as a Poll: dropped, never queued, and still answered below.
    const AuthLevel auth = _this->access(req->sess_ctx);
    if (_this->_remote != nullptr)
    {
        _this->_remote->submit(remote::gate(remote::parse({reinterpret_cast<const char*>(buf), ws_pkt.len}), auth));
    }

    char json[hud::kReplySize];
    const size_t json_len = _this->build_reply(json, sizeof(json), auth);

    httpd_ws_frame_t response_pkt = {0};
    response_pkt.type = HTTPD_WS_TYPE_TEXT;
    response_pkt.payload = reinterpret_cast<uint8_t*>(json);
    response_pkt.len = json_len;
    return httpd_ws_send_frame(req, &response_pkt);
}

size_t CommandsWebSocketHandler::build_reply(char* out, size_t size, AuthLevel auth) const
{
    hud::ReplyInput in;
    in.auth = auth;
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
