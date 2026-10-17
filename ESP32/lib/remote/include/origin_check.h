#pragma once

#include <cstddef>

namespace remote
{
/**
 * @brief Decides whether a WebSocket handshake may proceed, based on its Origin and Host headers.
 * @details Browsers let any web page open a WebSocket to a LAN address and always attach the page's Origin, so a
 * page from another site can be refused by requiring Origin to be exactly "http://" + Host or "https://" + Host
 * (Host includes any ":port"). The Host must also be an IPv4 literal (see is_ipv4_host): the board is always reached
 * by IP, and this defeats DNS rebinding, where an attacker's hostname is re-pointed at the board's LAN address so the
 * browser sends a matching Origin "http://evil.com" and Host "evil.com". A missing Origin (origin == nullptr or
 * origin_len == 0) is allowed: non-browser clients send none. A present Origin with a missing Host is rejected. The
 * comparison is byte-exact, case-sensitive, and reads at most the given lengths (the strings need not be
 * NUL-terminated). No heap.
 * @param origin Origin header value, or nullptr.
 * @param origin_len Length of origin in bytes.
 * @param host Host header value, or nullptr.
 * @param host_len Length of host in bytes.
 * @return true if the handshake is allowed.
 */
bool origin_allowed(const char* origin, size_t origin_len, const char* host, size_t host_len);

/**
 * @brief Checks that a Host header value is an IPv4 literal with an optional port.
 * @details Accepts four decimal octets (1-3 digits each, 0-255) separated by dots, optionally followed by ":" and a
 * port of 1-5 digits (1-65535). Hostnames, IPv6, empty octets, signs and trailing dots are rejected. Reads at most len
 * bytes (NUL termination not needed). No heap.
 * @param host Host header value, or nullptr.
 * @param len Length of host in bytes.
 * @return true if host is a valid IPv4 literal.
 */
bool is_ipv4_host(const char* host, size_t len);
} // namespace remote
