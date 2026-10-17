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
} // namespace

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
    return matches(origin, origin_len, "http://", 7, host, host_len) ||
           matches(origin, origin_len, "https://", 8, host, host_len);
}
} // namespace remote
