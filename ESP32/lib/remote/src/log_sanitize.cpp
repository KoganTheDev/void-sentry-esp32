#include "log_sanitize.h"

namespace remote
{
size_t sanitize_for_log(const char* src, size_t len, char* dst, size_t cap)
{
    if (dst == nullptr || cap == 0)
    {
        return 0;
    }
    size_t n = 0;
    while (n < len && n + 1 < cap)
    {
        const unsigned char c = static_cast<unsigned char>(src[n]);
        dst[n] = (c < 0x20 || c > 0x7e) ? '?' : static_cast<char>(c);
        ++n;
    }
    dst[n] = '\0';
    return n;
}
} // namespace remote
