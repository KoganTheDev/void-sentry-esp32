/**
 * @file jpeg_check.h
 * @brief Cheap integrity check for camera JPEG frames (pure C++, no platform headers).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace frames
{
/**
 * @brief True if the buffer starts with the JPEG SOI marker (FF D8) and ends with the EOI marker (FF D9).
 * @details A frame cut off mid-transfer has no EOI and fails this check.
 */
inline bool is_complete_jpeg(const uint8_t* data, size_t length)
{
    if (data == nullptr || length < 4)
    {
        return false;
    }
    return data[0] == 0xFF && data[1] == 0xD8 && data[length - 2] == 0xFF && data[length - 1] == 0xD9;
}
} // namespace frames
