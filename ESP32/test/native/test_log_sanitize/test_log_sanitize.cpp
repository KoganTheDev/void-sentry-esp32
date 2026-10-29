/**
 * @file test_log_sanitize.cpp
 * @brief Host-side tests for the serial log sanitizer.
 */

#include <unity.h>

#include "log_sanitize.h"

#include <cstring>

using namespace remote;

void setUp(void) {}
void tearDown(void) {}

static void test_printable_unchanged(void)
{
    char out[16];
    TEST_ASSERT_EQUAL_UINT(5, sanitize_for_log("Cafe1", 5, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("Cafe1", out);
}

static void test_control_and_high_bytes_replaced(void)
{
    const char in[] = {'a', '\x1b', '[', '\n', '\r', '\0', '\x7f', '\xc3', '~', ' '};
    char out[16];
    TEST_ASSERT_EQUAL_UINT(10, sanitize_for_log(in, sizeof(in), out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("a?[?????~ ", out);
}

static void test_truncates_and_terminates(void)
{
    char out[4];
    TEST_ASSERT_EQUAL_UINT(3, sanitize_for_log("abcdef", 6, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("abc", out);
}

static void test_empty_and_degenerate(void)
{
    char out[4] = {'x', 'x', 'x', 'x'};
    TEST_ASSERT_EQUAL_UINT(0, sanitize_for_log(nullptr, 0, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
    TEST_ASSERT_EQUAL_UINT(0, sanitize_for_log("abc", 3, out, 1));
    TEST_ASSERT_EQUAL_STRING("", out);
    out[0] = 'z';
    TEST_ASSERT_EQUAL_UINT(0, sanitize_for_log("abc", 3, out, 0));
    TEST_ASSERT_EQUAL_CHAR('z', out[0]);
    TEST_ASSERT_EQUAL_UINT(0, sanitize_for_log("abc", 3, nullptr, 4));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_printable_unchanged);
    RUN_TEST(test_control_and_high_bytes_replaced);
    RUN_TEST(test_truncates_and_terminates);
    RUN_TEST(test_empty_and_degenerate);
    return UNITY_END();
}
