/**
 * @file aim_map.h
 * @brief Maps a target's image position to turret angles (pure C++20, natively tested).
 * @details The camera is fixed (not on the turret), so aiming is open-loop: pan depends linearly on image x and tilt
 * on image y. The map comes from a 2-point joystick calibration, or from default_map() (camera next to the turret,
 * OV3660 field of view) until one exists. Angles: pan degrees from home (positive = right), tilt servo degrees
 * (larger = up).
 */

#pragma once

#include <cstdint>
#include <optional>

namespace aiming
{
constexpr int32_t kStepsPerRev = 2048; ///< 28BYJ-48 full steps per revolution
constexpr float kPanMinDeg = -90.0f;
constexpr float kPanMaxDeg = 90.0f;
constexpr float kTiltMinDeg = 20.0f;
constexpr float kTiltMaxDeg = 160.0f;
constexpr float kTiltHomeDeg = 90.0f;
constexpr int kFrameW = 320; ///< QVGA
constexpr int kFrameH = 240;
constexpr float kLimitMarginDeg = 0.5f; ///< limit_bits() reports an axis this close to a soft limit
constexpr float kMinCalDx = 80.0f;      ///< closer calibration points amplify aiming error
constexpr float kMinCalDy = 60.0f;

/** @brief Turret pose: pan degrees from home (positive = right), tilt servo degrees (larger = up). */
struct Angles {
    float pan = 0.0f;
    float tilt = kTiltHomeDeg;
};

/** @brief One calibration sample: the target's pixel and the turret angles that pointed at it. */
struct CalPoint {
    float x = 0.0f;
    float y = 0.0f;
    float pan = 0.0f;
    float tilt = kTiltHomeDeg;
};

/** @brief An image position in pixels; not clamped, so it can lie outside the frame. */
struct Pixel {
    float x = 0.0f;
    float y = 0.0f;
};

/** @brief pan = pan_a * x + pan_b, tilt = tilt_a * y + tilt_b. */
struct AimMap {
    float pan_a = 0.0f;
    float pan_b = 0.0f;
    float tilt_a = 0.0f;
    float tilt_b = kTiltHomeDeg;

    /** @brief Angles for an image position, clamped to the soft limits. */
    Angles aim(float x, float y) const;

    /**
     * @brief Image position that aim() maps to these angles: the exact inverse before clamping,
     * x = (pan - pan_b) / pan_a and y = (tilt - tilt_b) / tilt_a. Not clamped to the frame. An axis whose slope is 0 or
     * not finite (or whose result is not finite) returns the frame centre on that axis.
     */
    Pixel to_pixel(Angles a) const;
};

/** @brief Rectangle of frame pixels (inclusive corners) with x0 <= x1 and y0 <= y1. */
struct ReachBox {
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = kFrameW - 1;
    int16_t y1 = kFrameH - 1;
};

/** @brief Camera next to the turret, 66 x 50 degree field of view, image centre = home. */
AimMap default_map();

/** @brief Per-axis line through two calibration points; empty if they are too close or not finite. */
std::optional<AimMap> fit(const CalPoint& p1, const CalPoint& p2);

/**
 * @brief The image region whose aim lies inside the soft limits: the pixels where kPanMinDeg <= pan <= kPanMaxDeg and
 * kTiltMinDeg <= tilt <= kTiltMaxDeg (the inverse of the limits through the map), ordered, clamped to the frame and
 * rounded. An axis with a zero or non-finite slope spans the whole frame. A region wholly outside the frame
 * collapses onto the nearest edge column or row.
 */
ReachBox reach_box(const AimMap& m);

/**
 * @brief Soft-limit proximity bits for the HUD: 1 pan within kLimitMarginDeg of kPanMinDeg, 2 pan at the max, 4 tilt at
 * the min, 8 tilt at the max (inclusive; beyond a limit counts too, a non-finite angle sets no bit).
 */
uint8_t limit_bits(Angles a);

/**
 * @brief Clamps angles to the soft limits; non-finite pan becomes 0 and non-finite tilt becomes kTiltHomeDeg first.
 */
Angles clamp_angles(Angles a);

/**
 * @brief Converts pan degrees to 28BYJ-48 full steps (rounded). Non-finite input gives 0; the degrees are clamped
 * to [kPanMinDeg, kPanMaxDeg] before rounding, so the result is always in [-512, 512].
 */
int32_t pan_deg_to_steps(float deg);
/** @brief Converts a pan step count (28BYJ-48 full steps from home) to degrees; the inverse of pan_deg_to_steps. */
float pan_steps_to_deg(int32_t steps);
} // namespace aiming
