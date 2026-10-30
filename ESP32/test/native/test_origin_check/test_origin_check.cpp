/**
 * @file test_origin_check.cpp
 * @brief Host-side tests for the WebSocket handshake Origin/Host check.
 */

#include <unity.h>

#include "origin_check.h"

#include <cstring>

using namespace remote;

static bool allowed_any(const char* origin, const char* host)
{
    return origin_allowed_any_port(origin ? std::string_view(origin) : std::string_view(),
                                   host ? std::string_view(host) : std::string_view());
}

void setUp(void) {}
void tearDown(void) {}

static bool allowed(const char* origin, const char* host)
{
    return origin_allowed(origin ? std::string_view(origin) : std::string_view(),
                          host ? std::string_view(host) : std::string_view());
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
    TEST_ASSERT_TRUE(origin_allowed({origin, 14}, {host, 7}));
    TEST_ASSERT_FALSE(origin_allowed({origin, 15}, {host, 7})); // trailing byte is part of the origin
    TEST_ASSERT_FALSE(origin_allowed({origin, 14}, {host, 8})); // host longer than the origin's host part
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
    TEST_ASSERT_FALSE(is_ipv4_host({}));
}

static void test_is_ipv4_host_limits(void)
{
    TEST_ASSERT_TRUE(is_ipv4_host({"0.0.0.0", 7}));
    TEST_ASSERT_TRUE(is_ipv4_host({"255.255.255.255:65535", 21}));
    TEST_ASSERT_TRUE(is_ipv4_host({"1.2.3.4:1", 9}));
    TEST_ASSERT_FALSE(is_ipv4_host({"1.2.3.4:65536", 13}));
    TEST_ASSERT_TRUE(is_ipv4_host({"1.2.3.4:5", 7}));  // length respected: stops before ":5"
    TEST_ASSERT_FALSE(is_ipv4_host({"1.2.3.4:5", 8})); // ends at the colon
}

static void test_octet_bounds_each_position(void)
{
    TEST_ASSERT_TRUE(is_ipv4_host("0.0.0.255"));
    TEST_ASSERT_TRUE(is_ipv4_host("255.0.0.0"));
    TEST_ASSERT_FALSE(is_ipv4_host("256.0.0.0"));
    TEST_ASSERT_FALSE(is_ipv4_host("0.256.0.0"));
    TEST_ASSERT_FALSE(is_ipv4_host("0.0.256.0"));
    TEST_ASSERT_FALSE(is_ipv4_host("0.0.0.256"));
    TEST_ASSERT_FALSE(is_ipv4_host("999.999.999.999"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.a"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4 "));
    TEST_ASSERT_FALSE(is_ipv4_host(" 1.2.3.4"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4.5"));
}

static void test_leading_zeros_follow_three_digit_rule(void)
{
    // Up to 3 digits per octet are accepted whatever their value (leading zeros included); a 4th digit is not.
    TEST_ASSERT_TRUE(is_ipv4_host("001.002.003.004"));
    TEST_ASSERT_TRUE(is_ipv4_host("010.0.0.1"));
    TEST_ASSERT_FALSE(is_ipv4_host("0001.2.3.4"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.0004"));
    TEST_ASSERT_FALSE(is_ipv4_host("256.0.0.1")); // 3 digits but out of range
}

static void test_port_bounds(void)
{
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:0"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:00000"));
    TEST_ASSERT_TRUE(is_ipv4_host("1.2.3.4:1"));
    TEST_ASSERT_TRUE(is_ipv4_host("1.2.3.4:65535"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:65536"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:99999"));
    TEST_ASSERT_TRUE(is_ipv4_host("1.2.3.4:00080"));   // 5 digits with leading zeros, value 80
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:000080")); // 6 digits
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:-1"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:80:80"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4/"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4:80."));
}

static void test_trailing_dot_and_hostnames_rejected(void)
{
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4."));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4.:80"));
    TEST_ASSERT_FALSE(allowed("http://1.2.3.4.", "1.2.3.4."));
    TEST_ASSERT_FALSE(is_ipv4_host(".1.2.3.4"));
    TEST_ASSERT_FALSE(is_ipv4_host("1.2.3.4.evil.com"));
}

static void test_scheme_is_case_sensitive_and_exact(void)
{
    TEST_ASSERT_FALSE(allowed("HTTP://1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("Http://1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("HTTPS://1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http:/1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http//1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("ws://1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("1.2.3.4", "1.2.3.4")); // no scheme at all
}

static void test_origin_must_have_no_extras(void)
{
    TEST_ASSERT_FALSE(allowed("http://1.2.3.4/", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http://1.2.3.4/path", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http://user@1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http://1.2.3.4 ", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed(" http://1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http://http://1.2.3.4", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("http://", "1.2.3.4"));
    TEST_ASSERT_FALSE(allowed("https://", "1.2.3.4"));
}

static void test_null_origin_with_valid_or_missing_host(void)
{
    TEST_ASSERT_FALSE(allowed("null", "1.2.3.4:80"));
    TEST_ASSERT_FALSE(allowed("null", nullptr));
    TEST_ASSERT_FALSE(allowed("NULL", "1.2.3.4"));
}

/** Builds prefix + n copies of fill + suffix in a static buffer (no std::string: keeps the test free of libstdc++). */
static std::string_view build(char (&buf)[8192], const char* prefix, char fill, size_t n, const char* suffix = "")
{
    size_t len = 0;
    for (const char* p = prefix; *p; ++p)
        buf[len++] = *p;
    for (size_t i = 0; i < n; ++i)
        buf[len++] = fill;
    for (const char* p = suffix; *p; ++p)
        buf[len++] = *p;
    return {buf, len};
}

static void test_over_long_origin_and_host_rejected_without_overrun(void)
{
    static char host_buf[8192];
    static char origin_buf[8192];
    const std::string_view long_host = build(host_buf, "", '1', 4096);
    TEST_ASSERT_FALSE(is_ipv4_host(long_host));
    TEST_ASSERT_FALSE(origin_allowed(build(origin_buf, "http://", '1', 4096), long_host));
    // A valid host followed by a very long tail.
    const std::string_view tail = build(host_buf, "1.2.3.4", 'a', 4096);
    TEST_ASSERT_FALSE(is_ipv4_host(tail));
    TEST_ASSERT_FALSE(origin_allowed(build(origin_buf, "http://1.2.3.4", 'a', 4096), tail));
    // A valid origin with a very long junk suffix is not equal to the host.
    TEST_ASSERT_FALSE(origin_allowed(build(origin_buf, "http://1.2.3.4", '0', 4096), "1.2.3.4"));
    // Long digit run in the port.
    TEST_ASSERT_FALSE(is_ipv4_host(build(host_buf, "1.2.3.4:", '1', 4096)));
}

static void test_any_port_accepts(void)
{
    TEST_ASSERT_TRUE(allowed_any("http://192.168.1.64", "192.168.1.64:81"));
    TEST_ASSERT_TRUE(allowed_any("http://192.168.1.64:80", "192.168.1.64:81"));
    TEST_ASSERT_TRUE(allowed_any("https://192.168.1.64", "192.168.1.64:81"));
    TEST_ASSERT_TRUE(allowed_any("http://192.168.1.64:65535", "192.168.1.64"));
    TEST_ASSERT_TRUE(allowed_any("", "192.168.1.64:81"));
    TEST_ASSERT_TRUE(allowed_any(nullptr, nullptr));
}

static void test_any_port_refuses_other_hosts(void)
{
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.65", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.6", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64", "192.168.1.6:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64.evil.com", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://evil.com", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("null", "192.168.1.64:81"));
}

static void test_any_port_refuses_bad_hosts(void)
{
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64", "evil.com:81"));
    TEST_ASSERT_FALSE(allowed_any("http://evil.com", "evil.com:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64", ""));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64", "192.168.1.64:0"));
}

static void test_any_port_refuses_malformed_origins(void)
{
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64/x", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64:0", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64:65536", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64:", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64:abc", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64:80x", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64:123456", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http://192.168.1.64x", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("ftp://192.168.1.64", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("http:/192.168.1.64", "192.168.1.64:81"));
    TEST_ASSERT_FALSE(allowed_any("192.168.1.64", "192.168.1.64:81"));
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
    RUN_TEST(test_octet_bounds_each_position);
    RUN_TEST(test_leading_zeros_follow_three_digit_rule);
    RUN_TEST(test_port_bounds);
    RUN_TEST(test_trailing_dot_and_hostnames_rejected);
    RUN_TEST(test_scheme_is_case_sensitive_and_exact);
    RUN_TEST(test_origin_must_have_no_extras);
    RUN_TEST(test_null_origin_with_valid_or_missing_host);
    RUN_TEST(test_over_long_origin_and_host_rejected_without_overrun);
    RUN_TEST(test_any_port_accepts);
    RUN_TEST(test_any_port_refuses_other_hosts);
    RUN_TEST(test_any_port_refuses_bad_hosts);
    RUN_TEST(test_any_port_refuses_malformed_origins);
    return UNITY_END();
}
