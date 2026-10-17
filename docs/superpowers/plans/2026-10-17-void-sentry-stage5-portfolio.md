# Stage 5 Portfolio Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Replace the outdated README with an accurate one (architecture diagrams, measured numbers, wiring, build/test, badges) and add an engineering write-up.

**Architecture:** Documentation only, plus one Doxyfile line. Two independent files, written in parallel.

**Spec:** `docs/superpowers/specs/2026-10-17-void-sentry-stage5-portfolio-design.md`

## Global Constraints

- **Facts only from:** `CLAUDE.md`, `docs/superpowers/specs/*.md`, the code, and the facts list below. Never invent numbers, features or hardware. If something is not in those sources, leave it out.
- **No em dashes** anywhere (use " - "). Plain, specific language; no marketing filler ("blazing", "cutting-edge", "revolutionary").
- **Credentials:** never read or print the gitignored WiFi credentials header in `ESP32/include/`, and never run a shell command containing the word that file is named after (a hook blocks it). Refer to it in prose as "the WiFi credentials header (`include/secrets.h`, copied from `secrets.h.example`)"; writing that path in a Markdown file is fine.
- **Mermaid:** GitHub-flavoured Mermaid (`flowchart`), node labels in quotes when they contain punctuation, no HTML except `<br/>`.
- **Commits:** trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Facts list (measured on hardware unless noted; use these exact values)

- Hardware: Freenove ESP32-WROVER CAM (OV3660 camera, PSRAM required) on the Freenove GPIO Extension Board; 28BYJ-48 stepper + ULN2003 driver (yaw); SG90 servo (pitch); 2-axis joystick with push button. Pins: joystick X/Y GPIO 32/33 (ADC1, works with WiFi on), button GPIO 0, servo GPIO 14, stepper IN1-IN4 GPIO 13, 12, 15, 2. With the camera attached exactly 8 GPIOs are free and the turret uses all 8. `ESP32/include/pins.h` is the only place pins are defined, with `static_assert`s.
- Toolchain: Arduino core 2.0.17 (ESP-IDF 4.4, GCC 8.4) on PlatformIO, C++17 everywhere.
- Video: QVGA JPEG, quality 18, camera XCLK 12 MHz, about 16.6 fps capture. XCLK measurements (ping avg / 14 KB page fetch / fps): 20 MHz 265 ms / 1.1-5.0 s / 27.8 fps; 12 MHz 19 ms / 0.07-0.13 s / 16.6 fps.
- Frame path: camera task copies each frame into one of 4 fixed 32 KB PSRAM slots; readers hold RAII leases; no per-frame allocation. `/stream` is a push WebSocket, max 2 viewers, busy viewers skip frames.
- Detection: 1/8-scale DC-only JPEG decode to a 40x30 grey grid, about 7 ms versus about 71 ms at 1/4 scale; frame differencing with a running background (`MotionDetector`) and a tracker (`Tracker`, phases Idle / Tentative / Locked / Coasting); pipeline in internal RAM (~12.6 KB), tracking 0.6 ms there versus 1.1 ms in PSRAM. Scene-change rule: >35% foreground AND frame-mean shift > 25. Ghost fix: `fg_age_limit = 34` clears ghosts in about 3 s instead of about 14 s.
- Stage 4 per-frame detection (decode + track): empty scene 7.7-9.1 ms + 0.49-0.59 ms; person close with the turret following: worst 5 s window 11.4 ms (Stage 3: 25-30 ms); 83-84 frames analysed per 5 s (every captured frame); detect task stack 2,496 B free of 4,096; 20 min soak: no reboots, heap about 147 KB free (min 137 KB).
- Aiming: fixed camera, absolute aiming through a two-point linear `AimMap` (pixel -> pan/tilt degrees), calibrated from the joystick and stored in NVS with a CRC; trapezoidal stepper profile at a 1 kHz motor tick (max 400 steps/s, accel 800 steps/s^2), coils released after 500 ms idle; servo rate-limited to 120 deg/s; pan soft limits +-90 deg, tilt 20-160 deg; AI mode holds 2 s after the target is lost, then returns home.
- Tasks: see the task layout table in `CLAUDE.md` (core 1: camera copy prio 4, detect prio 3, stream prio 4, loop prio 1; core 0: WiFi 23, esp camera driver 23, esp_timer 22, lwIP 18, motor 7, httpd 5).
- HUD: served gzipped from flash; `/commands` request/reply JSON metrics; manual jog from WASD/arrows/on-screen buttons, resent every 100 ms, 300 ms dead man on the board, joystick wins; mode button toggles AI/manual; WebSocket handshakes from another browser origin, or with a non-IPv4 Host (DNS rebinding), are refused.
- The poll-storm bug (Stage 4): the HUD sent `fire:off` on every unlocked reply, each reply started another poll loop, loops doubled every round trip; core 0 idle fell to 0% and the task watchdog rebooted the board twice. Reproduced from a PC client: core 0 idle 12-35% (buggy loop) versus about 76% (fixed). Fixed HUD with 2 viewers: core 0 66-87% idle. Weak WiFi was ruled out: core 0 saturated at RSSI -56 to -70 dBm, and stayed 80%+ idle at -77 to -89 dBm without the storm. Measured with a tick-sampled per-core idle counter added for the purpose (FreeRTOS run-time stats are off in the precompiled SDK).
- Testing: 132 host unit tests (Unity, `pio test -e native`), also run under ASan + UBSan in CI; tracking regression bench on recorded clips (`real_cross_room`, `real_hand_pause`, `real_light_switch`) plus 4 synthetic clips with `.expect` files; cachegrind instruction counts in CI; cppcheck blocking in CI; clang-format 19 check; Doxygen published to GitHub Pages from `main`.
- Not yet verified on hardware: calibration save and reload across a reboot; HUD jog hands-on in a browser (protocol and dead man verified from a PC client).

### Task 1: README.md (repo root)

- [ ] Header: logo and title images already used today (`assets/void_sentry_logo_for_md_file_400x400_pixels.png`, `assets/void_sentry_title.png`), a one-line description, badges with the repo path `KoganTheDev/void-sentry-esp32`: `ci.yml` (name "CI"), `static_analysis.yml`, `doxygen_docs_generator.yml`, MIT license, C++17, platform ESP32 (shields.io static badges for the last three).
- [ ] Demo slot: an HTML comment `<!-- DEMO GIF: record with the board (HUD + turret following), save as assets/demo.gif -->` right under the header.
- [ ] Sections: What it does (3-5 sentences); Highlights (6-8 bullets with numbers from the facts list); Architecture (Mermaid flowchart of the data flow: OV3660 -> camera task -> FrameStore slots -> detect task (decode, MotionDetector, Tracker) -> track snapshot -> Controller (AimPolicy, AimMap) -> motor task -> stepper + servo; FrameStore -> stream task -> HUD; HUD `/commands` <-> Controller; joystick -> Controller) and a second Mermaid diagram or table for the dual-core task layout; Performance (table from the facts list); Hardware and wiring (parts + pin table); Getting started (prerequisites PlatformIO; the credentials header step; build, upload, monitor commands from `CLAUDE.md`; read the IP from the boot log); Using it (HUD URL, modes, joystick gestures, HUD controls, calibration steps); Testing and CI (the commands from `CLAUDE.md`); Project layout (a short tree of `ESP32/lib/*` with one line each, `web/`, `tools/`, `docs/`); Roadmap status (Stages 0-5 done or in progress, WiFi provisioning and the Arduino core 3.x / C++20 toolchain upgrade as next); link to `docs/engineering-notes.md` and the Doxygen site (`https://kogandev.github.io/void-sentry-esp32/` - check `doxygen_docs_generator.yml` and the repo name for the exact Pages URL and use what it implies); License.
- [ ] Keep it under about 250 lines.
- [ ] Verify: every relative link points to an existing file; Mermaid blocks parse (`npx -y @mermaid-js/mermaid-cli@11.4.0 -i <block>.mmd -o out.svg` if npx works offline/online, else review syntax carefully and say so); no em dashes (`grep -n "—" README.md` prints nothing). Commit: "Rewrite README: accurate architecture, measured performance, wiring, build and test".

### Task 2: `docs/engineering-notes.md` and Doxygen name

- [ ] One section per decision or debugging story, each written as: the problem -> what was tried or considered -> what won and why -> the evidence (numbers from the facts list). Cover: (1) fixed camera and absolute aiming instead of visual servoing; (2) 1/8-scale DC-only JPEG decode; (3) camera XCLK 12 MHz versus WiFi; (4) frame ownership without per-frame allocation (fixed PSRAM slots, leases); (5) detector rules learned from real clips (scene-change rule, warm-up, ghost age limit); (6) the motor task (own coil driver, dropped missed ticks, 1.5 ms step spacing, trapezoidal profile); (7) dual-core task layout; (8) the poll-storm investigation as a debugging story (symptom, wrong first hypothesis, the instrument built, the reproduction, the fix, why Stage 3's slowdown was very likely the same storm on core 1); (9) WebSocket origin check and DNS rebinding; (10) how correctness is checked (native tests, sanitizers, clip regression bench, cppcheck, hardware stats line).
- [ ] Start with a short intro (what the document is) and a table of contents. Keep it under about 300 lines. Use the specs in `docs/superpowers/specs/` and `CLAUDE.md` for detail; stay within the facts list for numbers.
- [ ] `ESP32/Doxyfile`: `PROJECT_NAME = "Void Sentry"` (that one line only).
- [ ] Verify: no em dashes; links resolve. Commit: "Add engineering notes; rename Doxygen project to Void Sentry".

### Task 3 (controller): review, push, PR, CI

### Task 4 (with the user, later): record the demo GIF and add it to the README slot
