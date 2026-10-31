#include "session_table.h"

namespace prov
{
namespace
{
/** @brief Every byte is compared whatever differs first, so the time says nothing about a partial match. */
bool equal_ct(const SessionToken& a, const SessionToken& b)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < kTokenLen; ++i)
    {
        diff |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

int hex_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

bool decode_token(std::string_view hex, SessionToken& out)
{
    if (hex.size() != kTokenHexLen)
    {
        return false;
    }
    SessionToken t{};
    for (size_t i = 0; i < kTokenLen; ++i)
    {
        const int hi = hex_value(hex[2 * i]);
        const int lo = hex_value(hex[2 * i + 1]);
        if (hi < 0 || lo < 0)
        {
            return false;
        }
        t[i] = static_cast<uint8_t>(hi * 16 + lo);
    }
    out = t;
    return true;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    {
        s.remove_suffix(1);
    }
    return s;
}
} // namespace

size_t SessionTable::find(const SessionToken& token) const
{
    size_t hit = kCapacity;
    for (size_t i = 0; i < kCapacity; ++i)
    {
        // Compare first, then look at the result: no slot is skipped early.
        const bool same = equal_ct(_slots[i].token, token);
        if (same && _slots[i].used)
        {
            hit = i;
        }
    }
    return hit;
}

void SessionTable::add(const SessionToken& token)
{
    size_t slot = find(token);
    if (slot == kCapacity)
    {
        slot = 0;
        for (size_t i = 0; i < kCapacity; ++i)
        {
            if (!_slots[i].used)
            {
                slot = i;
                break;
            }
            if (_slots[i].last_use < _slots[slot].last_use)
            {
                slot = i; // least recently used so far
            }
        }
    }
    _slots[slot].token = token;
    _slots[slot].used = true;
    _slots[slot].last_use = ++_clock;
}

bool SessionTable::contains(const SessionToken& token)
{
    const size_t slot = find(token);
    if (slot == kCapacity)
    {
        return false;
    }
    _slots[slot].last_use = ++_clock;
    return true;
}

bool SessionTable::remove(const SessionToken& token)
{
    const size_t slot = find(token);
    if (slot == kCapacity)
    {
        return false;
    }
    _slots[slot] = Slot{};
    return true;
}

void SessionTable::clear() { _slots = {}; }

size_t SessionTable::size() const
{
    size_t n = 0;
    for (const Slot& s : _slots)
    {
        n += s.used ? 1 : 0;
    }
    return n;
}

bool session_cookie(std::string_view header, SessionToken& out)
{
    if (header.size() > kMaxCookieHeader)
    {
        return false;
    }
    while (!header.empty())
    {
        const size_t semi = header.find(';');
        const std::string_view pair = header.substr(0, semi);
        header = semi == std::string_view::npos ? std::string_view() : header.substr(semi + 1);

        const size_t eq = pair.find('=');
        if (eq == std::string_view::npos)
        {
            continue;
        }
        if (trim(pair.substr(0, eq)) == kSessionCookie && decode_token(trim(pair.substr(eq + 1)), out))
        {
            return true;
        }
    }
    return false;
}

void token_hex(const SessionToken& token, char (&out)[kTokenHexLen + 1])
{
    static constexpr char kHex[] = "0123456789abcdef";
    for (size_t i = 0; i < kTokenLen; ++i)
    {
        out[2 * i] = kHex[token[i] >> 4];
        out[2 * i + 1] = kHex[token[i] & 0x0F];
    }
    out[kTokenHexLen] = '\0';
}
} // namespace prov
