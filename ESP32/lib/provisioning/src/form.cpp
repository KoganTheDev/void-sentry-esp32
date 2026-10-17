#include "form.h"

#include <cstring>

namespace prov
{
namespace
{
int hex_val(char c)
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

// Decodes s[0..n) into out (out_cap bytes incl. the NUL). False on a bad escape, a decoded NUL or no room.
bool decode(const char* s, size_t n, char* out, size_t out_cap, size_t* out_len)
{
    size_t o = 0;
    for (size_t i = 0; i < n; ++i)
    {
        char c = s[i];
        if (c == '+')
        {
            c = ' ';
        } else if (c == '%')
        {
            if (i + 2 >= n)
            {
                return false;
            }
            const int hi = hex_val(s[i + 1]);
            const int lo = hex_val(s[i + 2]);
            if (hi < 0 || lo < 0 || (hi == 0 && lo == 0))
            {
                return false;
            }
            c = static_cast<char>(hi * 16 + lo);
            i += 2;
        }
        if (o + 1 >= out_cap)
        {
            return false;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    if (out_len != nullptr)
    {
        *out_len = o;
    }
    return true;
}
} // namespace

bool form_field(const char* body, size_t len, const char* key, char* out, size_t out_cap, size_t* out_len)
{
    if (body == nullptr || key == nullptr || out == nullptr || out_cap == 0 || len > kMaxFormBody)
    {
        return false;
    }
    const size_t key_len = std::strlen(key);
    size_t pos = 0;
    while (pos <= len)
    {
        size_t end = pos;
        while (end < len && body[end] != '&')
        {
            ++end;
        }
        if (end - pos > key_len && std::memcmp(body + pos, key, key_len) == 0 && body[pos + key_len] == '=')
        {
            const size_t vstart = pos + key_len + 1;
            return decode(body + vstart, end - vstart, out, out_cap, out_len);
        }
        pos = end + 1;
    }
    return false;
}

bool valid_ssid(const char* s, size_t len) { return s != nullptr && len >= 1 && len <= 32; }

bool valid_password(const char* s, size_t len)
{
    if (len == 0)
    {
        return true;
    }
    if (s == nullptr || len < 8 || len > 63)
    {
        return false;
    }
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 32 || c > 126)
        {
            return false;
        }
    }
    return true;
}

bool valid_pin(const char* s, size_t len)
{
    if (s == nullptr || len < 4 || len > 8)
    {
        return false;
    }
    for (size_t i = 0; i < len; ++i)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            return false;
        }
    }
    return true;
}
} // namespace prov
