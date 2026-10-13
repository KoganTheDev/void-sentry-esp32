# Void Sentry Stage 4: Dual-Core Task Layout

Date: 2026-10-13. Status: written by the assistant; the user asked to skip spec/plan review and go straight to implementation.

## Problem (measured in Stage 3)

When a person sits close to the board while the turret follows them:

- **Detection slows down.** Decode + track rises from about 10 ms to 25-30 ms per frame.
- **Core 1 is preempted.** A fixed compute workload in `loop()` on core 1 goes from 10 ms to 28-55 ms.
- **The motor task falls behind.** Its late ticks on core 0 triple.

Four alternative causes were ruled out on hardware:

- motor motion
- PSRAM bandwidth
- viewer reconnects
- follow-style motion

The task layout explains the symptom:

| Task | Core | Priority |
|---|---|---|
| WiFi | 0 | 23 |
| lwIP | 0 | 18 |
| esp_timer | 0 | 22 |
| motor | 0 | 3 |
| camera | 1 | 2 |
| stream | 1 | 1 |
| `loop()` (detection) | 1 | 1 |
| httpd (`esp_http_server`) | **no affinity** | **5** |

When core 0 is busy with WiFi (e.g. retransmits), httpd runs on core 1 and preempts everything there, detection included. Detection runs at the lowest priority in `loop()`, so any network burst starves it. The suspected trigger for the bursts (a body near the PCB antenna) is unconfirmed, but the layout makes detection fragile against any network load.

## Goal

Detection gets a dedicated, higher-priority slot on core 1 that network work can't take. Network and motor work share core 0 in a defined priority order.

## Decisions

| Decision | Choice | Why |
|---|---|---|
| httpd affinity | Pin to core 0 (`config.core_id = 0`), priority 5 unchanged | Keeps network work on the network core. Core 1 becomes camera + detection + control only. |
| Motor task | Core 0, priority 3 -> 7 | It would otherwise be preempted by httpd (5) more often after the move. Its work is about 10-20 us per 1 ms tick, so a higher priority costs core 0 nearly nothing. It stays below esp_timer (22), lwIP (18) and WiFi (23). |
| Detection | New FreeRTOS task "detect" on core 1, priority 3, stack 4096 B | Woken by FrameStore on every published frame; processes the newest frame. Out of `loop()`, so the button, jog and stats loop cannot delay it, and it cannot delay them. |
| Camera task | Core 1, priority 2 -> 4 | Above detection: the copy is short (about 0.2-0.5 ms) and returns the driver buffer quickly. Detection (about 10 ms) must not delay it. |
| `loop()` / stream | `loop()` priority 1; stream priority 4 | Control runs at 25-40 Hz, is cheap, and reads only published snapshots. Changed during review: stream moved from 1 to 4, because its work per wake is tens of microseconds and at priority 1 it waited behind a whole detection (priority 3) on every frame, adding video latency. |
| FrameStore notification | Up to 2 subscribers (stream task, detection task), fixed array, atomic handles | No heap. |
| Detection reset | `reset()` becomes a request flag (atomic) consumed by the detection task before its next frame | `reset()` is called from `loop()` (mode toggles) while the detection task owns the pipeline. |
| Controller | No longer runs detection or touches FrameStore | Reads `get_track_snapshot()` every pass in AI and Calibrate modes. Detection runs continuously in every mode (the HUD box works in USER mode too). |
| Telemetry | Stats line adds per-window `det_frames`, `dec_win_us`, `trk_win_us` | Since-boot averages hid the Stage 3 problem. |

## Success criteria (on hardware)

| Criterion | Target |
|---|---|
| Empty scene, 1 viewer | `dec_win_us + trk_win_us` <= 15 ms; capture >= 16 fps; `det_frames` about 83 per 5 s |
| Person close, turret following, 1 viewer (the Stage 3 failure case) | `dec_win_us + trk_win_us` <= 15 ms in most windows (target: no window above 20 ms); `det_frames` about 83 per 5 s |
| Motor | `motor_late` no worse than Stage 3 at idle; aiming and jog unchanged |
| Behaviour | Modes, aiming, return home and HUD unchanged |
| Logic | Native suite, ASan + UBSan and CI all pass |

## Out of scope

WiFi provisioning, calibration UX changes, HUD manual controls.
