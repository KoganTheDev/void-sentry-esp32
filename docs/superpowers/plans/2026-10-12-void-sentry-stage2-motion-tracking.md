# Stage 2 Motion Detection and Persistent Tracking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect moving objects in the camera frames and keep a persistent single-target lock (stable id, short memory), shown on the web HUD, without moving the turret.

**Architecture:** A device adapter decodes each QVGA JPEG at 1/4 scale (ROM TJpgDec via `esp_jpg_decode`) into an 80x60 gray grid. A pure C++17 library `lib/tracking` (no Arduino/ESP-IDF headers) runs a background-model motion detector (conditional EMA, 3x3 opening, union-find blobs) and an alpha-beta single-target tracker. `CameraDiffDetection` glues them together in `loop()`, publishes a lock-protected snapshot for the `/commands` WebSocket, and the HUD draws the target box.

**Tech Stack:** C++17, PlatformIO (espressif32@6.12.0, Arduino core 2.0.17, GCC 8.4 on device; host g++ >= 8), Unity, ESP-IDF `esp_http_server`, esp32-camera `esp_jpg_decode`, Python 3 (Pillow, websockets) for fixture tools, GitHub Actions (valgrind cachegrind, perf best effort).

**Spec:** `docs/superpowers/specs/2026-10-12-void-sentry-stage2-motion-tracking-design.md`

## Global Constraints

- Language standard C++17 everywhere; no C++20 features (device GCC 8.4).
- `lib/tracking` must not include Arduino, ESP-IDF, or FreeRTOS headers; it is compiled by `native`, `native_sanitize`, the firmware, and the plain-g++ bench.
- No heap allocation anywhere on the per-frame detection path (pure lib: fixed `std::array` members only; device: one PSRAM allocation in `begin()`).
- Grid is exactly 80x60 (`kGridW = 80`, `kGridH = 60`, `kCells = 4800`); frames are QVGA 320x240, scale factor 4.
- Never read, print, or edit `ESP32/include/secrets.h`; never run a shell command containing the substring `secret` (a hook blocks it).
- Never use em dashes in code comments, docs, or commit messages; use " - ".
- Formatting: `.clang-format` (Allman braces for functions/classes/control statements, 4-space indent, 120 columns, `int* p`). The PostToolUse hook formats firmware C/C++ on edit; CI checks with clang-format 19.
- Public headers use Doxygen `@brief`/`@details` blocks.
- Pure-lib members use a trailing underscore (`config_`), as in `lib/frames`; device classes use a leading underscore with `this->` (as in `lib/detection`).
- Never pipe `pio` into `head`/`grep -m1`; redirect to a log file and grep the file. `pio` is `~/.platformio/penv/Scripts/pio.exe`, run from `ESP32/`.
- Commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Moving objects must not leave a trail.** An 8x8 square moving 1 cell/frame must produce a blob about 8 cells wide, not a smear. Pinned by `test_moving_square_leaves_no_trail` (Task 1).
2. **Fast movers.** A target moving 3 cells/frame must still lock and stay locked. Pinned by `test_locks_on_fast_target` (Task 2).
3. **Border targets.** A blob touching the grid edge keeps its full box (the opening treats outside as background but dilation restores the edge). Pinned by `test_blob_touching_border` (Task 1); the device snapshot clamps boxes to the frame (Task 3).
4. **Too many components.** A scene with hundreds of small components must not overflow fixed arrays; it caps at `kMaxBlobs` and counts the rest. Pinned by `test_many_components_are_capped` (Task 1).
5. **Brightest pixels and long runs.** A 255-valued static object must not wrap the Q8 background and must eventually be absorbed. Pinned by `test_static_white_object_is_absorbed_without_wrap` (Task 1).

---

## File map

| File | Task | Responsibility |
|---|---|---|
| `ESP32/lib/tracking/include/tracking_types.h` | 1 | Grid constants, `GrayFrame`, `Box`, `Blob`, configs, `DetectionResult`, `TrackPhase`, `TrackState` |
| `ESP32/lib/tracking/include/motion_detector.h`, `src/motion_detector.cpp` | 1 | Background model, mask, opening, scene-change guard, blobs |
| `ESP32/test/native/test_motion_detector/test_motion_detector.cpp` | 1 | Detector tests |
| `ESP32/lib/tracking/include/tracker.h`, `src/tracker.cpp` | 2 | Alpha-beta single-target tracker |
| `ESP32/lib/tracking/include/direction.h` | 2 | `direction_for()` -> `MoveX`/`MoveY` |
| `ESP32/test/native/test_tracker/test_tracker.cpp` | 2 | Tracker + direction tests |
| `ESP32/lib/detection/include/jpeg_gray.h`, `src/jpeg_gray.cpp` | 3 | JPEG -> 80x60 gray via `esp_jpg_decode` |
| `ESP32/lib/detection/include/track_snapshot.h` | 3 | `TrackSnapshot`, `DetectionStats` PODs |
| `ESP32/lib/detection/include/base_detection_module.h` | 3 | Interface: snapshot + stats replace `get_motion_data()` |
| `ESP32/lib/detection/include/camera_diff_detection.h`, `src/camera_diff_detection.cpp` | 3 | Rebuilt glue, PSRAM pipeline, timings, spinlock snapshot |
| `ESP32/lib/detection/include/motion_data.h` | 3 | Deleted |
| `ESP32/lib/network/include/websockets.h`, `src/turret_server.cpp` | 3 | `/commands` JSON from the snapshot |
| `ESP32/lib/controller/...` | 3 | `kAiAimEnabled = false` |
| `ESP32/src/main.cpp` | 3 | `detection_manager.begin()`, stats line fields |
| `web/web_ui.html` | 4 | Target box, id, state on the HUD |
| `tools/tracking/*.py` | 5 | Record, convert, synthetic clips, tool tests |
| `ESP32/bench/track_bench.cpp`, `ESP32/bench/fixtures/*` | 5 | Clip runner/regression/benchmark, fixtures |
| `.github/workflows/ci.yml` | 5 | `tracking-bench` job |
| `CLAUDE.md` | 5 | Document the tracking lib, bench, fixtures |

---

### Task 1: Tracking types and MotionDetector

**Files:**
- Create: `ESP32/lib/tracking/include/tracking_types.h`
- Create: `ESP32/lib/tracking/include/motion_detector.h`
- Create: `ESP32/lib/tracking/src/motion_detector.cpp`
- Test: `ESP32/test/native/test_motion_detector/test_motion_detector.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: `namespace tracking` with `kGridW`, `kGridH`, `kCells`, `kMaxBlobs`, `GrayFrame`, `Box{x0,y0,x1,y1; empty(), width(), height(), overlaps(), merge()}`, `Blob{box, area, cx, cy}`, `DetectorConfig`, `DetectionResult`, `TrackPhase`, `to_string(TrackPhase)`, `TrackerConfig`, `TrackState`; `class MotionDetector { explicit MotionDetector(const DetectorConfig& = {}); const DetectionResult& process(const GrayFrame&); void reset(); const DetectionResult& last() const; }`.

- [ ] **Step 1: Create `tracking_types.h`**

```cpp
/**
 * @file tracking_types.h
 * @brief Value types shared by the motion detector and the tracker (pure C++17, natively tested).
 * @details Everything works on an 80x60 gray grid: the camera's QVGA JPEG decoded at 1/4 scale. Coordinates are in
 * grid cells; multiply by 4 for QVGA pixels.
 */

#pragma once

#include <array>
#include <cstdint>

namespace tracking
{
constexpr int kGridW = 80;
constexpr int kGridH = 60;
constexpr int kCells = kGridW * kGridH;
constexpr int kMaxBlobs = 16;

/** @brief One 80x60 luma frame, row-major (index = y * kGridW + x). */
using GrayFrame = std::array<uint8_t, kCells>;

/** @brief Inclusive cell rectangle. A default-constructed box is empty. */
struct Box {
    int x0 = 0;
    int y0 = 0;
    int x1 = -1;
    int y1 = -1;

    bool empty() const { return x1 < x0 || y1 < y0; }
    int width() const { return empty() ? 0 : x1 - x0 + 1; }
    int height() const { return empty() ? 0 : y1 - y0 + 1; }

    bool overlaps(const Box& o) const
    {
        return !empty() && !o.empty() && x0 <= o.x1 && o.x0 <= x1 && y0 <= o.y1 && o.y0 <= y1;
    }

    /** @brief Grow to also cover @p o. Merging an empty box is a no-op; merging into an empty box copies @p o. */
    void merge(const Box& o)
    {
        if (o.empty())
        {
            return;
        }
        if (empty())
        {
            *this = o;
            return;
        }
        x0 = o.x0 < x0 ? o.x0 : x0;
        y0 = o.y0 < y0 ? o.y0 : y0;
        x1 = o.x1 > x1 ? o.x1 : x1;
        y1 = o.y1 > y1 ? o.y1 : y1;
    }
};

/** @brief One connected region of foreground cells. */
struct Blob {
    Box box;
    int area = 0;    ///< foreground cells
    float cx = 0.0f; ///< centroid, cells
    float cy = 0.0f;
};

/** @brief Motion detector tunables. Defaults are tuned for an indoor scene 1-3 m away. */
struct DetectorConfig {
    int diff_threshold = 20;             ///< |gray - background| above this is foreground
    int bg_shift = 4;                    ///< background cells learn 1/2^bg_shift of the difference per frame (1/16)
    int fg_shift = 7;                    ///< foreground cells learn 1/2^fg_shift (1/128): no trails, slow absorption
    float scene_change_fraction = 0.35f; ///< raw foreground above this fraction of the grid = scene change
    int warmup_frames = 8;               ///< frames with no blobs after the background is (re)seeded
    int min_blob_cells = 9;              ///< smaller blobs are dropped (the 3x3 opening already removes most)
    int max_blobs = kMaxBlobs;           ///< keep at most this many, largest first (clamped to kMaxBlobs)
};

/** @brief What the detector found in one frame. Blobs are sorted by area, largest first. */
struct DetectionResult {
    bool scene_change = false;   ///< too much of the frame changed; background was reset
    bool warming_up = false;     ///< background just (re)seeded; no blobs reported
    bool label_overflow = false; ///< ran out of provisional labels; no blobs reported
    int foreground_cells = 0;    ///< raw foreground count, before the opening
    int dropped_components = 0;  ///< components beyond the internal cap (not reported)
    int blob_count = 0;
    std::array<Blob, kMaxBlobs> blobs{};
};

enum class TrackPhase : uint8_t {
    Idle = 0,
    Tentative = 1,
    Locked = 2,
    Coasting = 3
};

inline const char* to_string(TrackPhase phase)
{
    switch (phase)
    {
    case TrackPhase::Tentative:
        return "tentative";
    case TrackPhase::Locked:
        return "locked";
    case TrackPhase::Coasting:
        return "coasting";
    default:
        return "idle";
    }
}

/** @brief Tracker tunables (frame counts assume ~17 fps). */
struct TrackerConfig {
    int confirm_frames = 3;        ///< consecutive matched frames before a candidate is Locked
    int coast_frames = 17;         ///< misses tolerated before a lost target is dropped (~1 s)
    float alpha = 0.5f;            ///< position gain of the alpha-beta filter
    float beta = 0.2f;             ///< velocity gain
    int gate_margin_min = 4;       ///< cells added around the predicted box
    float gate_margin_frac = 0.5f; ///< or this fraction of the target size, whichever is larger
    float velocity_decay = 0.8f;   ///< velocity multiplier per coasted frame
    int coast_gate_growth = 1;     ///< extra gate cells per coasted frame
};

/** @brief The tracked target. Positions in cells, velocity in cells per frame. id 0 = no target. */
struct TrackState {
    TrackPhase state = TrackPhase::Idle;
    uint32_t id = 0;
    Box box;
    float cx = 0.0f;
    float cy = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    int frames_since_seen = 0;
    int area = 0;
};
} // namespace tracking
```

- [ ] **Step 2: Write the failing detector tests** in `ESP32/test/native/test_motion_detector/test_motion_detector.cpp`

```cpp
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
```

- [ ] **Step 3: Run to confirm it fails**

Run (from `ESP32/`): `~/.platformio/penv/Scripts/pio.exe test -e native -f native/test_motion_detector > /tmp/t1.log 2>&1; tail -20 /tmp/t1.log` (use the scratchpad instead of /tmp if available).
Expected: compile error, `motion_detector.h: No such file or directory`.

- [ ] **Step 4: Create `motion_detector.h`**

```cpp
/**
 * @file motion_detector.h
 * @brief Finds moving regions in 80x60 gray frames with a background model (pure C++17, natively tested).
 * @details Per frame: foreground mask against an exponential-moving-average background, a scene-change guard,
 * a 3x3 binary opening to remove isolated cells, and 8-connected blobs via two-pass union-find labelling.
 * Background cells learn fast (1/16) and foreground cells slowly (1/128), so moving objects leave no trail while a
 * stopped object is still absorbed eventually. All state is fixed-size (~38 KB): no heap. NOT thread-safe.
 */

#pragma once

#include "tracking_types.h"

namespace tracking
{
class MotionDetector
{
public:
    explicit MotionDetector(const DetectorConfig& config = DetectorConfig{});

    /** @brief Analyse one frame. The returned reference stays valid until the next call. */
    const DetectionResult& process(const GrayFrame& frame);

    /** @brief Forget the background; the next frame re-seeds it and starts the warm-up. */
    void reset();

    const DetectionResult& last() const { return result_; }

private:
    static constexpr int kMaxLabels = kCells / 2;
    static constexpr int kMaxComponents = 64;

    struct Accum {
        int area = 0;
        int sum_x = 0;
        int sum_y = 0;
        Box box;
    };

    void seed_background(const GrayFrame& frame);
    void update_background(const GrayFrame& frame, bool selective);
    int build_mask(const GrayFrame& frame);
    void open_mask();
    void label_blobs();
    uint16_t find(uint16_t label);
    void unite(uint16_t a, uint16_t b);

    DetectorConfig config_;
    bool seeded_ = false;
    int warmup_left_ = 0;
    DetectionResult result_;
    std::array<uint16_t, kCells> background_{}; ///< Q8 fixed point (gray << 8)
    std::array<uint8_t, kCells> mask_{};
    std::array<uint8_t, kCells> scratch_{};
    std::array<uint16_t, kCells> labels_{};
    std::array<uint16_t, kMaxLabels + 1> parent_{};
    std::array<uint8_t, kMaxLabels + 1> component_of_{};
    std::array<Accum, kMaxComponents> accum_{};
};
} // namespace tracking
```

- [ ] **Step 5: Create `motion_detector.cpp`**

```cpp
#include "motion_detector.h"

#include <algorithm>
#include <cstdlib>

namespace tracking
{
namespace
{
constexpr uint8_t kNoComponent = 0;
constexpr uint8_t kDroppedComponent = 0xFF;
} // namespace

MotionDetector::MotionDetector(const DetectorConfig& config) : config_(config)
{
    config_.max_blobs = std::clamp(config_.max_blobs, 0, kMaxBlobs);
}

void MotionDetector::reset()
{
    seeded_ = false;
    warmup_left_ = 0;
    result_ = DetectionResult{};
}

const DetectionResult& MotionDetector::process(const GrayFrame& frame)
{
    result_ = DetectionResult{};
    if (!seeded_)
    {
        seed_background(frame);
        result_.warming_up = true;
        return result_;
    }

    const int raw = build_mask(frame);
    result_.foreground_cells = raw;
    if (raw > static_cast<int>(config_.scene_change_fraction * kCells))
    {
        // A light switched or the camera moved: nothing here is a target. Start over from this frame.
        seed_background(frame);
        result_.scene_change = true;
        result_.warming_up = true;
        return result_;
    }

    if (warmup_left_ > 0)
    {
        // Let auto-exposure settle: learn every cell at the fast rate, report nothing.
        --warmup_left_;
        update_background(frame, false);
        result_.warming_up = true;
        return result_;
    }

    // The mask was computed against the old background, so updating now does not affect this frame.
    update_background(frame, true);
    open_mask();
    label_blobs();
    return result_;
}

void MotionDetector::seed_background(const GrayFrame& frame)
{
    for (int i = 0; i < kCells; ++i)
    {
        background_[i] = static_cast<uint16_t>(frame[i] << 8);
    }
    seeded_ = true;
    warmup_left_ = config_.warmup_frames;
}

void MotionDetector::update_background(const GrayFrame& frame, bool selective)
{
    const int bg_div = 1 << config_.bg_shift;
    const int fg_div = 1 << config_.fg_shift;
    for (int i = 0; i < kCells; ++i)
    {
        const int target = frame[i] << 8;
        const int bg = background_[i];
        const int div = (selective && mask_[i]) ? fg_div : bg_div;
        background_[i] = static_cast<uint16_t>(bg + (target - bg) / div); // stays within [0, 255 << 8]
    }
}

int MotionDetector::build_mask(const GrayFrame& frame)
{
    int count = 0;
    for (int i = 0; i < kCells; ++i)
    {
        const int bg = (background_[i] + 128) >> 8;
        const int diff = std::abs(static_cast<int>(frame[i]) - bg);
        const uint8_t fg = diff > config_.diff_threshold ? 1 : 0;
        mask_[i] = fg;
        count += fg;
    }
    return count;
}

void MotionDetector::open_mask()
{
    // Erode into scratch_: a cell survives only if its whole 3x3 neighbourhood is foreground.
    // Cells outside the grid count as background.
    for (int y = 0; y < kGridH; ++y)
    {
        for (int x = 0; x < kGridW; ++x)
        {
            uint8_t keep = mask_[y * kGridW + x];
            for (int dy = -1; dy <= 1 && keep; ++dy)
            {
                for (int dx = -1; dx <= 1 && keep; ++dx)
                {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= kGridW || ny >= kGridH || !mask_[ny * kGridW + nx])
                    {
                        keep = 0;
                    }
                }
            }
            scratch_[y * kGridW + x] = keep;
        }
    }
    // Dilate back into mask_: foreground if any in-grid neighbour survived the erosion.
    for (int y = 0; y < kGridH; ++y)
    {
        for (int x = 0; x < kGridW; ++x)
        {
            uint8_t any = 0;
            for (int dy = -1; dy <= 1 && !any; ++dy)
            {
                for (int dx = -1; dx <= 1 && !any; ++dx)
                {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < kGridW && ny < kGridH && scratch_[ny * kGridW + nx])
                    {
                        any = 1;
                    }
                }
            }
            mask_[y * kGridW + x] = any;
        }
    }
}

uint16_t MotionDetector::find(uint16_t label)
{
    while (parent_[label] != label)
    {
        parent_[label] = parent_[parent_[label]]; // path halving
        label = parent_[label];
    }
    return label;
}

void MotionDetector::unite(uint16_t a, uint16_t b)
{
    a = find(a);
    b = find(b);
    if (a < b)
    {
        parent_[b] = a;
    } else if (b < a)
    {
        parent_[a] = b;
    }
}

void MotionDetector::label_blobs()
{
    // Pass 1: provisional labels from the already-visited 8-neighbours (W, NW, N, NE); record equivalences.
    int next = 1;
    for (int y = 0; y < kGridH; ++y)
    {
        for (int x = 0; x < kGridW; ++x)
        {
            const int i = y * kGridW + x;
            labels_[i] = 0;
            if (!mask_[i])
            {
                continue;
            }
            uint16_t neighbours[4];
            int n = 0;
            if (x > 0 && labels_[i - 1])
            {
                neighbours[n++] = labels_[i - 1];
            }
            if (y > 0)
            {
                const int up = i - kGridW;
                if (x > 0 && labels_[up - 1])
                {
                    neighbours[n++] = labels_[up - 1];
                }
                if (labels_[up])
                {
                    neighbours[n++] = labels_[up];
                }
                if (x < kGridW - 1 && labels_[up + 1])
                {
                    neighbours[n++] = labels_[up + 1];
                }
            }
            if (n == 0)
            {
                if (next > kMaxLabels)
                {
                    result_.label_overflow = true;
                    result_.blob_count = 0;
                    return;
                }
                parent_[next] = static_cast<uint16_t>(next);
                labels_[i] = static_cast<uint16_t>(next);
                ++next;
                continue;
            }
            labels_[i] = neighbours[0];
            for (int k = 1; k < n; ++k)
            {
                unite(neighbours[0], neighbours[k]);
            }
        }
    }

    // Resolve every label to its root, then give each root a compact component index (capped).
    for (int l = 1; l < next; ++l)
    {
        parent_[l] = find(static_cast<uint16_t>(l));
        component_of_[l] = kNoComponent;
    }
    int components = 0;
    for (int l = 1; l < next; ++l)
    {
        const uint16_t root = parent_[l];
        if (component_of_[root] != kNoComponent)
        {
            continue;
        }
        if (components < kMaxComponents)
        {
            accum_[components] = Accum{};
            ++components;
            component_of_[root] = static_cast<uint8_t>(components);
        } else
        {
            component_of_[root] = kDroppedComponent;
            ++result_.dropped_components;
        }
    }

    // Pass 2: accumulate area, centroid sums and boxes per component.
    for (int y = 0; y < kGridH; ++y)
    {
        for (int x = 0; x < kGridW; ++x)
        {
            const uint16_t label = labels_[y * kGridW + x];
            if (!label)
            {
                continue;
            }
            const uint8_t c = component_of_[parent_[label]];
            if (c == kDroppedComponent)
            {
                continue;
            }
            Accum& a = accum_[c - 1];
            ++a.area;
            a.sum_x += x;
            a.sum_y += y;
            a.box.merge(Box{x, y, x, y});
        }
    }

    // Keep the largest max_blobs components (insertion into a sorted, bounded array).
    for (int c = 0; c < components; ++c)
    {
        const Accum& a = accum_[c];
        if (a.area < config_.min_blob_cells)
        {
            continue;
        }
        Blob blob;
        blob.box = a.box;
        blob.area = a.area;
        blob.cx = static_cast<float>(a.sum_x) / static_cast<float>(a.area);
        blob.cy = static_cast<float>(a.sum_y) / static_cast<float>(a.area);

        int pos = result_.blob_count;
        while (pos > 0 && result_.blobs[pos - 1].area < blob.area)
        {
            --pos;
        }
        if (pos >= config_.max_blobs)
        {
            continue;
        }
        const int last = std::min(result_.blob_count, config_.max_blobs - 1);
        for (int k = last; k > pos; --k)
        {
            result_.blobs[k] = result_.blobs[k - 1];
        }
        result_.blobs[pos] = blob;
        if (result_.blob_count < config_.max_blobs)
        {
            ++result_.blob_count;
        }
    }
}
} // namespace tracking
```

- [ ] **Step 6: Run the tests until they pass**

Run (from `ESP32/`): `~/.platformio/penv/Scripts/pio.exe test -e native -f native/test_motion_detector > <scratch>/t1.log 2>&1`, then read the tail of the log.
Expected: `14 Tests 0 Failures 0 Ignored`, `PASSED`. If PlatformIO does not build `lib/tracking/src/*.cpp` for `native`, check that the test includes `motion_detector.h` by name (LDF finds the library from it); do not add `lib_deps` hacks without reporting it.

Then run the whole native suite to check nothing else broke: `pio test -e native > <scratch>/native.log 2>&1` -> all suites PASSED.

Also build the firmware once to prove the library compiles with GCC 8.4: `pio run -e freenove_esp32_wrover > <scratch>/fw.log 2>&1` -> `SUCCESS` (the library is not used by the firmware yet; LDF may skip it, which is fine).

- [ ] **Step 7: Commit**

```bash
git add ESP32/lib/tracking ESP32/test/native/test_motion_detector
git commit -m "Add motion detector: background model, opening, union-find blobs

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Tracker and direction_for

**Files:**
- Create: `ESP32/lib/tracking/include/tracker.h`
- Create: `ESP32/lib/tracking/src/tracker.cpp`
- Create: `ESP32/lib/tracking/include/direction.h`
- Test: `ESP32/test/native/test_tracker/test_tracker.cpp`

**Interfaces:**
- Consumes (Task 1, `tracking_types.h`): `Box`, `Blob`, `DetectionResult`, `TrackPhase`, `TrackerConfig`, `TrackState`, `kGridW`, `kGridH`, `kMaxBlobs`.
- Consumes (`ESP32/include/move_types.h`): `MoveX{None, Left, Right}`, `MoveY{None, Up, Down}` (constructible from `MoveXValue`/`MoveYValue`, comparable with `==`).
- Produces: `class tracking::Tracker { explicit Tracker(const TrackerConfig& = {}); const TrackState& update(const DetectionResult&); const TrackState& state() const; void reset(); }`; `std::tuple<MoveX, MoveY> tracking::direction_for(const TrackState&, float deadzone_cells = 5.0f)`.

- [ ] **Step 1: Write the failing tests** in `ESP32/test/native/test_tracker/test_tracker.cpp`

```cpp
/**
 * @file test_tracker.cpp
 * @brief Host-side tests for the single-target alpha-beta tracker and direction_for().
 */

#include <unity.h>

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

/** Lock onto a static 6x6 target at (10,10)-(15,15): exactly confirm_frames updates. */
static void lock_static(Tracker& t)
{
    for (int i = 0; i < TrackerConfig{}.confirm_frames; ++i)
    {
        t.update(with({rect_blob(10, 10, 15, 15)}));
    }
}

void test_locks_after_confirm_frames(void)
{
    Tracker t;
    TEST_ASSERT_TRUE(TrackPhase::Tentative == t.update(with({rect_blob(10, 10, 15, 15)})).state);
    TEST_ASSERT_TRUE(TrackPhase::Tentative == t.update(with({rect_blob(11, 10, 16, 15)})).state);
    TEST_ASSERT_EQUAL_UINT32(0, t.state().id);
    const TrackState& s = t.update(with({rect_blob(12, 10, 17, 15)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
}

void test_tracks_constant_velocity(void)
{
    Tracker t;
    for (int f = 0; f < 25; ++f)
    {
        t.update(with({rect_blob(10 + f, 20, 15 + f, 25)}));
    }
    const TrackState& s = t.state();
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.5f + 24.0f, s.cx);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1.0f, s.vx);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, s.vy);
}

void test_locks_on_fast_target(void)
{
    Tracker t;
    for (int f = 0; f < 15; ++f)
    {
        t.update(with({rect_blob(2 + 3 * f, 20, 9 + 3 * f, 27)}));
    }
    const TrackState& s = t.state();
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
    TEST_ASSERT_FLOAT_WITHIN(0.3f, 3.0f, s.vx);
}

void test_single_frame_blob_never_locks(void)
{
    Tracker t;
    t.update(with({rect_blob(10, 10, 15, 15)}));
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
    const TrackState& s = t.update(with({rect_blob(10, 10, 15, 15)}));
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
        t.update(with({rect_blob(50, 30, 70, 50), rect_blob(10, 10, 15, 15)}));
    }
    const TrackState& s = t.state();
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, s.cx);
    TEST_ASSERT_EQUAL_INT(36, s.area);
}

void test_merges_split_blobs_inside_gate(void)
{
    Tracker t;
    for (int i = 0; i < 3; ++i)
    {
        t.update(with({rect_blob(20, 20, 29, 29)}));
    }
    // The target splits into a top and a bottom part; a far blob must not be merged.
    const TrackState& s =
        t.update(with({rect_blob(60, 50, 69, 59), rect_blob(20, 20, 29, 23), rect_blob(20, 26, 29, 29)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_INT(80, s.area);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, s.cx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, s.cy);
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
    const TrackState& s = t.update(with({rect_blob(10, 10, 15, 15)}));
    TEST_ASSERT_TRUE(TrackPhase::Locked == s.state);
    TEST_ASSERT_EQUAL_UINT32(1, s.id);
}

void test_scene_change_while_tentative_goes_idle(void)
{
    Tracker t;
    t.update(with({rect_blob(10, 10, 15, 15)}));
    TEST_ASSERT_TRUE(TrackPhase::Idle == t.update(scene_change()).state);
}

void test_coasting_extrapolates_with_decay(void)
{
    Tracker t;
    for (int f = 0; f < 20; ++f)
    {
        t.update(with({rect_blob(10 + f, 20, 15 + f, 25)}));
    }
    const float cx = t.state().cx;
    const float vx = t.state().vx;
    const TrackState& s = t.update(none());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, cx + vx, s.cx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, vx * 0.8f, s.vx);
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
    auto d = direction_for(at(TrackPhase::Locked, 39.5f, 29.5f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Locked, 10.0f, 5.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::Left);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::Up);

    d = direction_for(at(TrackPhase::Coasting, 70.0f, 55.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::Right);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::Down);

    d = direction_for(at(TrackPhase::Locked, 44.0f, 34.0f)); // inside the 5-cell deadzone
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Idle, 10.0f, 5.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
    TEST_ASSERT_TRUE(std::get<1>(d) == MoveY::None);

    d = direction_for(at(TrackPhase::Tentative, 10.0f, 5.0f));
    TEST_ASSERT_TRUE(std::get<0>(d) == MoveX::None);
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
    RUN_TEST(test_reset_goes_idle_and_keeps_id_counter);
    RUN_TEST(test_direction_for);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm it fails**

Run: `pio test -e native -f native/test_tracker > <scratch>/t2.log 2>&1` -> compile error, `tracker.h` / `direction.h` not found.

- [ ] **Step 3: Create `tracker.h`**

```cpp
/**
 * @file tracker.h
 * @brief Single-target persistent tracker over detector blobs (pure C++17, natively tested).
 * @details States: Idle -> Tentative (candidate, must match confirm_frames in a row) -> Locked (id assigned) ->
 * Coasting (missed; predicted forward with decaying velocity) -> Locked again with the same id, or Idle after
 * coast_frames misses. Prediction is an alpha-beta filter (constant velocity, fixed gains). The measurement is the
 * union of every blob overlapping the gate around the prediction, so a target split into parts stays one box.
 * While Locked, blobs outside the gate are ignored even if larger. NOT thread-safe.
 */

#pragma once

#include "tracking_types.h"

namespace tracking
{
class Tracker
{
public:
    explicit Tracker(const TrackerConfig& config = TrackerConfig{});

    /** @brief Advance one frame. The returned reference stays valid until the next call. */
    const TrackState& update(const DetectionResult& detection);

    const TrackState& state() const { return state_; }

    /** @brief Drop the current target (Idle). Ids already handed out are never reused. */
    void reset();

private:
    Box gate() const;
    bool measure(const DetectionResult& detection, const Box& gate, Blob& out) const;
    void start_tentative(const Blob& blob);
    void correct(const Blob& measurement);
    void miss();
    void to_idle();
    void refresh_box();

    TrackerConfig config_;
    TrackState state_;
    int hits_ = 0;
    uint32_t last_id_ = 0;
    float width_ = 0.0f; ///< smoothed box size, cells
    float height_ = 0.0f;
};
} // namespace tracking
```

- [ ] **Step 4: Create `tracker.cpp`**

```cpp
#include "tracker.h"

#include <algorithm>
#include <cmath>

namespace tracking
{
Tracker::Tracker(const TrackerConfig& config) : config_(config) {}

void Tracker::reset() { to_idle(); }

const TrackState& Tracker::update(const DetectionResult& detection)
{
    if (detection.scene_change)
    {
        if (state_.state == TrackPhase::Tentative)
        {
            to_idle();
        } else if (state_.state != TrackPhase::Idle)
        {
            miss(); // Locked or Coasting: keep predicting, the target may still be there after the warm-up
        }
        return state_;
    }

    Blob measurement;
    switch (state_.state)
    {
    case TrackPhase::Idle:
        if (detection.blob_count > 0)
        {
            start_tentative(detection.blobs[0]); // blobs are sorted: [0] is the largest
        }
        break;
    case TrackPhase::Tentative:
        if (measure(detection, gate(), measurement))
        {
            correct(measurement);
            if (++hits_ >= config_.confirm_frames)
            {
                state_.state = TrackPhase::Locked;
                state_.id = ++last_id_;
            }
        } else
        {
            to_idle();
        }
        break;
    case TrackPhase::Locked:
    case TrackPhase::Coasting:
        if (measure(detection, gate(), measurement))
        {
            correct(measurement);
            state_.state = TrackPhase::Locked;
        } else
        {
            miss();
        }
        break;
    }
    return state_;
}

Box Tracker::gate() const
{
    const float px = state_.cx + state_.vx;
    const float py = state_.cy + state_.vy;
    const float grow = static_cast<float>(config_.coast_gate_growth * state_.frames_since_seen);
    const float mx = std::max(static_cast<float>(config_.gate_margin_min), config_.gate_margin_frac * width_) + grow;
    const float my = std::max(static_cast<float>(config_.gate_margin_min), config_.gate_margin_frac * height_) + grow;
    Box g;
    g.x0 = static_cast<int>(std::floor(px - (width_ - 1.0f) / 2.0f - mx));
    g.x1 = static_cast<int>(std::ceil(px + (width_ - 1.0f) / 2.0f + mx));
    g.y0 = static_cast<int>(std::floor(py - (height_ - 1.0f) / 2.0f - my));
    g.y1 = static_cast<int>(std::ceil(py + (height_ - 1.0f) / 2.0f + my));
    return g;
}

bool Tracker::measure(const DetectionResult& detection, const Box& gate, Blob& out) const
{
    Blob m;
    float wx = 0.0f;
    float wy = 0.0f;
    for (int k = 0; k < detection.blob_count; ++k)
    {
        const Blob& b = detection.blobs[k];
        if (!b.box.overlaps(gate))
        {
            continue;
        }
        m.box.merge(b.box);
        m.area += b.area;
        wx += b.cx * static_cast<float>(b.area);
        wy += b.cy * static_cast<float>(b.area);
    }
    if (m.area == 0)
    {
        return false;
    }
    m.cx = wx / static_cast<float>(m.area);
    m.cy = wy / static_cast<float>(m.area);
    out = m;
    return true;
}

void Tracker::start_tentative(const Blob& blob)
{
    state_ = TrackState{};
    state_.state = TrackPhase::Tentative;
    state_.cx = blob.cx;
    state_.cy = blob.cy;
    state_.area = blob.area;
    width_ = static_cast<float>(blob.box.width());
    height_ = static_cast<float>(blob.box.height());
    hits_ = 1;
    refresh_box();
}

void Tracker::correct(const Blob& measurement)
{
    const float px = state_.cx + state_.vx;
    const float py = state_.cy + state_.vy;
    const float rx = measurement.cx - px;
    const float ry = measurement.cy - py;
    state_.cx = px + config_.alpha * rx;
    state_.cy = py + config_.alpha * ry;
    state_.vx += config_.beta * rx;
    state_.vy += config_.beta * ry;
    width_ += config_.alpha * (static_cast<float>(measurement.box.width()) - width_);
    height_ += config_.alpha * (static_cast<float>(measurement.box.height()) - height_);
    state_.area = measurement.area;
    state_.frames_since_seen = 0;
    refresh_box();
}

void Tracker::miss()
{
    ++state_.frames_since_seen;
    if (state_.frames_since_seen > config_.coast_frames)
    {
        to_idle();
        return;
    }
    state_.state = TrackPhase::Coasting;
    state_.cx += state_.vx;
    state_.cy += state_.vy;
    state_.vx *= config_.velocity_decay;
    state_.vy *= config_.velocity_decay;
    refresh_box();
}

void Tracker::to_idle()
{
    state_ = TrackState{};
    hits_ = 0;
    width_ = 0.0f;
    height_ = 0.0f;
}

void Tracker::refresh_box()
{
    // Box of the smoothed size centred on the filtered position. Not clamped: callers clamp to the frame.
    const int w = std::max(1, static_cast<int>(std::lround(width_)));
    const int h = std::max(1, static_cast<int>(std::lround(height_)));
    state_.box.x0 = static_cast<int>(std::lround(state_.cx - (static_cast<float>(w) - 1.0f) / 2.0f));
    state_.box.y0 = static_cast<int>(std::lround(state_.cy - (static_cast<float>(h) - 1.0f) / 2.0f));
    state_.box.x1 = state_.box.x0 + w - 1;
    state_.box.y1 = state_.box.y0 + h - 1;
}
} // namespace tracking
```

- [ ] **Step 5: Create `direction.h`**

```cpp
/**
 * @file direction.h
 * @brief Maps a tracked target to a pan/tilt direction relative to the grid centre (pure C++17, header-only).
 * @details Only Locked and Coasting targets produce a direction. Stage 3 uses this to aim; Stage 2 only reports it.
 */

#pragma once

#include "move_types.h"
#include "tracking_types.h"

#include <tuple>

namespace tracking
{
inline std::tuple<MoveX, MoveY> direction_for(const TrackState& track, float deadzone_cells = 5.0f)
{
    if (track.state != TrackPhase::Locked && track.state != TrackPhase::Coasting)
    {
        return std::make_tuple(MoveX(MoveX::None), MoveY(MoveY::None));
    }
    const float dx = track.cx - (kGridW - 1) / 2.0f;
    const float dy = track.cy - (kGridH - 1) / 2.0f;
    MoveX x(MoveX::None);
    MoveY y(MoveY::None);
    if (dx < -deadzone_cells)
    {
        x = MoveX(MoveX::Left);
    } else if (dx > deadzone_cells)
    {
        x = MoveX(MoveX::Right);
    }
    if (dy < -deadzone_cells)
    {
        y = MoveY(MoveY::Up);
    } else if (dy > deadzone_cells)
    {
        y = MoveY(MoveY::Down);
    }
    return std::make_tuple(x, y);
}
} // namespace tracking
```

- [ ] **Step 6: Run the tests until they pass**

Run: `pio test -e native -f native/test_tracker > <scratch>/t2.log 2>&1` -> `13 Tests 0 Failures`, PASSED. Then `pio test -e native > <scratch>/native.log 2>&1` -> all suites PASSED.

If `test_locks_on_fast_target` fails, do not loosen the test: report the gate geometry you observed (the plan expects the 3-cells/frame target to stay inside a gate of half-width >= 4 + 4 = 8 cells around the 8-wide box).

- [ ] **Step 7: Commit**

```bash
git add ESP32/lib/tracking ESP32/test/native/test_tracker
git commit -m "Add alpha-beta single-target tracker and direction_for

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Device integration (decoder, detection glue, /commands, controller, stats)

**Files:**
- Create: `ESP32/lib/detection/include/jpeg_gray.h`, `ESP32/lib/detection/src/jpeg_gray.cpp`
- Create: `ESP32/lib/detection/include/track_snapshot.h`
- Modify: `ESP32/lib/detection/include/base_detection_module.h`
- Rewrite: `ESP32/lib/detection/include/camera_diff_detection.h`, `ESP32/lib/detection/src/camera_diff_detection.cpp`
- Delete: `ESP32/lib/detection/include/motion_data.h`
- Modify: `ESP32/lib/network/include/websockets.h` (CommandsWebSocketHandler::handler), `ESP32/lib/network/src/turret_server.cpp` (drop the `motion_data.h` include)
- Modify: `ESP32/lib/controller/include/controller.h`, `ESP32/lib/controller/src/controller.cpp`
- Modify: `ESP32/src/main.cpp`

**Interfaces:**
- Consumes (Tasks 1-2): `tracking::GrayFrame`, `tracking::MotionDetector`, `tracking::Tracker`, `tracking::TrackState`, `tracking::TrackPhase`, `tracking::to_string`, `tracking::direction_for`, `tracking::kGridW/kGridH`.
- Consumes (esp32-camera, `esp_jpg_decode.h`): `esp_err_t esp_jpg_decode(size_t len, jpg_scale_t scale, jpg_reader_cb reader, jpg_writer_cb writer, void* arg)`; `typedef size_t (*jpg_reader_cb)(void* arg, size_t index, uint8_t* buf, size_t len)`; `typedef bool (*jpg_writer_cb)(void* arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* data)`. Facts verified in `.pio/libdeps/freenove_esp32_wrover/esp32-camera/conversions/esp_jpg_decode.c`: it uses a **static** 3100-byte work buffer (no allocation; not re-entrant - only `loop()` may call it); the writer is first called with `data == NULL, x == 0, y == 0, w/h = output size`, then once per decoded block with RGB888 data (R, G, B byte order), then with `data == NULL, x == w, y == h` at the end; returning `false` from a block write aborts the decode; the reader may be called with `buf == NULL` to skip bytes.
- Produces: `bool jpeg_gray::decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out)`; `struct TrackSnapshot`, `struct DetectionStats`; `BaseDetectionModule::get_track_snapshot()`, `BaseDetectionModule::get_stats()`; `bool CameraDiffDetection::begin()`.

- [ ] **Step 1: Create `jpeg_gray.h` / `jpeg_gray.cpp`**

```cpp
/**
 * @file jpeg_gray.h
 * @brief Decodes a QVGA camera JPEG at 1/4 scale straight into the 80x60 gray grid the tracker uses.
 * @details Uses esp32-camera's esp_jpg_decode (TJpgDec), whose descaling outputs each 8x8 JPEG block as 2x2
 * averaged pixels - cheap, and the averaging filters noise. No allocation; NOT re-entrant (the decoder's work
 * buffer is static), so only the loop() task may call it.
 */

#pragma once

#include "tracking_types.h"

#include <cstddef>
#include <cstdint>

namespace jpeg_gray
{
/** @brief false if the JPEG is corrupt or its 1/4-scale size is not 80x60 (frame size must be QVGA). */
bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out);
} // namespace jpeg_gray
```

```cpp
#include "jpeg_gray.h"

#include <cstring>
#include <esp_jpg_decode.h>

namespace jpeg_gray
{
namespace
{
struct DecodeContext {
    const uint8_t* src;
    size_t len;
    tracking::GrayFrame* out;
    bool size_ok;
};

size_t read_jpeg(void* arg, size_t index, uint8_t* buf, size_t len)
{
    auto* ctx = static_cast<DecodeContext*>(arg);
    if (index >= ctx->len)
    {
        return 0;
    }
    if (len > ctx->len - index)
    {
        len = ctx->len - index;
    }
    if (buf)
    {
        memcpy(buf, ctx->src + index, len);
    }
    return len; // buf == NULL means "skip len bytes"
}

bool write_gray(void* arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* data)
{
    auto* ctx = static_cast<DecodeContext*>(arg);
    if (!data)
    {
        if (x == 0 && y == 0) // start of image: w/h are the scaled output size
        {
            ctx->size_ok = (w == tracking::kGridW && h == tracking::kGridH);
        }
        return ctx->size_ok;
    }
    if (!ctx->size_ok)
    {
        return false; // aborts the decode
    }
    for (uint16_t row = 0; row < h; ++row)
    {
        const int gy = y + row;
        for (uint16_t col = 0; col < w; ++col)
        {
            const int gx = x + col;
            if (gx >= tracking::kGridW || gy >= tracking::kGridH)
            {
                continue;
            }
            const uint8_t* p = data + 3 * (row * w + col);
            // Rec.601 luma in integer math: (77 R + 150 G + 29 B) / 256
            (*ctx->out)[gy * tracking::kGridW + gx] = static_cast<uint8_t>((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
        }
    }
    return true;
}
} // namespace

bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out)
{
    if (!jpeg || len == 0)
    {
        return false;
    }
    DecodeContext ctx{jpeg, len, &out, false};
    return esp_jpg_decode(len, JPG_SCALE_4X, read_jpeg, write_gray, &ctx) == ESP_OK && ctx.size_ok;
}
} // namespace jpeg_gray
```

- [ ] **Step 2: Create `track_snapshot.h`**

```cpp
/**
 * @file track_snapshot.h
 * @brief Plain copies of the tracker output and detection timings, safe to hand across tasks.
 * @details The detection module writes these under a lock in loop(); the /commands handler (httpd task) and the
 * stats line read copies. Coordinates are in frame pixels (QVGA), already scaled and clamped.
 */

#pragma once

#include "tracking_types.h"

#include <cstdint>

struct TrackSnapshot {
    tracking::TrackPhase state = tracking::TrackPhase::Idle;
    uint32_t id = 0;
    int x = 0; ///< centroid, pixels
    int y = 0;
    int bx = 0; ///< box, pixels, clamped to the frame
    int by = 0;
    int bw = 0;
    int bh = 0;
    float vx = 0.0f; ///< pixels per frame
    float vy = 0.0f;
    int area_px = 0;
    int frame_width = 320;
    int frame_height = 240;

    bool detected() const
    {
        return state == tracking::TrackPhase::Locked || state == tracking::TrackPhase::Coasting;
    }
};

struct DetectionStats {
    uint32_t frames = 0;          ///< frames analysed
    uint32_t decode_failures = 0; ///< corrupt JPEG or not QVGA
    uint32_t scene_changes = 0;
    uint32_t decode_us_avg = 0; ///< average since boot
    uint32_t track_us_avg = 0;  ///< detector + tracker, average since boot
};
```

- [ ] **Step 3: Update `base_detection_module.h`**

Replace the `#include "motion_data.h"` with `#include "track_snapshot.h"`, and replace the `get_motion_data()` virtual with:

```cpp
    /**
     * @brief Latest tracker output, safe to call from another task (implementations copy under a lock).
     * @return Default (Idle) snapshot when the module does not track.
     */
    virtual TrackSnapshot get_track_snapshot() const { return TrackSnapshot{}; }

    /** @brief Detection timings and counters for the stats line. */
    virtual DetectionStats get_stats() const { return DetectionStats{}; }
```

Delete `ESP32/lib/detection/include/motion_data.h`. Then `grep -rn "motion_data.h\|MotionData\|DetectionMetrics\|get_motion_data\|get_detection_metrics" ESP32/src ESP32/lib ESP32/test` and fix every hit in `src/` and `lib/`. On-device suites under `ESP32/test/` that reference these (they are already stale per CLAUDE.md) may be updated to the new API if trivial; otherwise leave them and list them in your report.

- [ ] **Step 4: Rewrite `camera_diff_detection.h`**

```cpp
#pragma once

#include "base_detection_module.h"
#include "motion_detector.h"
#include "tracker.h"

#include <freertos/FreeRTOS.h>

/**
 * @class CameraDiffDetection
 * @brief Motion detection and persistent single-target tracking on camera frames.
 * @details Each frame: JPEG -> 80x60 gray (jpeg_gray), MotionDetector, Tracker. The pipeline state (~50 KB) is
 * allocated once in PSRAM by begin(); nothing is allocated per frame. detect_object() runs in loop(); the snapshot
 * and stats getters may be called from other tasks.
 */
class CameraDiffDetection : public BaseDetectionModule
{
public:
    CameraDiffDetection() = default;
    ~CameraDiffDetection() override;
    CameraDiffDetection(const CameraDiffDetection&) = delete;
    CameraDiffDetection& operator=(const CameraDiffDetection&) = delete;

    /** @brief Allocate the pipeline in PSRAM. Without it detect_object() reports no motion. */
    bool begin();

    /** @brief Analyse one frame; returns the direction toward the tracked target (None when there is none). */
    std::tuple<MoveX, MoveY> detect_object(camera_buffer_t frame) override;

    TrackSnapshot get_track_snapshot() const override;
    DetectionStats get_stats() const override;

private:
    struct Pipeline {
        tracking::GrayFrame gray;
        tracking::MotionDetector detector;
        tracking::Tracker tracker;
    };

    Pipeline* _pipeline = nullptr;
    mutable portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
    TrackSnapshot _snapshot;
    DetectionStats _stats;
    uint64_t _decode_us_total = 0;
    uint64_t _track_us_total = 0;
};
```

- [ ] **Step 5: Rewrite `camera_diff_detection.cpp`** (replaces the whole file, including all commented-out code)

```cpp
#include "camera_diff_detection.h"

#include "direction.h"
#include "jpeg_gray.h"

#include <algorithm>
#include <esp32-hal-log.h> // without it this file's ESP_LOGx output is dropped
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <new>

static const char* TAG = "DETECTION";

namespace
{
TrackSnapshot make_snapshot(const tracking::TrackState& track, size_t frame_width, size_t frame_height)
{
    TrackSnapshot s;
    s.frame_width = static_cast<int>(frame_width);
    s.frame_height = static_cast<int>(frame_height);
    s.state = track.state;
    s.id = track.id;
    if (track.state == tracking::TrackPhase::Idle)
    {
        return s;
    }
    const int sx = s.frame_width / tracking::kGridW; // 4 for QVGA
    const int sy = s.frame_height / tracking::kGridH;
    s.x = std::clamp(static_cast<int>(track.cx * sx + sx / 2), 0, s.frame_width - 1);
    s.y = std::clamp(static_cast<int>(track.cy * sy + sy / 2), 0, s.frame_height - 1);
    const int x0 = std::clamp(track.box.x0 * sx, 0, s.frame_width);
    const int y0 = std::clamp(track.box.y0 * sy, 0, s.frame_height);
    const int x1 = std::clamp((track.box.x1 + 1) * sx, 0, s.frame_width);
    const int y1 = std::clamp((track.box.y1 + 1) * sy, 0, s.frame_height);
    s.bx = x0;
    s.by = y0;
    s.bw = x1 - x0;
    s.bh = y1 - y0;
    s.vx = track.vx * static_cast<float>(sx);
    s.vy = track.vy * static_cast<float>(sy);
    s.area_px = track.area * sx * sy;
    return s;
}
} // namespace

CameraDiffDetection::~CameraDiffDetection()
{
    if (this->_pipeline)
    {
        this->_pipeline->~Pipeline();
        heap_caps_free(this->_pipeline);
    }
}

bool CameraDiffDetection::begin()
{
    if (this->_pipeline)
    {
        return true;
    }
    void* mem = heap_caps_malloc(sizeof(Pipeline), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem)
    {
        ESP_LOGE(TAG, "Failed to allocate the %u-byte tracking pipeline in PSRAM", (unsigned)sizeof(Pipeline));
        return false;
    }
    this->_pipeline = new (mem) Pipeline{};
    ESP_LOGI(TAG, "Tracking pipeline ready (%u bytes in PSRAM)", (unsigned)sizeof(Pipeline));
    return true;
}

std::tuple<MoveX, MoveY> CameraDiffDetection::detect_object(camera_buffer_t frame)
{
    const auto no_move = std::make_tuple(MoveX(MoveX::None), MoveY(MoveY::None));
    if (!this->_pipeline || !frame.buffer || frame.length == 0)
    {
        return no_move;
    }

    const int64_t t0 = esp_timer_get_time();
    const bool decoded =
        jpeg_gray::decode(static_cast<const uint8_t*>(frame.buffer), frame.length, this->_pipeline->gray);
    const int64_t t1 = esp_timer_get_time();
    if (!decoded)
    {
        portENTER_CRITICAL(&this->_lock);
        ++this->_stats.decode_failures;
        portEXIT_CRITICAL(&this->_lock);
        ESP_LOGD(TAG, "JPEG decode failed (%u bytes)", (unsigned)frame.length);
        return no_move;
    }

    const tracking::DetectionResult& detection = this->_pipeline->detector.process(this->_pipeline->gray);
    const tracking::TrackState& track = this->_pipeline->tracker.update(detection);
    const int64_t t2 = esp_timer_get_time();

    const TrackSnapshot snapshot = make_snapshot(track, frame.width, frame.height);
    portENTER_CRITICAL(&this->_lock);
    this->_snapshot = snapshot;
    ++this->_stats.frames;
    if (detection.scene_change)
    {
        ++this->_stats.scene_changes;
    }
    this->_decode_us_total += static_cast<uint64_t>(t1 - t0);
    this->_track_us_total += static_cast<uint64_t>(t2 - t1);
    this->_stats.decode_us_avg = static_cast<uint32_t>(this->_decode_us_total / this->_stats.frames);
    this->_stats.track_us_avg = static_cast<uint32_t>(this->_track_us_total / this->_stats.frames);
    portEXIT_CRITICAL(&this->_lock);

    return tracking::direction_for(track);
}

TrackSnapshot CameraDiffDetection::get_track_snapshot() const
{
    portENTER_CRITICAL(&this->_lock);
    const TrackSnapshot copy = this->_snapshot;
    portEXIT_CRITICAL(&this->_lock);
    return copy;
}

DetectionStats CameraDiffDetection::get_stats() const
{
    portENTER_CRITICAL(&this->_lock);
    const DetectionStats copy = this->_stats;
    portEXIT_CRITICAL(&this->_lock);
    return copy;
}
```

Note: `tracking::Tracker`/`MotionDetector` are value types with no destructor work; the explicit `~Pipeline()` call is for correctness if that ever changes.

- [ ] **Step 6: `/commands` JSON from the snapshot** (`websockets.h`, `CommandsWebSocketHandler::handler`)

Replace the `if (_detection_instance != nullptr) { ... }` branch body with:

```cpp
            const TrackSnapshot t = _detection_instance->get_track_snapshot();
            // Existing keys first (older HUDs keep working), then the tracker fields.
            int len = snprintf((char*)json_buf, 256,
                               "{\"detected\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"pixels\":%d,"
                               "\"state\":\"%s\",\"id\":%u,\"bx\":%d,\"by\":%d,\"bw\":%d,\"bh\":%d,"
                               "\"vx\":%.1f,\"vy\":%.1f}",
                               t.detected() ? 1 : 0, t.x, t.y, t.frame_width, t.frame_height, t.area_px,
                               tracking::to_string(t.state), (unsigned)t.id, t.bx, t.by, t.bw, t.bh, (double)t.vx,
                               (double)t.vy);
            if (len < 0 || len >= 256)
            {
                len = snprintf((char*)json_buf, 256, "{\"detected\":0}");
            }
            out_buf = json_buf;
            out_len = len;
```

Remove the per-message `ESP_LOGD` metrics line (it logs at 25 Hz). Keep the existing fallback branch and the 256-byte malloc as they are (out of scope per spec). In `turret_server.cpp`, drop `#include "motion_data.h"` (include `track_snapshot.h` only if something there needs it).

- [ ] **Step 7: Controller: detection runs, the turret does not move in AI mode**

In `controller.h`, next to the existing `kUserModePeriodMs` constant, add:

```cpp
    /// Stage 2 detects and tracks only; Stage 3 turns AI aiming on (and adds move/settle handling).
    static constexpr bool kAiAimEnabled = false;
```

In `controller.cpp`, replace the final `this->_movement_manager.move_relative(move_directions); // Move stepper & motor` with:

```cpp
    if (kAiAimEnabled)
    {
        this->_movement_manager.move_relative(move_directions); // Move stepper & motor
    }
```

- [ ] **Step 8: `main.cpp`: allocate the pipeline, extend the stats line**

After the camera/frame-pipeline block in `setup()`, add:

```cpp
    if (!detection_manager.begin())
    {
        ESP_LOGE(TAG_MAIN, "Tracking pipeline allocation failed - continuing without detection");
    }
```

In `log_stream_stats()`, read `const DetectionStats det = detection_manager.get_stats();` and `const TrackSnapshot track = detection_manager.get_track_snapshot();` and append to the existing format string (keep every existing field and its order):

```
 decode_us_avg=%u track_us_avg=%u scene_changes=%u decode_fail=%u track=%s id=%u
```

with arguments `(unsigned)det.decode_us_avg, (unsigned)det.track_us_avg, (unsigned)det.scene_changes, (unsigned)det.decode_failures, tracking::to_string(track.state), (unsigned)track.id`.

- [ ] **Step 9: Build and check**

From `ESP32/`:
- `pio run -e freenove_esp32_wrover > <scratch>/fw.log 2>&1` -> `SUCCESS`, no new warnings in our files.
- `pio run -e pin_check > <scratch>/pin.log 2>&1` -> `SUCCESS`.
- `pio check --severity=high --severity=medium > <scratch>/check.log 2>&1` -> no new findings in `lib/tracking`, `lib/detection`, `lib/network`, `lib/controller`, `src`.
- `pio test -e native > <scratch>/native.log 2>&1` -> all PASSED.
Report the RAM/Flash usage lines from the firmware build.

- [ ] **Step 10: Commit**

```bash
git add -A ESP32/lib ESP32/src ESP32/test
git commit -m "Run detection and tracking on device; publish track snapshot to /commands

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: HUD target overlay

**Files:**
- Modify: `web/web_ui.html` (`ws_commands.onmessage`, `draw(data)`)

**Interfaces:**
- Consumes (Task 3): `/commands` JSON `{"detected","x","y","width","height","pixels","state":"idle|tentative|locked|coasting","id","bx","by","bw","bh","vx","vy"}` in frame pixels (frame size = `width` x `height`).
- Produces: nothing for later tasks.

- [ ] **Step 1: Store the tracker fields.** In `ws_commands.onmessage`, set `App.state.latestData` to:

```js
App.state.latestData = {
    lock: metrics.detected === 1,
    state: metrics.state || (metrics.detected ? 'locked' : 'idle'),
    id: metrics.id || 0,
    x: metrics.x,
    y: metrics.y,
    bx: metrics.bx || 0,
    by: metrics.by || 0,
    bw: metrics.bw || 0,
    bh: metrics.bh || 0,
    width: metrics.width || 320,
    height: metrics.height || 240,
    pixels: metrics.pixels
};
```

and replace the lock-indicator text logic so it shows `LOCKED #<id>`, `COASTING #<id>`, `ACQUIRING...` (tentative) or `SCANNING...` (idle). Keep the existing fire-button safety behaviour keyed on `detected`.

- [ ] **Step 2: Draw the box.** Replace `draw(data)` with:

```js
draw(data) {
    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    if (!data.lock) return;
    const sx = this.canvas.width / data.width;
    const sy = this.canvas.height / data.height;
    const coasting = data.state === 'coasting';
    this.ctx.strokeStyle = coasting ? '#ffaa00' : '#ff0000';
    this.ctx.lineWidth = 2;
    this.ctx.setLineDash(coasting ? [6, 4] : []);
    if (data.bw > 0 && data.bh > 0) {
        this.ctx.strokeRect(data.bx * sx, data.by * sy, data.bw * sx, data.bh * sy);
        this.ctx.font = '12px monospace';
        this.ctx.fillStyle = this.ctx.strokeStyle;
        this.ctx.fillText(`#${data.id}`, data.bx * sx + 2, Math.max(12, data.by * sy - 4));
    }
    const x = data.x * sx;
    const y = data.y * sy;
    this.ctx.setLineDash([]);
    this.ctx.beginPath();
    this.ctx.moveTo(x - 8, y); this.ctx.lineTo(x + 8, y);
    this.ctx.moveTo(x, y - 8); this.ctx.lineTo(x, y + 8);
    this.ctx.stroke();
},
```

- [ ] **Step 3: Verify**

- `python -m unittest discover -s web -p "test_*.py"` -> OK.
- From `ESP32/`: `pio run -e freenove_esp32_wrover > <scratch>/fw.log 2>&1` -> SUCCESS (regenerates `index_html.h`).
- Open `web/web_ui.html` in a browser from disk and confirm there are no console syntax errors on load (the sockets will fail to connect; that is expected).

- [ ] **Step 4: Commit**

```bash
git add web/web_ui.html
git commit -m "HUD: draw tracked target box, id and state

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Fixture tools, clip bench, CI job

**Files:**
- Create: `tools/tracking/make_synthetic_clips.py`, `tools/tracking/jpeg_to_gray.py`, `tools/tracking/record_stream.py`, `tools/tracking/test_tracking_tools.py`
- Create: `ESP32/bench/track_bench.cpp`
- Create: `ESP32/bench/fixtures/square_cross.gray`, `square_cross.expect`, `light_switch.gray`, `light_switch.expect` (the `.gray` files generated by the script)
- Modify: `.github/workflows/ci.yml` (new `tracking-bench` job)
- Modify: `CLAUDE.md`

**Interfaces:**
- Consumes (Tasks 1-2): `tracking::MotionDetector`, `tracking::Tracker`, `tracking::GrayFrame`, `tracking::kCells`, `tracking::TrackPhase`.
- Produces: clip format `<name>.gray` = concatenated 4800-byte frames (80x60, row-major); `<name>.expect` = `key=value` lines (`#` comments) with optional keys `lock_by_frame`, `max_ids`, `min_locked_frames`, `min_scene_changes`, `max_scene_changes`; `track_bench [--repeat N] <clip.gray>...` exits non-zero when any expectation fails.

- [ ] **Step 1: `tools/tracking/make_synthetic_clips.py`** (stdlib only, deterministic)

```python
"""Generate deterministic synthetic 80x60 gray clips for the tracking bench (no camera needed).

square_cross: background 60 with +-3 noise; an 8x8 square of value 190 enters at frame 10 and moves right
1 cell/frame until frame 69, then disappears (frames 70-89).
light_switch: background 60 for 30 frames, then the whole scene brightens to 140 (frames 30-59).
"""
import random
import sys
from pathlib import Path

W, H = 80, 60


def noisy_background(rng, level):
    return bytearray(max(0, min(255, level + rng.randint(-3, 3))) for _ in range(W * H))


def square_cross(rng):
    frames = []
    for f in range(90):
        frame = noisy_background(rng, 60)
        if 10 <= f < 70:
            x0, y0 = 2 + (f - 10), 26
            for y in range(y0, y0 + 8):
                for x in range(x0, x0 + 8):
                    frame[y * W + x] = 190
        frames.append(bytes(frame))
    return frames


def light_switch(rng):
    return [bytes(noisy_background(rng, 60 if f < 30 else 140)) for f in range(60)]


CLIPS = {"square_cross": square_cross, "light_switch": light_switch}


def main(out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for name, make in CLIPS.items():
        frames = make(random.Random(1))
        (out / f"{name}.gray").write_bytes(b"".join(frames))
        print(f"{name}: {len(frames)} frames")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "ESP32/bench/fixtures")
```

- [ ] **Step 2: `tools/tracking/jpeg_to_gray.py`** (Pillow)

```python
"""Convert a folder of QVGA JPEG frames (sorted by name) into one 80x60 gray clip (<name>.gray).

Pillow's draft() uses libjpeg DCT scaling, close to the ESP32's TJpgDec 1/4-scale decode.
Usage: python tools/tracking/jpeg_to_gray.py <jpeg_dir> <out.gray>
"""
import sys
from pathlib import Path

from PIL import Image

W, H = 80, 60


def jpeg_to_gray_bytes(path):
    with Image.open(path) as img:
        img.draft("L", (W, H))
        gray = img.convert("L")
        if gray.size != (W, H):
            gray = gray.resize((W, H), Image.BOX)
        return gray.tobytes()


def convert(jpeg_dir, out_path):
    files = sorted(Path(jpeg_dir).glob("*.jpg"))
    if not files:
        raise SystemExit(f"no .jpg files in {jpeg_dir}")
    data = b"".join(jpeg_to_gray_bytes(f) for f in files)
    Path(out_path).write_bytes(data)
    return len(files)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    print(f"{convert(sys.argv[1], sys.argv[2])} frames -> {sys.argv[2]}")
```

- [ ] **Step 3: `tools/tracking/record_stream.py`** (websockets)

```python
"""Record N seconds of JPEG frames from the turret's /stream WebSocket into a folder.

Usage: python tools/tracking/record_stream.py <board_ip> <seconds> <out_dir>
Read the board IP from the boot log (DHCP may change it after a reflash). Needs: pip install websockets
"""
import asyncio
import sys
import time
from pathlib import Path

import websockets


async def record(ip, seconds, out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    count = 0
    async with websockets.connect(f"ws://{ip}/stream", max_size=None) as ws:
        await ws.send(b" ")  # same 1-byte keepalive the HUD sends; also marks us as a viewer
        end = time.monotonic() + seconds
        last_keepalive = time.monotonic()
        while time.monotonic() < end:
            msg = await asyncio.wait_for(ws.recv(), timeout=5)
            if isinstance(msg, bytes) and msg[:2] == b"\xff\xd8":
                (out / f"{count:05d}.jpg").write_bytes(msg)
                count += 1
            if time.monotonic() - last_keepalive > 2:
                await ws.send(b" ")
                last_keepalive = time.monotonic()
    return count


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    n = asyncio.run(record(sys.argv[1], float(sys.argv[2]), sys.argv[3]))
    print(f"saved {n} frames to {sys.argv[3]}")
```

- [ ] **Step 4: `tools/tracking/test_tracking_tools.py`**

```python
"""Tests for the tracking fixture tools (run: python -m unittest discover -s tools/tracking -p "test_*.py")."""
import random
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import make_synthetic_clips  # noqa: E402


class SyntheticClipTests(unittest.TestCase):
    def test_frame_size_and_count(self):
        frames = make_synthetic_clips.square_cross(random.Random(1))
        self.assertEqual(90, len(frames))
        self.assertTrue(all(len(f) == 4800 for f in frames))

    def test_deterministic(self):
        a = make_synthetic_clips.square_cross(random.Random(1))
        b = make_synthetic_clips.square_cross(random.Random(1))
        self.assertEqual(a, b)

    def test_square_position(self):
        frame = make_synthetic_clips.square_cross(random.Random(1))[10]
        self.assertEqual(190, frame[26 * 80 + 2])
        self.assertEqual(190, frame[33 * 80 + 9])
        self.assertNotEqual(190, frame[26 * 80 + 10])


class JpegToGrayTests(unittest.TestCase):
    def test_qvga_jpeg_becomes_80x60(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow not installed")
        import jpeg_to_gray

        img = Image.new("RGB", (320, 240), (0, 0, 0))
        for y in range(80, 160):
            for x in range(160, 240):
                img.putpixel((x, y), (255, 255, 255))
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "00000.jpg"
            img.save(path, quality=90)
            data = jpeg_to_gray.jpeg_to_gray_bytes(path)
        self.assertEqual(4800, len(data))
        self.assertGreater(data[30 * 80 + 50], 200)  # inside the white square (cells 40-59 x 20-39)
        self.assertLess(data[5 * 80 + 5], 30)  # black corner


if __name__ == "__main__":
    unittest.main()
```


- [ ] **Step 5: `ESP32/bench/track_bench.cpp`**

```cpp
/**
 * @file track_bench.cpp
 * @brief Runs recorded 80x60 gray clips through MotionDetector + Tracker: regression checks and a host benchmark.
 * @details Usage: track_bench [--repeat N] <clip.gray>... For each clip, reads <clip>.expect (if present) and
 * checks it; exits 1 if any expectation fails or a clip is malformed. Built with plain g++ (see ci.yml).
 */

#include "motion_detector.h"
#include "tracker.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace tracking;

namespace
{
struct ClipResult {
    int frames = 0;
    int first_lock = -1;
    int ids = 0;
    int locked = 0;
    int coasting = 0;
    int scene_changes = 0;
    double ns_per_frame = 0.0;
};

std::map<std::string, long> read_expect(const std::string& clip_path)
{
    std::map<std::string, long> out;
    std::string path = clip_path.substr(0, clip_path.rfind('.')) + ".expect";
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        const auto eq = line.find('=');
        if (eq != std::string::npos)
        {
            out[line.substr(0, eq)] = std::strtol(line.c_str() + eq + 1, nullptr, 10);
        }
    }
    return out;
}

ClipResult run_clip(const std::vector<GrayFrame>& frames, int repeat)
{
    ClipResult r;
    const auto start = std::chrono::steady_clock::now();
    for (int rep = 0; rep < repeat; ++rep)
    {
        MotionDetector detector;
        Tracker tracker;
        std::set<uint32_t> ids;
        ClipResult pass;
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const DetectionResult& det = detector.process(frames[i]);
            const TrackState& t = tracker.update(det);
            pass.scene_changes += det.scene_change ? 1 : 0;
            if (t.state == TrackPhase::Locked)
            {
                ++pass.locked;
                if (pass.first_lock < 0)
                {
                    pass.first_lock = static_cast<int>(i);
                }
            }
            pass.coasting += t.state == TrackPhase::Coasting ? 1 : 0;
            if (t.id)
            {
                ids.insert(t.id);
            }
        }
        pass.ids = static_cast<int>(ids.size());
        r = pass; // every repetition is identical; keep the last
    }
    const auto ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    r.frames = static_cast<int>(frames.size());
    r.ns_per_frame = frames.empty() ? 0.0 : ns / (static_cast<double>(frames.size()) * repeat);
    return r;
}

bool check(const char* key, bool ok, long expected, long actual)
{
    if (!ok)
    {
        std::printf("  FAIL %s: expected %ld, got %ld\n", key, expected, actual);
    }
    return ok;
}
} // namespace

int main(int argc, char** argv)
{
    int repeat = 1;
    std::vector<std::string> clips;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--repeat" && i + 1 < argc)
        {
            repeat = std::max(1, std::atoi(argv[++i]));
        } else
        {
            clips.push_back(arg);
        }
    }
    if (clips.empty())
    {
        std::fprintf(stderr, "usage: track_bench [--repeat N] <clip.gray>...\n");
        return 2;
    }

    bool all_ok = true;
    for (const std::string& path : clips)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            std::printf("clip=%s ERROR unreadable\n", path.c_str());
            all_ok = false;
            continue;
        }
        const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (bytes.empty() || bytes.size() % kCells != 0)
        {
            std::printf("clip=%s ERROR size %zu is not a multiple of %d\n", path.c_str(), bytes.size(), kCells);
            all_ok = false;
            continue;
        }
        std::vector<GrayFrame> frames(bytes.size() / kCells);
        for (size_t f = 0; f < frames.size(); ++f)
        {
            for (int i = 0; i < kCells; ++i)
            {
                frames[f][i] = static_cast<uint8_t>(bytes[f * kCells + i]);
            }
        }

        const ClipResult r = run_clip(frames, repeat);
        std::printf("clip=%s frames=%d first_lock=%d ids=%d locked=%d coasting=%d scene_changes=%d "
                    "ns_per_frame=%.0f\n",
                    path.c_str(), r.frames, r.first_lock, r.ids, r.locked, r.coasting, r.scene_changes,
                    r.ns_per_frame);

        bool ok = true;
        for (const auto& [key, value] : read_expect(path))
        {
            if (key == "lock_by_frame")
            {
                ok &= check("lock_by_frame", r.first_lock >= 0 && r.first_lock <= value, value, r.first_lock);
            } else if (key == "max_ids")
            {
                ok &= check("max_ids", r.ids <= value, value, r.ids);
            } else if (key == "min_locked_frames")
            {
                ok &= check("min_locked_frames", r.locked >= value, value, r.locked);
            } else if (key == "min_scene_changes")
            {
                ok &= check("min_scene_changes", r.scene_changes >= value, value, r.scene_changes);
            } else if (key == "max_scene_changes")
            {
                ok &= check("max_scene_changes", r.scene_changes <= value, value, r.scene_changes);
            } else
            {
                std::printf("  FAIL unknown expectation key '%s'\n", key.c_str());
                ok = false;
            }
        }
        std::printf("  %s\n", ok ? "PASS" : "FAIL");
        all_ok &= ok;
    }
    return all_ok ? 0 : 1;
}
```

- [ ] **Step 6: Generate fixtures and write expectations**

Run from the repo root: `python tools/tracking/make_synthetic_clips.py ESP32/bench/fixtures`.

`ESP32/bench/fixtures/square_cross.expect`:

```
# Square enters at frame 10 (detector warm-up ends at frame 9); Locked on its 3rd frame.
lock_by_frame=14
max_ids=1
min_locked_frames=50
max_scene_changes=0
```

`ESP32/bench/fixtures/light_switch.expect`:

```
# The whole scene brightens at frame 30: one scene change, never a lock.
max_ids=0
min_scene_changes=1
max_scene_changes=1
```

- [ ] **Step 7: Build and run the bench locally** (Windows host g++ 14; no sanitizers on Windows)

From the repo root:

```bash
g++ -std=c++17 -O2 -g -Wall -Wextra -I ESP32/include -I ESP32/lib/tracking/include \
    ESP32/lib/tracking/src/*.cpp ESP32/bench/track_bench.cpp -o <scratch>/track_bench.exe
<scratch>/track_bench.exe ESP32/bench/fixtures/square_cross.gray ESP32/bench/fixtures/light_switch.gray
```

Expected: both clips `PASS`, exit code 0, square_cross `first_lock=12 ids=1`. If an expectation fails, find out why (do not loosen it without reporting the numbers). Also run `python -m unittest discover -s tools/tracking -p "test_*.py"` -> OK (the JPEG test may skip without Pillow; install it with `pip install pillow` to run it).

- [ ] **Step 8: CI job.** Add to `.github/workflows/ci.yml` (match the existing jobs' `actions/checkout` and `actions/setup-python` versions and indentation):

```yaml
    tracking-bench:
        runs-on: ubuntu-latest
        steps:
            - uses: actions/checkout@v4
            - uses: actions/setup-python@v5
              with:
                  python-version: "3.12"
            - name: Tool tests
              run: |
                  pip install pillow
                  python -m unittest discover -s tools/tracking -p "test_*.py"
            - name: Fixtures are up to date
              run: |
                  python tools/tracking/make_synthetic_clips.py /tmp/fixtures
                  cmp /tmp/fixtures/square_cross.gray ESP32/bench/fixtures/square_cross.gray
                  cmp /tmp/fixtures/light_switch.gray ESP32/bench/fixtures/light_switch.gray
            - name: Build bench (release and ASan+UBSan)
              run: |
                  SRC="ESP32/lib/tracking/src/*.cpp ESP32/bench/track_bench.cpp"
                  INC="-I ESP32/include -I ESP32/lib/tracking/include"
                  g++ -std=c++17 -O2 -g -fno-omit-frame-pointer -Wall -Wextra $INC $SRC -o track_bench
                  g++ -std=c++17 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined \
                      -fno-sanitize-recover=all $INC $SRC -o track_bench_asan
            - name: Clip regression under ASan+UBSan
              run: ./track_bench_asan ESP32/bench/fixtures/*.gray
            - name: Clip regression and timing (release)
              run: ./track_bench --repeat 20 ESP32/bench/fixtures/*.gray
            - name: Instruction counts per frame (cachegrind, deterministic)
              run: |
                  sudo apt-get update -qq && sudo apt-get install -y -qq valgrind
                  for clip in ESP32/bench/fixtures/*.gray; do
                      frames=$(( $(stat -c %s "$clip") / 4800 ))
                      valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file=/dev/null \
                          ./track_bench --repeat 10 "$clip" 2> cg.txt > /dev/null
                      ir=$(grep -oP 'I\s+refs:\s+\K[0-9,]+' cg.txt | tr -d ,)
                      echo "$(basename "$clip"): $(( ir / (frames * 10) )) instructions/frame (incl. clip load)"
                  done
            - name: perf stat (best effort; hardware counters may be unavailable on hosted runners)
              continue-on-error: true
              run: |
                  sudo apt-get install -y -qq linux-tools-common linux-tools-$(uname -r) || exit 1
                  sudo sysctl -w kernel.perf_event_paranoid=-1
                  perf stat -e instructions,task-clock ./track_bench --repeat 50 ESP32/bench/fixtures/*.gray
```

If the existing `format` job runs clang-format over a file glob that now includes `ESP32/bench/`, make sure `track_bench.cpp` is formatted (the PostToolUse hook formats files under the firmware tree on edit; run `clang-format -i` manually otherwise).

- [ ] **Step 9: Document in `CLAUDE.md`**

- Commands block: add `python tools/tracking/make_synthetic_clips.py ESP32/bench/fixtures` and the g++ bench build/run line from Step 7.
- Architecture: replace the Detection bullet with a description of the pipeline (`jpeg_gray` 1/4-scale decode -> `tracking::MotionDetector` -> `tracking::Tracker` in `lib/tracking`, pure and natively tested; `CameraDiffDetection` glue with PSRAM pipeline and a spinlock snapshot read by `/commands`; AI mode does not move the turret until Stage 3, `kAiAimEnabled`).
- CI line: add the `tracking-bench` job (ASan clip regression, cachegrind instructions/frame, perf best effort).
- Benchmark stats line: list the new fields.
- Fixtures: `.gray` format and `.expect` keys; real clips are recorded with `record_stream.py` + `jpeg_to_gray.py`.

- [ ] **Step 10: Commit**

```bash
git add tools/tracking ESP32/bench .github/workflows/ci.yml CLAUDE.md
git commit -m "Add tracking clip bench, fixture tools and CI tracking-bench job

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6 (controller + user, hardware): real clips and on-device verification

Not dispatched to a subagent: needs the board and the user in front of the camera.

- [ ] Flash `freenove_esp32_wrover`; read the IP and the stats line from the boot log (port COM3 or COM4).
- [ ] Record 3 clips of ~6 s with `record_stream.py` while the user performs: hand wave then stop for 1 s then wave again; a person crossing the room; light switched off/on. Convert each with `jpeg_to_gray.py` into `ESP32/bench/fixtures/real_<name>.gray`, write `.expect` files from the observed bench output (with margin), and commit.
- [ ] Measure the spec's success criteria from the stats line and the HUD: `decode_us_avg + track_us_avg <= 15000`, capture >= 16 fps, HUD fps unchanged, lock within ~5 frames, same id across a 1 s pause, light switch counted as a scene change without a false lock, 10-minute soak with flat `heap_min`/`psram_min`.
- [ ] Push, open the PR with the benchmark table (method + numbers, cachegrind instructions/frame from CI), confirm all CI checks pass.
