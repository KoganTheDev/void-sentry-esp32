/**
 * @file test_motion_detector.cpp
 * @brief Host-side tests for the background-model motion detector on synthetic 80x60 frames.
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
    fill_rect(with_rect, 10, 10, 20, 20, 200);

    TEST_ASSERT_TRUE(d.process(bg).warming_up); // seed
    for (int i = 0; i < 8; ++i)
    {
        const DetectionResult& r = d.process(with_rect);
        TEST_ASSERT_TRUE(r.warming_up);
        TEST_ASSERT_EQUAL_INT(0, r.blob_count);
    }
    // Warm-up learned every cell at the fast rate (1/16) for 8 frames: the rectangle is only partly absorbed
    // (diff 150 -> ~89), so the first real frame reports it.
    const DetectionResult& r = d.process(with_rect);
    TEST_ASSERT_FALSE(r.warming_up);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
}

void test_rectangle_becomes_one_blob(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 10, 20, 19, 29, 200);

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(10, r.blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(20, r.blobs[0].box.y0);
    TEST_ASSERT_EQUAL_INT(19, r.blobs[0].box.x1);
    TEST_ASSERT_EQUAL_INT(29, r.blobs[0].box.y1);
    TEST_ASSERT_EQUAL_INT(100, r.blobs[0].area);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.5f, r.blobs[0].cx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 24.5f, r.blobs[0].cy);
}

void test_isolated_noise_is_removed(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    f[5 * kGridW + 5] = 200;
    f[30 * kGridW + 40] = 200;
    f[59 * kGridW + 79] = 200;
    fill_rect(f, 60, 10, 61, 11, 200); // 2x2: no full 3x3 neighbourhood, removed by the opening

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(7, r.foreground_cells);
    TEST_ASSERT_EQUAL_INT(0, r.blob_count);
}

void test_two_blobs_sorted_by_area(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 5, 5, 8, 8, 200);     // 16 cells
    fill_rect(f, 40, 30, 49, 35, 200); // 60 cells

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
    fill_rect(f, 10, 10, 12, 30, 200); // left bar
    fill_rect(f, 30, 10, 32, 30, 200); // right bar
    fill_rect(f, 10, 28, 32, 30, 200); // bottom bar joins them only at the end of the raster scan

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(177, r.blobs[0].area);
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
    for (int i = 0; i < 8; ++i)
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

void test_static_white_object_is_absorbed_without_wrap(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 30, 20, 37, 27, 255);

    TEST_ASSERT_EQUAL_INT(1, d.process(f).blob_count);
    for (int i = 0; i < 40; ++i)
    {
        d.process(f);
    }
    TEST_ASSERT_EQUAL_INT(1, d.process(f).blob_count); // foreground learns slowly (1/128): still there
    for (int i = 0; i < 400; ++i)
    {
        d.process(f);
    }
    TEST_ASSERT_EQUAL_INT(0, d.process(f).blob_count); // absorbed, and the Q8 background did not wrap
    TEST_ASSERT_EQUAL_INT(0, d.process(f).foreground_cells);
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
        fill_rect(f, 10 + step, 26, 17 + step, 33, 190);
        r = &d.process(f);
    }
    TEST_ASSERT_EQUAL_INT(1, r->blob_count);
    TEST_ASSERT_EQUAL_INT(39, r->blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(46, r->blobs[0].box.x1);
}

void test_blob_touching_border(void)
{
    MotionDetector d;
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 0, 0, 5, 4, 200);

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_EQUAL_INT(1, r.blob_count);
    TEST_ASSERT_EQUAL_INT(0, r.blobs[0].box.x0);
    TEST_ASSERT_EQUAL_INT(0, r.blobs[0].box.y0);
    TEST_ASSERT_EQUAL_INT(5, r.blobs[0].box.x1);
    TEST_ASSERT_EQUAL_INT(4, r.blobs[0].box.y1);
    TEST_ASSERT_EQUAL_INT(30, r.blobs[0].area);
}

void test_max_blobs_keeps_largest(void)
{
    DetectorConfig cfg;
    cfg.max_blobs = 2;
    MotionDetector d(cfg);
    GrayFrame bg = flat(50);
    warm_up(d, bg);
    GrayFrame f = bg;
    fill_rect(f, 2, 2, 4, 4, 200);     // 9
    fill_rect(f, 20, 2, 25, 7, 200);   // 36
    fill_rect(f, 40, 30, 44, 34, 200); // 25

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
    for (int y = 0; y + 2 < kGridH; y += 4)
    {
        for (int x = 0; x + 2 < kGridW; x += 4)
        {
            fill_rect(f, x, y, x + 2, y + 2, 200); // 3x3 squares, 1-cell gaps: 20 x 15 = 300 components
        }
    }

    const DetectionResult& r = d.process(f);
    TEST_ASSERT_FALSE(r.label_overflow);
    TEST_ASSERT_EQUAL_INT(kMaxBlobs, r.blob_count);
    TEST_ASSERT_EQUAL_INT(300 - 64, r.dropped_components);
    TEST_ASSERT_EQUAL_INT(9, r.blobs[0].area);
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
    RUN_TEST(test_isolated_noise_is_removed);
    RUN_TEST(test_two_blobs_sorted_by_area);
    RUN_TEST(test_u_shape_is_one_blob);
    RUN_TEST(test_diagonal_neighbours_connect);
    RUN_TEST(test_brightness_step_is_scene_change);
    RUN_TEST(test_static_white_object_is_absorbed_without_wrap);
    RUN_TEST(test_moving_square_leaves_no_trail);
    RUN_TEST(test_blob_touching_border);
    RUN_TEST(test_max_blobs_keeps_largest);
    RUN_TEST(test_min_blob_cells_drops_small_blobs);
    RUN_TEST(test_many_components_are_capped);
    RUN_TEST(test_reset_reseeds);
    return UNITY_END();
}
