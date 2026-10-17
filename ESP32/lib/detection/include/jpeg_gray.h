/**
 * @file jpeg_gray.h
 * @brief Decodes a QVGA camera JPEG at 1/8 scale straight into the 40x30 gray grid the tracker uses.
 * @details Uses esp32-camera's esp_jpg_decode (TJpgDec). At 1/8 scale the ROM decoder keeps only each 8x8 block's
 * DC coefficient (the block average) and skips the IDCT entirely: measured ~7 ms per frame on this board versus
 * ~71 ms at 1/4 scale (1/2 and 1/4 run the full IDCT and then average). The block average also filters noise.
 * No allocation; NOT re-entrant (the decoder's work buffer is static), so only the detection task may call it.
 */

#pragma once

#include "tracking_types.h"

#include <cstddef>
#include <cstdint>

namespace jpeg_gray
{
/** @brief false if the JPEG is corrupt or its 1/8-scale size is not 40x30 (frame size must be QVGA). */
bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out);
} // namespace jpeg_gray
