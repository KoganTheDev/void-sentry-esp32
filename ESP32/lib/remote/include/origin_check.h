#pragma once

#include <cstddef>

namespace remote
{
/**
 * @brief Decides whether a WebSocket handshake may proceed, based on its Origin and Host headers.
 * @details Browsers let any web page open a WebSocket to a LAN address and always attach the page's Origin, so a
 * page from another site can be refused by requiring Origin to be exactly "http://" + Host or "https://" + Host
 * (Host includes any ":port"). A missing Origin (origin == nullptr or origin_len == 0) is allowed: non-browser clients
 * send none. A present Origin with a missing Host is rejected. The comparison is byte-exact, case-sensitive, and
 * reads at most the given lengths (the strings need not be NUL-terminated). No heap.
 * @param origin Origin header value, or nullptr.
 * @param origin_len Length of origin in bytes.
 * @param host Host header value, or nullptr.
 * @param host_len Length of host in bytes.
 * @return true if the handshake is allowed.
 */
bool origin_allowed(const char* origin, size_t origin_len, const char* host, size_t host_len);
} // namespace remote
