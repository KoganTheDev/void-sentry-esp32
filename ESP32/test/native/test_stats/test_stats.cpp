/**
 * @file test_stats.cpp
 * @brief Host-side tests for the windowed stats arithmetic.
 */

#include <unity.h>

#include "windowed_counter.h"

void setUp(void) {}
void tearDown(void) {}

void test_first_sample_is_the_total(void)
{
    stats::WindowedCounter<uint32_t> c;
    TEST_ASSERT_EQUAL_UINT32(100, c.sample(100));
}

void test_later_samples_are_deltas(void)
{
    stats::WindowedCounter<uint32_t> c;
    c.sample(100);
    TEST_ASSERT_EQUAL_UINT32(50, c.sample(150));
    TEST_ASSERT_EQUAL_UINT32(0, c.sample(150));
    TEST_ASSERT_EQUAL_UINT32(7, c.sample(157));
}

void test_delta_survives_a_32_bit_wrap(void)
{
    stats::WindowedCounter<uint32_t> c;
    c.sample(0xFFFFFFF0u);
    TEST_ASSERT_EQUAL_UINT32(0x20, c.sample(0x10u));
}

void test_64_bit_counter(void)
{
    stats::WindowedCounter<uint64_t> c;
    c.sample(5'000'000'000ull);
    TEST_ASSERT_TRUE(c.sample(5'000'000'123ull) == 123ull);
}

void test_window_avg(void)
{
    TEST_ASSERT_EQUAL_UINT32(10, stats::window_avg<uint32_t>(100, 10));
    TEST_ASSERT_EQUAL_UINT32(3, stats::window_avg<uint32_t>(10, 3)); // truncates like the old integer division
    TEST_ASSERT_EQUAL_UINT32(0, stats::window_avg<uint32_t>(100, 0));
    TEST_ASSERT_TRUE(stats::window_avg<uint64_t>(9'000'000'000ull, 3) == 3'000'000'000ull);
}

void test_window_percent(void)
{
    TEST_ASSERT_EQUAL_UINT(50, stats::window_percent(5, 10));
    TEST_ASSERT_EQUAL_UINT(99, stats::window_percent(999, 1000));
    TEST_ASSERT_EQUAL_UINT(0, stats::window_percent(5, 0));
    // part * 100 must not overflow 32 bits: 5 s of microseconds * 100 does not fit in uint32
    TEST_ASSERT_EQUAL_UINT(80, stats::window_percent(4'000'000'000ull, 5'000'000'000ull));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_first_sample_is_the_total);
    RUN_TEST(test_later_samples_are_deltas);
    RUN_TEST(test_delta_survives_a_32_bit_wrap);
    RUN_TEST(test_64_bit_counter);
    RUN_TEST(test_window_avg);
    RUN_TEST(test_window_percent);
    return UNITY_END();
}
