#include "calibration_blob.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace aiming
{
namespace
{
constexpr uint32_t kMagic = 0x314D4941u; // "AIM1" little-endian
constexpr uint16_t kVersion = 1;
constexpr size_t kCrcOffset = kBlobSize - 4;

/// Writes the object representation of @p v (host order, little-endian on every target) at @p dst.
template <typename T> void put(uint8_t* dst, T v)
{
    const auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(v);
    std::copy(bytes.begin(), bytes.end(), dst);
}

/// Reads a T from the object representation at @p src.
template <typename T> T get(const uint8_t* src)
{
    std::array<uint8_t, sizeof(T)> bytes{};
    std::copy(src, src + sizeof(T), bytes.begin());
    return std::bit_cast<T>(bytes);
}
} // namespace

Blob serialize(const AimMap& map)
{
    Blob b{};
    put(b.data(), kMagic);
    put(b.data() + 4, kVersion);
    put(b.data() + 6, uint16_t{0}); // reserved
    put(b.data() + 8, map.pan_a);
    put(b.data() + 12, map.pan_b);
    put(b.data() + 16, map.tilt_a);
    put(b.data() + 20, map.tilt_b);
    put(b.data() + kCrcOffset, crc32(std::span<const uint8_t>(b).first(kCrcOffset)));
    return b;
}

std::optional<AimMap> deserialize(std::span<const uint8_t> blob)
{
    if (blob.size() != kBlobSize)
    {
        return std::nullopt;
    }
    const uint8_t* data = blob.data();
    if (get<uint32_t>(data) != kMagic || get<uint16_t>(data + 4) != kVersion ||
        get<uint32_t>(data + kCrcOffset) != crc32(blob.first(kCrcOffset)))
    {
        return std::nullopt;
    }
    const float values[4] = {get<float>(data + 8), get<float>(data + 12), get<float>(data + 16), get<float>(data + 20)};
    for (float v : values)
    {
        if (!std::isfinite(v))
        {
            return std::nullopt;
        }
    }
    return AimMap{.pan_a = values[0], .pan_b = values[1], .tilt_a = values[2], .tilt_b = values[3]};
}
} // namespace aiming
