/**
 * @file test_origin_check.cpp
 * @brief Host-side tests for the WebSocket handshake Origin/Host check.
 */

#include <unity.h>

#include "origin_check.h"

#include <cstring>

using namespace remote;

void setUp(void) {}
void tearDown(void) {}

static bool allowed(const char* origin, const char* host)
{
    return origin_allowed(origin, origin ? std::strlen(origin) : 0, host, host ? std::strlen(host) : 0);
}

static void test_matching_http_and_https(void)
{
    TEST_ASSERT_TRUE(allowed("http://192.168.1.65", "192.168.1.65"));
    TEST_ASSERT_TRUE(allowed("https://192.168.1.65", "192.168.1.65"));
}

static void test_mismatched_host(void)
{
    TEST_ASSERT_FALSE(allowed("http://evil.com", "192.168.1.65"));
    TEST_ASSERT_FALSE(allowed("ftp://192.168.1.65", "192.168.1.65"));
}

static void test_port_must_match(void)
{
    TEST_ASSERT_FALSE(allowed("http://192.168.1.65:8080", "192.168.1.65"));
    TEST_ASSERT_TRUE(allowed("http://192.168.1.65:8080", "192.168.1.65:8080"));
    TEST_ASSERT_FALSE(allowed("http://192.168.1.65", "192.168.1.65:8080"));
}

static void test_prefix_tricks_rejected(void)
{
    TEST_ASSERT_FALSE(allowed("http://192.168.1.65.evil.com", "192.168.1.65"));
    TEST_ASSERT_FALSE(allowed("http://192.168.1.6", "192.168.1.65"));
    TEST_ASSERT_FALSE(allowed("http://192.168.1.65", "192.168.1.6"));
}

static void test_null_origin_rejected(void) { TEST_ASSERT_FALSE(allowed("null", "192.168.1.65")); }

static void test_no_origin_allowed(void)
{
    TEST_ASSERT_TRUE(allowed(nullptr, "192.168.1.65"));
    TEST_ASSERT_TRUE(allowed("", "192.168.1.65"));
    TEST_ASSERT_TRUE(allowed(nullptr, nullptr));
}

static void test_empty_host_with_origin_rejected(void)
{
    TEST_ASSERT_FALSE(allowed("http://192.168.1.65", ""));
    TEST_ASSERT_FALSE(allowed("http://192.168.1.65", nullptr));
    TEST_ASSERT_FALSE(allowed("http://", ""));
}

static void test_lengths_respected(void)
{
    // Not NUL-terminated and followed by junk: only the given lengths count.
    const char origin[] = {'h', 't', 't', 'p', ':', '/', '/', '1', '.', '2', '.', '3', '.', '4', 'X', 'X'};
    const char host[] = {'1', '.', '2', '.', '3', '.', '4', 'Y', 'Y'};
    TEST_ASSERT_TRUE(origin_allowed(origin, 14, host, 7));
    TEST_ASSERT_FALSE(origin_allowed(origin, 15, host, 7)); // trailing byte is part of the origin
    TEST_ASSERT_FALSE(origin_allowed(origin, 14, host, 8)); // host longer than the origin's host part
}

static void test_dns_rebinding_rejected(void)
{
    TEST_ASSERT_FALSE(allowed("http://evil.com", "evil.com"));
    TEST_ASSERT_FALSE(allowed("http://localhost", "localhost"));
    TEST_ASSERT_FALSE(allowed("http://localhost:80", "localhost:80"));
}

static void test_ipv4_host_with_matching_origin_allowed(void)
{
    TEST_ASSERT_TRUE(allowed("http://192.168.1.65", "192.168.1.65"));
    TEST_ASSERT_TRUE(allowed("http://192.168.1.65:80", "192.168.1.65:80"));
}

static void test_malformed_ipv4_hosts_rejected(void)
{
    const char* bad[] = {"256.1.1.1", "1.2.3",    "1.2.3.4.", "1.2.3.4:",  "1.2.3.4:99999", "01234.1.1.1",
                         "1..3.4",    "+1.2.3.4", "-1.2.3.4", "1.2.3.4:0", "[::1]",         "1.2.3.4:80a"};
    for (const char* h : bad)
    {
        char origin[64] = "http://";
        std::strcat(origin, h);
        TEST_ASSERT_FALSE_MESSAGE(allowed(origin, h), h);
    }
    TEST_ASSERT_FALSE(is_ipv4_host(nullptr, 0));
}

static void test_is_ipv4_host_limits(void)
{
    TEST_ASSERT_TRUE(is_ipv4_host("0.0.0.0", 7));
    TEST_ASSERT_TRUE(is_ipv4_host("255.255.255.255:65535", 21));
    TEST_ASSERT_TRUE(is_ipv4_host("1.2.3.4:1", 9));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:65536", 13));
    TEST_ASSERT_TRUE(is_ipv4_host("1.2.3.4:5", 7));  // length respected: stops before ":5"
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:5", 8)); // ends at the colon
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_matching_http_and_https);
    RUN_TEST(test_mismatched_host);
    RUN_TEST(test_port_must_match);
    RUN_TEST(test_prefix_tricks_rejected);
    RUN_TEST(test_null_origin_rejected);
    RUN_TEST(test_no_origin_allowed);
    RUN_TEST(test_empty_host_with_origin_rejected);
    RUN_TEST(test_lengths_respected);
    RUN_TEST(test_dns_rebinding_rejected);
    RUN_TEST(test_ipv4_host_with_matching_origin_allowed);
    RUN_TEST(test_malformed_ipv4_hosts_rejected);
    RUN_TEST(test_is_ipv4_host_limits);
    return UNITY_END();
}
