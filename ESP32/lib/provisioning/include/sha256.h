/**
 * @file sha256.h
 * @brief SHA-256 (FIPS 180-4), pure C++17, no heap.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace prov
{
/** @brief SHA-256 digest of a buffer. @p data may be null only when @p len is 0. */
std::array<uint8_t, 32> sha256(const uint8_t* data, size_t len);
} // namespace prov
