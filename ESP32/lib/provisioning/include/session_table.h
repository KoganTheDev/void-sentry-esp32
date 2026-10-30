/**
 * @file session_table.h
 * @brief HUD login sessions: a fixed table of random tokens and the session cookie parser (pure C++20, no heap).
 * @details The board hands a browser a 128-bit random token after a correct PIN (POST /api/hud/login) as the cookie
 * `vs_session=<32 hex digits>`. The table lives in RAM only, so every session ends at a reboot. Token generation
 * (esp_fill_random) stays in the device glue; nothing here needs randomness.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace prov
{
constexpr size_t kTokenLen = 16;               ///< 128 bits
constexpr size_t kTokenHexLen = 2 * kTokenLen; ///< hex digits in the cookie value
constexpr size_t kMaxCookieHeader = 1024;      ///< longer Cookie headers are ignored (httpd caps headers at 1 KB)
constexpr std::string_view kSessionCookie = "vs_session";

using SessionToken = std::array<uint8_t, kTokenLen>;

/**
 * @brief Up to kCapacity live login tokens; adding one more evicts the least recently used.
 * @details "Used" means added or matched by contains(), so a HUD that keeps polling keeps its session while an
 * abandoned browser is the one evicted. contains() compares every byte of every slot whatever matches, so its time
 * does not depend on how much of a guessed token is right. Not thread-safe: the owner guards it with a lock.
 */
class SessionTable
{
public:
    static constexpr size_t kCapacity = 4;

    /** @brief Stores a token (refreshes it if already present); a full table evicts the least recently used. */
    void add(const SessionToken& token);

    /** @brief True if the token is live; a hit counts as a use for the LRU order. Constant time in the token. */
    bool contains(const SessionToken& token);

    /** @brief Forgets a token; false if it was not there. */
    bool remove(const SessionToken& token);

    /** @brief Forgets every token (logout everywhere: the PIN changed or the board is reset). */
    void clear();

    /** @brief Number of live tokens (0 to kCapacity). */
    size_t size() const;

private:
    struct Slot {
        SessionToken token{};
        uint64_t last_use = 0;
        bool used = false;
    };

    /** @brief Index of the slot holding token, or kCapacity. Constant time in the token. */
    size_t find(const SessionToken& token) const;

    std::array<Slot, kCapacity> _slots{};
    uint64_t _clock = 0; ///< use counter for the LRU order; 64 bits never wraps
};

/**
 * @brief Extracts the vs_session token from a Cookie request header value.
 * @details The header is `name=value` pairs separated by ';' with optional spaces or tabs around names and values
 * (RFC 6265). The first pair named exactly vs_session whose value is exactly 32 hex digits (either case) wins; pairs
 * without '=' and other cookies are skipped. A header longer than kMaxCookieHeader yields nothing. Reads at most the
 * given length (no NUL needed).
 * @param header Cookie header value; empty if the header is absent.
 * @param out Receives the token; unchanged when false is returned.
 * @return true if a token was found.
 */
bool session_cookie(std::string_view header, SessionToken& out);

/** @brief Writes the token as 32 lowercase hex digits and a NUL (the Set-Cookie value). */
void token_hex(const SessionToken& token, char (&out)[kTokenHexLen + 1]);
} // namespace prov
