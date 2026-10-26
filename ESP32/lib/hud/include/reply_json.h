/**
 * @file reply_json.h
 * @brief Formats the /commands JSON reply from the tracker snapshot and the controller status (pure C++20, natively
 * tested).
 * @details No Arduino or ESP-IDF headers and no heap use: the output goes into a caller buffer (the httpd task's
 * stack). The existing keys keep their order and format so older HUDs keep working; the keys added by the HUD
 * overhaul follow them (see docs/superpowers/specs/2026-10-26-hud-ux-overhaul-design.md, section "Reply JSON").
 */

#pragma once

#include "calibration_session.h"
#include "control_status.h"
#include "track_snapshot.h"

#include <cstddef>
#include <cstdint>

namespace hud
{
/**
 * @brief Buffer size that always holds a full reply.
 * @details test_reply_json formats the worst case (every counter at its maximum, every int16 negative, every float
 * at FLT_MAX width, the longest calibration error text; 739 bytes) and asserts it is shorter than this with at least 64
 * bytes to spare. A reply that still does not fit falls back to {"detected":0}.
 */
constexpr size_t kReplySize = 832;

/** @brief Inputs of one reply. Null pointers mean that part is not available. */
struct ReplyInput {
    const TrackSnapshot* track = nullptr;  ///< Tracker output; null sends the fixed no-detection fallback.
    const ControlStatus* status = nullptr; ///< Controller state; null omits the controller keys and "vw".
    uint32_t viewers = 0;                  ///< /stream viewers connected (0 to 2), sent as "vw" with the status keys.
};

/**
 * @brief Writes the NUL-terminated JSON reply into out and returns its length (without the NUL).
 * @details Without a track the fixed fallback {"detected":0,"x":0,"y":0,"width":320,"height":240,"pixels":0} is
 * written. With a track the tracker keys follow, then, when a status is present, the controller keys (mode, cal,
 * cal_state, calibrated, pan, tilt, cal_error, cal_saves, cal_miss, stick, ax, ay, lim, rx0, ry0, rx1, ry1, home,
 * ret, spd) and finally vw. If everything does not fit in size bytes the reply is {"detected":0}; below 15 bytes not
 * even that fits and an empty string (length 0) is written.
 */
size_t format_reply(const ReplyInput& in, char* out, size_t size);

/** @brief Wire name of a calibration state: idle, point1, point2, done or failed. */
const char* cal_state_name(aiming::CalState st);

/** @brief Wire name of ControlStatus::speed: slow (0), normal (1) or fast (2); anything else reads as normal. */
const char* speed_name(uint8_t speed);
} // namespace hud
