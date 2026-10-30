/**
 * @file wifi_http.h
 * @brief HTTP routes of the WiFi setup page, its JSON API and the HUD login API.
 */

#pragma once

#include <esp_http_server.h>

class WifiManager;

/**
 * @brief Registers `/wifi`, `/api/wifi/status|scan|saved`, `POST /api/wifi/add|forget`, `POST /api/hud/login|logout`
 * and the catch-all `GET /*`.
 * @details The server must use `httpd_uri_match_wildcard`, and this must be called after every other route: the
 * catch-all matches anything, so exact routes registered earlier win. In setup mode the catch-all answers 302 to
 * `http://192.168.4.1/wifi` (phone captive-portal probes request arbitrary hostnames), otherwise 404. Only POSTs are
 * origin-checked (and need an IPv4-literal Host, like the whole API). The HUD login checks the same PIN as the WiFi
 * API and sets the `vs_session` cookie that /commands reads at its handshake. Passwords, PINs and session tokens are
 * never logged.
 * @param server Running httpd instance.
 * @param wifi Manager the handlers read and update; must outlive the server.
 */
void register_wifi_routes(httpd_handle_t server, WifiManager& wifi);
