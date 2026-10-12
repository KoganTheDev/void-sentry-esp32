/**
 * @file motion_detector.h
 * @brief Finds moving regions in 40x30 gray frames with a background model (pure C++17, natively tested).
 * @details Per frame: foreground mask against an exponential-moving-average background, a scene-change guard,
 * and 8-connected blobs via two-pass union-find labelling. There is no morphological opening: each cell is already
 * an 8x8 block average (1/8 DC decode), so an opening would erase hand-sized targets; isolated noise cells are
 * rejected by DetectorConfig::min_blob_cells instead.
 * Background cells learn fast (1/16) and foreground cells slowly (1/128), so moving objects leave no trail while a
 * stopped object is still absorbed eventually. A cell that stays foreground for more than fg_age_limit frames goes
 * back to the fast rate, so ghosts (cells that differ because the background itself was wrong) clear in a few
 * seconds instead of ~14 s. All state is fixed-size (~12 KB): no heap. NOT thread-safe.
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
    void label_blobs();
    uint16_t find(uint16_t label);
    void unite(uint16_t a, uint16_t b);

    DetectorConfig config_;
    bool seeded_ = false;
    int warmup_left_ = 0;
    DetectionResult result_;
    std::array<uint16_t, kCells> background_{}; ///< Q8 fixed point (gray << 8)
    std::array<uint8_t, kCells> mask_{};
    std::array<uint8_t, kCells> fg_age_{}; ///< consecutive raw-foreground frames per cell (saturates at 255)
    std::array<uint16_t, kCells> labels_{};
    std::array<uint16_t, kMaxLabels + 1> parent_{};
    std::array<uint8_t, kMaxLabels + 1> component_of_{};
    std::array<Accum, kMaxComponents> accum_{};
};
} // namespace tracking
