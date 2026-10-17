#pragma once

/**
 * @file platform_compat.h
 * @brief The only place where Arduino core 2 (ESP-IDF 4.4) and core 3 (ESP-IDF 5.x) APIs differ.
 * @details Stage 7 side-by-side phase: every source builds on both cores; core-version `#if`s live here and
 *          nowhere else. Deleted at cut-over, keeping only the core 3 branches.
 *          (JPEG decoding needs no shim: both cores use the vendored C TJpgDec in lib/tjpgd, see tjpgd_c.h.)
 */

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace compat
{
/// (always_inline: called from an IRAM tick hook.)
/// FreeRTOS: IDF 5 renamed xTaskGetIdleTaskHandleForCPU to ...ForCore (core 2 keeps the CPU name).
__attribute__((always_inline)) inline TaskHandle_t idle_task_for_core(BaseType_t core)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return xTaskGetIdleTaskHandleForCore(core);
#else
    return xTaskGetIdleTaskHandleForCPU(core);
#endif
}

/// FreeRTOS: IDF 5 renamed xTaskGetCurrentTaskHandleForCPU to ...ForCore.
__attribute__((always_inline)) inline TaskHandle_t current_task_for_core(BaseType_t core)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return xTaskGetCurrentTaskHandleForCore(core);
#else
    return xTaskGetCurrentTaskHandleForCPU(core);
#endif
}

/// @brief Handshake hook of a WebSocket endpoint: gets the upgrade request (headers readable, req->user_ctx set).
using WsHandshakeHook = esp_err_t (*)(httpd_req_t* req);

/**
 * @brief Attaches the WebSocket handshake hooks to an endpoint before httpd_register_uri_handler().
 * @details esp_http_server changed how a WebSocket handshake reaches the application:
 *          - IDF 4.4 (core 2) answers the upgrade with 101 and then calls the URI handler once with HTTP_GET. The
 *            handler runs both hooks itself from that call (see ws_handshake_in_handler()); refusing there closes a
 *            session that is already upgraded.
 *          - IDF 5.5 (core 3) never calls the handler for the handshake. It runs ws_pre_handshake_cb before the 101
 *            reply (an error refuses the upgrade) and ws_post_handshake_cb after it. Both need
 *            CONFIG_HTTPD_WS_PRE/POST_HANDSHAKE_CB_SUPPORT, set through custom_sdkconfig in platformio.ini.
 * @param uri Endpoint being registered (is_websocket = true).
 * @param pre Runs before the upgrade; return ESP_FAIL to refuse it (the origin check).
 * @param post Runs once the session is a WebSocket (viewer registration); may be nullptr.
 */
inline void set_ws_handshake_hooks(httpd_uri_t& uri, WsHandshakeHook pre, WsHandshakeHook post)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
#if !CONFIG_HTTPD_WS_PRE_HANDSHAKE_CB_SUPPORT || !CONFIG_HTTPD_WS_POST_HANDSHAKE_CB_SUPPORT
#error "core 3 needs CONFIG_HTTPD_WS_PRE/POST_HANDSHAKE_CB_SUPPORT=y (custom_sdkconfig in platformio.ini)"
#endif
    uri.ws_pre_handshake_cb = pre;
    uri.ws_post_handshake_cb = post;
#else
    (void)uri;
    (void)pre;
    (void)post;
#endif
}

/**
 * @brief True when the URI handler is being called for the completed handshake rather than for a frame.
 * @details Only IDF 4.4 does that (method HTTP_GET); on IDF 5.5 the handshake goes to the hooks of
 *          set_ws_handshake_hooks() instead, so this is always false there.
 */
inline bool ws_handshake_in_handler(const httpd_req_t* req)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    (void)req;
    return false;
#else
    return req->method == HTTP_GET;
#endif
}

/**
 * @brief Makes the "minimum free since" counters (heap_min, psram_min) meaningful; call once at the top of setup().
 * @details IDF 5.x zeroes the minimum-free counter of any heap added after startup, and Arduino core 3 adds PSRAM to
 *          the heap after startup (psramAddToHeap in initArduino), so ESP.getMinFreePsram() would stay 0 forever.
 *          On core 3 this restarts every heap's minimum from its current free size, so both counters count from
 *          setup() instead of from boot. Core 2 (IDF 4.4) tracks PSRAM from boot already: no-op there.
 */
inline void start_min_free_tracking()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    heap_caps_monitor_local_minimum_free_size_start(); // never stopped: stop would restore the zeroed minimum
#endif
}
} // namespace compat
