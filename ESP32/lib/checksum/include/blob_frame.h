/**
 * @file blob_frame.h
 * @brief Shared framing for versioned NVS blobs: magic, body, CRC-32 (pure C++20, natively tested).
 * @details Layout (host byte order, little-endian on every target): magic (u32), body (any length, version and
 * fields included), CRC-32 of everything before it (u32). The caller owns the body layout and its version byte(s).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace checksum
{
/** @brief Bytes the frame adds around the body: the magic in front and the CRC behind. */
constexpr size_t kFrameOverhead = 8;

/**
 * @brief Stamps @p magic into the first four bytes and the CRC-32 of all but the last four bytes into the last four.
 * @details The caller has already written the body at offset 4. @p blob must be at least kFrameOverhead bytes.
 */
void frame_blob(std::span<uint8_t> blob, uint32_t magic);

/**
 * @brief Checks the magic and the CRC; returns the body (bytes 4 .. size-4), or empty if either check fails.
 * @details Length policy stays with the caller (compare blob.size() to the expected size first).
 */
std::optional<std::span<const uint8_t>> unframe_blob(std::span<const uint8_t> blob, uint32_t magic);
} // namespace checksum
