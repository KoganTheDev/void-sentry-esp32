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

// 1-5 digits, value 1..65535.
bool valid_port(std::string_view digits)
{
    if (digits.empty() || digits.size() > 5 || !all_digits(digits))
    {
        return false;
    }
    int port = 0;
    for (const char c : digits)
    {
        port = port * 10 + (c - '0');
    }
    return port >= 1 && port <= 65535;
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
    return valid_port(host.substr(pos));
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

bool origin_allowed_any_port(std::string_view origin, std::string_view host)
{
    if (origin.empty())
    {
        return true;
    }
    if (!is_ipv4_host(host))
    {
        return false;
    }
    const std::string_view ip = host.substr(0, host.find(':'));
    for (const std::string_view scheme : {std::string_view("http://"), std::string_view("https://")})
    {
        if (origin.starts_with(scheme) && origin.substr(scheme.size()).starts_with(ip))
        {
            const std::string_view rest = origin.substr(scheme.size() + ip.size());
            if (rest.empty() || (rest.front() == ':' && valid_port(rest.substr(1))))
            {
                return true;
            }
        }
    }
    return false;
}
} // namespace remote
