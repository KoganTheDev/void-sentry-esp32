/**
 * @file remote_inbox.h
 * @brief Holds the latest remote command state with a dead-man jog timeout (pure C++20, natively tested).
 * @details Not thread-safe by itself; the owner (Controller) guards it with a spinlock. Time is millis()-style
 * uint32 and wraparound-safe.
 */

#pragma once

#include "remote_command.h"

#include <optional>

namespace remote
{
constexpr uint32_t kJogTimeoutMs = 300; ///< a jog expires this long after the last Move
constexpr float kJogSpeed = 0.6f;       ///< jog magnitude per axis while active at JogSpeed::Normal
constexpr float kJogSpeedSlow = 0.25f;  ///< jog magnitude at JogSpeed::Slow
constexpr float kJogSpeedFast = 1.0f;   ///< jog magnitude at JogSpeed::Fast

/** @brief Jog request in the same units as Joystick/Turret::jog. */
struct Jog {
    float pan = 0.0f;
    float tilt = 0.0f;
};

/** @brief Image pixel requested by an Aim command. */
struct AimPixel {
    int16_t x = 0;
    int16_t y = 0;
};

/** @brief Stores the latest Move (with timeout), the jog speed and the pending mode, calibration and aim requests. */
class RemoteInbox
{
public:
    /**
     * @brief Applies a command. Move stores direction and time, Stop ends the jog, Mode, Calibrate and Aim store a
     * one-shot request (latest wins; Aim also ends an active jog), Speed sets the persistent jog speed.
     */
    void submit(const Command& cmd, uint32_t now_ms);

    /** @brief True if a Move arrived and less than kJogTimeoutMs have passed since. */
    bool jog_active(uint32_t now_ms) const;

    /** @brief The magnitude of the current speed setting times the direction while active, else {0, 0}. */
    Jog jog(uint32_t now_ms) const;

    /** @brief The latest Mode command, returned once. */
    std::optional<RemoteMode> take_mode_request();

    /** @brief The latest Calibrate action, returned once. */
    std::optional<CalAction> take_cal_request();

    /** @brief The latest Aim pixel, returned once. */
    std::optional<AimPixel> take_aim_request();

    /** @brief The persistent jog speed setting (default Normal). */
    JogSpeed speed() const { return speed_; }

private:
    bool has_move_ = false;
    int8_t pan_ = 0;
    int8_t tilt_ = 0;
    uint32_t move_ms_ = 0;
    std::optional<RemoteMode> mode_request_;
    std::optional<CalAction> cal_request_;
    std::optional<AimPixel> aim_request_;
    JogSpeed speed_ = JogSpeed::Normal;
};
} // namespace remote
