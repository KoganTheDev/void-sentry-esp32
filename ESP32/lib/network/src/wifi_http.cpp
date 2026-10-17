#include "wifi_http.h"

#include "form.h"
#include "handshake_guard.h"
#include "wifi_html.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>

static const char* TAG_WIFI_HTTP = "WIFI_HTTP";

namespace
{
constexpr size_t kBodyBuf = prov::kMaxFormBody + 1;
constexpr size_t kEscCap = 6 * prov::kSsidMax + 1; // worst case: every byte becomes \u00XX

/**
 * @brief JSON string escaping: quote, backslash and control characters (as \u00XX); other bytes pass through.
 * @return Length written, excluding the NUL. Output is always NUL-terminated and truncated on whole escapes.
 */
size_t json_escape(const char* in, char* out, size_t cap)
{
    static const char* hex = "0123456789abcdef";
    size_t o = 0;
    for (const unsigned char* p = (const unsigned char*)in; *p != 0; ++p)
    {
        if (o + 7 > cap)
        {
            break; // room for the longest escape plus the NUL
        }
        const unsigned char c = *p;
        if (c == '"' || c == '\\')
        {
            out[o++] = '\\';
            out[o++] = (char)c;
        } else if (c < 0x20)
        {
            out[o++] = '\\';
            out[o++] = 'u';
            out[o++] = '0';
            out[o++] = '0';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0F];
        } else
        {
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return o;
}

/** @brief Overwrites a buffer holding a password or PIN in a way the compiler may not drop. */
void wipe(char* buf, size_t len)
{
    volatile char* p = buf;
    for (size_t i = 0; i < len; ++i)
    {
        p[i] = 0;
    }
}

esp_err_t send_json(httpd_req_t* req, const char* status, const char* body)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

WifiManager& manager(httpd_req_t* req) { return *static_cast<WifiManager*>(req->user_ctx); }

/**
 * @brief DNS-rebinding guard for the JSON API: the Host header must be an IPv4 literal (the board is always reached
 * by IP). A rebinding page reaches the board under its own hostname, so it cannot read saved SSIDs, the scan or the
 * IP. The setup page and the captive-portal catch-all stay reachable under any hostname.
 * @return true if allowed; otherwise a 403 was already sent.
 */
bool api_host_ok(httpd_req_t* req)
{
    char host[128];
    const size_t len = httpd_req_get_hdr_value_len(req, "Host");
    if (len > 0 && len < sizeof(host) && httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK &&
        remote::is_ipv4_host(host, len))
    {
        return true;
    }
    httpd_resp_set_hdr(req, "Connection", "close");
    send_json(req, "403 Forbidden", "{\"error\":\"host not allowed\"}");
    return false;
}

esp_err_t page_handler(httpd_req_t* req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char*)WIFI_PAGE_GZ, WIFI_PAGE_GZ_LEN);
}

esp_err_t status_handler(httpd_req_t* req)
{
    if (!api_host_ok(req))
    {
        return ESP_OK;
    }
    const WifiManager::Status s = manager(req).status();
    char ssid[kEscCap];
    json_escape(s.ssid, ssid, sizeof(ssid));
    char body[kEscCap + 160];
    snprintf(body, sizeof(body), "{\"mode\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"pin_set\":%s,\"failure\":\"%s\"}",
             s.mode, ssid, s.ip, s.pin_set ? "true" : "false", s.failure);
    return send_json(req, "200 OK", body);
}

esp_err_t scan_handler(httpd_req_t* req)
{
    if (!api_host_ok(req))
    {
        return ESP_OK;
    }
    constexpr size_t kCap = 20;
    prov::ScanResult found[kCap];
    const size_t n = manager(req).scan(found, kCap);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send_chunk(req, "{\"networks\":[", HTTPD_RESP_USE_STRLEN);
    for (size_t i = 0; i < n; ++i)
    {
        char ssid[kEscCap];
        json_escape(found[i].ssid, ssid, sizeof(ssid));
        char item[kEscCap + 64];
        snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secured\":%s}", i == 0 ? "" : ",", ssid,
                 (int)found[i].rssi, found[i].secured ? "true" : "false");
        httpd_resp_send_chunk(req, item, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "]}", HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t saved_handler(httpd_req_t* req)
{
    if (!api_host_ok(req))
    {
        return ESP_OK;
    }
    char ssids[prov::kMaxNetworks][prov::kSsidMax + 1];
    const size_t n = manager(req).saved(ssids, prov::kMaxNetworks);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send_chunk(req, "{\"saved\":[", HTTPD_RESP_USE_STRLEN);
    for (size_t i = 0; i < n; ++i)
    {
        char ssid[kEscCap];
        json_escape(ssids[i], ssid, sizeof(ssid));
        char item[kEscCap + 8];
        snprintf(item, sizeof(item), "%s\"%s\"", i == 0 ? "" : ",", ssid);
        httpd_resp_send_chunk(req, item, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "]}", HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, nullptr, 0);
}

/**
 * @brief Origin check, then reads the whole (<= 256 B) form body into buf (NUL-terminated).
 * @return true if the body is in buf; otherwise a response was already sent.
 */
bool read_form(httpd_req_t* req, char* buf, size_t* len)
{
    if (!api_host_ok(req))
    {
        return false;
    }
    if (!handshake_origin_ok(req, TAG_WIFI_HTTP))
    {
        httpd_resp_set_hdr(req, "Connection", "close");
        send_json(req, "403 Forbidden", "{\"error\":\"origin not allowed\"}");
        return false;
    }
    const size_t total = req->content_len;
    if (total == 0 || total > prov::kMaxFormBody)
    {
        httpd_resp_set_hdr(req, "Connection", "close"); // the unread body must not be parsed as the next request
        send_json(req, "400 Bad Request", "{\"error\":\"bad request size\"}");
        return false;
    }
    // A slow client must not hold the single httpd task (it also serves /stream and /commands): allow at most
    // kMaxRecvTimeouts receive timeouts (recv_wait_timeout each) for the whole body, then give up with 408.
    constexpr int kMaxRecvTimeouts = 2;
    int timeouts = 0;
    size_t got = 0;
    while (got < total)
    {
        const int r = httpd_req_recv(req, buf + got, total - got);
        if (r <= 0)
        {
            if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts <= kMaxRecvTimeouts)
            {
                continue;
            }
            if (r == HTTPD_SOCK_ERR_TIMEOUT)
            {
                httpd_resp_set_hdr(req, "Connection", "close");
                send_json(req, "408 Request Timeout", "{\"error\":\"request timeout\"}");
                return false;
            }
            httpd_resp_set_hdr(req, "Connection", "close");
            send_json(req, "400 Bad Request", "{\"error\":\"bad request\"}");
            return false;
        }
        got += (size_t)r;
    }
    buf[got] = '\0';
    *len = got;
    return true;
}

esp_err_t send_outcome(httpd_req_t* req, const WifiManager::Outcome& out, const char* bad_input_msg)
{
    switch (out.result)
    {
    case WifiManager::Result::Ok:
        return send_json(req, "200 OK", "{\"ok\":true}");
    case WifiManager::Result::WrongPin:
        return send_json(req, "403 Forbidden", "{\"error\":\"wrong pin\"}");
    case WifiManager::Result::LockedOut: {
        char body[40];
        snprintf(body, sizeof(body), "{\"locked_s\":%u}", (unsigned)out.seconds_left);
        return send_json(req, "429 Too Many Requests", body);
    }
    case WifiManager::Result::BadInput:
        break;
    }
    char body[160];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", bad_input_msg);
    return send_json(req, "400 Bad Request", body);
}

esp_err_t add_handler(httpd_req_t* req)
{
    char body[kBodyBuf];
    char ssid[prov::kSsidMax + 1];
    char pass[prov::kPassMax + 1];
    char pin[16];
    size_t body_len = 0;
    esp_err_t err = ESP_OK;

    if (!read_form(req, body, &body_len))
    {
        wipe(body, sizeof(body));
        return ESP_OK;
    }
    size_t ssid_len = 0;
    size_t pass_len = 0;
    size_t pin_len = 0;
    if (!prov::form_field(body, body_len, "ssid", ssid, sizeof(ssid), &ssid_len))
    {
        err = send_json(req, "400 Bad Request", "{\"error\":\"missing or too long ssid\"}");
    } else
    {
        if (!prov::form_field(body, body_len, "password", pass, sizeof(pass), &pass_len))
        {
            pass[0] = '\0';
            pass_len = 0;
        }
        if (!prov::form_field(body, body_len, "pin", pin, sizeof(pin), &pin_len))
        {
            pin[0] = '\0'; // a missing or oversized PIN is just a wrong one
            pin_len = 0;
        }
        const WifiManager::Outcome out = manager(req).add(ssid, ssid_len, pass, pass_len, pin, pin_len, millis());
        err = send_outcome(req, out, "check ssid (1-32 chars), password (empty or 8-63 chars) and pin (4-8 digits)");
    }
    wipe(body, sizeof(body));
    wipe(pass, sizeof(pass));
    wipe(pin, sizeof(pin));
    return err;
}

esp_err_t forget_handler(httpd_req_t* req)
{
    char body[kBodyBuf];
    char ssid[prov::kSsidMax + 1];
    char pin[16];
    size_t body_len = 0;
    esp_err_t err = ESP_OK;

    if (!read_form(req, body, &body_len))
    {
        wipe(body, sizeof(body));
        return ESP_OK;
    }
    size_t ssid_len = 0;
    size_t pin_len = 0;
    if (!prov::form_field(body, body_len, "ssid", ssid, sizeof(ssid), &ssid_len))
    {
        err = send_json(req, "400 Bad Request", "{\"error\":\"missing or too long ssid\"}");
    } else
    {
        if (!prov::form_field(body, body_len, "pin", pin, sizeof(pin), &pin_len))
        {
            pin[0] = '\0';
            pin_len = 0;
        }
        const WifiManager::Outcome out = manager(req).forget(ssid, ssid_len, pin, pin_len, millis());
        err = send_outcome(req, out, "unknown network, or no PIN has been set yet");
    }
    wipe(body, sizeof(body));
    wipe(pin, sizeof(pin));
    return err;
}

/** @brief Catch-all GET: captive-portal redirect in setup mode, 404 otherwise. Never origin-checked. */
esp_err_t catch_all_handler(httpd_req_t* req)
{
    if (manager(req).setup_mode())
    {
        httpd_resp_set_status(req, "302 Found");
        httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/wifi");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        return httpd_resp_send(req, nullptr, 0);
    }
    return httpd_resp_send_404(req);
}

void add_route(httpd_handle_t server, const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*),
               WifiManager& wifi)
{
    httpd_uri_t route = {};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    route.user_ctx = &wifi;
    if (httpd_register_uri_handler(server, &route) != ESP_OK)
    {
        ESP_LOGE(TAG_WIFI_HTTP, "Failed to register %s", uri);
    }
}
} // namespace

void register_wifi_routes(httpd_handle_t server, WifiManager& wifi)
{
    add_route(server, "/wifi", HTTP_GET, page_handler, wifi);
    add_route(server, "/api/wifi/status", HTTP_GET, status_handler, wifi);
    add_route(server, "/api/wifi/scan", HTTP_GET, scan_handler, wifi);
    add_route(server, "/api/wifi/saved", HTTP_GET, saved_handler, wifi);
    add_route(server, "/api/wifi/add", HTTP_POST, add_handler, wifi);
    add_route(server, "/api/wifi/forget", HTTP_POST, forget_handler, wifi);
    add_route(server, "/*", HTTP_GET, catch_all_handler, wifi); // last: it matches everything
}
