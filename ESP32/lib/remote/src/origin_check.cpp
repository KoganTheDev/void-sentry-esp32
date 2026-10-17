#include "origin_check.h"

namespace remote
{
namespace
{
bool matches(std::string_view origin, std::string_view scheme, std::string_view host)
{
    return origin.size() == scheme.size() + host.size() && origin.starts_with(scheme) &&
           origin.substr(scheme.size()) == host;
}

bool all_digits(std::string_view s)
{
    for (const char c : s)
    {
        if (c < '0' || c > '9')
        {
            return false;
        }
    }
    return true;
}
} // namespace

bool is_ipv4_host(std::string_view host)
{
    const size_t len = host.size();
    if (len == 0)
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
    const std::string_view port_digits = host.substr(pos);
    if (port_digits.empty() || port_digits.size() > 5 || !all_digits(port_digits))
    {
        return false;
    }
    int port = 0;
    for (const char c : port_digits)
    {
        port = port * 10 + (c - '0');
    }
    return port >= 1 && port <= 65535;
}

bool origin_allowed(std::string_view origin, std::string_view host)
{
    if (origin.empty())
    {
        return true;
    }
    if (host.empty())
    {
        return false;
    }
    if (!is_ipv4_host(host))
    {
        return false;
    }
    return matches(origin, "http://", host) || matches(origin, "https://", host);
}
} // namespace remote
