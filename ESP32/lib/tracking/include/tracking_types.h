/**
 * @file tracking_types.h
 * @brief Value types shared by the motion detector and the tracker (pure C++17, natively tested).
 * @details Everything works on a 40x30 gray grid: the camera's QVGA JPEG decoded at 1/8 scale (one DC value per 8x8
 * block). Coordinates are in grid cells; multiply by 8 for QVGA pixels.
 */

#pragma once

#include <array>
#include <cstdint>

namespace tracking
{
constexpr int kGridW = 40;
constexpr int kGridH = 30;
constexpr int kCells = kGridW * kGridH;
constexpr int kMaxBlobs = 16;

/** @brief One 40x30 luma frame, row-major (index = y * kGridW + x). */
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
    int fg_age_limit = 34;               ///< after this many consecutive foreground frames (~2 s) a cell learns at
                                         ///< bg_shift again, so ghosts of a wrong background clear quickly
    float scene_change_fraction = 0.35f; ///< raw foreground above this fraction of the grid = scene change
    int warmup_frames = 8;               ///< frames with no blobs after the background is (re)seeded
    int min_blob_cells = 2;              ///< smaller blobs are dropped (rejects isolated noise cells)
    int max_blobs = kMaxBlobs;           ///< keep at most this many, largest first (clamped to kMaxBlobs)
};

/** @brief What the detector found in one frame. Blobs are sorted by area, largest first. */
struct DetectionResult {
    bool scene_change = false;   ///< too much of the frame changed; background was reset
    bool warming_up = false;     ///< background just (re)seeded; no blobs reported
    bool label_overflow = false; ///< ran out of provisional labels; no blobs reported
    int foreground_cells = 0;    ///< raw foreground cell count (before blob filtering)
    int dropped_components = 0;  ///< components beyond the internal cap (not reported)
    int blob_count = 0;
    std::array<Blob, kMaxBlobs> blobs{};
};

enum class TrackPhase : uint8_t { Idle = 0, Tentative = 1, Locked = 2, Coasting = 3 };

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
    int gate_margin_min = 2;       ///< cells added around the predicted box
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
