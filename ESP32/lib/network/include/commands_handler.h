#pragma once

#include "base_detection_module.h"
#include "control_status.h"
#include "remote_command.h"
#include "stream_broadcaster.h"
#include <cstddef>
#include <esp_http_server.h>

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
                             const StreamBroadcaster* stream = nullptr);

    ~CommandsWebSocketHandler();

    /** @brief Registers the WebSocket endpoint on the server. */
    void register_endpoint();

    /** @brief Removes the endpoint from the server. */
    void unregister_endpoint();

    /** @brief Longest accepted message in bytes; the longest real command ("move:down-right") is 15. */
    static constexpr size_t kMaxMessage = 64;

    /** @brief ws_pre_handshake_cb: refuses pages from other origins before the 101 upgrade reply (ESP_FAIL refuses). */
    static esp_err_t pre_handshake(httpd_req_t* req);

    /** @brief ws_post_handshake_cb: logs the new session once it is a WebSocket. */
    static esp_err_t post_handshake(httpd_req_t* req);

    /** @brief httpd callback for /commands: parses one text frame and sends the JSON reply (the handshake goes to the
     * pre/post hooks, never here). */
    static esp_err_t websocket_handler(httpd_req_t* req);

private:
    /** @brief Writes the NUL-terminated JSON metrics into out (size bytes) and returns its length. */
    size_t build_reply(char* out, size_t size) const;

    httpd_handle_t _server_handle;
    const char* _uri;
    BaseDetectionModule* _detection_instance;
    const BaseStatusProvider* _status_provider;
    remote::BaseRemoteSink* _remote;
    const StreamBroadcaster* _stream;
};
