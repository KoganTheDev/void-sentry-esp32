/**
 * @file nvs_blob.h
 * @brief One read sequence for fixed-size blobs in NVS, shared by the calibration and WiFi stores (device only).
 */

#pragma once

#include <Preferences.h>
#include <cstddef>
#include <cstdint>
#include <span>

namespace nvs_blob
{
/** @brief Outcome of read(). */
enum class ReadStatus
{
    Absent,   ///< the key does not exist (stored length 0)
    Mismatch, ///< stored with a different length than expected, or the read came back short
    Ok        ///< @p out holds exactly the stored bytes
};

/**
 * @brief Reads the blob under @p key into @p out, requiring the stored length to equal out.size().
 * @details Same sequence the stores used inline: length query, size check, then the read with a size check.
 */
inline ReadStatus read(Preferences& prefs, const char* key, std::span<uint8_t> out)
{
    const size_t stored = prefs.getBytesLength(key);
    if (stored == 0)
    {
        return ReadStatus::Absent;
    }
    if (stored != out.size() || prefs.getBytes(key, out.data(), out.size()) != out.size())
    {
        return ReadStatus::Mismatch;
    }
    return ReadStatus::Ok;
}
} // namespace nvs_blob
