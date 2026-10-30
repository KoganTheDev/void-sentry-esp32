/**
 * @file status_fields.h
 * @brief Computes the pose-derived HUD fields of ControlStatus (pure C++20, natively tested).
 * @details The controller fills the mode, calibration and counter fields itself; this helper derives the fields that
 * only depend on the current pose, the active aim map and the AI return-home state: the aim pixel (ax, ay), the
 * soft-limit bits (lim), the reachable box (rx0..ry1), the home countdown (home) and the returning flag (ret). See
 * docs/superpowers/specs/2026-10-26-hud-ux-overhaul-design.md, section "Reply JSON".
 */

#pragma once

#include "aim_map.h"
#include "control_status.h"

#include <cstdint>

namespace hud
{
/** @brief A pose further than this from home on either axis counts as "travelling home" after a Home command. */
constexpr float kReturningMarginDeg = 1.0f;

/** @brief AI return-home state read from the aiming::AimPolicy. Ignored unless ai_mode is set. */
struct HomeState {
    bool ai_mode = false;           ///< The controller is in ControlMode::Ai.
    bool homed = false;             ///< aiming::AimPolicy::homed(): Home has fired since the last lock or reset.
    uint32_t home_remaining_ms = 0; ///< aiming::AimPolicy::home_remaining_ms() at this pass.
};

/**
 * @brief Rounds a pixel coordinate to int16, saturating at the int16 range (a nearly flat map can put the inverse of a
 * pose far outside the frame); a non-finite value gives 0.
 */
int16_t round_pixel(float v);

/**
 * @brief Fills aim_x, aim_y, lim, rx0, ry0, rx1, ry1, home_ms and returning of @p st from st.angles (which must
 * already hold the current pose), the active map and the AI home state.
 * @details aim_x/aim_y = round_pixel(map.to_pixel(pose)) (not clamped to the frame); lim = aiming::limit_bits(pose);
 * the box = aiming::reach_box(map); home_ms = home.home_remaining_ms in AI mode, else 0; returning = AI mode, Home has
 * fired and the pose is more than kReturningMarginDeg from (0, aiming::kTiltHomeDeg) on either axis. The other fields
 * are left untouched.
 */
void fill_pose_fields(ControlStatus& st, const aiming::AimMap& map, const HomeState& home);
} // namespace hud
