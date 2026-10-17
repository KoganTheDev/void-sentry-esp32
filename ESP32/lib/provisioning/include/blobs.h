/**
 * @file blobs.h
 * @brief Versioned, CRC-checked byte layouts for the saved networks and the PIN record (pure C++20, no heap).
 * @details Little-endian: magic (u32; "VSWN" nets, "VSWP" pin), version (u8 = 1), count (nets) or set (pin) (u8),
 * fixed-size fields with unused entries zeroed, then CRC-32 of everything before it. Anything that fails a check
 * deserializes to nothing.
 */

#pragma once

#include "network_list.h"

#include <array>
#include <optional>
#include <span>

namespace prov
{
constexpr size_t kNetsBlobSize = 4 + 1 + 1 + kMaxNetworks * (kSsidMax + 1 + kPassMax + 1 + 4) + 4;
constexpr size_t kPinBlobSize = 4 + 1 + 1 + kSaltLen + kHashLen + 4;

/** @brief Packs the list into the nets blob. */
std::array<uint8_t, kNetsBlobSize> serialize_nets(const NetworkList& list);
/** @brief Parses a nets blob; empty on wrong length, magic, version, CRC, count, unterminated or invalid fields. */
std::optional<NetworkList> deserialize_nets(std::span<const uint8_t> blob);
/** @brief Packs the PIN record into the pin blob. */
std::array<uint8_t, kPinBlobSize> serialize_pin(const PinRecord& rec);
/** @brief Parses a pin blob; empty on wrong length, magic, version, CRC or set byte. */
std::optional<PinRecord> deserialize_pin(std::span<const uint8_t> blob);
} // namespace prov
