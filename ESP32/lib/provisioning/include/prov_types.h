/**
 * @file prov_types.h
 * @brief Plain value types shared by the WiFi provisioning logic (pure C++20, no heap).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace prov
{
constexpr size_t kMaxNetworks = 3;
constexpr size_t kSsidMax = 32;
constexpr size_t kPassMax = 64; // buffer bound; valid passwords are empty or 8-63 chars
constexpr size_t kSaltLen = 16;
constexpr size_t kHashLen = 32;

/** @brief Length of the NUL-terminated string at @p s, looking at no more than @p cap bytes (cap if unterminated). */
inline size_t bounded_len(const char* s, size_t cap)
{
    size_t n = 0;
    while (n < cap && s[n] != '\0')
    {
        ++n;
    }
    return n;
}

/** @brief One saved network. Strings are NUL-terminated. */
struct NetworkEntry {
    char ssid[kSsidMax + 1] = {};
    char pass[kPassMax + 1] = {};
    uint32_t last_ok_seq = 0; // larger = more recently added or joined
};

/** @brief Salted PIN hash as stored in NVS. */
struct PinRecord {
    bool set = false;
    uint8_t salt[kSaltLen] = {};
    uint8_t hash[kHashLen] = {};
};

/** @brief One access point seen by a scan. */
struct ScanResult {
    char ssid[kSsidMax + 1] = {};
    int8_t rssi = -127;
    bool secured = true;
};

/** @brief Saved networks present in a scan, strongest first. */
struct Candidates {
    uint8_t idx[kMaxNetworks] = {}; // indices into NetworkList, strongest first
    size_t count = 0;
};
} // namespace prov
