/**
 * @file test_motion_detector.cpp
 * @brief Host-side tests for the background-model motion detector on synthetic 40x30 frames.
 */

#include <unity.h>

#include "motion_detector.h"

using namespace tracking;

void setUp(void) {}
void tearDown(void) {}

static GrayFrame flat(uint8_t value)
{
    GrayFrame f;
    f.fill(value);
    return f;
}

static void fill_rect(GrayFrame& f, int x0, int y0, int x1, int y1, uint8_t value)
{
    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            f[y * kGridW + x] = value;
        }
    }
}

/** Seed frame + warm-up frames, so the next process() call detects. */
static void warm_up(MotionDetector& d, const GrayFrame& background)
{
    for (int i = 0; i < 1 + DetectorConfig{}.warmup_frames; ++i)
    {
        d.process(background);
    }
}

void test_seed_and_warmup_report_no_blobs(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    GrayFrame with_rect = bg;
    fill_rect(with_rect, 5, 5, 10, 10, 200);

    TEST_ASSERT_TRUE(d.process(bg).warming_up); // seed
    for (int i = 0; i < DetectorConfig{}.warmup_frames; ++i)
    {
        const DetectionResult& r = d.process(with_rect);
        TEST_ASSERT_TRUE(r.warming_up);
        TEST_ASSERT_EQUAL_INT(0, r.blob_count);
    }
    // Warm-up copies every frame into the background (it does not learn at 1/16), so a rectangle that is present
    // during warm-up is fully absorbed and the first real frame reports nothing. (It used to be only partly
    // absorbed, diff 150 -> ~89, and showed up as a blob.)
    const DetectionResult& r = d.process(with_rect);
    TEST_ASSERT_FALSE(r.warming_up);
    TEST_ASSERT_EQUAL_INT(0, r.blob_count);
    TEST_ASSERT_EQUAL_INT(0, r.foreground_cells);
}

void test_rectangle_becomes_one_blob(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 10, 18, 19, 27, 200); // 10x10 = 100 cells; centroid = box midpoint = (10+19)/2, (18+27)/2

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(10, r.blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(18, r.blobs[0].box.y0);
    TEST_ASSERT_EQUAL_INT(19, r.blobs[0].box.x1);
    TEST_ASSERT_EQUAL_INT(27, r.blobs[0].box.y1);
    TEST_ASSERT_EQUAL_INT(100, r.blobs[0].area);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.5f, r.blobs[0].cx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 22.5f, r.blobs[0].cy);
}

void test_single_cells_are_dropped_by_min_blob_cells(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    f[5 * kGridW + 5] = 200; // three isolated cells (one in the last row/column corner)
    f[15 * kGridW + 20] = 200;
    f[29 * kGridW + 39] = 200;

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(3, r.foreground_cells); // they are foreground, but each is a 1-cell component < 2
    TEST_ASSERT_EQUAL_INT(0, r.blob_count);
}

void test_two_cell_blob_is_reported(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    f[5 * kGridW + 5] = 200;           // a single cell: dropped
    fill_rect(f, 30, 10, 31, 10, 200); // 2x1 blob: exactly min_blob_cells, reported

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(3, r.foreground_cells);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(2, r.blobs[0].area);
    TEST_ASSERT_EQUAL_INT(30, r.blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(31, r.blobs[0].box.x1);
    TEST_ASSERT_EQUAL_INT(10, r.blobs[0].box.y0);
    TEST_ASSERT_EQUAL_INT(10, r.blobs[0].box.y1);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 30.5f, r.blobs[0].cx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, r.blobs[0].cy);
}

void test_two_blobs_sorted_by_area(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 2, 2, 5, 5, 200);     // 4x4 = 16 cells
    fill_rect(f, 20, 15, 29, 20, 200); // 10x6 = 60 cells

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(2, r.blob_count);
    TEST_ASSERT_EQUAL_INT(60, r.blobs[0].area);
    TEST_ASSERT_EQUAL_INT(16, r.blobs[1].area);
}

void test_u_shape_is_one_blob(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 5, 5, 7, 25, 200);   // left bar, 3x21 = 63
    fill_rect(f, 20, 5, 22, 25, 200); // right bar, 63
    fill_rect(f, 5, 23, 22, 25, 200); // bottom bar 18x3 = 54, joins them only at the end of the raster scan

    // Union by inclusion-exclusion: the bottom bar overlaps each bar in a 3x3 = 9 cell corner, so
    // 63 + 63 + 54 - 9 - 9 = 162.
    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(162, r.blobs[0].area);
}

void test_diagonal_neighbours_connect(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 10, 10, 12, 12, 200);
    fill_rect(f, 13, 13, 15, 15, 200); // touches the first square only at a corner

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(18, r.blobs[0].area);
}

void test_brightness_step_is_scene_change(void)
{
    MotionDetector d;
    warm_up(d, flat(50));

    const DetectionResult& step = d.process(flat(120));
    TEST_ASSERT_TRUE(step.scene_change);
    TEST_ASSERT_EQUAL_INT(0, step.blob_count);
    for (int i = 0; i < DetectorConfig{}.warmup_frames; ++i)
    {
        const DetectionResult& r = d.process(flat(120));
        TEST_ASSERT_TRUE(r.warming_up);
        TEST_ASSERT_FALSE(r.scene_change);
    }
    const DetectionResult& r = d.process(flat(120));
    TEST_ASSERT_FALSE(r.warming_up);
    TEST_ASSERT_FALSE(r.scene_change);
    TEST_ASSERT_EQUAL_INT(0, r.blob_count);
}

void test_large_local_object_is_not_a_scene_change(void)
{
    MotionDetector d;
    GrayFrame bg = flat(100);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 0, 0, kGridW - 1, 14, 140); // 40x15 = 600 cells = 50% of the grid (> 35%), mean shift = 20 (< 25)

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_FALSE(r.scene_change);
    TEST_ASSERT_FALSE(r.warming_up);
    TEST_ASSERT_EQUAL_INT(600, r.foreground_cells);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(600, r.blobs[0].area);
}

void test_warmup_absorbs_settling_exposure(void)
{
    MotionDetector d;
    warm_up(d, flat(50));
    TEST_ASSERT_TRUE(d.process(flat(120)).scene_change); // lights on: reseed + warm-up

    // Auto-exposure keeps drifting for 10 frames (5 gray levels each, 120 -> 170), then holds at 170.
    int value = 120;
    for (int i = 0; i < 10; ++i)
    {
        value += 5;
        const DetectionResult& r = d.process(flat(static_cast<uint8_t>(value)));
        TEST_ASSERT_TRUE(r.warming_up);
        TEST_ASSERT_FALSE(r.scene_change);
    }
    for (int i = 0; i < DetectorConfig{}.warmup_frames - 10; ++i)
    {
        TEST_ASSERT_TRUE(d.process(flat(170)).warming_up);
    }
    const DetectionResult& r = d.process(flat(170));
    TEST_ASSERT_FALSE(r.warming_up);
    TEST_ASSERT_EQUAL_INT(0, r.foreground_cells);
    TEST_ASSERT_EQUAL_INT(0, r.blob_count);
}

void test_static_white_object_is_absorbed_without_wrap(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 15, 10, 22, 17, 255);

    TEST_ASSERT_EQUAL_INT(1, d.process(f).blob_count);
    for (int i = 0; i < 40; ++i)
    {
        d.process(f);
    }
    TEST_ASSERT_EQUAL_INT(1, d.process(f).blob_count); // foreground learns slowly (1/128) at first: still there
    for (int i = 0; i < 400; ++i)
    {
        d.process(f);
    }
    TEST_ASSERT_EQUAL_INT(0, d.process(f).blob_count); // absorbed (the age limit switched it to the fast rate), and the
                                                       // Q8 background did not wrap
    TEST_ASSERT_EQUAL_INT(0, d.process(f).foreground_cells);
}

/**
 * Ghost bound. A cell whose background is wrong by 130 (190 vs 60) is foreground from the first frame after the
 * object leaves. For fg_age_limit = 34 frames it learns at 1/128: 130 * (127/128)^34 = ~100. Then it learns at
 * 1/16: a blob needs the rounded background within 20 of 60, so 100 * (15/16)^n <= 20.5 gives n = 25. Total
 * ~59 frames; the bound used is 34 + 40 = 74 (margin for integer truncation). Never aging the foreground rate would
 * need ~240 frames, so a regression to that behaviour fails this bound.
 */
static constexpr int kGhostBound = 34 + 40;

static constexpr int kPausedGhostBound = 24;

static int frames_until_no_blobs(MotionDetector& d, const GrayFrame& f, int limit)
{
    for (int n = 1; n <= limit; ++n)
    {
        if (d.process(f).blob_count == 0)
        {
            return n;
        }
    }
    return -1;
}

void test_object_in_seed_frame_ghost_clears(void)
{
    MotionDetector d;
    const GrayFrame bg = flat(60);
    GrayFrame with_square = bg;
    fill_rect(with_square, 5, 13, 8, 16, 190); // 4x4
    warm_up(d, with_square);                   // the square is part of the seed background
    TEST_ASSERT_EQUAL_INT(0, d.process(with_square).blob_count);

    // It leaves: the background still remembers it, so the empty spot is a ghost blob at first.
    const DetectionResult& first = d.process(bg);
    TEST_ASSERT_EQUAL_INT(1, first.blob_count);
    TEST_ASSERT_TRUE(first.blobs[0].box.x0 >= 5 && first.blobs[0].box.x1 <= 8);
    TEST_ASSERT_TRUE(first.blobs[0].box.y0 >= 13 && first.blobs[0].box.y1 <= 16);

    const int n = frames_until_no_blobs(d, bg, kGhostBound);
    TEST_ASSERT_TRUE_MESSAGE(n > 0, "ghost still present after the bound");
    TEST_ASSERT_EQUAL_INT(0, d.process(bg).blob_count);
}

void test_paused_object_ghost_clears_after_leaving(void)
{
    MotionDetector d;
    const GrayFrame bg = flat(60);
    warm_up(d, bg);
    GrayFrame with_square = bg;
    fill_rect(with_square, 5, 13, 8, 16, 190); // 4x4
    for (int i = 0; i < 40; ++i)
    {
        d.process(with_square); // static for 40 frames: past the age limit, background partly absorbed it
    }
    TEST_ASSERT_EQUAL_INT(1, d.process(with_square).blob_count);

    // Left behind. The square is foreground for 41 frames: the first 34 learn at 1/128, the last 7 (age 35..41) at
    // 1/16 because they are past fg_age_limit. The background is then 190 - 63.4 = 126.6, i.e. 66.6 above the empty
    // spot's 60. The cells are still foreground (age 42 and up, so 1/16 from the first frame), and a blob needs the
    // rounded background more than 20 from 60 (>= 20.5 above). 66.6 * (15/16)^n >= 20.5 holds for n <= 18, so frames
    // 1..19 still show the blob and the first empty frame is 20 (an integer simulation of the update agrees). The
    // bound 24 leaves 4 frames of margin.
    // If the cells had never aged (1/128 for every frame, the behaviour before the age rule) the ghost would last
    // about 73 frames, so that old behaviour FAILS this assertion.
    const int n = frames_until_no_blobs(d, bg, kPausedGhostBound);
    TEST_ASSERT_TRUE_MESSAGE(n > 0, "ghost still present after the bound");
    TEST_ASSERT_EQUAL_INT(0, d.process(bg).blob_count);
}

void test_moving_square_leaves_no_trail(void)
{
    MotionDetector d;
    GrayFrame bg = flat(60);
    warm_up(d, bg);
    const DetectionResult* r = nullptr;
    for (int step = 0; step < 30; ++step)
    {
        GrayFrame f = bg;
        fill_rect(f, 2 + step, 13, 5 + step, 16, 190); // 4x4 square moving 1 cell per frame
        r = &d.process(f);
    }
    // Last frame: x = 31..34. A trail would widen the box or add cells behind the square.
    TEST_ASSERT_EQUAL_INT(1, r->blob_count);
    TEST_ASSERT_EQUAL_INT(31, r->blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(34, r->blobs[0].box.x1);
    TEST_ASSERT_EQUAL_INT(16, r->blobs[0].area);
}

void test_blob_touching_border(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 0, 0, 3, 2, 200); // 4x3 = 12 cells in the top-left corner

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(0, r.blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(0, r.blobs[0].box.y0);
    TEST_ASSERT_EQUAL_INT(3, r.blobs[0].box.x1);
    TEST_ASSERT_EQUAL_INT(2, r.blobs[0].box.y1);
    TEST_ASSERT_EQUAL_INT(12, r.blobs[0].area);
}

void test_max_blobs_keeps_largest(void)
{
    DetectorConfig cfg;
    cfg.max_blobs = 2;
    MotionDetector d(cfg);
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 2, 2, 4, 4, 200);     // 3x3 = 9
    fill_rect(f, 10, 2, 15, 7, 200);   // 6x6 = 36
    fill_rect(f, 25, 15, 29, 19, 200); // 5x5 = 25

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(2, r.blob_count);
    TEST_ASSERT_EQUAL_INT(36, r.blobs[0].area);
    TEST_ASSERT_EQUAL_INT(25, r.blobs[1].area);
}

void test_min_blob_cells_drops_small_blobs(void)
{
    DetectorConfig cfg;
    cfg.min_blob_cells = 20;
    MotionDetector d(cfg);
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 10, 10, 13, 13, 200); // 16 cells

    TEST_ASSERT_EQUAL_INT(0, d.process(f).blob_count);
}

void test_many_components_are_capped(void)
{
    DetectorConfig cfg;
    cfg.scene_change_fraction = 1.0f; // let a busy frame through to stress the labelling arrays
    MotionDetector d(cfg);
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    // 2x2 blobs (>= min_blob_cells) on a pitch of 3 (1-cell gaps, so no 8-connection between them). Columns start at
    // x = 0, 3, ..., 36 (13 of them; x = 39 would need cell 40) and rows at y = 0, 3, ..., 27 (10): 130 components,
    // 520 foreground cells (below the scene-change limit only because the fraction is 1.0).
    for (int y = 0; y + 1 < kGridH; y += 3)
    {
        for (int x = 0; x + 1 < kGridW; x += 3)
        {
            fill_rect(f, x, y, x + 1, y + 1, 200);
        }
    }

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_FALSE(r.label_overflow);
    TEST_ASSERT_EQUAL_INT(kMaxBlobs, r.blob_count);
    TEST_ASSERT_EQUAL_INT(520, r.foreground_cells);
    TEST_ASSERT_EQUAL_INT(130 - 64, r.dropped_components); // only the first 64 components are tracked
    TEST_ASSERT_EQUAL_INT(4, r.blobs[0].area);
}

void test_reset_reseeds(void)
{
    MotionDetector d;
    warm_up(d, flat(50));
    d.reset();
    TEST_ASSERT_TRUE(d.process(flat(50)).warming_up);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_seed_and_warmup_report_no_blobs);
    RUN_TEST(test_rectangle_becomes_one_blob);
    RUN_TEST(test_single_cells_are_dropped_by_min_blob_cells);
    RUN_TEST(test_two_cell_blob_is_reported);
    RUN_TEST(test_two_blobs_sorted_by_area);
    RUN_TEST(test_u_shape_is_one_blob);
    RUN_TEST(test_diagonal_neighbours_connect);
    RUN_TEST(test_brightness_step_is_scene_change);
    RUN_TEST(test_large_local_object_is_not_a_scene_change);
    RUN_TEST(test_warmup_absorbs_settling_exposure);
    RUN_TEST(test_static_white_object_is_absorbed_without_wrap);
    RUN_TEST(test_moving_square_leaves_no_trail);
    RUN_TEST(test_object_in_seed_frame_ghost_clears);
    RUN_TEST(test_paused_object_ghost_clears_after_leaving);
    RUN_TEST(test_blob_touching_border);
    RUN_TEST(test_max_blobs_keeps_largest);
    RUN_TEST(test_min_blob_cells_drops_small_blobs);
    RUN_TEST(test_many_components_are_capped);
    RUN_TEST(test_reset_reseeds);
    return UNITY_END();
}
