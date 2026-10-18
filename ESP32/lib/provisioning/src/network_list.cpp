#include "network_list.h"

#include "form.h"

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

void fill(NetworkEntry& e, const char* ssid, size_t ssid_len, const char* pass, size_t pass_len, uint32_t seq)
{
    e = NetworkEntry{};
    std::memcpy(e.ssid, ssid, ssid_len);
    if (pass_len > 0)
    {
        std::memcpy(e.pass, pass, pass_len);
    }
    e.last_ok_seq = seq;
}
} // namespace

NetworkList::AddResult NetworkList::add(const char* ssid, size_t ssid_len, const char* pass, size_t pass_len)
{
    if (!valid_ssid({ssid, ssid_len}) || !valid_password({pass, pass_len}))
    {
        return AddResult::Invalid;
    }
    const int existing = find(ssid, ssid_len);
    if (existing >= 0)
    {
        fill(_entries[existing], ssid, ssid_len, pass, pass_len, ++_seq);
        return AddResult::Replaced;
    }
    if (_count < kMaxNetworks)
    {
        fill(_entries[_count++], ssid, ssid_len, pass, pass_len, ++_seq);
        return AddResult::Added;
    }
    size_t victim = 0;
    for (size_t i = 1; i < _count; ++i)
    {
        if (_entries[i].last_ok_seq < _entries[victim].last_ok_seq)
        {
            victim = i;
        }
    }
    fill(_entries[victim], ssid, ssid_len, pass, pass_len, ++_seq);
    return AddResult::Evicted;
}

bool NetworkList::forget(const char* ssid, size_t ssid_len)
{
    const int i = find(ssid, ssid_len);
    if (i < 0)
    {
        return false;
    }
    for (size_t j = static_cast<size_t>(i); j + 1 < _count; ++j)
    {
        _entries[j] = _entries[j + 1];
    }
    --_count;
    _entries[_count] = NetworkEntry{};
    return true;
}

void NetworkList::mark_joined(size_t index)
{
    if (index < _count)
    {
        _entries[index].last_ok_seq = ++_seq;
    }
}

int NetworkList::find(const char* ssid, size_t ssid_len) const
{
    if (ssid == nullptr)
    {
        return -1;
    }
    for (size_t i = 0; i < _count; ++i)
    {
        if (std::strlen(_entries[i].ssid) == ssid_len && std::memcmp(_entries[i].ssid, ssid, ssid_len) == 0)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

size_t NetworkList::size() const { return _count; }

const NetworkEntry& NetworkList::at(size_t index) const { return _entries[index]; }

bool NetworkList::restore(const NetworkEntry& e)
{
    if (_count >= kMaxNetworks)
    {
        return false;
    }
    const size_t ssid_len = bounded_len(e.ssid, sizeof(e.ssid));
    const size_t pass_len = bounded_len(e.pass, sizeof(e.pass));
    if (ssid_len == sizeof(e.ssid) || pass_len == sizeof(e.pass) || !valid_ssid({e.ssid, ssid_len}) ||
        !valid_password({e.pass, pass_len}))
    {
        return false;
    }
    _entries[_count++] = e;
    if (e.last_ok_seq > _seq)
    {
        _seq = e.last_ok_seq;
    }
    return true;
}

uint32_t NetworkList::seq() const { return _seq; }

void NetworkList::clear()
{
    for (NetworkEntry& e : _entries)
    {
        e = NetworkEntry{};
    }
    _count = 0;
    _seq = 0;
}
} // namespace prov
