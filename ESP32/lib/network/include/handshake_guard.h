#pragma once

#include "origin_check.h"
#include <esp_http_server.h>
#include <esp_log.h>

/**
 * @brief Reads the Origin and Host headers of a WebSocket handshake or a browser POST and applies
 * remote::origin_allowed.
 * @details Headers are copied into 128 B stack buffers. An Origin too long for the buffer counts as not allowed; a
 * Host too long counts as empty (so any present Origin is rejected). A rejection is logged with the origin only.
 * @param req The handshake request (req->method == HTTP_GET).
 * @param tag Log tag of the calling endpoint.
 * @return true if the handshake may proceed; false means the handler should return ESP_FAIL.
 */
inline bool handshake_origin_ok(httpd_req_t* req, const char* tag)
{
    constexpr size_t kHdrBuf = 128;
    char origin[kHdrBuf];
    char host[kHdrBuf];
    size_t origin_len = 0;
    size_t host_len = 0;
    bool origin_too_long = false;

    const size_t o_len = httpd_req_get_hdr_value_len(req, "Origin");
    if (o_len > 0)
    {
        if (o_len >= kHdrBuf || httpd_req_get_hdr_value_str(req, "Origin", origin, kHdrBuf) != ESP_OK)
        {
            origin_too_long = true;
        } else
        {
            origin_len = o_len;
        }
    }
    const size_t h_len = httpd_req_get_hdr_value_len(req, "Host");
    if (h_len > 0 && h_len < kHdrBuf && httpd_req_get_hdr_value_str(req, "Host", host, kHdrBuf) == ESP_OK)
    {
        host_len = h_len;
    }

    if (origin_too_long)
    {
        ESP_LOGW(tag, "request refused: Origin header too long");
        return false;
    }
    if (!remote::origin_allowed(origin, origin_len, host, host_len))
    {
        ESP_LOGW(tag, "request refused: Origin %.*s", (int)origin_len, origin);
        return false;
    }
    return true;
}
