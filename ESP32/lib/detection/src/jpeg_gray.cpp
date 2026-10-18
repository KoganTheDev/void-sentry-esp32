#include "jpeg_gray.h"

#include <cstring>
#include <tjpgd_c.h>

namespace jpeg_gray
{
namespace
{
struct DecodeContext {
    const uint8_t* src;
    size_t len;
    size_t index;
    tracking::GrayFrame* out;
};

/// TJpgDec input callback: copy `n` bytes of the JPEG into `buf` (buf == nullptr means skip); returns bytes consumed.
UINT read_jpeg(JDEC* jd, BYTE* buf, UINT n)
{
    auto* ctx = static_cast<DecodeContext*>(jd->device);
    const size_t left = ctx->len - ctx->index;
    const size_t take = n < left ? n : left;
    if (buf)
    {
        memcpy(buf, ctx->src + ctx->index, take);
    }
    ctx->index += take;
    return static_cast<UINT>(take);
}

/// TJpgDec output callback: one RGB888 block of the 1/8-scale image, converted to luma in the 40x30 grid.
UINT write_gray(JDEC* jd, void* bitmap, JRECT* rect)
{
    auto* ctx = static_cast<DecodeContext*>(jd->device);
    const auto* data = static_cast<const uint8_t*>(bitmap);
    const int w = rect->right - rect->left + 1;
    for (int gy = rect->top; gy <= rect->bottom; ++gy)
    {
        for (int gx = rect->left; gx <= rect->right; ++gx)
        {
            if (gx < tracking::kGridW && gy < tracking::kGridH)
            {
                const uint8_t* p = data + 3 * ((gy - rect->top) * w + (gx - rect->left));
                // Rec.601 luma in integer math: (77 R + 150 G + 29 B) / 256
                (*ctx->out)[gy * tracking::kGridW + gx] =
                    static_cast<uint8_t>((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
            }
        }
    }
    return 1;
}
} // namespace

bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out)
{
    if (!jpeg || len == 0)
    {
        return false;
    }
    // 3100 bytes is the pool esp32-camera's esp_jpg_decode used for the same decoder (QVGA, 1/8 scale).
    static uint8_t work[3100] __attribute__((aligned(4)));
    constexpr BYTE kScale8 = 3; // 1/8: DC coefficient only, no IDCT
    DecodeContext ctx{jpeg, len, 0, &out};
    JDEC jd;
    if (jd_prepare(&jd, read_jpeg, work, sizeof(work), &ctx) != JDR_OK)
    {
        return false;
    }
    if ((jd.width >> kScale8) != tracking::kGridW || (jd.height >> kScale8) != tracking::kGridH)
    {
        return false; // not QVGA
    }
    return jd_decomp(&jd, write_gray, kScale8) == JDR_OK;
}
} // namespace jpeg_gray
