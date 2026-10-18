# Void Sentry engineering notes

This document records the main design decisions and one debugging story from building Void Sentry (an ESP32-CAM pan/tilt turret that tracks motion and serves a browser HUD). Each section follows the same shape: the problem, what was tried or considered, what won and why, and the evidence. The numbers come from measurements on the board or from the CI-checked test suite. The full specs, with the complete alternatives tables, are in [`docs/superpowers/specs/`](superpowers/specs/); the project overview is in the [README](../README.md).

## Contents

1. [Fixed camera and absolute aiming](#1-fixed-camera-and-absolute-aiming)
2. [Decoding JPEG at 1/8 scale](#2-decoding-jpeg-at-18-scale)
3. [Camera clock speed versus WiFi](#3-camera-clock-speed-versus-wifi)
4. [Frame ownership without per-frame allocation](#4-frame-ownership-without-per-frame-allocation)
5. [Detector rules learned from real clips](#5-detector-rules-learned-from-real-clips)
6. [The motor task](#6-the-motor-task)
7. [Dual-core task layout](#7-dual-core-task-layout)
8. [The poll storm: a debugging story](#8-the-poll-storm-a-debugging-story)
9. [WebSocket origin check and DNS rebinding](#9-websocket-origin-check-and-dns-rebinding)
10. [How correctness is checked](#10-how-correctness-is-checked)
11. [WiFi provisioning and the PIN](#11-wifi-provisioning-and-the-pin)

---

## 1. Fixed camera and absolute aiming

**Problem.** The turret has to point at a moving target found in the camera image.

**Considered.**
- Visual servoing: "target is left of centre, so step left", repeated until the target is centred. This converges only when the camera is mounted on the turret, because each step shifts the image.
- Absolute aiming: map the target's image position to a pan/tilt angle and drive the motors there.

**What won and why.** The camera is fixed on the base, not on the turret. With a fixed camera the image never changes when the turret moves, so a visual-servoing loop would see the target still left of centre and spin forever. Absolute aiming also removes ego-motion: the detector never has to pause or compensate while the turret moves, so detection runs continuously. The aim is a two-point linear `AimMap` (pixel to degrees, one line per axis). It is calibrated from the joystick at two opposite corners (procedure in the README) and stored in NVS as a CRC-checked blob. Without a saved map a default map based on the camera's field of view is used and the HUD shows UNCALIBRATED. A fit from two points that are too close together is rejected, because it would amplify error.

**Cost, stated honestly.** Nothing in the image confirms where the turret points, so accuracy depends on the calibration and on the stepper not skipping steps. The check is by eye. An `AimPolicy` adds a 1 degree deadband so tracking noise does not make the motors dither. When the target is lost it holds the aim for 2 s, then returns home.

**Evidence.** `AimMap`, `CalibrationBlob`, `AimPolicy`, `ButtonClassifier` and `CalibrationSession` are pure C++ with native tests (also under ASan and UBSan). Not yet verified on hardware: saving the calibration and reloading it across a reboot.

## 2. Decoding JPEG at 1/8 scale

**Problem.** The stream needs JPEG, but detection needs pixels. The detection budget was about 15 ms per frame at roughly 17 fps, and the first idea (decode the JPEG to a smaller image, then diff) was too slow.

**Considered.**
- Decode at 1/4 scale (80x60): about 71 ms. Over budget.
- Switch the camera to grayscale or YUV and encode JPEG in software for the stream: encode cost, and it undoes the streaming work.
- Full-size RGB decode: far more pixels than needed, plus a large buffer.

**What won and why.** TJpgDec at 1/8 scale (`JPG_SCALE_8X`) into a 40x30 grey grid. At 1/8 each output cell is one 8x8 JPEG block, and the decoder only needs that block's DC coefficient (its average), so the inverse DCT is skipped entirely. The 1/2 and 1/4 modes still run the IDCT on every block and then average, which is why they cost about as much as a full decode. Measured: about 7 ms at 1/8 versus about 71 ms at 1/4. A side benefit is that the block average is a free noise filter, and there is no large RGB buffer and no per-frame allocation.

**Which TJpgDec.** `lib/tjpgd` vendors the C build of TJpgDec from esp32-camera 2.0.4 instead of calling the ROM copy, which is about 40% slower (Stage 7 measurement; core 2 had linked the C copy by accident through esp32-camera, and moving to core 3 fell back to the ROM copy and lost about 5 ms per frame). Keep it at the default `-Os`: `-O2` measured 3 times slower.

**Cost.** A 40x30 grid, so one cell is 8x8 pixels of the QVGA frame and a hand at 2 m is only about 1.5x3 cells. That is why the detector has no morphological opening (it would erase hand-sized targets) and uses a minimum blob of 2 cells instead.

**Evidence.** Stage 4 measured decode at 7.7-9.1 ms and detector plus tracker at 0.49-0.59 ms per frame in an empty scene. The tracking pipeline runs from internal RAM (about 12.6 KB), where tracking takes 0.6 ms versus 1.1 ms in PSRAM.

## 3. Camera clock speed versus WiFi

**Problem.** The camera's XCLK is normally 20 MHz. At that setting the WiFi on this board became almost unusable.

**Measured** (board 2 m from the router; ping average / fetch of a 14 KB page / capture fps):

| XCLK | Ping | Page fetch | Capture fps |
|---|---|---|---|
| 20 MHz | 265 ms | 1.1-5.0 s | 27.8 |
| 16 MHz | 27 ms | 0.1-2.4 s (erratic) | 17.8 |
| 12 MHz | 19 ms | 0.07-0.13 s | 16.6 |
| 10 MHz | 17 ms | 0.14-0.19 s | 13.8 |
| camera off | 10 ms | 0.07 s | - |

10 MHz gave up frame rate for little WiFi gain, and 16 MHz was erratic.

**What won and why.** 12 MHz, about 16.6 fps. A higher XCLK means more camera clock and DMA activity, which degrades WiFi on this board. 12 MHz is the highest frame rate that keeps the network healthy, and latency matters more than frame rate for a turret that has to react. JPEG quality is also set to 18 rather than 12 (`camera.h`): at 12, close-up or noisy scenes made QVGA frames 2.5-4 times larger (measured 2026-10-13), which doubled the decode time on the detection path (decode cost scales with bytes) and loaded WiFi. `jpeg_b_avg` in the stats line shows frame size. WiFi modem sleep is disabled for the same latency reason.

**Lesson.** The "obvious" setting (max clock, best quality) was the wrong one. The only way to find that out was to measure ping, page fetch and fps together.

## 4. Frame ownership without per-frame allocation

**Problem.** The original code had three faults on the frame path: a use-after-free race (the HTTP task sent a camera buffer that `loop()` was freeing and replacing), a `malloc` per frame whose result was never checked, and a polling stream where every frame paid a round trip.

**Considered.** Reference-counted heap buffers, a double buffer, or a pool of fixed slots with explicit ownership.

**What won and why.** Four fixed 32 KB slots allocated once in PSRAM. A camera task copies each driver frame into a slot and returns the driver buffer immediately. Readers (the detect task and the stream task) hold move-only RAII `FrameLease` objects, and a leased slot is never rewritten: if every slot is held, the new frame is dropped and counted rather than overwriting a frame in use. Four slots is one being written, one latest, and up to two held by readers. Bookkeeping lives in a pure class, `FrameSlots`, so it can be tested on the host and under sanitizers; the device wrapper `FrameStore` only adds the buffers and a lock held for bookkeeping, never while copying or sending. Incomplete JPEGs (no `FF D8` ... `FF D9`) are dropped as `corrupt`.

Streaming became push-based. `/stream` is a WebSocket where a stream task wakes on each published frame and queues one send per idle viewer onto the httpd task (max 2 viewers). A viewer still busy with the previous frame is skipped, so latency stays low instead of building a backlog.

**Evidence.** `FrameSlots`, `ViewerSet` and `is_complete_jpeg` have native tests under ASan and UBSan. On hardware, `heap_min` and `psram_min` in the serial stats line stay flat; the 20-minute soak in Stage 4 ended with about 147 KB of heap free (minimum 137 KB) and no reboots. There is no ASan on the Xtensa target, so on-device memory safety rests on this design (no allocation per frame, bounded buffers) plus watching those two counters.

## 5. Detector rules learned from real clips

**Problem.** A detector tuned on synthetic frames behaved well; on real recordings it produced false scene changes and "ghost" targets.

**The detector** is frame differencing against a running background (`MotionDetector`) feeding a tracker (`Tracker`, phases Idle, Tentative, Locked, Coasting). Real clips were recorded on the board (`real_cross_room`, `real_hand_pause`, `real_light_switch`) and committed as regression fixtures next to four generated synthetic clips.

**Three rules came from the real clips.**

1. **Scene-change rule: more than 35% foreground AND a frame-mean shift above 25.** The first version used only the fraction. A person standing close covers 35-45% of the grid, so `real_cross_room` produced 5 false scene changes, each blinding the tracker for about a second. The difference is that auto-exposure holds the frame mean when a person walks in (the mean moved by at most about 11), whereas a light switch moves it by roughly 170-210 grey levels. Requiring both conditions separates the two cases.
2. **Warm-up copies the frame into the background.** For the first 17 frames (about 1 s) the background is a copy of the current frame instead of slowly learning it. Otherwise exposure settling, and a lamp's afterglow after it is switched off, stayed partly in the background and the fading spot was locked onto as a target.
3. **Foreground age limit (`fg_age_limit = 34`).** Foreground cells normally learn into the background slowly (1/128 per frame) so a moving object leaves no trail. But a cell that differs because the background itself is wrong (an object present in the first frame that then leaves, or an object that paused and left) would stay foreground for about 14 s. After a cell has been foreground for more than 34 consecutive frames it goes back to the fast rate (1/16). Ghosts now clear in about 3 s instead of about 14 s. A 4-cell object moving 1 cell per frame covers any one cell for about 4 frames, so the no-trail behaviour is unaffected.

**Evidence.** The ghost rule has two synthetic clips (`seed_object`, `pause_leave`) with an expectation that the tracker is Idle on every frame after a given frame (`no_track_after_frame`). Both fail without the age rule. The real clips each have an `.expect` file (lock by a given frame, at most one id, scene-change count), checked by the bench program in CI.

## 6. The motor task

**Problem.** A 28BYJ-48 stepper must be driven smoothly while a detection pass of about 10 ms runs on the same chip, and it should not overheat when idle.

**Considered.** The Arduino `Stepper` library (its `step(n)` busy-waits, which would stall whatever called it), stepping from `loop()` (it would stutter behind detection), and a timer ISR (riskier shared state).

**What won and why.** A dedicated motor task on core 0 with its own 4-phase coil driver, ticking at 1 kHz with `vTaskDelayUntil`. `loop()` only publishes setpoints through a small mailbox. A pure `StepPlanner` produces a trapezoidal velocity profile (max 400 steps/s, acceleration 800 steps/s^2) so the motor starts and stops without skipping steps, and a target change mid-move, including a reversal, decelerates first instead of flipping direction at speed. Coils are released after 500 ms idle, which stops the motor heating when it is not moving. The servo (pitch) is rate-limited to 120 deg/s. Soft limits are pan +-90 deg and tilt 20-160 deg.

**Two details found on the hardware.**
- **Dropped missed ticks.** If the task is starved (WiFi or lwIP on core 0, flash cache windows), FreeRTOS advances the wake time one tick per call and returns immediately, so missed ticks would run back-to-back within microseconds and the stepper would be driven faster than it can follow. The task resyncs to the current tick instead and counts the event as `motor_late`; the planner just runs slower in wall time.
- **1.5 ms minimum step spacing.** The 28BYJ-48 cannot follow coil steps closer together than that, so the task emits at most one coil step per 1.5 ms regardless of what the planner says.

**Evidence.** `StepPlanner` is tested on the host for reaching the target exactly, never exceeding the speed or acceleration limits (checked every tick), no overshoot, and the reversal case. `motor_late` and `motor_stack_free` are reported in the stats line.

## 7. Dual-core task layout

**Problem (measured in Stage 3).** With a person sitting close to the board and the turret following them, per-frame detection slowed from about 10 ms to 25-30 ms, a fixed compute test on core 1 went from 10 ms to 28-55 ms, and the motor task's late ticks tripled. Four causes were ruled out on hardware: motor motion, PSRAM bandwidth, viewer reconnects and follow-style motion.

**What the layout showed.** The HTTP server task had no core affinity and priority 5, while detection ran in `loop()` at priority 1. Any network burst could land on core 1 and preempt detection.

**What won and why.**

The task table (cores, priorities, stack sizes) is in `CLAUDE.md`, the single place it is kept.

Core 0 is network, motor and the camera driver; core 1 is the camera copy, detection and control. httpd is pinned to core 0 so network work cannot preempt detection. Detection moved out of `loop()` into its own task, woken by `FrameStore` on each published frame, so button handling and stats cannot delay it. The camera copy task sits above detection because its copy is short and it must hand the driver buffer back quickly. The stream task was moved from priority 1 to 4 during review: its work per wake is tiny, but at priority 1 it waited behind a whole detection pass on every frame, adding video latency. The motor task went from 3 to 7 so httpd cannot starve it; its work is about 10-20 us per tick, so the cost to core 0 is small.

**Evidence.** Per-frame detection (decode plus track) with the person close and the turret following: worst 5 s window 11.4 ms, versus 25-30 ms in Stage 3. 83-84 frames analysed per 5 s, which is every captured frame. The detect task's stack has 2,496 B free of 4,096. The stats line now reports per-window averages (`dec_win_us`, `trk_win_us`), because the since-boot averages had hidden the Stage 3 problem.

## 8. The poll storm: a debugging story

This is the longest story and the most useful one to tell, because the first explanation was wrong.

**Symptom.** After the Stage 4 layout change, core 0 sometimes saturated and the task watchdog (which watches the core 0 IDLE task) rebooted the board. It happened twice.

**Wrong first hypothesis.** The Stage 3 notes had suspected a body near the PCB antenna causing WiFi retransmits and network bursts. If that were the cause, the problem should track signal strength.

**How it was ruled out.** An instrument was needed first, because FreeRTOS run-time statistics were turned off in the precompiled SDK (core 2), so there was no direct per-core load number. Core 3 enables them, but the tick sampler was kept at the time so the numbers stay comparable (in Stage 8 it was replaced by the FreeRTOS run-time counters, which agreed within 2 points on the board: hook 80% idle, counters 78-79%). A tick-sampled idle counter was added: at 1 kHz it records which core's IDLE task is running, and the stats line reports `idle0` and `idle1` as the percentage of samples in the last 5 s. With that:
- Core 0 saturated at RSSI -56 to -70 dBm (a good signal).
- Without the problem it stayed above 80% idle even at -77 to -89 dBm (a poor signal).

So signal strength did not predict the failure. Weak WiFi was ruled out.

**The real cause.** The HUD's `/commands` channel is request/reply: it polls the board and the board answers every message with one JSON reply. The old HUD sent `fire:off` whenever a reply showed no lock, and each reply scheduled another poll. Every unlocked reply therefore started a new poll loop on top of the existing one, so the number of loops doubled every round trip until httpd saturated core 0.

**Reproduction.** The buggy loop was reproduced from a PC client with no board changes: core 0 idle was 12-35% with the buggy loop versus about 76% with the fixed one.

**The fix.** All HUD sends go through one `App.sendCommand()`, and the next poll is scheduled only when no message is already awaiting its reply, so exactly one poll loop can exist. After the fix, with 2 viewers, core 0 was 66-87% idle.

**Link to the Stage 3 slowdown.** The Stage 3 symptom (detection slowing when a person sat close) was very likely the same storm, running on core 1 because httpd was not yet pinned to a core. This is an inference, not a proof: nobody re-measured Stage 3 with the fixed HUD. It fits the evidence: the layout let httpd preempt detection on core 1, and a network-side load explains why four hardware-side causes (motor motion, PSRAM bandwidth, viewer reconnects, follow-style motion) were each ruled out.

**What it teaches.** Build the instrument before you theorise, test the hypothesis against the instrument, and do not stop at the first plausible cause (a body near the antenna) just because it matches a physical story. A client-side protocol bug looked like a radio problem.

## 9. WebSocket origin check and DNS rebinding

**Problem.** Once the HUD could drive the motors (Stage 4b: held keys send `move:<dir>`, resent every 100 ms, with a 300 ms dead man on the board and the joystick always winning), a web page from any other site open in a browser on the same network could try to open a WebSocket to the board. Browsers do not apply the same-origin policy to WebSockets, so the server has to check.

**What won and why.** At the handshake of both `/commands` and `/stream`, `remote::origin_allowed` is applied: when an `Origin` header is present it must equal `http(s)://` plus the `Host`, and the `Host` must be an IPv4 literal with an optional port. The IPv4 rule is what defeats DNS rebinding, where an attacker's hostname is re-pointed at the board's LAN address so the browser thinks it is still talking to the attacker's origin. The board is always reached by IP address, so requiring an IP literal costs nothing. A missing `Origin` is allowed (non-browser clients do not send one). The check is pure code with native tests; the device glue only copies the headers into small stack buffers.

**Limit.** Non-browser clients on the LAN are not authenticated. LAN authentication is a known follow-up, so the HUD should be treated as trusted-network only.

**Evidence.** The origin check and the command parser are native-tested under ASan and UBSan. The protocol and dead man were verified from a PC client; HUD jog in a real browser is not yet verified on hardware.

## 10. How correctness is checked

The board has no ASan, so most of the effort goes into making logic testable on a PC and into measuring the rest.

- **Host unit tests.** Unity suites under `ESP32/test/native/` (`pio test -e native`) for the pure libraries (`tracking`, `aiming`, `frames`, `remote`, `provisioning`, `checksum`, `jpeg_gray`, `stats`) and the shared value types. The CI `coverage` job reports line coverage per library and fails below 99% on the pure ones. Rule: code under native test includes no Arduino or ESP-IDF headers, which is why the logic is split from the device glue throughout.
- **Sanitizers.** The same tests run under ASan and UBSan in CI. These do not work on the Windows toolchain used here and not on Xtensa, so CI is the sanitizer runner by design.
- **Clip regression bench.** `track_bench` runs recorded and synthetic clips through detector and tracker and checks per-clip `.expect` files. The same bench is built under ASan and UBSan, and CI reports cachegrind instruction counts per frame (deterministic, unlike wall time on shared runners). CI also regenerates the synthetic clips and compares them byte for byte.
- **Static analysis and formatting.** cppcheck blocks on warning, performance and portability findings; clang-format 19 is checked (not applied) in CI.
- **Hardware stats line.** Every 5 s the firmware logs frame counters, `heap_min` and `psram_min` (a downward drift means a leak), per-window decode and track times, stack high-water marks, motor late ticks and per-core idle. Performance changes record their method and numbers in the PR.
- **Docs.** Public headers use Doxygen blocks, published to GitHub Pages from `main`.

## 11. WiFi provisioning and the PIN

**Problem.** WiFi credentials were compiled into the firmware (a gitignored header), so changing network meant a rebuild, and the board was unusable away from home.

**What won and why.** Networks are provisioned on the device and kept in NVS (up to 3, the strongest in range is joined). With no known network in range, or after 120 s lost, the board opens a WPA2 hotspot `VoidSentry-XXXX` with a random password shown only on the serial log, and a DNS catch-all sends a phone to the `/wifi` page. While the hotspot is up, `/` and any unknown GET answer 302 to `/wifi`. After a successful join the hotspot stays up for another 10 s so the phone page can read the new IP. The connection logic is a pure state machine (`prov::WifiPlan`) that returns actions, so it is tested on the host; `WifiManager` only executes them and never blocks `loop()`.

**The PIN.** Changing networks over WiFi needs a PIN, stored as a salted SHA-256. After 5 consecutive wrong PINs the guard locks out for 60 s, doubling per further lockout and capped at 1 h (`pin_guard.h`). The lockout is RAM-only, so a reboot resets it; that needs physical access or a crash. A first PIN is accepted only while the hotspot is up or nothing is saved.

**Same host checks as the HUD.** `/api/wifi/*` requires an IPv4-literal Host header (DNS rebinding, section 9) and POSTs are origin-checked.

**Not yet verified on hardware:** calibration save and reload across a reboot, and HUD jog hands-on in a browser.
