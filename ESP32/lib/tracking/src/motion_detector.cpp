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
