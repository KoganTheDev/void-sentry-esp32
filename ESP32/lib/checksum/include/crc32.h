/**
 * @file crc32.h
 * @brief Shared CRC-32 for versioned NVS blobs (pure C++20, natively tested).
 */

#pragma once

#include <cstdint>
#include <span>

namespace checksum
{
/**
 * @brief CRC-32 (IEEE 802.3, reflected, init 0xFFFFFFFF, final xor 0xFFFFFFFF).
 * @details Table-driven; the 256-entry table is built at compile time and lives in flash.
 */
uint32_t crc32(std::span<const uint8_t> data);
} // namespace checksum
