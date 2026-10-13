/**
 * @file test_tracker.cpp
 * @brief Host-side tests for the single-target alpha-beta tracker and direction_for().
 */

#include <unity.h>

#include <initializer_list>

#include "direction.h"
#include "tracker.h"

using namespace tracking;

void setUp(void) {}
void tearDown(void) {}

static Blob rect_blob(int x0, int y0, int x1, int y1)
{
    Blob b;
    b.box = Box{x0, y0, x1, y1};
    b.area = (x1 - x0 + 1) * (y1 - y0 + 1);
    b.cx = (x0 + x1) / 2.0f;
    b.cy = (y0 + y1) / 2.0f;
    return b;
}

/** Blobs must be passed largest first, as the detector reports them. */
static DetectionResult with(std::initializer_list<Blob> blobs)
{
    DetectionResult r;
    for (const Blob& b : blobs)
    {
        r.blobs[r.blob_count++] = b;
    }
    return r;
}

static DetectionResult none() { return DetectionResult{}; }

static DetectionResult scene_change()
{
    DetectionResult r;
    r.scene_change = true;
    r.warming_up = true;
    return r;
}

/** Lock onto a static 4x4 target at (5,5)-(8,8): exactly confirm_frames updates. */
static void lock_static(Tracker& t)
{
    for (int i = 0; i < TrackerConfig{}.confirm_frames; ++i)
    {
        t.update(with({rect_blob(5, 5, 8, 8)}));
    }
}

void test_locks_after_confirm_frames(void)
{
    Tracker t;
    TEST_ASSERT_TRUE(TrackPhase::Tentative == t.update(with({rect_blob(5, 5, 8, 8)})).state);
    TEST_ASSERT_TRUE(TrackPhase::Tentative == t.update(with({rect_blob(6, 5, 9, 8)})).state);
    TEST_ASSERT_EQUAL_UINT32(0, t.state().id);
    const TrackState& s = t.update(with({rect_blob(7, 5, 10, 8)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
}

void test_tracks_constant_velocity(void)
{
    Tracker t;
    for (int f = 0; f < 25; ++f)
    {
        t.update(with({rect_blob(5 + f, 10, 8 + f, 13)}));
    }
    const TrackState& s = t.state();
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 6.5f + 24.0f, s.cx); // 4-wide box: cx = x0 + 1.5
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, s.vx);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s.vy);
}

void test_locks_on_fast_target(void)
{
    Tracker t;
    for (int f = 0; f < 15; ++f)
    {
        t.update(with({rect_blob(1 + 2 * f, 10, 4 + 2 * f, 13)}));
    }
    const TrackState& s = t.state();
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 2.0f, s.vx);
}

void test_single_frame_blob_never_locks(void)
{
    Tracker t;
    t.update(with({rect_blob(5, 5, 8, 8)}));
    TEST_ASSERT_TRUE(TrackPhase::Idle == t.update(none()).state);
    TEST_ASSERT_EQUAL_UINT32(0, t.state().id);
    lock_static(t);
    TEST_ASSERT_EQUAL_UINT32(1, t.state().id); // the flicker consumed no id
}

void test_pause_then_reappear_keeps_id(void)
{
    Tracker t;
    lock_static(t);
    for (int i = 0; i < 10; ++i)
    {
        t.update(none());
    }
    TEST_ASSERT_TRUE(TrackPhase::Coasting == t.state().state);
    TEST_ASSERT_EQUAL_INT(10, t.state().frames_since_seen);
    const TrackState& s = t.update(with({rect_blob(5, 5, 8, 8)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
    TEST_ASSERT_EQUAL_INT(0, s.frames_since_seen);
}

void test_lost_after_coast_frames(void)
{
    Tracker t;
    lock_static(t);
    for (int i = 0; i < 17; ++i)
    {
        t.update(none());
    }
    TEST_ASSERT_TRUE(TrackPhase::Coasting == t.state().state);
    TEST_ASSERT_TRUE(TrackPhase::Idle == t.update(none()).state);
    TEST_ASSERT_EQUAL_UINT32(0, t.state().id);
    lock_static(t);
    TEST_ASSERT_EQUAL_UINT32(2, t.state().id); // ids are never reused
}

void test_ignores_bigger_blob_outside_gate(void)
{
    Tracker t;
    lock_static(t);
    for (int i = 0; i < 5; ++i)
    {
        t.update(with({rect_blob(20, 15, 35, 28), rect_blob(5, 5, 8, 8)}));
    }
    const TrackState& s = t.state();
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6.5f, s.cx);
    TEST_ASSERT_EQUAL_INT(16, s.area);
}

void test_merges_split_blobs_inside_gate(void)
{
    Tracker t;
    for (int i = 0; i < 3; ++i)
    {
        t.update(with({rect_blob(10, 10, 17, 17)})); // 8x8 target, centre (13.5, 13.5)
    }
    // The target splits into a top part (rows 10-12) and a bottom part (rows 15-17), two empty rows between; a far
    // blob must not be merged. Merged area 24 + 24 = 48; centroid y = (11 * 24 + 16 * 24) / 48 = 13.5.
    const TrackState& s =
        t.update(with({rect_blob(30, 22, 37, 29), rect_blob(10, 10, 17, 12), rect_blob(10, 15, 17, 17)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_INT(48, s.area);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.5f, s.cx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.5f, s.cy);
}

void test_scene_change_while_locked_coasts_then_reacquires(void)
{
    Tracker t;
    lock_static(t);
    TEST_ASSERT_TRUE(TrackPhase::Coasting == t.update(scene_change()).state);
    for (int i = 0; i < 8; ++i)
    {
        t.update(none()); // detector warm-up frames
    }
    TEST_ASSERT_TRUE(TrackPhase::Coasting == t.state().state);
    const TrackState& s = t.update(with({rect_blob(5, 5, 8, 8)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
}

void test_scene_change_while_tentative_goes_idle(void)
{
    Tracker t;
    t.update(with({rect_blob(5, 5, 8, 8)}));
    TEST_ASSERT_TRUE(TrackPhase::Idle == t.update(scene_change()).state);
}

void test_coasting_extrapolates_with_decay(void)
{
    Tracker t;
    for (int f = 0; f < 20; ++f)
    {
        t.update(with({rect_blob(5 + f, 10, 8 + f, 13)}));
    }
    const float cx = t.state().cx;
    const float vx = t.state().vx;
    const TrackState& s = t.update(none());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cx + vx, s.cx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, vx * 0.8f, s.vx);
}

void test_coasting_stays_on_the_grid(void)
{
    Tracker t;
    for (int f = 0; f < 19; ++f)
    {
        // 2 cells/frame; the last input box (f = 18) is 36..39, so every input lies on the 40-wide grid
        t.update(with({rect_blob(2 * f, 10, 2 * f + 3, 13)}));
    }
    TEST_ASSERT_TRUE(TrackPhase::Locked == t.state().state);
    for (int i = 0; i < 10; ++i)
    {
        const TrackState& s = t.update(none());
        TEST_ASSERT_TRUE(TrackPhase::Coasting == s.state);
        TEST_ASSERT_TRUE(s.cx <= 39.0f);
        TEST_ASSERT_TRUE(s.cx >= 0.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 39.0f, t.state().cx); // pinned at the right edge, not at 45+
}

void test_reset_goes_idle_and_keeps_id_counter(void)
{
    Tracker t;
    lock_static(t);
    t.reset();
    TEST_ASSERT_TRUE(TrackPhase::Idle == t.state().state);
    lock_static(t);
    TEST_ASSERT_EQUAL_UINT32(2, t.state().id);
}

static TrackState at(TrackPhase phase, float cx, float cy)
{
    TrackState s;
    s.state = phase;
    s.cx = cx;
    s.cy = cy;
    return s;
}

void test_direction_for(void)
{
    // Grid centre is (19.5, 14.5); the default deadzone is 2.5 cells.
    auto d = direction_for(at(TrackPhase::Locked, 19.5f, 14.5f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Locked, 5.0f, 2.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::Left);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::Up);

    d = direction_for(at(TrackPhase::Coasting, 35.0f, 27.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::Right);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::Down);

    d = direction_for(at(TrackPhase::Locked, 21.5f, 16.0f)); // dx = 2.0, dy = 1.5: inside the 2.5-cell deadzone
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Locked, 22.0f, 17.0f)); // dx = dy = 2.5 exactly: still inside (strict compare)
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Locked, 22.2f, 17.2f)); // just outside the deadzone
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::Right);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::Down);

    d = direction_for(at(TrackPhase::Idle, 5.0f, 2.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Tentative, 5.0f, 2.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_locks_after_confirm_frames);
    RUN_TEST(test_tracks_constant_velocity);
    RUN_TEST(test_locks_on_fast_target);
    RUN_TEST(test_single_frame_blob_never_locks);
    RUN_TEST(test_pause_then_reappear_keeps_id);
    RUN_TEST(test_lost_after_coast_frames);
    RUN_TEST(test_ignores_bigger_blob_outside_gate);
    RUN_TEST(test_merges_split_blobs_inside_gate);
    RUN_TEST(test_scene_change_while_locked_coasts_then_reacquires);
    RUN_TEST(test_scene_change_while_tentative_goes_idle);
    RUN_TEST(test_coasting_extrapolates_with_decay);
    RUN_TEST(test_coasting_stays_on_the_grid);
    RUN_TEST(test_reset_goes_idle_and_keeps_id_counter);
    RUN_TEST(test_direction_for);
    return UNITY_END();
}
