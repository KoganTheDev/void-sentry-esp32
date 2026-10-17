/**
 * @file crc32.h
 * @brief Shared CRC-32 for versioned NVS blobs (pure C++17, natively tested).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace checksum
{
/** @brief CRC-32 (IEEE 802.3, reflected, init 0xFFFFFFFF, final xor 0xFFFFFFFF). */
uint32_t crc32(const uint8_t* data, size_t len);
} // namespace checksum
