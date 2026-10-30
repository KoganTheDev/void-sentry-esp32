#pragma once

#include "auth_level.h"
#include "base_detection_module.h"
#include "control_status.h"
#include "remote_command.h"
#include "stream_broadcaster.h"
#include <cstddef>
#include <esp_http_server.h>

class WifiManager;

/**
 * @class CommandsWebSocketHandler
 * @brief The /commands WebSocket: parses each HUD message, hands it to the remote sink and replies with JSON metrics.
 * @details Request/reply: every received text frame gets exactly one JSON reply (the HUD's poll loop depends on it).
 * Receive and reply buffers live on the httpd task's stack (64 B and hud::kReplySize), so there is no heap use per
 * message and nothing to leak when a send fails.
 *
 * Access control: anyone may watch (every message gets its reply), only a logged-in session may control. The
 * handshake copies a valid `vs_session` login token into the httpd session context (16 B on the heap per logged-in
 * socket, freed by httpd when the socket closes, so a reused fd never inherits it). Every message then asks the
 * WifiManager for the session's AuthLevel, and a viewer's control commands go to the sink as a Poll
 * (remote::gate). Because the token is checked per message, a logout or a new PIN takes control away from an open
 * socket at once; logging in needs a new socket, whose handshake carries the new cookie. The level is the reply's
 * "auth" key.
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
     * @param auth Optional WiFi manager that owns the PIN and the login sessions; must outlive the handler. When null
     * there is no PIN to log in with, so every session has AuthLevel::Open (as on a board with no PIN set).
     */
    CommandsWebSocketHandler(httpd_handle_t server_handle, const char* uri, BaseDetectionModule* detection = nullptr,
                             const BaseStatusProvider* status = nullptr, remote::BaseRemoteSink* remote = nullptr,
                             const StreamBroadcaster* stream = nullptr, WifiManager* auth = nullptr);

    ~CommandsWebSocketHandler();

    /** @brief Registers the WebSocket endpoint on the server. */
    void register_endpoint();

    /** @brief Removes the endpoint from the server. */
    void unregister_endpoint();

    /** @brief Longest accepted message in bytes; the longest real command ("move:down-right") is 15. */
    static constexpr size_t kMaxMessage = 64;

    /** @brief ws_pre_handshake_cb: refuses pages from other origins before the 101 upgrade reply (ESP_FAIL refuses). */
    static esp_err_t pre_handshake(httpd_req_t* req);

    /** @brief ws_post_handshake_cb: remembers the session's login token (if valid) and logs the new session. */
    static esp_err_t post_handshake(httpd_req_t* req);

    /** @brief httpd callback for /commands: parses one text frame and sends the JSON reply (the handshake goes to the
     * pre/post hooks, never here). */
    static esp_err_t websocket_handler(httpd_req_t* req);

private:
    /** @brief Writes the NUL-terminated JSON metrics into out (size bytes) and returns its length. */
    size_t build_reply(char* out, size_t size, AuthLevel auth) const;

    /** @brief AuthLevel of a session whose httpd context is @p sess_ctx (null: no login token stored). */
    AuthLevel access(const void* sess_ctx) const;

    httpd_handle_t _server_handle;
    const char* _uri;
    BaseDetectionModule* _detection_instance;
    const BaseStatusProvider* _status_provider;
    remote::BaseRemoteSink* _remote;
    const StreamBroadcaster* _stream;
    WifiManager* _auth;
};
