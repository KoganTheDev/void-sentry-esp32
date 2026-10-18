/**
 * @file test_jpeg_gray.cpp
 * @brief Host-side tests for the 1/8-scale JPEG to 40x30 gray decoder, on QVGA fixtures made by
 * tools/jpeg_fixtures/make_jpeg_fixtures.py.
 */

#include <unity.h>

#include <cstdlib>

#include "jpeg_fixtures.h"
#include "jpeg_gray.h"

using namespace tracking;

void setUp(void) {}
void tearDown(void) {}

namespace
{
constexpr int kTolerance = 6; // JPEG quantisation plus the integer luma approximation

template <size_t N> bool decode(const uint8_t (&jpeg)[N], GrayFrame& out) { return jpeg_gray::decode(jpeg, N, out); }

bool near(int actual, int expected) { return std::abs(actual - expected) <= kTolerance; }
} // namespace

void test_uniform_image_decodes_to_uniform_grid(void)
{
    GrayFrame g{};
    TEST_ASSERT_TRUE(decode(jpeg_fixtures::kUniformJpeg, g));
    for (int i = 0; i < kCells; ++i)
    {
        TEST_ASSERT_TRUE_MESSAGE(near(g[i], 128), "cell not near 128");
    }
}

void test_half_image_left_dark_right_bright(void)
{
    GrayFrame g{};
    TEST_ASSERT_TRUE(decode(jpeg_fixtures::kHalfJpeg, g));
    for (int y = 0; y < kGridH; ++y)
    {
        for (int x = 0; x < kGridW; ++x)
        {
            const int expected = x < kGridW / 2 ? 32 : 224;
            TEST_ASSERT_TRUE_MESSAGE(near(g[y * kGridW + x], expected), "cell not near expected side value");
        }
    }
}

void test_decode_overwrites_previous_content(void)
{
    GrayFrame g{};
    g.fill(7);
    TEST_ASSERT_TRUE(decode(jpeg_fixtures::kUniformJpeg, g));
    TEST_ASSERT_TRUE(near(g[0], 128));
    TEST_ASSERT_TRUE(near(g[kCells - 1], 128));
}

void test_truncated_jpeg_fails(void)
{
    GrayFrame g{};
    TEST_ASSERT_FALSE(decode(jpeg_fixtures::kTruncatedJpeg, g));
}

void test_non_qvga_size_is_rejected(void)
{
    GrayFrame g{};
    TEST_ASSERT_FALSE(decode(jpeg_fixtures::kWrongSizeJpeg, g));
}

void test_header_only_prefix_fails(void)
{
    GrayFrame g{};
    TEST_ASSERT_FALSE(jpeg_gray::decode(jpeg_fixtures::kHalfJpeg, 2, g));
}

void test_garbage_bytes_fail(void)
{
    const uint8_t junk[64] = {0x12, 0x34, 0x56};
    GrayFrame g{};
    TEST_ASSERT_FALSE(jpeg_gray::decode(junk, sizeof(junk), g));
}

void test_null_and_empty_input_fail(void)
{
    GrayFrame g{};
    TEST_ASSERT_FALSE(jpeg_gray::decode(nullptr, 100, g));
    TEST_ASSERT_FALSE(jpeg_gray::decode(jpeg_fixtures::kUniformJpeg, 0, g));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_uniform_image_decodes_to_uniform_grid);
    RUN_TEST(test_half_image_left_dark_right_bright);
    RUN_TEST(test_decode_overwrites_previous_content);
    RUN_TEST(test_truncated_jpeg_fails);
    RUN_TEST(test_non_qvga_size_is_rejected);
    RUN_TEST(test_header_only_prefix_fails);
    RUN_TEST(test_garbage_bytes_fail);
    RUN_TEST(test_null_and_empty_input_fail);
    return UNITY_END();
}
