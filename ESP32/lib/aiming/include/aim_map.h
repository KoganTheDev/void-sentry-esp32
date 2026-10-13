/**
 * @file aim_map.h
 * @brief Maps a target's image position to turret angles (pure C++17, natively tested).
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
constexpr float kMinCalDx = 80.0f; ///< closer calibration points amplify aiming error
constexpr float kMinCalDy = 60.0f;

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

/** @brief pan = pan_a * x + pan_b, tilt = tilt_a * y + tilt_b. */
struct AimMap {
    float pan_a = 0.0f;
    float pan_b = 0.0f;
    float tilt_a = 0.0f;
    float tilt_b = kTiltHomeDeg;

    /** @brief Angles for an image position, clamped to the soft limits. */
    Angles aim(float x, float y) const;
};

/** @brief Camera next to the turret, 66 x 50 degree field of view, image centre = home. */
AimMap default_map();

/** @brief Per-axis line through two calibration points; empty if they are too close or not finite. */
std::optional<AimMap> fit(const CalPoint& p1, const CalPoint& p2);

Angles clamp_angles(Angles a);
int32_t pan_deg_to_steps(float deg);
float pan_steps_to_deg(int32_t steps);
} // namespace aiming
