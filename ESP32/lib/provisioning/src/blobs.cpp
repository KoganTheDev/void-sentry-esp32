#include "blobs.h"

#include "crc32.h"

#include <cstring>
#include <span>

namespace prov
{
namespace
{
constexpr uint32_t kNetsMagic = 0x4E575356u; // "VSWN" little-endian
constexpr uint32_t kPinMagic = 0x50575356u;  // "VSWP" little-endian
constexpr uint8_t kVersion = 1;
constexpr size_t kEntrySize = kSsidMax + 1 + kPassMax + 1 + 4;

bool header_ok(std::span<const uint8_t> data, size_t want_len, uint32_t magic)
{
    const size_t len = data.size();
    if (len != want_len)
    {
        return false;
    }
    uint32_t m = 0;
    std::memcpy(&m, data.data(), 4);
    if (m != magic || data[4] != kVersion)
    {
        return false;
    }
    uint32_t stored = 0;
    std::memcpy(&stored, data.data() + len - 4, 4);
    return stored == checksum::crc32(data.first(len - 4));
}
} // namespace

std::array<uint8_t, kNetsBlobSize> serialize_nets(const NetworkList& list)
{
    std::array<uint8_t, kNetsBlobSize> b{};
    std::memcpy(b.data(), &kNetsMagic, 4);
    b[4] = kVersion;
    b[5] = static_cast<uint8_t>(list.size());
    for (size_t i = 0; i < list.size(); ++i)
    {
        uint8_t* p = b.data() + 6 + i * kEntrySize;
        const NetworkEntry& e = list.at(i);
        std::memcpy(p, e.ssid, kSsidMax + 1);
        std::memcpy(p + kSsidMax + 1, e.pass, kPassMax + 1);
        std::memcpy(p + kSsidMax + 1 + kPassMax + 1, &e.last_ok_seq, 4);
    }
    const uint32_t crc = checksum::crc32(std::span<const uint8_t>(b).first(kNetsBlobSize - 4));
    std::memcpy(b.data() + kNetsBlobSize - 4, &crc, 4);
    return b;
}

std::optional<NetworkList> deserialize_nets(std::span<const uint8_t> blob)
{
    if (!header_ok(blob, kNetsBlobSize, kNetsMagic) || blob[5] > kMaxNetworks)
    {
        return std::nullopt;
    }
    NetworkList list;
    for (size_t i = 0; i < blob[5]; ++i)
    {
        const uint8_t* p = blob.data() + 6 + i * kEntrySize;
        NetworkEntry e;
        std::memcpy(e.ssid, p, kSsidMax + 1);
        std::memcpy(e.pass, p + kSsidMax + 1, kPassMax + 1);
        std::memcpy(&e.last_ok_seq, p + kSsidMax + 1 + kPassMax + 1, 4);
        if (!list.restore(e)) // rejects unterminated fields and invalid entries
        {
            return std::nullopt;
        }
    }
    return list;
}

std::array<uint8_t, kPinBlobSize> serialize_pin(const PinRecord& rec)
{
    std::array<uint8_t, kPinBlobSize> b{};
    std::memcpy(b.data(), &kPinMagic, 4);
    b[4] = kVersion;
    b[5] = rec.set ? 1 : 0;
    std::memcpy(b.data() + 6, rec.salt, kSaltLen);
    std::memcpy(b.data() + 6 + kSaltLen, rec.hash, kHashLen);
    const uint32_t crc = checksum::crc32(std::span<const uint8_t>(b).first(kPinBlobSize - 4));
    std::memcpy(b.data() + kPinBlobSize - 4, &crc, 4);
    return b;
}

std::optional<PinRecord> deserialize_pin(std::span<const uint8_t> blob)
{
    if (!header_ok(blob, kPinBlobSize, kPinMagic) || blob[5] > 1)
    {
        return std::nullopt;
    }
    PinRecord rec;
    rec.set = blob[5] == 1;
    std::memcpy(rec.salt, blob.data() + 6, kSaltLen);
    std::memcpy(rec.hash, blob.data() + 6 + kSaltLen, kHashLen);
    return rec;
}
} // namespace prov
