/**
 * @file test_aim_map.cpp
 * @brief Host-side tests for the image-to-angle map, its 2-point fit, and the NVS calibration blob.
 */

#include <unity.h>

#include "aim_map.h"
#include "calibration_blob.h"

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
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.0f, m.aim(320.0f, 120.0f).pan);   // right edge: +half FOV
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -33.0f, m.aim(0.0f, 120.0f).pan);    // left edge
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 115.0f, m.aim(160.0f, 0.0f).tilt);   // top of image: tilt up
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, m.aim(160.0f, 240.0f).tilt);  // bottom: tilt down
}

void test_fit_reproduces_points_and_is_linear(void)
{
    const CalPoint p1{40.0f, 50.0f, -30.0f, 110.0f};
    const CalPoint p2{280.0f, 200.0f, 25.0f, 70.0f};
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
    const AimMap steep{2.0f, -320.0f, -2.0f, 330.0f};
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
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32(reinterpret_cast<const uint8_t*>(s), 9));
}

void test_blob_round_trip(void)
{
    const AimMap m{0.25f, -40.0f, -0.2f, 112.0f};
    const Blob b = serialize(m);
    const auto back = deserialize(b.data(), b.size());
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_EQUAL_FLOAT(m.pan_a, back->pan_a);
    TEST_ASSERT_EQUAL_FLOAT(m.pan_b, back->pan_b);
    TEST_ASSERT_EQUAL_FLOAT(m.tilt_a, back->tilt_a);
    TEST_ASSERT_EQUAL_FLOAT(m.tilt_b, back->tilt_b);
}

void test_blob_rejects_wrong_length(void)
{
    const Blob b = serialize(default_map());
    TEST_ASSERT_FALSE(deserialize(b.data(), b.size() - 1).has_value());
    TEST_ASSERT_FALSE(deserialize(nullptr, kBlobSize).has_value());
}

void test_blob_rejects_bad_magic_version_and_crc(void)
{
    Blob b = serialize(default_map());
    Blob bad_magic = b;
    bad_magic[0] ^= 0xFF;
    TEST_ASSERT_FALSE(deserialize(bad_magic.data(), bad_magic.size()).has_value());

    Blob bad_version = b;
    bad_version[4] = 9; // version field (little-endian uint16 at offset 4)
    TEST_ASSERT_FALSE(deserialize(bad_version.data(), bad_version.size()).has_value());

    Blob bad_payload = b;
    bad_payload[10] ^= 0x01; // inside the floats: CRC must catch it
    TEST_ASSERT_FALSE(deserialize(bad_payload.data(), bad_payload.size()).has_value());
}

void test_blob_rejects_non_finite_values(void)
{
    AimMap m = default_map();
    m.tilt_b = std::numeric_limits<float>::infinity();
    const Blob b = serialize(m); // CRC is valid, value is not
    TEST_ASSERT_FALSE(deserialize(b.data(), b.size()).has_value());
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
    return UNITY_END();
}
