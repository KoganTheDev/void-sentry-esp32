#pragma once

#include <cstddef>

namespace remote
{
/**
 * @brief Copies untrusted bytes into a fixed buffer that is safe to print to the serial log.
 * @details Request headers and SSIDs come from the network, so they may hold control bytes (ESC sequences that
 * repaint a terminal, CR/LF that forge log lines) or NULs that cut a "%.*s" short. Every byte below 0x20 or above 0x7e
 * is replaced by '?'. At most cap - 1 bytes are copied and the result is always NUL-terminated. Never use it on a
 * password. No heap.
 * @param src Source bytes (need not be NUL-terminated; may be null if len is 0).
 * @param len Number of source bytes to consider.
 * @param dst Destination buffer.
 * @param cap Capacity of dst in bytes, NUL included. If 0, nothing is written.
 * @return Number of characters written, excluding the NUL.
 */
size_t sanitize_for_log(const char* src, size_t len, char* dst, size_t cap);
} // namespace remote
