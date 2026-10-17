#include "crc32.h"

#include <array>

namespace checksum
{
namespace
{
constexpr std::array<uint32_t, 256> make_table()
{
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i)
    {
        uint32_t crc = i;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        table[i] = crc;
    }
    return table;
}

constexpr std::array<uint32_t, 256> kTable = make_table();
static_assert(kTable[1] == 0x77073096u && kTable[255] == 0x2D02EF8Du, "CRC-32 table is wrong");

constexpr uint32_t crc32_impl(std::span<const uint8_t> data)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (const uint8_t byte : data)
    {
        crc = (crc >> 8) ^ kTable[(crc ^ byte) & 0xFFu];
    }
    return ~crc;
}

constexpr uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
static_assert(crc32_impl(kCheck) == 0xCBF43926u, "CRC-32 check value is wrong");
} // namespace

uint32_t crc32(std::span<const uint8_t> data) { return crc32_impl(data); }
} // namespace checksum
