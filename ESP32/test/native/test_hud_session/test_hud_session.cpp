/**
 * @file test_hud_session.cpp
 * @brief Host-side tests for the HUD login session table and the vs_session cookie parser.
 */

#include <unity.h>

#include "session_table.h"

#include <cstring>
#include <string>

using namespace prov;

void setUp(void) {}
void tearDown(void) {}

namespace
{
SessionToken token(uint8_t seed)
{
    SessionToken t{};
    for (size_t i = 0; i < kTokenLen; ++i)
    {
        t[i] = static_cast<uint8_t>(seed + i * 17);
    }
    return t;
}

const char* kHex = "00112233445566778899aabbccddeeff";

SessionToken hex_token()
{
    SessionToken t{};
    for (size_t i = 0; i < kTokenLen; ++i)
    {
        t[i] = static_cast<uint8_t>(i * 0x11);
    }
    return t;
}

void expect_token(const char* header, const SessionToken& want)
{
    SessionToken got{};
    TEST_ASSERT_TRUE_MESSAGE(session_cookie(header, got), header);
    TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(want.data(), got.data(), kTokenLen, header);
}

void expect_none(std::string_view header)
{
    SessionToken got = token(99);
    const SessionToken before = got;
    TEST_ASSERT_FALSE_MESSAGE(session_cookie(header, got), std::string(header).c_str());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(before.data(), got.data(), kTokenLen); // untouched on failure
}
} // namespace

// ---- SessionTable ----

void test_empty_table_contains_nothing(void)
{
    SessionTable t;
    TEST_ASSERT_EQUAL_UINT32(0, t.size());
    TEST_ASSERT_FALSE(t.contains(token(1)));
    TEST_ASSERT_FALSE(t.contains(SessionToken{})); // an all-zero token never matches an empty slot
    TEST_ASSERT_FALSE(t.remove(token(1)));
}

void test_add_contains_remove(void)
{
    SessionTable t;
    t.add(token(1));
    t.add(token(2));
    TEST_ASSERT_EQUAL_UINT32(2, t.size());
    TEST_ASSERT_TRUE(t.contains(token(1)));
    TEST_ASSERT_TRUE(t.contains(token(2)));
    TEST_ASSERT_FALSE(t.contains(token(3)));
    TEST_ASSERT_TRUE(t.remove(token(1)));
    TEST_ASSERT_FALSE(t.contains(token(1)));
    TEST_ASSERT_FALSE(t.remove(token(1)));
    TEST_ASSERT_TRUE(t.contains(token(2)));
    TEST_ASSERT_EQUAL_UINT32(1, t.size());
}

void test_one_byte_difference_does_not_match(void)
{
    SessionTable t;
    const SessionToken a = token(5);
    t.add(a);
    for (size_t i = 0; i < kTokenLen; ++i)
    {
        SessionToken b = a;
        b[i] ^= 0x01;
        TEST_ASSERT_FALSE(t.contains(b));
    }
    TEST_ASSERT_TRUE(t.contains(a));
}

void test_adding_twice_keeps_one_slot(void)
{
    SessionTable t;
    t.add(token(1));
    t.add(token(1));
    TEST_ASSERT_EQUAL_UINT32(1, t.size());
    TEST_ASSERT_TRUE(t.remove(token(1)));
    TEST_ASSERT_FALSE(t.contains(token(1)));
}

void test_full_table_evicts_the_oldest(void)
{
    SessionTable t;
    for (uint8_t i = 1; i <= SessionTable::kCapacity; ++i)
    {
        t.add(token(i));
    }
    TEST_ASSERT_EQUAL_UINT32(SessionTable::kCapacity, t.size());
    t.add(token(50)); // evicts token(1), added first and never used since
    TEST_ASSERT_EQUAL_UINT32(SessionTable::kCapacity, t.size());
    TEST_ASSERT_FALSE(t.contains(token(1)));
    TEST_ASSERT_TRUE(t.contains(token(50)));
    for (uint8_t i = 2; i <= SessionTable::kCapacity; ++i)
    {
        TEST_ASSERT_TRUE(t.contains(token(i)));
    }
}

void test_use_keeps_a_session_from_eviction(void)
{
    SessionTable t;
    for (uint8_t i = 1; i <= SessionTable::kCapacity; ++i)
    {
        t.add(token(i));
    }
    TEST_ASSERT_TRUE(t.contains(token(1))); // the polling HUD: now the most recently used
    t.add(token(60));                       // evicts token(2)
    TEST_ASSERT_TRUE(t.contains(token(1)));
    TEST_ASSERT_FALSE(t.contains(token(2)));
    t.add(token(61)); // token(3) is now the least recently used
    TEST_ASSERT_FALSE(t.contains(token(3)));
    TEST_ASSERT_TRUE(t.contains(token(60)));
    TEST_ASSERT_TRUE(t.contains(token(61)));
}

void test_re_adding_refreshes_the_lru_order(void)
{
    SessionTable t;
    for (uint8_t i = 1; i <= SessionTable::kCapacity; ++i)
    {
        t.add(token(i));
    }
    t.add(token(1)); // a second login with the same token: refreshed, not duplicated
    t.add(token(70));
    TEST_ASSERT_TRUE(t.contains(token(1)));
    TEST_ASSERT_FALSE(t.contains(token(2)));
}

void test_removed_slot_is_reused_before_evicting(void)
{
    SessionTable t;
    for (uint8_t i = 1; i <= SessionTable::kCapacity; ++i)
    {
        t.add(token(i));
    }
    TEST_ASSERT_TRUE(t.remove(token(3)));
    t.add(token(80));
    for (uint8_t i = 1; i <= SessionTable::kCapacity; ++i)
    {
        TEST_ASSERT_EQUAL(i != 3, t.contains(token(i)));
    }
    TEST_ASSERT_TRUE(t.contains(token(80)));
}

void test_clear_forgets_everything(void)
{
    SessionTable t;
    t.add(token(1));
    t.add(token(2));
    t.clear();
    TEST_ASSERT_EQUAL_UINT32(0, t.size());
    TEST_ASSERT_FALSE(t.contains(token(1)));
    TEST_ASSERT_FALSE(t.contains(token(2)));
    t.add(token(3));
    TEST_ASSERT_TRUE(t.contains(token(3)));
}

// ---- token_hex ----

void test_token_hex_round_trips(void)
{
    char hex[kTokenHexLen + 1];
    std::memset(hex, 'Z', sizeof(hex));
    token_hex(hex_token(), hex);
    TEST_ASSERT_EQUAL_STRING(kHex, hex);
    const SessionToken t = token(200);
    token_hex(t, hex);
    TEST_ASSERT_EQUAL_UINT32(kTokenHexLen, strlen(hex));
    const std::string header = std::string("vs_session=") + hex;
    expect_token(header.c_str(), t);
}

// ---- session_cookie ----

void test_cookie_alone(void) { expect_token("vs_session=00112233445566778899aabbccddeeff", hex_token()); }

void test_cookie_among_others_with_spaces(void)
{
    expect_token("a=1; vs_session=00112233445566778899aabbccddeeff; b=2", hex_token());
    expect_token("theme=dark;vs_session=00112233445566778899aabbccddeeff", hex_token());
    expect_token("  vs_session = 00112233445566778899aabbccddeeff  ;x=y", hex_token());
    expect_token("x=y;\tvs_session=00112233445566778899AABBCCDDEEFF\t", hex_token()); // upper case hex too
}

void test_cookie_first_valid_one_wins(void)
{
    SessionToken other{};
    other.fill(0xAB);
    expect_token("vs_session=abababababababababababababababab; vs_session=00112233445566778899aabbccddeeff", other);
    // A malformed first one is skipped, not fatal.
    expect_token("vs_session=zz; vs_session=00112233445566778899aabbccddeeff", hex_token());
}

void test_cookie_name_must_match_exactly(void)
{
    expect_none("xvs_session=00112233445566778899aabbccddeeff");
    expect_none("vs_session2=00112233445566778899aabbccddeeff");
    expect_none("VS_SESSION=00112233445566778899aabbccddeeff");
    expect_none("vs_ses=00112233445566778899aabbccddeeff");
    expect_none("other=vs_session=00112233445566778899aabbccddeeff");
}

void test_cookie_value_must_be_32_hex_digits(void)
{
    expect_none("vs_session=00112233445566778899aabbccddeef");   // 31
    expect_none("vs_session=00112233445566778899aabbccddeeff0"); // 33
    expect_none("vs_session=00112233445566778899aabbccddeefg");  // not hex
    expect_none("vs_session=0011223344556677 8899aabbccddeeff"); // inner space
    expect_none("vs_session=\"00112233445566778899aabbccddeeff\"");
    expect_none("vs_session=");
    expect_none("vs_session=-0112233445566778899aabbccddeeff");
    expect_none("vs_session=@@112233445566778899aabbccddeeff");
    expect_none("vs_session=``112233445566778899aabbccddeeff");
    expect_none("vs_session=GG112233445566778899aabbccddeeff");
    expect_none("vs_session=0:112233445566778899aabbccddeeff");
}

void test_cookie_malformed_headers(void)
{
    expect_none("");
    expect_none(";;;");
    expect_none("vs_session");
    expect_none("vs_session;00112233445566778899aabbccddeeff");
    expect_none("=00112233445566778899aabbccddeeff");
    expect_none("   ");
}

void test_cookie_oversize_header_is_ignored(void)
{
    std::string header = "vs_session=00112233445566778899aabbccddeeff; pad=";
    header.append(kMaxCookieHeader - header.size(), 'x');
    TEST_ASSERT_EQUAL_UINT32(kMaxCookieHeader, header.size());
    expect_token(header.c_str(), hex_token()); // exactly at the cap: still read
    header.push_back('x');
    expect_none(header);
}

void test_cookie_reads_only_the_given_length(void)
{
    const char buf[] = "vs_session=00112233445566778899aabbccddeeffEXTRA";
    SessionToken got{};
    TEST_ASSERT_TRUE(session_cookie(std::string_view(buf, 43), got));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(hex_token().data(), got.data(), kTokenLen);
    TEST_ASSERT_FALSE(session_cookie(std::string_view(buf, 42), got));
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_table_contains_nothing);
    RUN_TEST(test_add_contains_remove);
    RUN_TEST(test_one_byte_difference_does_not_match);
    RUN_TEST(test_adding_twice_keeps_one_slot);
    RUN_TEST(test_full_table_evicts_the_oldest);
    RUN_TEST(test_use_keeps_a_session_from_eviction);
    RUN_TEST(test_re_adding_refreshes_the_lru_order);
    RUN_TEST(test_removed_slot_is_reused_before_evicting);
    RUN_TEST(test_clear_forgets_everything);
    RUN_TEST(test_token_hex_round_trips);
    RUN_TEST(test_cookie_alone);
    RUN_TEST(test_cookie_among_others_with_spaces);
    RUN_TEST(test_cookie_first_valid_one_wins);
    RUN_TEST(test_cookie_name_must_match_exactly);
    RUN_TEST(test_cookie_value_must_be_32_hex_digits);
    RUN_TEST(test_cookie_malformed_headers);
    RUN_TEST(test_cookie_oversize_header_is_ignored);
    RUN_TEST(test_cookie_reads_only_the_given_length);
    return UNITY_END();
}
