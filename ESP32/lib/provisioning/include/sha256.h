/**
 * @file sha256.h
 * @brief SHA-256 (FIPS 180-4), pure C++20, no heap.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace prov
{
/** @brief SHA-256 digest of a buffer. An empty span hashes the empty message. */
std::array<uint8_t, 32> sha256(std::span<const uint8_t> data);
} // namespace prov
