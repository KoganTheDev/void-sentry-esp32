#pragma once

#include "log_sanitize.h"
#include "origin_check.h"
#include "session_table.h"
#include <esp32-hal-log.h> // route the refusal log through Arduino's logger (otherwise silenced)
#include <esp_http_server.h>
#include <esp_log.h>

// IDF 5.5 never calls a WebSocket URI handler for the handshake: ws_pre_handshake_cb runs before the 101 reply (an
// error refuses the upgrade) and ws_post_handshake_cb after it. Both are off in the precompiled Arduino libs, so
// platformio.ini sets them through custom_sdkconfig.
#if !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT || !CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
// cppcheck-suppress preprocessorErrorDirective
#error "needs CONFIG_HTTPD_WS_PRE/POST_HANDSHAKE_CB_SUPPORT=y (custom_sdkconfig in platformio.ini)"
#endif

/** @brief Result of read_header(). */
enum class HeaderStatus {
    Absent,  ///< no such header, or it is empty
    TooLong, ///< present but does not fit the buffer (or the copy failed)
    Ok       ///< copied, NUL-terminated, length in *len
};

/**
 * @brief Copies one request header into @p buf (capacity @p cap, NUL included) and reports its length in @p len.
 * @details Shared by the WebSocket handshake guard and the WiFi API's Host check, so both read headers the same way.
 */
inline HeaderStatus read_header(httpd_req_t* req, const char* name, char* buf, size_t cap, size_t* len)
{
    *len = 0;
    const size_t n = httpd_req_get_hdr_value_len(req, name);
    if (n == 0)
    {
        return HeaderStatus::Absent;
    }
    if (n >= cap || httpd_req_get_hdr_value_str(req, name, buf, cap) != ESP_OK)
    {
        return HeaderStatus::TooLong;
    }
    *len = n;
    return HeaderStatus::Ok;
}

/**
 * @brief Reads the Origin and Host headers of a WebSocket handshake or a browser POST and applies
 * remote::origin_allowed.
 * @details Headers are copied into 128 B stack buffers. An Origin too long for the buffer counts as not allowed; a
 * Host too long counts as empty (so any present Origin is rejected). A rejection is logged with the origin only.
 * @param req The handshake request (req->method == HTTP_GET).
 * @param tag Log tag of the calling endpoint.
 * @param any_port If true, use remote::origin_allowed_any_port (the endpoint lives on a different port than the page,
 * e.g. /stream on port 81); /commands and the WiFi API keep the exact check.
 * @return true if the handshake may proceed; false means the handler should return ESP_FAIL.
 */
inline bool handshake_origin_ok(httpd_req_t* req, const char* tag, bool any_port = false)
{
    constexpr size_t kHdrBuf = 128;
    char origin[kHdrBuf];
    char host[kHdrBuf];
    size_t origin_len = 0;
    size_t host_len = 0;

    const bool origin_too_long = read_header(req, "Origin", origin, kHdrBuf, &origin_len) == HeaderStatus::TooLong;
    if (read_header(req, "Host", host, kHdrBuf, &host_len) != HeaderStatus::Ok)
    {
        host_len = 0; // absent or too long both count as empty
    }

    if (origin_too_long)
    {
        ESP_LOGW(tag, "request refused: Origin header too long");
        return false;
    }
    const std::string_view o(origin, origin_len);
    const std::string_view h(host, host_len);
    if (!(any_port ? remote::origin_allowed_any_port(o, h) : remote::origin_allowed(o, h)))
    {
        char safe[kHdrBuf]; // the Origin is attacker-controlled: no control bytes into the serial log
        remote::sanitize_for_log(origin, origin_len, safe, sizeof(safe));
        ESP_LOGW(tag, "request refused: Origin %s", safe);
        return false;
    }
    return true;
}

/**
 * @brief Reads the HUD login token from the request's Cookie header (the `vs_session` cookie, prov::session_cookie).
 * @details Shared by the login API (logout) and the /commands handshake. The header is copied into a stack buffer of
 * prov::kMaxCookieHeader + 1 bytes; a longer one counts as no cookie. The token is only parsed here, not checked.
 * @return true and the token in @p token if a well-formed vs_session cookie was sent.
 */
inline bool read_session_cookie(httpd_req_t* req, prov::SessionToken& token)
{
    char cookie[prov::kMaxCookieHeader + 1];
    size_t len = 0;
    return read_header(req, "Cookie", cookie, sizeof(cookie), &len) == HeaderStatus::Ok &&
           prov::session_cookie({cookie, len}, token);
}
