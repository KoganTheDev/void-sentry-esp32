#include "wifi_plan.h"

namespace prov
{
namespace
{
Action make(ActionKind k, uint8_t network = 0)
{
    Action a;
    a.kind = k;
    a.network = network;
    return a;
}
} // namespace

Action WifiPlan::start(bool have_saved, uint32_t now_ms)
{
    if (_state != PlanState::Boot)
    {
        return Action{};
    }
    if (have_saved)
    {
        _state = PlanState::Scanning;
        return make(ActionKind::StartScan);
    }
    _state = PlanState::Setup;
    _hotspot = true;
    _scan_ms = now_ms;
    return make(ActionKind::StartHotspot);
}

Action WifiPlan::join_next(uint32_t now_ms)
{
    _state = PlanState::Joining;
    _join_ms = now_ms;
    return make(ActionKind::Join, _cand.idx[_next++]);
}

Action WifiPlan::on_scan_done(const Candidates& c, uint32_t now_ms)
{
    if (_state != PlanState::Scanning && _state != PlanState::Setup && _state != PlanState::Lost)
    {
        return Action{};
    }
    const bool added = _added_scan;
    _added_scan = false;
    if (c.count > 0)
    {
        _cand = c;
        _next = 0;
        return join_next(now_ms);
    }
    if (added && _state != PlanState::Scanning)
    {
        _last_fail = JoinFail::NotFound; // the added network is not in range (or the scan failed)
    }
    if (_state == PlanState::Scanning)
    {
        _state = PlanState::Setup;
        _scan_ms = now_ms;
        if (!_hotspot)
        {
            _hotspot = true;
            return make(ActionKind::StartHotspot);
        }
    }
    return Action{};
}

Action WifiPlan::on_joined(uint32_t)
{
    if (_state != PlanState::Joining && _state != PlanState::Lost && _state != PlanState::Setup)
    {
        return Action{};
    }
    _state = PlanState::Connected;
    _added_scan = false;
    if (_hotspot)
    {
        _hotspot = false;
        return make(ActionKind::StopHotspot);
    }
    return Action{};
}

Action WifiPlan::fail_over(uint32_t now_ms)
{
    if (_next < _cand.count)
    {
        return join_next(now_ms);
    }
    _state = PlanState::Setup;
    _scan_ms = now_ms;
    if (!_hotspot)
    {
        _hotspot = true;
        return make(ActionKind::StartHotspot);
    }
    return Action{};
}

Action WifiPlan::on_join_failed(JoinFail why, uint32_t now_ms)
{
    if (_state != PlanState::Joining)
    {
        return Action{};
    }
    _last_fail = why;
    return fail_over(now_ms);
}

Action WifiPlan::on_disconnected(uint32_t now_ms)
{
    if (_state != PlanState::Connected)
    {
        return Action{};
    }
    _state = PlanState::Lost;
    _lost_ms = now_ms;
    return Action{};
}

Action WifiPlan::on_network_added(uint32_t now_ms)
{
    if (_state != PlanState::Setup && _state != PlanState::Lost)
    {
        return Action{};
    }
    if (_state == PlanState::Setup)
    {
        _scan_ms = now_ms;
    }
    _last_fail = JoinFail::None;
    _added_scan = true;
    return make(ActionKind::StartScan);
}

Action WifiPlan::tick(uint32_t now_ms)
{
    switch (_state)
    {
    case PlanState::Joining:
        if (static_cast<uint32_t>(now_ms - _join_ms) >= kJoinTimeoutMs)
        {
            _last_fail = JoinFail::Timeout;
            return fail_over(now_ms);
        }
        return Action{};
    case PlanState::Lost:
        if (static_cast<uint32_t>(now_ms - _lost_ms) >= kLostToSetupMs)
        {
            _state = PlanState::Setup;
            _scan_ms = now_ms;
            if (!_hotspot)
            {
                _hotspot = true;
                return make(ActionKind::StartHotspot);
            }
        }
        return Action{};
    case PlanState::Setup:
        if (static_cast<uint32_t>(now_ms - _scan_ms) >= kSetupRescanMs)
        {
            _scan_ms = now_ms;
            return make(ActionKind::StartScan);
        }
        return Action{};
    default:
        return Action{};
    }
}

PlanState WifiPlan::state() const { return _state; }

bool WifiPlan::hotspot_up() const { return _hotspot; }

JoinFail WifiPlan::last_failure() const { return _last_fail; }
} // namespace prov
