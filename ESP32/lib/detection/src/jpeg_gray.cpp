#include "jpeg_gray.h"

#include <cstring>
#include <platform_compat.h>

namespace jpeg_gray
{
namespace
{
struct DecodeContext {
    const uint8_t* src;
    size_t len;
    tracking::GrayFrame* out;
    bool size_ok;
};

size_t read_jpeg(void* arg, size_t index, uint8_t* buf, size_t len)
{
    auto* ctx = static_cast<DecodeContext*>(arg);
    if (index >= ctx->len)
    {
        return 0;
    }
    if (len > ctx->len - index)
    {
        len = ctx->len - index;
    }
    if (buf)
    {
        memcpy(buf, ctx->src + index, len);
    }
    return len; // buf == NULL means "skip len bytes"
}

bool write_gray(void* arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* data)
{
    auto* ctx = static_cast<DecodeContext*>(arg);
    if (!data)
    {
        if (x == 0 && y == 0) // start of image: w/h are the scaled output size
        {
            ctx->size_ok = (w == tracking::kGridW && h == tracking::kGridH);
        }
        return ctx->size_ok;
    }
    if (!ctx->size_ok)
    {
        return false; // aborts the decode
    }
    for (uint16_t row = 0; row < h; ++row)
    {
        const int gy = y + row;
        for (uint16_t col = 0; col < w; ++col)
        {
            const int gx = x + col;
            if (gx >= tracking::kGridW || gy >= tracking::kGridH)
            {
                continue;
            }
            const uint8_t* p = data + 3 * (row * w + col);
            // Rec.601 luma in integer math: (77 R + 150 G + 29 B) / 256
            (*ctx->out)[gy * tracking::kGridW + gx] = static_cast<uint8_t>((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
        }
    }
    return true;
}
} // namespace

bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out)
{
    if (!jpeg || len == 0)
    {
        return false;
    }
    DecodeContext ctx{jpeg, len, &out, false};
    return compat::jpg_decode_8x(len, read_jpeg, write_gray, &ctx) == ESP_OK && ctx.size_ok;
}
} // namespace jpeg_gray
