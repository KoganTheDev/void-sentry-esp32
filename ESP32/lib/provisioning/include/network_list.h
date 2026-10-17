/**
 * @file network_list.h
 * @brief Fixed-capacity list of saved WiFi networks with recency-based eviction (pure C++17, no heap).
 */

#pragma once

#include "prov_types.h"

namespace prov
{
/** @brief Up to kMaxNetworks saved networks; the least recently added or joined one is evicted when full. */
class NetworkList
{
public:
    enum class AddResult { Added, Replaced, Evicted, Invalid };

    /** @brief Adds or updates a network. SSID 1-32 bytes, password empty or 8-63 printable ASCII bytes. */
    AddResult add(const char* ssid, size_t ssid_len, const char* pass, size_t pass_len);
    /** @brief Removes a network by byte-exact SSID; false if absent. */
    bool forget(const char* ssid, size_t ssid_len);
    /** @brief Marks an entry as just joined (last_ok_seq = ++seq). */
    void mark_joined(size_t index);
    /** @brief Index of the byte-exact SSID, or -1. */
    int find(const char* ssid, size_t ssid_len) const;
    size_t size() const;
    const NetworkEntry& at(size_t index) const;
    /** @brief Appends an entry as loaded from storage; false if full or the entry is invalid. */
    bool restore(const NetworkEntry& e);
    uint32_t seq() const;
    void clear();

private:
    NetworkEntry _entries[kMaxNetworks];
    size_t _count = 0;
    uint32_t _seq = 0;
};
} // namespace prov
