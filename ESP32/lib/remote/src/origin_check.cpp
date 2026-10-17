#include "origin_check.h"

#include <cstring>

namespace remote
{
namespace
{
bool matches(const char* origin, size_t origin_len, const char* scheme, size_t scheme_len, const char* host,
             size_t host_len)
{
    return origin_len == scheme_len + host_len && std::memcmp(origin, scheme, scheme_len) == 0 &&
           std::memcmp(origin + scheme_len, host, host_len) == 0;
}

bool all_digits(const char* s, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            return false;
        }
    }
    return true;
}
} // namespace

bool is_ipv4_host(const char* host, size_t len)
{
    if (host == nullptr || len == 0)
    {
        return false;
    }
    size_t pos = 0;
    for (int octet = 0; octet < 4; ++octet)
    {
        size_t start = pos;
        int value = 0;
        while (pos < len && host[pos] >= '0' && host[pos] <= '9' && pos - start < 3)
        {
            value = value * 10 + (host[pos] - '0');
            ++pos;
        }
        if (pos == start || value > 255)
        {
            return false;
        }
        if (octet < 3)
        {
            if (pos >= len || host[pos] != '.')
            {
                return false;
            }
            ++pos;
        }
    }
    if (pos == len)
    {
        return true;
    }
    if (host[pos] != ':')
    {
        return false;
    }
    ++pos;
    size_t port_len = len - pos;
    if (port_len < 1 || port_len > 5 || !all_digits(host + pos, port_len))
    {
        return false;
    }
    int port = 0;
    for (size_t i = pos; i < len; ++i)
    {
        port = port * 10 + (host[i] - '0');
    }
    return port >= 1 && port <= 65535;
}

bool origin_allowed(const char* origin, size_t origin_len, const char* host, size_t host_len)
{
    if (origin == nullptr || origin_len == 0)
    {
        return true;
    }
    if (host == nullptr || host_len == 0)
    {
        return false;
    }
    if (!is_ipv4_host(host, host_len))
    {
        return false;
    }
    return matches(origin, origin_len, "http://", 7, host, host_len) ||
           matches(origin, origin_len, "https://", 8, host, host_len);
}
} // namespace remote
