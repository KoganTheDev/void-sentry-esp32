/**
 * @file wifi_plan.h
 * @brief WiFi connection state machine: events in, actions out (pure C++17, no heap).
 */

#pragma once

#include "prov_types.h"

namespace prov
{
enum class PlanState { Boot, Scanning, Joining, Connected, Lost, Setup };
enum class ActionKind { None, StartScan, Join, StartHotspot, StopHotspot };
enum class JoinFail { None, Timeout, WrongPassword, NotFound, Other };

struct Action {
    ActionKind kind = ActionKind::None;
    uint8_t network = 0; // Join: index into NetworkList
};

/**
 * @brief Decides what the WiFi glue should do next. Events that do not apply to the current state return
 * Action{None} and change nothing. All time arithmetic is wrap-safe.
 */
class WifiPlan
{
public:
    static constexpr uint32_t kJoinTimeoutMs = 15000;
    static constexpr uint32_t kLostToSetupMs = 120000;
    static constexpr uint32_t kSetupRescanMs = 30000;

    /** @brief Boot only: saved networks -> Scanning + StartScan; none -> Setup + StartHotspot. */
    Action start(bool have_saved, uint32_t now_ms);
    /** @brief Scan finished (Scanning, Setup or Lost). Candidates -> Joining; none -> Setup (Scanning only). */
    Action on_scan_done(const Candidates& c, uint32_t now_ms);
    /** @brief Joining, Lost or Setup -> Connected; StopHotspot if the hotspot is up. */
    Action on_joined(uint32_t now_ms);
    /** @brief Joining only: next remembered candidate, else Setup (+ StartHotspot if not up). */
    Action on_join_failed(JoinFail why, uint32_t now_ms);
    /** @brief Connected -> Lost (auto-reconnect retries; nothing issued). */
    Action on_disconnected(uint32_t now_ms);
    /**
     * @brief Setup or Lost -> StartScan (and the last failure is cleared; a scan that then finds no candidate records
     * NotFound); other states only store the network (None).
     */
    Action on_network_added(uint32_t now_ms);
    /** @brief Timers: join timeout, Lost -> Setup, Setup rescan. */
    Action tick(uint32_t now_ms);

    PlanState state() const;
    bool hotspot_up() const;
    JoinFail last_failure() const;

private:
    Action fail_over(uint32_t now_ms);
    Action join_next(uint32_t now_ms);

    PlanState _state = PlanState::Boot;
    bool _hotspot = false;
    JoinFail _last_fail = JoinFail::None;
    bool _added_scan = false; // the pending scan was triggered by an added network
    Candidates _cand;
    size_t _next = 0;
    uint32_t _join_ms = 0;
    uint32_t _lost_ms = 0;
    uint32_t _scan_ms = 0;
};
} // namespace prov
