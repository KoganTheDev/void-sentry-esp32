#include "choose.h"

#include <cstring>

namespace prov
{
namespace
{
size_t bounded_len(const char* s, size_t cap)
{
    size_t n = 0;
    while (n < cap && s[n] != '\0')
    {
        ++n;
    }
    return n;
}
} // namespace

Candidates choose_networks(std::span<const ScanResult> scan, const NetworkList& list)
{
    Candidates out;
    int best[kMaxNetworks] = {};
    bool present[kMaxNetworks] = {};
    const size_t n = list.size();
    for (size_t i = 0; i < n; ++i)
    {
        const size_t len = std::strlen(list.at(i).ssid);
        best[i] = -1000;
        for (size_t s = 0; s < scan.size(); ++s)
        {
            const size_t slen = bounded_len(scan[s].ssid, sizeof(scan[s].ssid));
            if (slen == 0 || slen != len || std::memcmp(scan[s].ssid, list.at(i).ssid, len) != 0)
            {
                continue;
            }
            present[i] = true;
            if (scan[s].rssi > best[i])
            {
                best[i] = scan[s].rssi;
            }
        }
    }
    // Repeatedly take the strongest remaining entry; a strict greater-than keeps list order on ties.
    bool used[kMaxNetworks] = {};
    for (;;)
    {
        int pick = -1;
        for (size_t i = 0; i < n; ++i)
        {
            if (present[i] && !used[i] && (pick < 0 || best[i] > best[pick]))
            {
                pick = static_cast<int>(i);
            }
        }
        if (pick < 0)
        {
            break;
        }
        used[pick] = true;
        out.idx[out.count++] = static_cast<uint8_t>(pick);
    }
    return out;
}
} // namespace prov
