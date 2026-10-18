#include "blob_frame.h"

#include "crc32.h"

#include <cstring>

namespace checksum
{
void frame_blob(std::span<uint8_t> blob, uint32_t magic)
{
    if (blob.size() < kFrameOverhead)
    {
        return;
    }
    std::memcpy(blob.data(), &magic, 4);
    const uint32_t crc = crc32(std::span<const uint8_t>(blob).first(blob.size() - 4));
    std::memcpy(blob.data() + blob.size() - 4, &crc, 4);
}

std::optional<std::span<const uint8_t>> unframe_blob(std::span<const uint8_t> blob, uint32_t magic)
{
    if (blob.size() < kFrameOverhead)
    {
        return std::nullopt;
    }
    uint32_t stored_magic = 0;
    std::memcpy(&stored_magic, blob.data(), 4);
    uint32_t stored_crc = 0;
    std::memcpy(&stored_crc, blob.data() + blob.size() - 4, 4);
    if (stored_magic != magic || stored_crc != crc32(blob.first(blob.size() - 4)))
    {
        return std::nullopt;
    }
    return blob.subspan(4, blob.size() - kFrameOverhead);
}
} // namespace checksum
