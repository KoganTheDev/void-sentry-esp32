/**
 * @file test_provisioning_store.cpp
 * @brief Host-side tests for CRC-32, the saved-network list, the network chooser and the NVS blob formats.
 */

#include <unity.h>

#include "aim_map.h"
#include "blobs.h"
#include "calibration_blob.h"
#include "choose.h"
#include "crc32.h"
#include "network_list.h"

#include <array>
#include <cstring>
#include <span>

using namespace prov;

void setUp(void) {}
void tearDown(void) {}

static NetworkList::AddResult add(NetworkList& l, const char* ssid, const char* pass)
{
    return l.add(ssid, std::strlen(ssid), pass, std::strlen(pass));
}

static int find(const NetworkList& l, const char* ssid) { return l.find(ssid, std::strlen(ssid)); }

static ScanResult scan_entry(const char* ssid, int8_t rssi)
{
    ScanResult r;
    std::strncpy(r.ssid, ssid, kSsidMax);
    r.rssi = rssi;
    return r;
}

// ---- crc32 ----

void test_crc32_known_value(void)
{
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, checksum::crc32({reinterpret_cast<const uint8_t*>(s), 9}));
    TEST_ASSERT_EQUAL_HEX32(0u, checksum::crc32({}));
}

void test_aiming_crc32_alias_matches(void)
{
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, aiming::crc32({reinterpret_cast<const uint8_t*>(s), 9}));
}

void test_calibration_blob_still_round_trips(void)
{
    const aiming::AimMap m{.pan_a = 10.0f, .pan_b = 0.5f, .tilt_a = -3.0f, .tilt_b = 0.25f};
    const aiming::Blob b = aiming::serialize(m);
    const auto back = aiming::deserialize(b);
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_EQUAL_FLOAT(10.0f, back->pan_a);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, back->tilt_b);
}

// ---- NetworkList ----

void test_list_add_and_find(void)
{
    NetworkList l;
    TEST_ASSERT_TRUE(add(l, "Home", "password1") == NetworkList::AddResult::Added);
    TEST_ASSERT_TRUE(add(l, "Cafe", "") == NetworkList::AddResult::Added);
    TEST_ASSERT_EQUAL_UINT(2, l.size());
    TEST_ASSERT_EQUAL_INT(0, find(l, "Home"));
    TEST_ASSERT_EQUAL_INT(1, find(l, "Cafe"));
    TEST_ASSERT_EQUAL_STRING("password1", l.at(0).pass);
    TEST_ASSERT_EQUAL_UINT32(2, l.seq());
}

void test_list_find_is_byte_exact(void)
{
    NetworkList l;
    add(l, "Home", "password1");
    TEST_ASSERT_EQUAL_INT(-1, find(l, "home"));
    TEST_ASSERT_EQUAL_INT(-1, find(l, "Hom"));
    TEST_ASSERT_EQUAL_INT(-1, find(l, "Home2"));
    TEST_ASSERT_EQUAL_INT(-1, l.find(nullptr, 4));
}

void test_list_replace_in_place(void)
{
    NetworkList l;
    add(l, "A", "password1");
    add(l, "B", "password2");
    TEST_ASSERT_TRUE(add(l, "A", "newpass12") == NetworkList::AddResult::Replaced);
    TEST_ASSERT_EQUAL_UINT(2, l.size());
    TEST_ASSERT_EQUAL_INT(0, find(l, "A"));
    TEST_ASSERT_EQUAL_STRING("newpass12", l.at(0).pass);
    TEST_ASSERT_EQUAL_UINT32(3, l.at(0).last_ok_seq);
}

void test_list_evicts_oldest_non_joined(void)
{
    NetworkList l;
    add(l, "A", "password1"); // seq 1
    add(l, "B", "password2"); // seq 2
    add(l, "C", "password3"); // seq 3
    l.mark_joined(0);         // A becomes seq 4, so B is the oldest
    TEST_ASSERT_TRUE(add(l, "D", "password4") == NetworkList::AddResult::Evicted);
    TEST_ASSERT_EQUAL_UINT(3, l.size());
    TEST_ASSERT_EQUAL_INT(-1, find(l, "B"));
    TEST_ASSERT_EQUAL_INT(0, find(l, "A"));
    TEST_ASSERT_EQUAL_INT(1, find(l, "D")); // took B's slot
    TEST_ASSERT_EQUAL_INT(2, find(l, "C"));
}

void test_list_evict_ties_lowest_index(void)
{
    NetworkList l;
    NetworkEntry e;
    std::strcpy(e.ssid, "A");
    e.last_ok_seq = 5;
    TEST_ASSERT_TRUE(l.restore(e));
    std::strcpy(e.ssid, "B");
    TEST_ASSERT_TRUE(l.restore(e));
    std::strcpy(e.ssid, "C");
    TEST_ASSERT_TRUE(l.restore(e));
    TEST_ASSERT_TRUE(add(l, "D", "") == NetworkList::AddResult::Evicted);
    TEST_ASSERT_EQUAL_INT(0, find(l, "D"));
}

void test_list_rejects_invalid(void)
{
    NetworkList l;
    TEST_ASSERT_TRUE(add(l, "", "password1") == NetworkList::AddResult::Invalid);
    char long_ssid[40];
    std::memset(long_ssid, 'x', sizeof(long_ssid));
    TEST_ASSERT_TRUE(l.add(long_ssid, 33, "", 0) == NetworkList::AddResult::Invalid);
    TEST_ASSERT_TRUE(l.add(long_ssid, 32, "", 0) == NetworkList::AddResult::Added);
    TEST_ASSERT_TRUE(add(l, "B", "short") == NetworkList::AddResult::Invalid); // 5 chars
    TEST_ASSERT_TRUE(add(l, "B", "1234567") == NetworkList::AddResult::Invalid);
    TEST_ASSERT_TRUE(add(l, "B", "12345678") == NetworkList::AddResult::Added);
    char long_pass[70];
    std::memset(long_pass, 'p', sizeof(long_pass));
    TEST_ASSERT_TRUE(l.add("C", 1, long_pass, 64) == NetworkList::AddResult::Invalid);
    TEST_ASSERT_TRUE(l.add("C", 1, long_pass, 63) == NetworkList::AddResult::Added);
    TEST_ASSERT_TRUE(add(l, "D", "pass\x01word") == NetworkList::AddResult::Invalid);
    TEST_ASSERT_EQUAL_UINT(3, l.size());
}

void test_list_forget(void)
{
    NetworkList l;
    add(l, "A", "password1");
    add(l, "B", "password2");
    add(l, "C", "password3");
    TEST_ASSERT_TRUE(l.forget("B", 1));
    TEST_ASSERT_FALSE(l.forget("B", 1));
    TEST_ASSERT_EQUAL_UINT(2, l.size());
    TEST_ASSERT_EQUAL_INT(0, find(l, "A"));
    TEST_ASSERT_EQUAL_INT(1, find(l, "C"));
    TEST_ASSERT_EQUAL_STRING("password3", l.at(1).pass);
}

void test_list_restore_keeps_seq_at_max(void)
{
    NetworkList l;
    NetworkEntry e;
    std::strcpy(e.ssid, "A");
    e.last_ok_seq = 40;
    TEST_ASSERT_TRUE(l.restore(e));
    std::strcpy(e.ssid, "B");
    e.last_ok_seq = 7;
    TEST_ASSERT_TRUE(l.restore(e));
    TEST_ASSERT_EQUAL_UINT32(40, l.seq());
    add(l, "C", "");
    TEST_ASSERT_EQUAL_UINT32(41, l.at(2).last_ok_seq);
}

void test_list_restore_rejects_full_and_invalid(void)
{
    NetworkList l;
    NetworkEntry bad;
    TEST_ASSERT_FALSE(l.restore(bad)); // empty SSID
    std::strcpy(bad.ssid, "A");
    std::strcpy(bad.pass, "short");
    TEST_ASSERT_FALSE(l.restore(bad));
    NetworkEntry e;
    std::strcpy(e.ssid, "A");
    TEST_ASSERT_TRUE(l.restore(e));
    TEST_ASSERT_TRUE(l.restore(e));
    TEST_ASSERT_TRUE(l.restore(e));
    TEST_ASSERT_FALSE(l.restore(e));
}

void test_list_clear(void)
{
    NetworkList l;
    add(l, "A", "");
    l.clear();
    TEST_ASSERT_EQUAL_UINT(0, l.size());
    TEST_ASSERT_EQUAL_UINT32(0, l.seq());
}

// ---- choose_networks ----

void test_choose_orders_by_rssi(void)
{
    NetworkList l;
    add(l, "A", "");
    add(l, "B", "");
    add(l, "C", "");
    const ScanResult scan[] = {scan_entry("A", -70), scan_entry("C", -40), scan_entry("B", -55)};
    const Candidates c = choose_networks({scan, 3}, l);
    TEST_ASSERT_EQUAL_UINT(3, c.count);
    TEST_ASSERT_EQUAL_UINT8(2, c.idx[0]);
    TEST_ASSERT_EQUAL_UINT8(1, c.idx[1]);
    TEST_ASSERT_EQUAL_UINT8(0, c.idx[2]);
}

void test_choose_duplicate_ssids_take_max(void)
{
    NetworkList l;
    add(l, "A", "");
    add(l, "B", "");
    const ScanResult scan[] = {scan_entry("A", -80), scan_entry("B", -60), scan_entry("A", -30)};
    const Candidates c = choose_networks({scan, 3}, l);
    TEST_ASSERT_EQUAL_UINT(2, c.count);
    TEST_ASSERT_EQUAL_UINT8(0, c.idx[0]);
    TEST_ASSERT_EQUAL_UINT8(1, c.idx[1]);
}

void test_choose_ties_keep_list_order(void)
{
    NetworkList l;
    add(l, "A", "");
    add(l, "B", "");
    add(l, "C", "");
    const ScanResult scan[] = {scan_entry("C", -50), scan_entry("B", -50), scan_entry("A", -50)};
    const Candidates c = choose_networks({scan, 3}, l);
    TEST_ASSERT_EQUAL_UINT(3, c.count);
    TEST_ASSERT_EQUAL_UINT8(0, c.idx[0]);
    TEST_ASSERT_EQUAL_UINT8(1, c.idx[1]);
    TEST_ASSERT_EQUAL_UINT8(2, c.idx[2]);
}

void test_choose_ignores_empty_ssids_and_unsaved(void)
{
    NetworkList l;
    add(l, "A", "");
    const ScanResult scan[] = {scan_entry("", -20), scan_entry("Other", -30), scan_entry("a", -30),
                               scan_entry("A", -90)};
    const Candidates c = choose_networks({scan, 4}, l);
    TEST_ASSERT_EQUAL_UINT(1, c.count);
    TEST_ASSERT_EQUAL_UINT8(0, c.idx[0]);
}

void test_choose_no_matches(void)
{
    NetworkList l;
    add(l, "A", "");
    const ScanResult scan[] = {scan_entry("Z", -30)};
    TEST_ASSERT_EQUAL_UINT(0, choose_networks({scan, 1}, l).count);
    TEST_ASSERT_EQUAL_UINT(0, choose_networks({}, l).count);
    NetworkList empty;
    TEST_ASSERT_EQUAL_UINT(0, choose_networks({scan, 1}, empty).count);
}

// ---- blobs ----

static NetworkList sample_list()
{
    NetworkList l;
    add(l, "Home", "pa&ss=w+d%1 x"); // special characters must survive byte-exact
    l.add("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D", 8, "", 0);
    add(l, "Cafe", "password3");
    l.mark_joined(1);
    return l;
}

static void refresh_crc(uint8_t* b, size_t len)
{
    const uint32_t crc = checksum::crc32({b, len - 4});
    std::memcpy(b + len - 4, &crc, 4);
}

void test_nets_round_trip(void)
{
    const NetworkList l = sample_list();
    const auto blob = serialize_nets(l);
    const auto back = deserialize_nets(blob);
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_EQUAL_UINT(3, back->size());
    for (size_t i = 0; i < 3; ++i)
    {
        TEST_ASSERT_EQUAL_STRING(l.at(i).ssid, back->at(i).ssid);
        TEST_ASSERT_EQUAL_STRING(l.at(i).pass, back->at(i).pass);
        TEST_ASSERT_EQUAL_UINT32(l.at(i).last_ok_seq, back->at(i).last_ok_seq);
    }
    TEST_ASSERT_EQUAL_STRING("pa&ss=w+d%1 x", back->at(0).pass);
    TEST_ASSERT_EQUAL_UINT8(0xD7, static_cast<uint8_t>(back->at(1).ssid[0]));
    TEST_ASSERT_EQUAL_UINT8(0xA9, static_cast<uint8_t>(back->at(1).ssid[1]));
    TEST_ASSERT_EQUAL_UINT32(l.seq(), back->seq());
}

void test_nets_empty_list_round_trip(void)
{
    const NetworkList l;
    const auto blob = serialize_nets(l);
    const auto back = deserialize_nets(blob);
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_EQUAL_UINT(0, back->size());
}

void test_nets_layout_constants(void)
{
    TEST_ASSERT_EQUAL_UINT(6 + 3 * 102 + 4, kNetsBlobSize);
    const auto blob = serialize_nets(sample_list());
    TEST_ASSERT_EQUAL_HEX8(0x56, blob[0]); // "VSWN" little-endian
    TEST_ASSERT_EQUAL_HEX8(0x53, blob[1]);
    TEST_ASSERT_EQUAL_HEX8(0x57, blob[2]);
    TEST_ASSERT_EQUAL_HEX8(0x4E, blob[3]);
    TEST_ASSERT_EQUAL_UINT8(1, blob[4]);
    TEST_ASSERT_EQUAL_UINT8(3, blob[5]);
}

void test_nets_rejects_bad_length_and_null(void)
{
    const auto blob = serialize_nets(sample_list());
    TEST_ASSERT_FALSE(deserialize_nets({blob.data(), blob.size() - 1}).has_value());
    TEST_ASSERT_FALSE(deserialize_nets({blob.data(), 0}).has_value());
    TEST_ASSERT_FALSE(deserialize_nets({}).has_value());
    std::array<uint8_t, kNetsBlobSize + 1> longer{};
    std::memcpy(longer.data(), blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(longer).has_value());
}

void test_nets_rejects_bad_magic(void)
{
    auto blob = serialize_nets(sample_list());
    blob[0] ^= 0xFF;
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_bad_version(void)
{
    auto blob = serialize_nets(sample_list());
    blob[4] = 2;
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_bad_crc(void)
{
    auto blob = serialize_nets(sample_list());
    blob[10] ^= 0x01; // flip a payload bit without fixing the CRC
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
    blob = serialize_nets(sample_list());
    blob[blob.size() - 1] ^= 0x01;
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_count_over_three(void)
{
    auto blob = serialize_nets(sample_list());
    blob[5] = 4;
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_unterminated_ssid(void)
{
    auto blob = serialize_nets(sample_list());
    for (size_t i = 0; i < kSsidMax + 1; ++i)
    {
        blob[6 + i] = 'x'; // entry 0 SSID field has no NUL
    }
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_unterminated_password(void)
{
    auto blob = serialize_nets(sample_list());
    for (size_t i = 0; i < kPassMax + 1; ++i)
    {
        blob[6 + kSsidMax + 1 + i] = 'p'; // entry 0 password field has no NUL
    }
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_invalid_entry(void)
{
    auto blob = serialize_nets(sample_list());
    blob[6] = 0; // entry 0 now has an empty SSID
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());

    blob = serialize_nets(sample_list());
    const size_t pass_off = 6 + kSsidMax + 1;
    std::memset(&blob[pass_off], 0, kPassMax + 1);
    std::memcpy(&blob[pass_off], "abc", 3); // password of 3 characters is neither empty nor 8-63
    refresh_crc(blob.data(), blob.size());
    TEST_ASSERT_FALSE(deserialize_nets(blob).has_value());
}

void test_nets_rejects_all_zero_buffer(void)
{
    const std::array<uint8_t, kNetsBlobSize> zeros{};
    TEST_ASSERT_FALSE(deserialize_nets(zeros).has_value());
    const std::array<uint8_t, kPinBlobSize> pin_zeros{};
    TEST_ASSERT_FALSE(deserialize_pin(pin_zeros).has_value());
}

static PinRecord sample_pin()
{
    PinRecord r;
    r.set = true;
    for (size_t i = 0; i < kSaltLen; ++i)
    {
        r.salt[i] = static_cast<uint8_t>(i + 1);
    }
    for (size_t i = 0; i < kHashLen; ++i)
    {
        r.hash[i] = static_cast<uint8_t>(0xA0 + i);
    }
    return r;
}

void test_pin_round_trip(void)
{
    const PinRecord r = sample_pin();
    const auto blob = serialize_pin(r);
    const auto back = deserialize_pin(blob);
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_TRUE(back->set);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(r.salt, back->salt, kSaltLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(r.hash, back->hash, kHashLen);

    const auto unset = serialize_pin(PinRecord{});
    const auto back2 = deserialize_pin(unset);
    TEST_ASSERT_TRUE(back2.has_value());
    TEST_ASSERT_FALSE(back2->set);
}

void test_pin_layout_constants(void)
{
    TEST_ASSERT_EQUAL_UINT(6 + 16 + 32 + 4, kPinBlobSize);
    const auto blob = serialize_pin(sample_pin());
    TEST_ASSERT_EQUAL_HEX8(0x56, blob[0]); // "VSWP" little-endian
    TEST_ASSERT_EQUAL_HEX8(0x50, blob[3]);
}

void test_pin_rejections(void)
{
    const auto good = serialize_pin(sample_pin());
    TEST_ASSERT_FALSE(deserialize_pin({good.data(), good.size() - 1}).has_value());
    TEST_ASSERT_FALSE(deserialize_pin({}).has_value());

    auto b = good;
    b[1] ^= 0xFF; // magic
    refresh_crc(b.data(), b.size());
    TEST_ASSERT_FALSE(deserialize_pin(b).has_value());

    b = good;
    b[4] = 9; // version
    refresh_crc(b.data(), b.size());
    TEST_ASSERT_FALSE(deserialize_pin(b).has_value());

    b = good;
    b[20] ^= 0x01; // CRC mismatch
    TEST_ASSERT_FALSE(deserialize_pin(b).has_value());

    b = good;
    b[5] = 2; // set byte out of range
    refresh_crc(b.data(), b.size());
    TEST_ASSERT_FALSE(deserialize_pin(b).has_value());
}

// ---- golden bytes: the NVS blob layouts are frozen; saved data must load after any refactor ----

void test_golden_calibration_blob(void)
{
    const uint8_t want[aiming::kBlobSize] = {0x41, 0x49, 0x4D, 0x31, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                                             0x20, 0x41, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x40, 0xC0,
                                             0x00, 0x00, 0x80, 0x3E, 0x39, 0xC0, 0x75, 0xC4};
    const aiming::Blob b = aiming::serialize({.pan_a = 10.0f, .pan_b = 0.5f, .tilt_a = -3.0f, .tilt_b = 0.25f});
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want, b.data(), sizeof(want));
    TEST_ASSERT_TRUE(aiming::deserialize(std::span<const uint8_t>(want)).has_value());
}

void test_golden_nets_blob(void)
{
    // Entry stride 102: ssid[33], pass[65], last_ok_seq (u32 LE). Unused entries and padding are zero.
    NetworkList l;
    add(l, "home", "password1");
    add(l, "lab", "");
    l.mark_joined(0);
    std::array<uint8_t, kNetsBlobSize> want{};
    const uint8_t head[6] = {0x56, 0x53, 0x57, 0x4E, 0x01, 0x02};
    std::memcpy(want.data(), head, 6);
    std::memcpy(want.data() + 6, "home", 4);
    std::memcpy(want.data() + 6 + 33, "password1", 9);
    want[6 + 98] = 3;
    std::memcpy(want.data() + 6 + 102, "lab", 3);
    want[6 + 102 + 98] = 2;
    const uint8_t crc[4] = {0x39, 0x40, 0xC3, 0x33};
    std::memcpy(want.data() + kNetsBlobSize - 4, crc, 4);
    const auto b = serialize_nets(l);
    TEST_ASSERT_EQUAL_UINT(316, kNetsBlobSize);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want.data(), b.data(), kNetsBlobSize);
    TEST_ASSERT_TRUE(deserialize_nets(want).has_value());
}

void test_golden_pin_blob(void)
{
    PinRecord r;
    r.set = true;
    for (size_t i = 0; i < kSaltLen; ++i)
    {
        r.salt[i] = static_cast<uint8_t>(i + 1);
    }
    for (size_t i = 0; i < kHashLen; ++i)
    {
        r.hash[i] = static_cast<uint8_t>(0xA0 + i);
    }
    std::array<uint8_t, kPinBlobSize> want{0x56, 0x53, 0x57, 0x50, 0x01, 0x01};
    std::memcpy(want.data() + 6, r.salt, kSaltLen);
    std::memcpy(want.data() + 6 + kSaltLen, r.hash, kHashLen);
    const uint8_t crc[4] = {0x3F, 0x9F, 0x12, 0x2A};
    std::memcpy(want.data() + kPinBlobSize - 4, crc, 4);
    const auto b = serialize_pin(r);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(want.data(), b.data(), kPinBlobSize);
    TEST_ASSERT_TRUE(deserialize_pin(want).has_value());

    std::array<uint8_t, kPinBlobSize> unset{0x56, 0x53, 0x57, 0x50, 0x01, 0x00};
    const uint8_t crc0[4] = {0xA8, 0x91, 0x54, 0xAF};
    std::memcpy(unset.data() + kPinBlobSize - 4, crc0, 4);
    const auto u = serialize_pin(PinRecord{});
    TEST_ASSERT_EQUAL_HEX8_ARRAY(unset.data(), u.data(), kPinBlobSize);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc32_known_value);
    RUN_TEST(test_aiming_crc32_alias_matches);
    RUN_TEST(test_calibration_blob_still_round_trips);
    RUN_TEST(test_golden_calibration_blob);
    RUN_TEST(test_golden_nets_blob);
    RUN_TEST(test_golden_pin_blob);
    RUN_TEST(test_list_add_and_find);
    RUN_TEST(test_list_find_is_byte_exact);
    RUN_TEST(test_list_replace_in_place);
    RUN_TEST(test_list_evicts_oldest_non_joined);
    RUN_TEST(test_list_evict_ties_lowest_index);
    RUN_TEST(test_list_rejects_invalid);
    RUN_TEST(test_list_forget);
    RUN_TEST(test_list_restore_keeps_seq_at_max);
    RUN_TEST(test_list_restore_rejects_full_and_invalid);
    RUN_TEST(test_list_clear);
    RUN_TEST(test_choose_orders_by_rssi);
    RUN_TEST(test_choose_duplicate_ssids_take_max);
    RUN_TEST(test_choose_ties_keep_list_order);
    RUN_TEST(test_choose_ignores_empty_ssids_and_unsaved);
    RUN_TEST(test_choose_no_matches);
    RUN_TEST(test_nets_round_trip);
    RUN_TEST(test_nets_empty_list_round_trip);
    RUN_TEST(test_nets_layout_constants);
    RUN_TEST(test_nets_rejects_bad_length_and_null);
    RUN_TEST(test_nets_rejects_bad_magic);
    RUN_TEST(test_nets_rejects_bad_version);
    RUN_TEST(test_nets_rejects_bad_crc);
    RUN_TEST(test_nets_rejects_count_over_three);
    RUN_TEST(test_nets_rejects_unterminated_ssid);
    RUN_TEST(test_nets_rejects_unterminated_password);
    RUN_TEST(test_nets_rejects_invalid_entry);
    RUN_TEST(test_nets_rejects_all_zero_buffer);
    RUN_TEST(test_pin_round_trip);
    RUN_TEST(test_pin_layout_constants);
    RUN_TEST(test_pin_rejections);
    return UNITY_END();
}
