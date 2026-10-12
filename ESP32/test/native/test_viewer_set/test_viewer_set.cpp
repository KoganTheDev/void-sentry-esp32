/**
 * @file test_viewer_set.cpp
 * @brief Host-side tests for stream viewer bookkeeping (capacity, busy skipping, no duplicate frames).
 */

#include <unity.h>

#include "viewer_set.h"

using namespace frames;

void setUp(void) {}
void tearDown(void) {}

void test_add_until_full(void)
{
    ViewerSet viewers;
    TEST_ASSERT_TRUE(viewers.add(10));
    TEST_ASSERT_TRUE(viewers.add(11));
    TEST_ASSERT_FALSE(viewers.add(12));
    TEST_ASSERT_EQUAL_UINT(2, viewers.size());
}

void test_add_twice_is_idempotent(void)
{
    ViewerSet viewers;
    TEST_ASSERT_TRUE(viewers.add(10));
    TEST_ASSERT_TRUE(viewers.add(10));
    TEST_ASSERT_EQUAL_UINT(1, viewers.size());
}

void test_ready_for_skips_busy_viewer(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.add(11);
    viewers.mark_sending(10, 5);
    ReadyList ready = viewers.ready_for(6);
    TEST_ASSERT_EQUAL_UINT(1, ready.count);
    TEST_ASSERT_EQUAL_INT(11, ready.fds[0]);
}

void test_never_sends_same_sequence_twice(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.mark_sending(10, 5);
    viewers.mark_done(10);
    TEST_ASSERT_EQUAL_UINT(0, viewers.ready_for(5).count);
    TEST_ASSERT_EQUAL_UINT(1, viewers.ready_for(6).count);
}

void test_remove_busy_viewer_frees_capacity(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.add(11);
    viewers.mark_sending(10, 3);
    viewers.remove(10);
    TEST_ASSERT_EQUAL_UINT(1, viewers.size());
    TEST_ASSERT_FALSE(viewers.index_of(10).has_value());
    TEST_ASSERT_TRUE(viewers.add(12));
}

void test_index_of(void)
{
    ViewerSet viewers;
    viewers.add(10);
    viewers.add(11);
    TEST_ASSERT_EQUAL_UINT(1, *viewers.index_of(11));
    TEST_ASSERT_FALSE(viewers.index_of(99).has_value());
}

void test_marks_on_unknown_fd_are_ignored(void)
{
    ViewerSet viewers;
    viewers.mark_sending(99, 1);
    viewers.mark_done(99);
    viewers.remove(99);
    TEST_ASSERT_EQUAL_UINT(0, viewers.size());
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_add_until_full);
    RUN_TEST(test_add_twice_is_idempotent);
    RUN_TEST(test_ready_for_skips_busy_viewer);
    RUN_TEST(test_never_sends_same_sequence_twice);
    RUN_TEST(test_remove_busy_viewer_frees_capacity);
    RUN_TEST(test_index_of);
    RUN_TEST(test_marks_on_unknown_fd_are_ignored);
    return UNITY_END();
}
