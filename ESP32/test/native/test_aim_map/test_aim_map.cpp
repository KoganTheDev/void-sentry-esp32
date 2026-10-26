/**
 * @file test_aim_map.cpp
 * @brief Host-side tests for the image-to-angle map, its 2-point fit, and the NVS calibration blob.
 */

#include <unity.h>

#include "aim_map.h"
#include "calibration_blob.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

using namespace aiming;

void setUp(void) {}
void tearDown(void) {}

void test_default_map_centre_is_home(void)
{
    const Angles a = default_map().aim(160.0f, 120.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, a.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, a.tilt);
}

void test_default_map_edges(void)
{
    const AimMap m = default_map();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.0f, m.aim(320.0f, 120.0f).pan);  // right edge: +half FOV
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -33.0f, m.aim(0.0f, 120.0f).pan);   // left edge
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 115.0f, m.aim(160.0f, 0.0f).tilt);  // top of image: tilt up
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, m.aim(160.0f, 240.0f).tilt); // bottom: tilt down
}

void test_fit_reproduces_points_and_is_linear(void)
{
    const CalPoint p1{.x = 40.0f, .y = 50.0f, .pan = -30.0f, .tilt = 110.0f};
    const CalPoint p2{.x = 280.0f, .y = 200.0f, .pan = 25.0f, .tilt = 70.0f};
    const auto m = fit(p1, p2);
    TEST_ASSERT_TRUE(m.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -30.0f, m->aim(40.0f, 50.0f).pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, m->aim(40.0f, 50.0f).tilt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, m->aim(280.0f, 200.0f).pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 70.0f, m->aim(280.0f, 200.0f).tilt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.5f, m->aim(160.0f, 125.0f).pan); // midpoint
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, m->aim(160.0f, 125.0f).tilt);
}

void test_fit_rejects_points_too_close(void)
{
    TEST_ASSERT_FALSE(fit({100, 50, 0, 100}, {170, 200, 10, 70}).has_value()); // dx 70 < 80
    TEST_ASSERT_FALSE(fit({40, 100, 0, 100}, {280, 150, 10, 70}).has_value()); // dy 50 < 60
    TEST_ASSERT_TRUE(fit({40, 100, 0, 100}, {120, 160, 10, 70}).has_value());  // exactly 80 / 60
}

void test_fit_rejects_non_finite(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(fit({40, 50, nan, 100}, {280, 200, 10, 70}).has_value());
}

void test_aim_is_clamped_to_soft_limits(void)
{
    const AimMap steep{.pan_a = 2.0f, .pan_b = -320.0f, .tilt_a = -2.0f, .tilt_b = 330.0f};
    const Angles lo = steep.aim(0.0f, 240.0f);
    const Angles hi = steep.aim(320.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(kPanMinDeg, lo.pan);
    TEST_ASSERT_EQUAL_FLOAT(kTiltMinDeg, lo.tilt);
    TEST_ASSERT_EQUAL_FLOAT(kPanMaxDeg, hi.pan);
    TEST_ASSERT_EQUAL_FLOAT(kTiltMaxDeg, hi.tilt);
}

void test_deg_steps_round_trip(void)
{
    TEST_ASSERT_EQUAL_INT32(512, pan_deg_to_steps(90.0f));
    TEST_ASSERT_EQUAL_INT32(-512, pan_deg_to_steps(-90.0f));
    TEST_ASSERT_EQUAL_INT32(6, pan_deg_to_steps(1.0f)); // 5.69 rounds to 6
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, pan_steps_to_deg(512));
}

void test_crc32_known_vector(void)
{
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32({reinterpret_cast<const uint8_t*>(s), 9}));
}

void test_blob_round_trip(void)
{
    const AimMap m{.pan_a = 0.25f, .pan_b = -40.0f, .tilt_a = -0.2f, .tilt_b = 112.0f};
    const Blob b = serialize(m);
    const auto back = deserialize(b);
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_EQUAL_FLOAT(m.pan_a, back->pan_a);
    TEST_ASSERT_EQUAL_FLOAT(m.pan_b, back->pan_b);
    TEST_ASSERT_EQUAL_FLOAT(m.tilt_a, back->tilt_a);
    TEST_ASSERT_EQUAL_FLOAT(m.tilt_b, back->tilt_b);
}

void test_blob_rejects_wrong_length(void)
{
    const Blob b = serialize(default_map());
    TEST_ASSERT_FALSE(deserialize({b.data(), b.size() - 1}).has_value());
    TEST_ASSERT_FALSE(deserialize({}).has_value());
}

void test_blob_rejects_bad_magic_version_and_crc(void)
{
    Blob b = serialize(default_map());
    Blob bad_magic = b;
    bad_magic[0] ^= 0xFF;
    TEST_ASSERT_FALSE(deserialize(bad_magic).has_value());

    Blob bad_version = b;
    bad_version[4] = 9; // version field (little-endian uint16 at offset 4)
    TEST_ASSERT_FALSE(deserialize(bad_version).has_value());

    Blob bad_payload = b;
    bad_payload[10] ^= 0x01; // inside the floats: CRC must catch it
    TEST_ASSERT_FALSE(deserialize(bad_payload).has_value());
}

void test_blob_rejects_non_finite_values(void)
{
    AimMap m = default_map();
    m.tilt_b = std::numeric_limits<float>::infinity();
    const Blob b = serialize(m); // CRC is valid, value is not
    TEST_ASSERT_FALSE(deserialize(b).has_value());
}

void test_blob_rejects_non_finite_in_every_field(void)
{
    const float bad[] = {std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::infinity()};
    for (const float v : bad)
    {
        for (int field = 0; field < 4; ++field)
        {
            AimMap m = default_map();
            float* slots[] = {&m.pan_a, &m.pan_b, &m.tilt_a, &m.tilt_b};
            *slots[field] = v;
            TEST_ASSERT_FALSE(deserialize(serialize(m)).has_value()); // the CRC is valid, only the value is not
        }
    }
}

void test_blob_rejects_corruption_in_each_header_and_crc_byte(void)
{
    const Blob good = serialize(default_map());
    TEST_ASSERT_TRUE(deserialize(good).has_value());
    // magic (0..3), version (4..5) and the stored CRC (the last 4 bytes) each reject on a single flipped bit
    const size_t offsets[] = {0, 1, 2, 3, 4, 5, good.size() - 4, good.size() - 3, good.size() - 2, good.size() - 1};
    for (const size_t off : offsets)
    {
        Blob b = good;
        b[off] ^= 0x01;
        TEST_ASSERT_FALSE(deserialize(b).has_value());
    }
}

void test_blob_rejects_oversized_and_all_zero_input(void)
{
    const Blob b = serialize(default_map());
    std::array<uint8_t, sizeof(Blob) + 1> longer{};
    std::copy(b.begin(), b.end(), longer.begin());
    TEST_ASSERT_FALSE(deserialize(longer).has_value());
    const Blob zeros{};
    TEST_ASSERT_FALSE(deserialize(zeros).has_value());
}

void test_aim_non_finite_pixel_falls_back_to_home(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const Angles a = default_map().aim(nan, nan);
    TEST_ASSERT_TRUE(std::isfinite(a.pan));
    TEST_ASSERT_TRUE(std::isfinite(a.tilt));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, a.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, a.tilt);
}

void test_aim_infinite_pixel_gives_finite_in_range_angles(void)
{
    const float inf = std::numeric_limits<float>::infinity();
    const Angles a = default_map().aim(inf, -inf);
    TEST_ASSERT_TRUE(std::isfinite(a.pan));
    TEST_ASSERT_TRUE(std::isfinite(a.tilt));
    TEST_ASSERT_TRUE(a.pan >= kPanMinDeg && a.pan <= kPanMaxDeg);
    TEST_ASSERT_TRUE(a.tilt >= kTiltMinDeg && a.tilt <= kTiltMaxDeg);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, a.pan);          // non-finite pan -> home
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, a.tilt); // non-finite tilt -> home
}

void test_pan_deg_to_steps_guards_non_finite_and_huge(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_EQUAL_INT32(0, pan_deg_to_steps(nan));
    TEST_ASSERT_EQUAL_INT32(0, pan_deg_to_steps(std::numeric_limits<float>::infinity()));
    TEST_ASSERT_EQUAL_INT32(512, pan_deg_to_steps(1e30f));
    TEST_ASSERT_EQUAL_INT32(-512, pan_deg_to_steps(-1e30f));
}

void test_fit_overflow_gives_finite_or_nothing(void)
{
    // dx = 6e38 overflows float to inf: must be rejected, not turned into a degenerate map.
    TEST_ASSERT_FALSE(fit({3e38f, 50, 0, 100}, {-3e38f, 200, 10, 70}).has_value());

    // Pan values near FLT_MAX: (p2.pan - p1.pan) overflows, so pan_a would be inf.
    TEST_ASSERT_FALSE(fit({40, 50, 3e38f, 100}, {280, 200, -3e38f, 70}).has_value());

    // Large but representable x values: whatever comes back must have all-finite coefficients.
    const auto m = fit({1e38f, 50, 0, 100}, {-1e38f, 200, 10, 70});
    if (m.has_value())
    {
        TEST_ASSERT_TRUE(std::isfinite(m->pan_a));
        TEST_ASSERT_TRUE(std::isfinite(m->pan_b));
        TEST_ASSERT_TRUE(std::isfinite(m->tilt_a));
        TEST_ASSERT_TRUE(std::isfinite(m->tilt_b));
    }
}

static void expect_round_trip(const AimMap& m, Angles a)
{
    const Pixel px = m.to_pixel(a);
    const Angles back = m.aim(px.x, px.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, a.pan, back.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, a.tilt, back.tilt);
}

void test_to_pixel_known_values(void)
{
    const AimMap m = default_map();
    const Pixel c = m.to_pixel(Angles{0.0f, kTiltHomeDeg});
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 160.0f, c.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, c.y);
    const Pixel r = m.to_pixel(Angles{33.0f, 115.0f}); // right edge, top
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 320.0f, r.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, r.y);
    // Not clamped: an angle far outside the frame gives a pixel outside the frame.
    TEST_ASSERT_TRUE(m.to_pixel(Angles{90.0f, 90.0f}).x > 320.0f);
    TEST_ASSERT_TRUE(m.to_pixel(Angles{-90.0f, 90.0f}).x < 0.0f);
}

void test_to_pixel_round_trip_default_map(void)
{
    const AimMap m = default_map();
    expect_round_trip(m, Angles{0.0f, 90.0f});
    expect_round_trip(m, Angles{12.5f, 100.0f});
    expect_round_trip(m, Angles{-33.0f, 65.0f});
    expect_round_trip(m, Angles{kPanMaxDeg, kTiltMaxDeg});
    expect_round_trip(m, Angles{kPanMinDeg, kTiltMinDeg});
}

void test_to_pixel_round_trip_fitted_map_with_negative_slopes(void)
{
    // pan falls and tilt falls as x and y grow: both slopes negative.
    const auto m = fit(CalPoint{.x = 40.0f, .y = 50.0f, .pan = 30.0f, .tilt = 120.0f},
                       CalPoint{.x = 280.0f, .y = 200.0f, .pan = -25.0f, .tilt = 60.0f});
    TEST_ASSERT_TRUE(m.has_value());
    TEST_ASSERT_TRUE(m->pan_a < 0.0f);
    TEST_ASSERT_TRUE(m->tilt_a < 0.0f);
    expect_round_trip(*m, Angles{0.0f, 90.0f});
    expect_round_trip(*m, Angles{-20.0f, 70.0f});
    expect_round_trip(*m, Angles{30.0f, 120.0f});
    // And a map with a negative pan slope but a positive tilt slope.
    const AimMap mixed{.pan_a = -0.3f, .pan_b = 40.0f, .tilt_a = 0.4f, .tilt_b = 30.0f};
    expect_round_trip(mixed, Angles{10.0f, 100.0f});
    const Pixel px = mixed.to_pixel(Angles{10.0f, 100.0f});
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, px.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 175.0f, px.y);
}

void test_to_pixel_degenerate_slope_gives_frame_centre(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const AimMap flat{.pan_a = 0.0f, .pan_b = 5.0f, .tilt_a = 0.0f, .tilt_b = 100.0f};
    Pixel p = flat.to_pixel(Angles{20.0f, 120.0f});
    TEST_ASSERT_EQUAL_FLOAT(kFrameW / 2.0f, p.x);
    TEST_ASSERT_EQUAL_FLOAT(kFrameH / 2.0f, p.y);
    const AimMap bad{.pan_a = nan, .pan_b = 0.0f, .tilt_a = inf, .tilt_b = 90.0f};
    p = bad.to_pixel(Angles{20.0f, 120.0f});
    TEST_ASSERT_EQUAL_FLOAT(kFrameW / 2.0f, p.x);
    TEST_ASSERT_EQUAL_FLOAT(kFrameH / 2.0f, p.y);
    // One axis fine, the other degenerate: only the bad axis falls back.
    const AimMap half{.pan_a = 0.5f, .pan_b = -80.0f, .tilt_a = 0.0f, .tilt_b = 90.0f};
    p = half.to_pixel(Angles{0.0f, 90.0f});
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 160.0f, p.x);
    TEST_ASSERT_EQUAL_FLOAT(kFrameH / 2.0f, p.y);
    // Finite slope but a non-finite offset or angle also gives the centre instead of a NaN.
    const AimMap bad_b{.pan_a = 1.0f, .pan_b = nan, .tilt_a = 1.0f, .tilt_b = 0.0f};
    p = bad_b.to_pixel(Angles{nan, 100.0f});
    TEST_ASSERT_EQUAL_FLOAT(kFrameW / 2.0f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, p.y);
}

void test_reach_box_default_map_is_whole_frame(void)
{
    const ReachBox b = reach_box(default_map());
    TEST_ASSERT_EQUAL_INT(0, b.x0);
    TEST_ASSERT_EQUAL_INT(0, b.y0);
    TEST_ASSERT_EQUAL_INT(kFrameW - 1, b.x1);
    TEST_ASSERT_EQUAL_INT(kFrameH - 1, b.y1);
}

void test_reach_box_coarse_map_is_inside_frame(void)
{
    // pan -90..90 -> x 10..190; tilt 160..20 -> y 40..180 (falling slope, so the ends swap).
    const AimMap m{.pan_a = 1.0f, .pan_b = -100.0f, .tilt_a = -1.0f, .tilt_b = 200.0f};
    ReachBox b = reach_box(m);
    TEST_ASSERT_EQUAL_INT(10, b.x0);
    TEST_ASSERT_EQUAL_INT(40, b.y0);
    TEST_ASSERT_EQUAL_INT(190, b.x1);
    TEST_ASSERT_EQUAL_INT(180, b.y1);
    // The corners of the box aim exactly at the soft limits.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kPanMinDeg, m.aim(10.0f, 40.0f).pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kPanMaxDeg, m.aim(190.0f, 40.0f).pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltMaxDeg, m.aim(10.0f, 40.0f).tilt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltMinDeg, m.aim(10.0f, 180.0f).tilt);
    // A negative pan slope gives the same ordered box.
    const AimMap neg{.pan_a = -1.0f, .pan_b = 100.0f, .tilt_a = -1.0f, .tilt_b = 200.0f};
    b = reach_box(neg);
    TEST_ASSERT_EQUAL_INT(10, b.x0);
    TEST_ASSERT_EQUAL_INT(190, b.x1);
    TEST_ASSERT_TRUE(b.x0 <= b.x1);
    TEST_ASSERT_TRUE(b.y0 <= b.y1);
}

void test_reach_box_rounds_and_clamps(void)
{
    const AimMap m{.pan_a = 1.0f, .pan_b = -100.6f, .tilt_a = 1.0f, .tilt_b = -40.4f};
    const ReachBox b = reach_box(m); // x 10.6..190.6 -> 11..191, y: tilt 20..160 -> 60.4..200.4 -> 60..200
    TEST_ASSERT_EQUAL_INT(11, b.x0);
    TEST_ASSERT_EQUAL_INT(191, b.x1);
    TEST_ASSERT_EQUAL_INT(60, b.y0);
    TEST_ASSERT_EQUAL_INT(200, b.y1);
    // A region entirely right of / below the frame collapses onto the last column / row.
    const AimMap away{.pan_a = 1.0f, .pan_b = -500.0f, .tilt_a = 1.0f, .tilt_b = -500.0f};
    const ReachBox c = reach_box(away);
    TEST_ASSERT_EQUAL_INT(kFrameW - 1, c.x0);
    TEST_ASSERT_EQUAL_INT(kFrameW - 1, c.x1);
    TEST_ASSERT_EQUAL_INT(kFrameH - 1, c.y0);
    TEST_ASSERT_EQUAL_INT(kFrameH - 1, c.y1);
    // ... and one entirely left of / above it collapses onto column / row 0.
    const AimMap before{.pan_a = 1.0f, .pan_b = 500.0f, .tilt_a = 1.0f, .tilt_b = 500.0f};
    const ReachBox d = reach_box(before);
    TEST_ASSERT_EQUAL_INT(0, d.x0);
    TEST_ASSERT_EQUAL_INT(0, d.x1);
    TEST_ASSERT_EQUAL_INT(0, d.y0);
    TEST_ASSERT_EQUAL_INT(0, d.y1);
}

void test_reach_box_degenerate_slope_is_whole_frame(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const AimMap flat{.pan_a = 0.0f, .pan_b = 0.0f, .tilt_a = nan, .tilt_b = 90.0f};
    const ReachBox b = reach_box(flat);
    TEST_ASSERT_EQUAL_INT(0, b.x0);
    TEST_ASSERT_EQUAL_INT(0, b.y0);
    TEST_ASSERT_EQUAL_INT(kFrameW - 1, b.x1);
    TEST_ASSERT_EQUAL_INT(kFrameH - 1, b.y1);
}

void test_limit_bits(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, limit_bits(Angles{0.0f, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(1, limit_bits(Angles{kPanMinDeg, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(2, limit_bits(Angles{kPanMaxDeg, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(4, limit_bits(Angles{0.0f, kTiltMinDeg}));
    TEST_ASSERT_EQUAL_UINT8(8, limit_bits(Angles{0.0f, kTiltMaxDeg}));
    TEST_ASSERT_EQUAL_UINT8(1 | 4, limit_bits(Angles{kPanMinDeg, kTiltMinDeg}));
    TEST_ASSERT_EQUAL_UINT8(2 | 8, limit_bits(Angles{kPanMaxDeg, kTiltMaxDeg}));
    TEST_ASSERT_EQUAL_UINT8(1 | 8, limit_bits(Angles{kPanMinDeg, kTiltMaxDeg}));
    TEST_ASSERT_EQUAL_UINT8(2 | 4, limit_bits(Angles{kPanMaxDeg, kTiltMinDeg}));
}

void test_limit_bits_threshold_is_half_a_degree(void)
{
    TEST_ASSERT_EQUAL_UINT8(1, limit_bits(Angles{kPanMinDeg + 0.5f, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(0, limit_bits(Angles{kPanMinDeg + 0.6f, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(2, limit_bits(Angles{kPanMaxDeg - 0.5f, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(0, limit_bits(Angles{kPanMaxDeg - 0.6f, 90.0f}));
    TEST_ASSERT_EQUAL_UINT8(4, limit_bits(Angles{0.0f, kTiltMinDeg + 0.5f}));
    TEST_ASSERT_EQUAL_UINT8(0, limit_bits(Angles{0.0f, kTiltMinDeg + 0.6f}));
    TEST_ASSERT_EQUAL_UINT8(8, limit_bits(Angles{0.0f, kTiltMaxDeg - 0.5f}));
    TEST_ASSERT_EQUAL_UINT8(0, limit_bits(Angles{0.0f, kTiltMaxDeg - 0.6f}));
    // Beyond the limit (never produced by aim(), but harmless) still counts as at the limit.
    TEST_ASSERT_EQUAL_UINT8(1 | 4, limit_bits(Angles{-120.0f, 0.0f}));
    TEST_ASSERT_EQUAL_UINT8(2 | 8, limit_bits(Angles{120.0f, 200.0f}));
    // A non-finite angle sets no bit.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_EQUAL_UINT8(0, limit_bits(Angles{nan, nan}));
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_map_centre_is_home);
    RUN_TEST(test_default_map_edges);
    RUN_TEST(test_fit_reproduces_points_and_is_linear);
    RUN_TEST(test_fit_rejects_points_too_close);
    RUN_TEST(test_fit_rejects_non_finite);
    RUN_TEST(test_aim_is_clamped_to_soft_limits);
    RUN_TEST(test_deg_steps_round_trip);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_blob_round_trip);
    RUN_TEST(test_blob_rejects_wrong_length);
    RUN_TEST(test_blob_rejects_bad_magic_version_and_crc);
    RUN_TEST(test_blob_rejects_non_finite_values);
    RUN_TEST(test_blob_rejects_non_finite_in_every_field);
    RUN_TEST(test_blob_rejects_corruption_in_each_header_and_crc_byte);
    RUN_TEST(test_blob_rejects_oversized_and_all_zero_input);
    RUN_TEST(test_aim_non_finite_pixel_falls_back_to_home);
    RUN_TEST(test_aim_infinite_pixel_gives_finite_in_range_angles);
    RUN_TEST(test_pan_deg_to_steps_guards_non_finite_and_huge);
    RUN_TEST(test_fit_overflow_gives_finite_or_nothing);
    RUN_TEST(test_to_pixel_known_values);
    RUN_TEST(test_to_pixel_round_trip_default_map);
    RUN_TEST(test_to_pixel_round_trip_fitted_map_with_negative_slopes);
    RUN_TEST(test_to_pixel_degenerate_slope_gives_frame_centre);
    RUN_TEST(test_reach_box_default_map_is_whole_frame);
    RUN_TEST(test_reach_box_coarse_map_is_inside_frame);
    RUN_TEST(test_reach_box_rounds_and_clamps);
    RUN_TEST(test_reach_box_degenerate_slope_is_whole_frame);
    RUN_TEST(test_limit_bits);
    RUN_TEST(test_limit_bits_threshold_is_half_a_degree);
    return UNITY_END();
}
