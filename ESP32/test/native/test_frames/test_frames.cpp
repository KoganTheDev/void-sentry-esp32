/**
 * @file test_frames.cpp
 * @brief Host-side tests for the frame slot bookkeeping, leases and the JPEG completeness check.
 */

#include <unity.h>
#include <utility>

#include "frame_slots.h"
#include "jpeg_check.h"

using namespace frames;

void setUp(void) {}
void tearDown(void) {}

namespace
{
FrameMeta meta_of(size_t length)
{
    FrameMeta m;
    m.length = length;
    m.width = 320;
    m.height = 240;
    return m;
}

// Publishes one frame and returns the slot it went into.
size_t publish_one(FrameSlots& slots, size_t length = 100)
{
    auto slot = slots.begin_write();
    TEST_ASSERT_TRUE(slot.has_value());
    slots.publish(*slot, meta_of(length));
    return *slot;
}

struct CountingReleaser : SlotReleaser {
    int calls = 0;
    size_t last_slot = 99;
    void release_slot(size_t slot) override
    {
        ++calls;
        last_slot = slot;
    }
};
} // namespace

// --- is_complete_jpeg ---

void test_jpeg_accepts_soi_and_eoi(void)
{
    const uint8_t frame[] = {0xFF, 0xD8, 0x00, 0x11, 0xFF, 0xD9};
    const uint8_t minimal[] = {0xFF, 0xD8, 0xFF, 0xD9};
    TEST_ASSERT_TRUE(is_complete_jpeg(frame, sizeof(frame)));
    TEST_ASSERT_TRUE(is_complete_jpeg(minimal, sizeof(minimal)));
}

void test_jpeg_rejects_null_and_short(void)
{
    const uint8_t data[] = {0xFF, 0xD8, 0xFF};
    TEST_ASSERT_FALSE(is_complete_jpeg(nullptr, 10));
    TEST_ASSERT_FALSE(is_complete_jpeg(data, 0));
    TEST_ASSERT_FALSE(is_complete_jpeg(data, 3));
}

void test_jpeg_rejects_missing_soi(void)
{
    const uint8_t data[] = {0x00, 0xD8, 0x11, 0xFF, 0xD9};
    TEST_ASSERT_FALSE(is_complete_jpeg(data, sizeof(data)));
}

void test_jpeg_rejects_truncated_frame(void)
{
    const uint8_t data[] = {0xFF, 0xD8, 0x11, 0x22, 0x33};
    TEST_ASSERT_FALSE(is_complete_jpeg(data, sizeof(data)));
}

// --- FrameSlots ---

void test_acquire_before_publish_is_empty(void)
{
    FrameSlots slots;
    TEST_ASSERT_FALSE(slots.acquire_latest().has_value());
}

void test_acquire_returns_latest_frame(void)
{
    FrameSlots slots;
    publish_one(slots, 100);
    size_t newest = publish_one(slots, 200);
    auto lease = slots.acquire_latest();
    TEST_ASSERT_TRUE(lease.has_value());
    TEST_ASSERT_EQUAL_UINT(newest, lease->slot());
    TEST_ASSERT_EQUAL_UINT(200, lease->meta().length);
    TEST_ASSERT_EQUAL_UINT32(2, lease->meta().sequence);
}

void test_sequences_increase(void)
{
    FrameSlots slots;
    for (uint32_t i = 1; i <= 10; ++i)
    {
        publish_one(slots);
        auto lease = slots.acquire_latest();
        TEST_ASSERT_EQUAL_UINT32(i, lease->meta().sequence);
    }
    TEST_ASSERT_EQUAL_UINT32(10, slots.frames_published());
}

void test_leased_slot_is_never_rewritten(void)
{
    FrameSlots slots;
    size_t held_slot = publish_one(slots);
    auto held = slots.acquire_latest();
    for (int i = 0; i < 20; ++i)
    {
        auto slot = slots.begin_write();
        TEST_ASSERT_TRUE(slot.has_value());
        TEST_ASSERT_NOT_EQUAL(held_slot, *slot);
        slots.publish(*slot, meta_of(50));
    }
    TEST_ASSERT_EQUAL_UINT(held_slot, held->slot());
}

void test_drops_when_every_slot_is_held(void)
{
    FrameSlots slots;
    publish_one(slots);
    auto a = slots.acquire_latest();
    publish_one(slots);
    auto b = slots.acquire_latest();
    publish_one(slots);
    auto c = slots.acquire_latest();
    auto writing = slots.begin_write();
    TEST_ASSERT_TRUE(writing.has_value());
    TEST_ASSERT_FALSE(slots.begin_write().has_value());
    TEST_ASSERT_EQUAL_UINT32(1, slots.frames_dropped_no_slot());
}

void test_released_stale_slot_becomes_free(void)
{
    FrameSlots slots;
    size_t first = publish_one(slots);
    auto held = slots.acquire_latest();
    auto w1 = slots.begin_write();
    auto w2 = slots.begin_write();
    auto w3 = slots.begin_write();
    TEST_ASSERT_TRUE(w1.has_value() && w2.has_value() && w3.has_value());
    slots.publish(*w1, meta_of(10)); // `first` is now stale but still leased
    TEST_ASSERT_FALSE(slots.begin_write().has_value());
    held->release();
    auto next = slots.begin_write();
    TEST_ASSERT_TRUE(next.has_value());
    TEST_ASSERT_EQUAL_UINT(first, *next);
}

void test_lease_move_releases_exactly_once(void)
{
    CountingReleaser releaser;
    {
        FrameLease a(releaser, 2, meta_of(10));
        FrameLease b(std::move(a));
        TEST_ASSERT_FALSE(a.valid());
        TEST_ASSERT_TRUE(b.valid());
    }
    TEST_ASSERT_EQUAL_INT(1, releaser.calls);
    TEST_ASSERT_EQUAL_UINT(2, releaser.last_slot);
}

void test_lease_move_assignment_releases_old_slot(void)
{
    CountingReleaser releaser;
    FrameLease a(releaser, 1, meta_of(10));
    FrameLease b(releaser, 3, meta_of(10));
    b = std::move(a);
    TEST_ASSERT_EQUAL_INT(1, releaser.calls);
    TEST_ASSERT_EQUAL_UINT(3, releaser.last_slot);
    b.release();
    TEST_ASSERT_EQUAL_INT(2, releaser.calls);
    TEST_ASSERT_EQUAL_UINT(1, releaser.last_slot);
    b.release();
    TEST_ASSERT_EQUAL_INT(2, releaser.calls);
}

void test_abort_frees_slot_and_counts_reason(void)
{
    FrameSlots slots;
    auto w = slots.begin_write();
    slots.abort_write(*w, AbortReason::Oversize);
    w = slots.begin_write();
    slots.abort_write(*w, AbortReason::Corrupt);
    w = slots.begin_write();
    slots.abort_write(*w, AbortReason::CaptureFailed);
    TEST_ASSERT_EQUAL_UINT32(1, slots.frames_dropped_oversize());
    TEST_ASSERT_EQUAL_UINT32(1, slots.frames_dropped_corrupt());
    for (size_t i = 0; i < kSlotCount; ++i)
    {
        TEST_ASSERT_TRUE(slots.begin_write().has_value());
    }
    TEST_ASSERT_EQUAL_UINT32(0, slots.frames_dropped_no_slot());
    TEST_ASSERT_FALSE(slots.acquire_latest().has_value());
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_jpeg_accepts_soi_and_eoi);
    RUN_TEST(test_jpeg_rejects_null_and_short);
    RUN_TEST(test_jpeg_rejects_missing_soi);
    RUN_TEST(test_jpeg_rejects_truncated_frame);
    RUN_TEST(test_acquire_before_publish_is_empty);
    RUN_TEST(test_acquire_returns_latest_frame);
    RUN_TEST(test_sequences_increase);
    RUN_TEST(test_leased_slot_is_never_rewritten);
    RUN_TEST(test_drops_when_every_slot_is_held);
    RUN_TEST(test_released_stale_slot_becomes_free);
    RUN_TEST(test_lease_move_releases_exactly_once);
    RUN_TEST(test_lease_move_assignment_releases_old_slot);
    RUN_TEST(test_abort_frees_slot_and_counts_reason);
    return UNITY_END();
}
