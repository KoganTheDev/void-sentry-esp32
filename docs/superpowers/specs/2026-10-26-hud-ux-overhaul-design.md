# Void Sentry: HUD UI/UX overhaul

Date: 2026-10-26. Status: approved by the user after iterating on a clickable mockup (`docs/design/hud-mockup.html`, also published at https://claude.ai/artifact/FkVSSasPY63xtNrimj8JGT). This document is the contract between the firmware, the mock board and the HUD.

## Problem

The HUD grew stage by stage. A wiring audit against the firmware and UX research (teleoperation interfaces, drone ground control, PTZ camera UIs, mobile touch, WCAG) found:

- Dead or misleading controls: `Engage Fire` (the firmware ignores `fire:*`), `CONF` and `RAM` (never written), fake `PAN/TILT 0.0 / 90.0` and `MODE AI` before the first reply.
- A Google Fonts `<link>` that blocks rendering offline (hotspot mode has no internet), an unclosed `.dashboard` div, a `console.log` on every 25 Hz reply.
- Only `/stream` health is shown; a frozen frame looks live; a third viewer is silently dropped.
- D-pad `<div>`s with mouse+touch handlers, no focus or labels, 50 px targets; presses in AI mode and mode clicks during calibration are silently ignored.
- Calibration cannot be driven from the HUD, its success is invisible (the firmware goes straight to AI and `cal_state` never reads `done`), and "no target visible" only reaches the serial log.
- `user-scalable=no`, arrow keys scroll the page, the pulse animation ignores `prefers-reduced-motion`, some status is colour-only.

## Goal

Every control works as labelled. The operator always sees the mode, link and video health, calibration, where the turret points and what it tracks. Calibration and aiming work from the HUD. The page works on desktop, phone portrait and phone landscape, and offline.

## Decisions

| Decision | Choice | Why |
|---|---|---|
| Look | Existing black/orange identity, the project logo, system font stack, `tabular-nums` | No network fetches; the board is often reached with no internet |
| Buttons | "Hardware" style: keys with a lip that sink when pressed, a one-piece cross d-pad with a recessed recenter button, a round arcade FIRE in a bezel (86 px desktop, 112 px phone) | Chosen by the user from three proposals; the pressed look confirms a touch registered |
| Reticle | "Classic" (ring, ticks, centre dot) in red over a black under-stroke; target brackets white | Chosen by the user; white brackets keep the red reticle distinct |
| Where the turret points | The board sends the aim pixel `ax,ay` (inverse of the active aim map applied to the current pose); the reticle is drawn there | The camera is fixed, so the inverse map gives the exact image point the turret aims at |
| Aim latency | A dashed ghost reticle follows the finger instantly; the solid reticle only moves with the board's reply; a dotted line and a time-left label join them | Shows the motor lag instead of hiding it; fire and the on-target cue use the real reticle |
| Sending aim points | `aim:x,y` at most every 100 ms while dragging, latest wins, release point always sent | No queue of stale points for the turret to replay |
| Events over a polled channel | Counters (`cal_saves`, `cal_miss`) instead of one-shot flags | The HUD polls at about 25 Hz and can miss a one-pass flag; a counter change is never missed |
| Fire | HUD effect only (no actuator, all free GPIOs are used): tracer, then HIT with sparks and a floating "HIT!" when the reticle is inside the box (+6 px) of a locked or coasting target; miss = tracer only | Requested by the user; `fire:*` stays parsed for a future actuator but the HUD stops sending it |
| Auto-fire | Off by default, confirm dialog to arm, blinking chip while armed, disarms on link loss or reload | The user asked for an explicit opt-in |
| Target ids | Never displayed; a change of the board's `id` replays the bracket snap-in | The user does not want counting; a target switch must still be visible |
| Calibration from the HUD | `cal:start`, `cal:point`, `cal:cancel` with the button's exact semantics | One state machine for both inputs |
| Viewers | Reply field `vw`; the HUD shows n/2 and detects a refused third viewer (open then close with no frames, 3 times) | The board can only stream to 2 viewers |
| Haptics | Android `navigator.vibrate`; iPhone: click on the label of a hidden `<input type="checkbox" switch>` (iOS 17.4+), only inside taps (hit, arrow at an end stop) | Safari has no Vibration API; one source says iOS 26.5 blocks the switch trick, so it is best effort |

## Protocol (`/commands`)

Request/reply: every text message gets exactly one JSON reply, and the HUD keeps exactly one poll loop (CLAUDE.md, notes s8). Messages are at most 64 bytes, exact and case-sensitive.

| Message | Meaning |
|---|---|
| `move:<dir>` (`up`, `down`, `left`, `right`, `up-left`, `up-right`, `down-left`, `down-right`), `move:stop` | Unchanged. HUD jog, resent every 100 ms, 300 ms dead man, User and Calibrate only, the stick wins |
| `mode:ai`, `mode:user` | Unchanged. Dropped while calibrating |
| `fire:on`, `fire:off` | Unchanged (parsed, no effect). The HUD no longer sends them |
| `cal:start` | Same as a long press when not calibrating: remember the mode, enter Calibrate at point 1. Ignored while calibrating |
| `cal:point` | Same as a short press while calibrating: record a point (or restart after `failed`). The jog is zeroed and the pose is recorded on the first control pass where `pan_speed() == 0`, or after 1 s with the current pose. Ignored outside Calibrate |
| `cal:cancel` | Same as a long press while calibrating: cancel, restore the previous mode, keep the old map. Ignored outside Calibrate |
| `aim:<x>,<y>` | User and Calibrate only. `x` in 0..319, `y` in 0..239, plain decimal (1 to 3 digits each, no sign, no spaces). The pixel goes through the active `AimMap` (`aim()` already clamps to the soft limits) to `set_target_deg`. Cancels an active HUD jog. Malformed or out-of-range values are a Poll |
| `speed:slow`, `speed:normal`, `speed:fast` | HUD jog magnitude 0.25, 0.6 (today's value), 1.0. Persists until changed; the default is normal |
| anything else (including `" "`) | Poll |

### Reply JSON

The existing keys keep their order and meaning, then the new keys. All new keys are always present when the controller status is available.

| Key | Type | Meaning |
|---|---|---|
| `detected`, `x`, `y`, `width`, `height`, `pixels`, `state`, `id`, `bx`, `by`, `bw`, `bh`, `vx`, `vy` | as today | tracker |
| `mode`, `cal`, `cal_state`, `calibrated`, `pan`, `tilt`, `cal_error` | as today | controller |
| `cal_saves` | uint | successful calibrations since boot |
| `cal_miss` | uint | calibration records ignored because no target was visible (button or HUD) |
| `stick` | 0/1 | the physical joystick is deflected and overriding the HUD jog |
| `ax`, `ay` | int | aim pixel: `AimMap` inverse of the current pose, rounded, not clamped (may be outside the frame) |
| `lim` | uint bitmask | 1 pan at min, 2 pan at max, 4 tilt at min, 8 tilt at max (within 0.5 degrees of the soft limit) |
| `rx0`, `ry0`, `rx1`, `ry1` | int | reachable box in pixels: the image region whose aim lies inside the soft limits, clamped to the frame, `rx0 <= rx1`, `ry0 <= ry1` |
| `home` | uint | ms until the AI return-home fires (target lost, Idle or Tentative, not yet homed); 0 when none is pending or not in AI |
| `ret` | 0/1 | the turret is travelling home after a Home command (more than 1 degree from home) |
| `spd` | string | `slow`, `normal` or `fast` (the HUD resyncs its speed control from it after a reconnect) |
| `vw` | uint | `/stream` viewers connected (0 to 2) |
| `auth` | 0/1/2 | added later (control login, engineering notes s9): 0 view only (a viewer's control commands are dropped), 1 logged in, 2 no PIN set so control is open |

The reply is built by a pure formatter (`lib/hud`, natively tested) into a stack buffer whose size is asserted by a test to fit the worst case. On overflow it still falls back to `{"detected":0}`.

## HUD layout and behaviour

Reference implementation: `docs/design/hud-mockup.html`. The real HUD keeps everything except the review bar, the simulation, the style and reticle pickers (Hardware and Classic only) and the "Test haptic" button.

- **Status bar:** logo; chips MODE, VIDEO (fps, `FROZEN n s`, `BUSY`), LINK (round-trip ms, `OFFLINE`), VIEWERS (`n/2`, amber at 2/2, hidden on phones unless 2/2), CAL (`OK`, `UNCALIBRATED`), AUTO-FIRE ARMED (only while armed), WiFi SSID linking to `/wifi`. Icons in this order: help, snapshot, event log (no unseen-count badge: the user found it intrusive), fullscreen (hidden on phones).
- **Video:** state word top-left: TRACKING, COASTING, ACQUIRING..., SCANNING, `SCANNING · HOME IN 1.4 s` (from `home`), RETURNING HOME (from `ret`). Top-right: fps, replaced by `FROZEN n s` (amber, frame greyed) after 1 s without frames; at 5 s the stream socket is closed and reopened. Cards: "Video busy" for a refused third viewer, "Turret link lost" when `/commands` is down. Bottom-left: LOW LIGHT when the mean frame brightness (sampled twice a second from a small canvas) is under about 25/255.
- **Overlay:** white corner brackets with a dark halo (amber dashed while coasting, light dotted while acquiring); a new `id` replays a 280 ms snap from 1.4x size (ease-out with overshoot). A soft beige trail of the last 1.2 s of positions. The red Classic reticle at `ax,ay` (dashed with a `~` when uncalibrated). On-target cue while the reticle is on a locked target: inner brackets snap in once (300 ms ease-out with overshoot), then breathe (sine ease-in-out, 1.6 s cycle, 3 px), ON TARGET fades in, FIRE glows on the same rhythm. Calibration: P1 marker and the shaded "too close" zone (80 x 60 px each side).
- **Aiming:** arrows (pointer capture, stop on up/cancel/lostpointercapture/blur/hidden), Slow/Normal/Fast (Shift held = slow), tap or drag on the video in User/Calibrate, recenter button and `H` (aim at 160,120). Ghost reticle, dotted line and time-left label (distance over the slew speed observed from `ax,ay`, default 80 px/s, plus half the round trip). Out of reach (outside the `rx/ry` box): amber ghost, an x where the turret will stop, faint reach box while dragging. End stops from `lim`: the arrow dims with a red edge; pressing it toasts and buzzes.
- **Fire:** see Decisions. 700 ms cooldown drawn as a radial fill.
- **Mode and calibration:** segmented AI / MANUAL / CALIBRATE with "switching..." until the reply confirms; blocked actions toast the reason. Wizard: steps, live checks (target locked, turret still), Record disabled with the reason, failure with Restart, inline cancel confirm, success from a `cal_saves` increment. `cal_miss` increments toast "No target visible: wave at the spot". `stick` toasts "Joystick has control" while an arrow is held.
- **Feedback:** toasts are deduplicated per kind until the visible one disappears; an `aria-live` region announces mode and link changes. Event log drawer (closed by default): filters All/Targets/System, consecutive repeats folded (x3), relative times with the clock time on hover. Snapshot (`P`): frame plus overlay downloaded as `void-sentry_YYYY-MM-DD_HH-MM-SS.png`.
- **Keyboard:** WASD/arrows (preventDefault), Shift slow, H recenter, M AI/Manual, C calibrate, Enter record, Esc close or cancel, Space fire, P snapshot, F fullscreen, ? help. Ignored with Ctrl/Alt/Meta or while a dialog is open.
- **Layouts:** desktop video plus a 320 px panel; phone portrait video, mode, then the controls row with no scrolling at 390x844; phone landscape video plus a 300 px panel. `touch-action: manipulation` on the page and `none` on the pad, FIRE and the video. No `user-scalable=no`. `prefers-reduced-motion` turns the animations off.

## Success criteria

| Criterion | Target |
|---|---|
| Logic | Parser, inbox, inverse map, reach box and reply formatter natively tested; native, ASan + UBSan and coverage (99% floor on pure libs) pass in CI |
| HUD | Every control exercised against the mock board in a browser at 1440x900, 390x844 and 844x390; `web/test_web_ui.py` passes (no external resources, balanced tags, only known commands) |
| Hardware | Jog and tap-to-aim move the turret and stop; the reticle matches the turret's aim after calibration; HUD calibration saves and survives a reboot; a third viewer sees BUSY |
| No regression | `idle0` with one HUD stays at least 60%; detection windows unchanged |

## Out of scope

A firing actuator, LAN authentication, the `/stream` stall root cause (the HUD only reconnects on a frozen stream), the logging rework.
