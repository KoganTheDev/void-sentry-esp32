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
    const char origin[] = {'h', 't', 't', 'p', ':', '/', '/', 'a', 'b', 'X', 'X'};
    const char host[] = {'a', 'b', 'Y', 'Y'};
    TEST_ASSERT_TRUE(origin_allowed(origin, 9, host, 2));
    TEST_ASSERT_FALSE(origin_allowed(origin, 10, host, 2)); // trailing byte is part of the origin
    TEST_ASSERT_FALSE(origin_allowed(origin, 9, host, 3));  // host longer than the origin's host part
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
    return UNITY_END();
}
