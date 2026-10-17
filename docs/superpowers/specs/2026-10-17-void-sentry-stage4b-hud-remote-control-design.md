# Void Sentry Stage 4b: HUD Remote Control

Date: 2026-10-17. Status: written by the assistant; the user asked to continue implementing and to batch the hardware tests for later.

## Problem

The roadmap's Stage 4 ("Control loop") includes parsing the HUD's `move:` and `mode:` commands. The Stage 4 dual-core spec left them out of scope, so today:

- The HUD's arrow buttons and WASD keys send `move:<dir>` / `move:stop`, but the firmware ignores every `/commands` payload and only answers with metrics.
- The HUD's mode button is disabled; modes change only with the joystick button.
- `CommandsWebSocketHandler` calls `malloc` twice per message (about 25 messages/s per HUD) and leaks the JSON buffer when a send fails.

## Goal

Drive the turret from the HUD in manual mode, and switch AI/manual from the HUD, without weakening the joystick path or the Stage 4 task layout.

## Decisions

| Decision | Choice | Why |
|---|---|---|
| Wire protocol | `move:<dir>` where dir is `up`, `down`, `left`, `right`, or a diagonal `up-left`, `up-right`, `down-left`, `down-right`; `move:stop`; `mode:ai`, `mode:user`; `fire:on`, `fire:off`; anything else (including the `" "` poll) is a poll | Keeps the strings the HUD already sends; diagonals cover two held keys without a new message type |
| Parsing | New pure library `lib/remote` (namespace `remote`, no Arduino/ESP-IDF headers, no heap): `parse(const char*, size_t) -> Command` | Natively testable under ASan + UBSan, like `lib/aiming` |
| Jog speed | A HUD move jogs at 0.6 of full jog speed on each active axis | Keyboard input is all-or-nothing; full speed overshoots by hand |
| Dead-man timeout | A remote jog expires 300 ms after the last `move:<dir>`; the HUD resends the held direction every 100 ms | A dropped connection or a lost `move:stop` must not leave the turret running into a soft limit |
| Arbitration | Only in User and Calibrate mode. A deflected joystick wins over the HUD; otherwise an unexpired remote jog; otherwise stop. `move:` in Ai mode is ignored | The physical control is the safety override |
| Mode command | `mode:ai` / `mode:user` behave like a short press toward that mode (detection reset, policy reset, jog stop). Ignored while calibrating, and a no-op when already in that mode | Calibration is a joystick-and-button flow; a stray HUD click must not break a session |
| Fire | Parsed and accepted; no effect (there is no firing hardware) | Keeps the protocol complete; firing is out of scope |
| Thread safety | The httpd task (core 0) calls `BaseRemoteSink::submit(const remote::Command&)`; `Controller` implements it by storing into a `remote::RemoteInbox` under a spinlock with `millis()`. `Controller::run()` (core 1) reads the jog and takes a pending mode request | Same mailbox pattern as `Turret`; no blocking in critical sections |
| Handler allocation | Receive into a fixed 64-byte stack buffer (larger frames close the session), build the JSON in a stack buffer, no `malloc`, no leak on send failure | No per-message heap, which matches the frame path |
| HUD | Mode button enabled (sends `mode:ai`/`mode:user`); held keys/buttons combine into one direction, resent every 100 ms; `move:stop` on release, window blur and tab hide; manual controls dimmed in Ai mode | Matches the dead-man timeout and makes the state visible |

## Success criteria

| Criterion | Target |
|---|---|
| Logic | Parser and inbox natively tested (all directions, diagonals, stop, modes, fire, unknown, truncated/oversized input, expiry boundary at 300 ms, mode request taken once); native, ASan + UBSan and CI pass |
| HUD jog (hardware, later) | Manual mode: arrows/WASD jog the turret; release stops within about 0.3 s; closing the tab while holding stops within 0.3 s |
| Mode (hardware, later) | HUD button toggles AI/manual; ignored while calibrating; joystick still works and wins over the HUD |
| No regression | `idle0` with 1 HUD stays at least 60%; detection windows unchanged |

## Out of scope

Calibration from the HUD, firing hardware, WiFi provisioning.
