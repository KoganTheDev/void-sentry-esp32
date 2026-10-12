/**
 * @file jpeg_gray.h
 * @brief Decodes a QVGA camera JPEG at 1/4 scale straight into the 80x60 gray grid the tracker uses.
 * @details Uses esp32-camera's esp_jpg_decode (TJpgDec), whose descaling outputs each 8x8 JPEG block as 2x2
 * averaged pixels - cheap, and the averaging filters noise. No allocation; NOT re-entrant (the decoder's work
 * buffer is static), so only the loop() task may call it.
 */

#pragma once

#include "tracking_types.h"

#include <cstddef>
#include <cstdint>

namespace jpeg_gray
{
/** @brief false if the JPEG is corrupt or its 1/4-scale size is not 80x60 (frame size must be QVGA). */
bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out);
} // namespace jpeg_gray
