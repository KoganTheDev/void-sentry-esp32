/**
 * @file test_motion_detector_equiv.cpp
 * @brief Proves the fused-pass MotionDetector gives bit-identical results to the frozen two-pass reference.
 * @details Every frame of every committed bench clip and of seeded pseudo-random sequences (moving and parked
 * objects, global brightness steps that cause scene changes, noise bursts, resets) goes through both detectors; the
 * whole DetectionResult (flags, counts, every blob) must match on every frame.
 */

#include <unity.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "motion_detector.h"
#include "reference_motion_detector.h"

using namespace tracking;

void setUp(void) {}
void tearDown(void) {}

static bool same_result(const DetectionResult& a, const DetectionResult& b)
{
    if (a.scene_change != b.scene_change || a.warming_up != b.warming_up || a.label_overflow != b.label_overflow ||
        a.foreground_cells != b.foreground_cells || a.dropped_components != b.dropped_components ||
        a.blob_count != b.blob_count)
    {
        return false;
    }
    for (int i = 0; i < a.blob_count; ++i)
    {
        const Blob& x = a.blobs[i];
        const Blob& y = b.blobs[i];
        if (x.box.x0 != y.box.x0 || x.box.y0 != y.box.y0 || x.box.x1 != y.box.x1 || x.box.y1 != y.box.y1 ||
            x.area != y.area || x.cx != y.cx || x.cy != y.cy)
        {
            return false;
        }
    }
    return true;
}

struct Totals {
    int frames = 0;
    int scene_changes = 0;
    int blob_frames = 0;
};

static void run_both(const std::vector<GrayFrame>& frames, const DetectorConfig& cfg, int reset_every, Totals& t)
{
    MotionDetector fused(cfg);
    tracking_ref::ReferenceMotionDetector ref(cfg);
    for (size_t n = 0; n < frames.size(); ++n)
    {
        if (reset_every > 0 && n > 0 && n % reset_every == 0)
        {
            fused.reset();
            ref.reset();
        }
        const DetectionResult& a = fused.process(frames[n]);
        const DetectionResult& b = ref.process(frames[n]);
        char msg[64];
        std::snprintf(msg, sizeof msg, "result differs at frame %u", static_cast<unsigned>(n));
        TEST_ASSERT_TRUE_MESSAGE(same_result(a, b), msg);
        ++t.frames;
        t.scene_changes += a.scene_change ? 1 : 0;
        t.blob_frames += a.blob_count > 0 ? 1 : 0;
    }
}

static bool load_clip(const char* name, std::vector<GrayFrame>& out)
{
    // pio test runs from ESP32/; allow the repo root too.
    for (const char* prefix : {"bench/fixtures/", "ESP32/bench/fixtures/", "../bench/fixtures/"})
    {
        const std::string path = std::string(prefix) + name;
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f)
        {
            continue;
        }
        GrayFrame frame;
        while (std::fread(frame.data(), 1, kCells, f) == static_cast<size_t>(kCells))
        {
            out.push_back(frame);
        }
        std::fclose(f);
        return !out.empty();
    }
    return false;
}

void test_bench_clips_match(void)
{
    const char* clips[] = {"light_switch.gray",      "pause_leave.gray", "real_cross_room.gray", "real_hand_pause.gray",
                           "real_light_switch.gray", "seed_object.gray", "square_cross.gray"};
    Totals t;
    for (const char* name : clips)
    {
        std::vector<GrayFrame> frames;
        TEST_ASSERT_TRUE_MESSAGE(load_clip(name, frames), name);
        run_both(frames, DetectorConfig{}, 0, t);
    }
    TEST_ASSERT_GREATER_THAN(300, t.frames);
    TEST_ASSERT_GREATER_THAN(0, t.scene_changes); // the light-switch clips exercise the early-return path
    TEST_ASSERT_GREATER_THAN(0, t.blob_frames);
}

// xorshift32: deterministic, no <random> state differences across libstdc++ versions.
struct Rng {
    uint32_t s;
    uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int range(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

static uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

static std::vector<GrayFrame> random_sequence(uint32_t seed, int count, int noise)
{
    Rng rng{seed * 2654435761u + 1u};
    std::vector<GrayFrame> frames;
    int base_level = 90;
    int rx = 5, ry = 5, vx = 1, vy = 1; // moving object
    int park_x = 25, park_y = 15, park_until = -1;
    for (int n = 0; n < count; ++n)
    {
        if (n % 130 == 129)
        {
            base_level += (rng.range(2) ? 70 : -60); // global step: scene change
        }
        if (n % 90 == 20)
        {
            park_until = n + 40 + rng.range(60); // an object stops for a while (fg_age / ghost paths)
            park_x = rng.range(30);
            park_y = rng.range(20);
        }
        GrayFrame f;
        for (int y = 0; y < kGridH; ++y)
        {
            for (int x = 0; x < kGridW; ++x)
            {
                f[y * kGridW + x] = clamp8(base_level + ((x * 3 + y * 5) % 17) + rng.range(2 * noise + 1) - noise);
            }
        }
        rx += vx;
        ry += vy;
        if (rx < 0 || rx > kGridW - 8)
        {
            vx = -vx;
            rx += 2 * vx;
        }
        if (ry < 0 || ry > kGridH - 8)
        {
            vy = -vy;
            ry += 2 * vy;
        }
        for (int y = ry; y < ry + 6; ++y)
        {
            for (int x = rx; x < rx + 6; ++x)
            {
                f[y * kGridW + x] = 230;
            }
        }
        if (n < park_until)
        {
            for (int y = park_y; y < park_y + 7 && y < kGridH; ++y)
            {
                for (int x = park_x; x < park_x + 9 && x < kGridW; ++x)
                {
                    f[y * kGridW + x] = 10;
                }
            }
        }
        if (n % 200 == 150)
        {
            for (int i = 0; i < kCells; ++i) // checkerboard burst: many tiny blobs
            {
                if (((i % kGridW) + (i / kGridW)) % 2 == 0)
                {
                    f[i] = clamp8(f[i] + 100);
                }
            }
        }
        frames.push_back(f);
    }
    return frames;
}

void test_random_sequences_match_default_config(void)
{
    Totals t;
    for (uint32_t seed = 1; seed <= 12; ++seed)
    {
        run_both(random_sequence(seed, 700, 2 + static_cast<int>(seed % 4)), DetectorConfig{}, 0, t);
    }
    TEST_ASSERT_GREATER_THAN(10, t.scene_changes);
    TEST_ASSERT_GREATER_THAN(1000, t.blob_frames);
}

void test_random_sequences_match_with_resets_and_odd_configs(void)
{
    Totals t;
    DetectorConfig tight;
    tight.diff_threshold = 6;
    tight.fg_age_limit = 5;
    tight.warmup_frames = 3;
    DetectorConfig slow;
    slow.bg_shift = 6;
    slow.fg_shift = 9;
    slow.scene_change_fraction = 0.2f;
    slow.scene_change_mean_shift = 10;
    slow.max_blobs = 3;
    for (uint32_t seed = 20; seed < 28; ++seed)
    {
        const std::vector<GrayFrame> frames = random_sequence(seed, 600, 3);
        run_both(frames, tight, 0, t);
        run_both(frames, slow, 0, t);
        run_both(frames, DetectorConfig{}, 97, t); // reset() mid-stream, including during warm-up
    }
    TEST_ASSERT_GREATER_THAN(0, t.scene_changes);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_bench_clips_match);
    RUN_TEST(test_random_sequences_match_default_config);
    RUN_TEST(test_random_sequences_match_with_resets_and_odd_configs);
    return UNITY_END();
}
