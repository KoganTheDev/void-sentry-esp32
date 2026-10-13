/**
 * @file calibration_blob.h
 * @brief Versioned, CRC-checked byte layout for saving an AimMap to NVS (pure C++17, natively tested).
 * @details Layout (little-endian, 28 bytes): magic "AIM1" (u32), version (u16, = 1), reserved (u16, 0),
 * pan_a, pan_b, tilt_a, tilt_b (4 x f32), CRC-32 of the first 24 bytes (u32). Both the ESP32 and x86 are
 * little-endian, so fields are copied with memcpy. Anything that fails a check deserializes to nothing, and the
 * caller falls back to default_map().
 */

#pragma once

#include "aim_map.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace aiming
{
constexpr size_t kBlobSize = 28;
using Blob = std::array<uint8_t, kBlobSize>;

uint32_t crc32(const uint8_t* data, size_t len);
Blob serialize(const AimMap& map);
std::optional<AimMap> deserialize(const uint8_t* data, size_t len);
} // namespace aiming
