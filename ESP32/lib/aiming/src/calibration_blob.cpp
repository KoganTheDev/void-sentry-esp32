#include "calibration_blob.h"

#include <cmath>
#include <cstring>

namespace aiming
{
namespace
{
constexpr uint32_t kMagic = 0x314D4941u; // "AIM1" little-endian
constexpr uint16_t kVersion = 1;
constexpr size_t kCrcOffset = kBlobSize - 4;
} // namespace

Blob serialize(const AimMap& map)
{
    Blob b{};
    const uint16_t reserved = 0;
    const float values[4] = {map.pan_a, map.pan_b, map.tilt_a, map.tilt_b};
    std::memcpy(b.data(), &kMagic, 4);
    std::memcpy(b.data() + 4, &kVersion, 2);
    std::memcpy(b.data() + 6, &reserved, 2);
    std::memcpy(b.data() + 8, values, 16);
    const uint32_t crc = crc32(b.data(), kCrcOffset);
    std::memcpy(b.data() + kCrcOffset, &crc, 4);
    return b;
}

std::optional<AimMap> deserialize(const uint8_t* data, size_t len)
{
    if (!data || len != kBlobSize)
    {
        return std::nullopt;
    }
    uint32_t magic = 0;
    uint16_t version = 0;
    uint32_t stored_crc = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 2);
    std::memcpy(&stored_crc, data + kCrcOffset, 4);
    if (magic != kMagic || version != kVersion || stored_crc != crc32(data, kCrcOffset))
    {
        return std::nullopt;
    }
    float values[4];
    std::memcpy(values, data + 8, 16);
    for (float v : values)
    {
        if (!std::isfinite(v))
        {
            return std::nullopt;
        }
    }
    return AimMap{values[0], values[1], values[2], values[3]};
}
} // namespace aiming
