/**
 * @file jpeg_gray.h
 * @brief Decodes a QVGA camera JPEG at 1/8 scale straight into the 40x30 gray grid the tracker uses.
 * @details Uses the vendored C TJpgDec (lib/tjpgd). At 1/8 scale the decoder keeps only each 8x8
 * block's DC coefficient (the block average) and skips the IDCT entirely (1/2 and 1/4 run the full IDCT and then
 * average, ~71 ms at 1/4). The block average also filters noise. The ESP32 ROM has the same decoder, but it is ~40%
 * slower (Stage 7: 14.1 ms vs 9.0 ms for one 4.1 KB frame on core 3), so it is not used.
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
