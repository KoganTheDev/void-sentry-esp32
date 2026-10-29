# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Void Sentry: ESP32-CAM (Freenove ESP32-WROVER, OV3660) pan/tilt turret that tracks motion via frame differencing and serves a web HUD. Arduino framework on PlatformIO.

Docs split: `README.md` is for people, `docs/engineering-notes.md` holds measurements and the reasons behind decisions (cited as "notes s<N>"), this file holds rules, commands and gotchas. API docs are the Doxygen `@brief`/`@details` blocks in headers; keep that style for new public APIs.

## Commands

All PlatformIO commands run from `ESP32/`. `pio` is not on PATH here; use `~/.platformio/penv/Scripts/pio.exe` (or the VS Code extension).

```bash
pio run -e freenove_esp32_wrover          # build the turret firmware
pio run -e freenove_esp32_wrover -t upload
pio run -e pin_check -t upload            # wiring bring-up test (no camera/WiFi), read serial after
pio device monitor                        # serial @ 115200; `wifi-reset` + Enter erases saved WiFi networks and the PIN (reboots)
pio test -e native                        # host unit tests, no board needed
pio test -e native_sanitize               # ASan+UBSan, Linux/CI only
pio test -e freenove_esp32_wrover -f test_wifi_manager   # one on-device suite (board connected)
pio check --severity=high --severity=medium   # cppcheck
python tools/tracking/make_synthetic_clips.py ESP32/bench/fixtures   # regenerate synthetic clips (repo root)
python tools/jpeg_fixtures/make_jpeg_fixtures.py ESP32/test/native/test_jpeg_gray/jpeg_fixtures.h   # regenerate the jpeg_gray test JPEGs (needs Pillow)
g++ -std=c++20 -O2 -g -Wall -Wextra -I ESP32/include -I ESP32/lib/tracking/include     ESP32/lib/tracking/src/*.cpp ESP32/bench/track_bench.cpp -o track_bench   # clip bench
./track_bench [--repeat N] ESP32/bench/fixtures/*.gray   # exits non-zero if any .expect fails
python -m unittest discover -s tools/tracking -p "test_*.py"   # fixture tool tests
python -m unittest discover -s web -p "test_*.py"              # HUD generator tests
python -m unittest discover -s tools/hud_mock -p "test_*.py"   # mock board tests
python tools/hud_mock/mock_board.py   # fake board for HUD work (serves web/*.html, /commands, /stream; flags in its docstring), open http://127.0.0.1:8080
```

- Envs are in `platformio.ini`: firmware `freenove_esp32_wrover`, `pin_check` (only `src/pin_check/`), and host-only `native`, `native_sanitize`, `native_coverage` (tests under `test/native/`; the last two are Linux/CI only). Device envs ignore `test/native/*`.
- **Toolchain: Arduino core 3.3.12 via pioarduino (ESP-IDF 5.5.5, GCC 14), C++20 everywhere** (firmware, native, cppcheck, CI). Core 3 passes `-std=gnu++2b`/`2a` itself, so the firmware env unflags them and sets `-std=gnu++20`. The esp32-camera driver is bundled with core 3 (not a `lib_dep`).
- **`custom_sdkconfig`** (firmware env, inherited unchanged by `pin_check`) enables the WebSocket handshake callbacks the origin check and `/stream` viewer registration need (`handshake_guard.h` has an `#error` if they are off), 80 MHz flash/PSRAM, and a 100 ms lwIP TCP timer (the default 250 ms gives a 1.5 s minimum RTO on a LAN, which froze the stream; notes s13). Do not raise `CONFIG_LWIP_TCP_SND_BUF_DEFAULT`: doubling it did not help and cost ~40 KB of `heap_min`. pioarduino rebuilds the Arduino libs for it: the first build after a clone or any change to that text is **slow (20-40 min)**, then cached in `~/.platformio` keyed on the exact text, so both firmware envs must keep it identical or each env switch rebuilds. Cached builds take 1-3 minutes.
- That build generates gitignored files in `ESP32/` (`sdkconfig.*`, `CMakeLists.txt`, `dependencies.lock`, `.dummy/`, `managed_components/`). The partition table is the project copy `partitions_min_spiffs.csv` (NVS offsets unchanged, so saved WiFi and calibration survive). A package error mentioning `FRAMEWORK_DIR None`: run `pio pkg install -e <env>` from PowerShell. `IncompatiblePlatform ... PlatformIO Core v6.2.0-post.1`: the platform's venv `~/.platformio/penv` got PyPI `pioarduino 6.2.0.post1`, which semver reads as a prerelease below 6.2.0; pin it with `penv python -m pip install "pioarduino==6.2.0"` (CI does this in `ci.yml`).
- `native` uses the first `g++` on PATH and needs GCC >= 10. Here that is WinLibs GCC 14.2 + LLVM 19 (clang-format, clangd), but an old MinGW 6.3 at `C:\MinGW\bin` may shadow it: check `g++ --version` if native builds fail. Code under native test must not include Arduino/ESP-IDF headers.
- On-device suites under `ESP32/test/` are Unity sketches (`setup()` calls `UNITY_BEGIN()`) read over serial. There are two: `test_wifi_manager` and `test_turret_server` (board connected).
- **Never pipe `pio` into something that exits early** (`grep -m1`, `head`): on Windows pio blocks forever writing to the closed pipe and the build looks hung. Redirect to a log file and grep the file.
- Serial is the CH340 (COM3 or COM4, changes on re-plug; `pio device list`). To read without resetting the board, open the port with DTR/RTS held low. **The board's IP:** DHCP may change it after reflashing; read it from the boot log, never assume it.
- Formatting: `.clang-format` (LLVM base, Allman braces, 4 spaces, 120 columns, `int* p`), clang-format **19** locally and in CI (`pip install "clang-format==19.1.*"`). The `PostToolUse` hook `.claude/hooks/format_cpp.py` formats firmware C/C++ on edit; it invokes `python`, so with only `python3` it exits 127 and nothing is formatted (CI's `--dry-run --Werror` check still catches it; CI never commits).
- CI: `ci.yml` (format, web generator, builds, native tests, sanitizers, coverage with a 99% floor on the pure libs, tracking bench), `static_analysis.yml` (cppcheck `--std=c++20`, blocks on warning/performance/portability; suppress an intentional finding with `// cppcheck-suppress <id>`) and a Doxygen publish to `gh-pages` on push to `main`.
- Arduino's logger only shows `ESP_LOGx` from files that include `esp32-hal-log.h` (or `Arduino.h`); pure ESP-IDF files must include it or their logs are silently dropped.

## Sanitizers and profiling

Use them on the target where each one actually works:

- **ASan + UBSan, host only.** Any logic that can be native-tested must also pass under `-fsanitize=address,undefined -fno-omit-frame-pointer` (`native_sanitize`, a CI job on Ubuntu). Design new pure-logic modules so they can run there. Never weaken a test to silence a sanitizer report; fix the bug.
- **Not on Windows.** The MinGW toolchains here (WinLibs GCC 14, Clang 19) ship no `libasan`/`libubsan` (`cannot find -lasan`, verified). **Decision: CI is the sanitizer and host-benchmark runner.** No local WSL/Linux setup, so don't suggest installing one; push and read the CI job output.
- **Not on the ESP32.** No ASan for Xtensa. On-device memory safety is by design (no per-frame allocation, bounded buffers) plus watching `heap_min` and `psram_min` in the stats line.
- **`perf`, host only (CI).** Shared runners make wall time noisy: compare within one job or use `perf stat -e instructions`; build with `-O2 -g -fno-omit-frame-pointer`.
- **On-device profiling:** `esp_timer_get_time()` or `ESP.getCycleCount()`; report per-stage averages in the stats line, not per frame (logging distorts timing).
- **Benchmarks are reproducible:** each perf-relevant change records its method and numbers (fps, latency median, `heap_min`) in the PR description.

## Required local setup

- None for building: a fresh clone builds as is. WiFi credentials are never in the repo or the firmware source; networks are provisioned on the device and kept in NVS. `.gitignore` still lists `secrets.h` so a stray copy is never committed.
- Never log, print or commit WiFi passwords, the PIN or the hotspot password (the hotspot password appears only on the board's serial log; the user reads it there).

## Architecture

Frames flow camera task -> `FrameStore` -> detect task and stream task. `loop()` runs `controller.run()`, `WifiManager::maintain()`, `telemetry::log_stats()` and yields 1 tick; it does no detection. All objects are globals in `src/main.cpp`, wired by reference (manual dependency injection). `README.md` has the data-flow diagram.

- **`Controller`** (`lib/controller`) depends only on interfaces (`BaseDetectionModule`, `BaseTurret`) plus `Joystick`, never `Camera` or `FrameStore`. Modes `ControlMode {Ai, User, Calibrate}` (wire names `ai|user|calibrate`). Button (`aiming::ButtonClassifier`): short press toggles Ai/User or records a calibration point, 1.5 s long press enters or cancels Calibrate. Jogging runs at 25 Hz without needing frames; detection runs in its own task in every mode; mode toggles call `detection.reset()`, which only sets a request flag.
- **Detection** (`lib/detection`; pure logic in `lib/jpeg_gray` and `lib/tracking`): JPEG -> `jpeg_gray` (1/8-scale DC-only decode to a 40x30 gray grid, one cell = 8x8 QVGA pixels; notes s2) -> `MotionDetector` -> `Tracker`. The decoder is the vendored C TJpgDec in `lib/tjpgd`, not the ROM copy (~40% slower); keep it at the default `-Os` (`-O2` measured 3x slower). `CameraDiffDetection` allocates its pipeline once in internal RAM, not PSRAM (~12.6 KB, faster; notes s2), and runs it on the "detect" task, woken by `FrameStore::publish`. The camera is fixed, so `detect_object`'s direction return is unused for aiming. `TestDetection` is a stub.
- **Aiming and movement** (`lib/aiming` pure, `lib/motion` device): `AimPolicy` maps the track position through the calibrated two-point `AimMap` to absolute pan/tilt (notes s1). `Turret` calls are non-blocking mailbox writes read by the motor task (notes s6). The calibration is saved in NVS (`cal_store`, namespace `aim`, key `map`, CRC-checked) and loaded in `Controller::begin()`; without one the HUD shows UNCALIBRATED.
- **Frame ownership** (`lib/frame_store`, pure bookkeeping in `lib/frames`; notes s4): the camera task copies each driver frame into one of 4 fixed 32 KB PSRAM slots and returns the driver buffer at once. Readers hold move-only RAII `FrameLease`s and a leased slot is never rewritten (the new frame is dropped and counted). No per-frame allocation on the frame path. PSRAM is mandatory. `FrameStore` wakes its (max 2) subscribers, stream and detect, with `xTaskNotifyGive` on every publish.

**Task layout** (core 1: camera copy, detection, control; core 0: network, motor, camera driver; notes s7). Constraint: httpd stays pinned to core 0, otherwise network bursts preempt detection. Rows in priority order; app task stacks are the `xTaskCreatePinnedToCore` and `httpd_config_t` values in code, `-` for IDF-owned tasks.

| Task | Core | Priority | Stack (B) |
|---|---|---|---|
| WiFi | 0 | 23 | - |
| `cam_task` (esp32-camera driver, `CONFIG_CAMERA_CORE0`) | 0 | 23 | - |
| esp_timer | 0 | 22 | - |
| `arduino_events` (WiFi events only) | 1 | 19 | - |
| lwIP | 0 | 18 | - |
| motor | 0 | 7 | 3072 |
| httpd (`esp_http_server`, default priority) | 0 | 5 | 8192 |
| camera copy | 1 | 4 | 4096 |
| stream | 1 | 4 | 4096 |
| detect | 1 | 3 | 4096 |
| `loop()` (control, stats) | 1 | 1 | - |

- **WiFi provisioning** (pure `lib/provisioning`, namespace `prov`; glue in `lib/network`; notes s11): up to 3 networks, the PIN and the hotspot password in NVS (namespace `wifi`, keys `nets`, `pin`, `appw`; CRC-checked). `prov::WifiPlan` is the state machine, `WifiManager` executes its actions. `begin()` returns at once and `maintain()` never blocks (async scans, WiFi events only set atomic flags, one mutex guards state shared with httpd). The hotspot `VoidSentry-XXXX` (192.168.4.1) opens with no saved network in range or after 120 s lost. A transient join failure (reason AUTH_EXPIRE) is retried twice on the same network before failing over, and the first Setup rescan after a failed join comes after 5 s, later ones every 30 s.
  - `wifi_http.cpp` invariants: every `/api/wifi/*` request needs an IPv4-literal Host (anti DNS rebinding) and POSTs are origin-checked; the catch-all `GET /*` must be registered last (`httpd_uri_match_wildcard`); a present but invalid `password` is a 400, only a missing key means an open network; POST bodies are capped at 256 B under a 3 s deadline.
  - One shared `PinGuard` (`pin_guard.h`): the 5th consecutive wrong PIN locks out for `min(60 s * 2^k, 1 h)`, k = lockouts since the last success. The lockout is RAM-only, so a reboot resets it. A first PIN is accepted only while the hotspot is up or nothing is saved.
  - Never log or return passwords or the PIN.
- **Network** (`lib/network`): `HttpServer` wraps ESP-IDF `esp_http_server` (not the Arduino WebServer); handlers are static C callbacks, so instances reach state through static members / `user_ctx`. `/commands` replies to every message with JSON; `remote::parse` (pure `lib/remote`) reads each message from a fixed 64 B buffer (a longer or non-text frame closes the session) and `RemoteInbox` keeps the latest Move (300 ms dead man, jog speed slow 0.25 / normal 0.6 / fast 1.0), the jog speed setting and one-shot mode, calibration (`cal:*`) and aim (`aim:x,y`) requests. The reply JSON is built by the pure `hud::format_reply` (`lib/hud`) into a stack buffer of `hud::kReplySize`, sized by `test_reply_json` for the worst case. `control_status.h` and `track_snapshot.h` live in `ESP32/include/` so native code can use them. `Controller::submit` runs on the httpd task and stores under its own spinlock.
  - HUD jogs (`move:<dir>`, resent every 100 ms) apply in User and Calibrate only (zero while the button is held in Calibrate), expire 300 ms after the last one, and lose to the joystick. `fire` is parsed but not acted on (the HUD never sends it). The HUD mode button is dropped (never deferred) during calibration.
  - HUD calibration: `cal:start|point|cancel` have the button's exact semantics (start = long press when idle, point = short press, cancel = long press while calibrating); `cal:point` records once the pan has stopped, at most 1 s later. A `cal:*` arriving on a pass where the button produced an event is dropped, and a knob record clears a pending HUD one. A zero tilt jog freezes the servo where it is (`Turret`), so a stop also ends an aim's tilt slew. `aim:x,y` (x 0..319, y 0..239; User/Calibrate only) goes through the active `AimMap`, cancels a HUD jog, and holds until a jog or the stick takes over (the 25 Hz idle jog of (0,0) must not cancel it). `speed:slow|normal|fast` sets the HUD jog magnitude. Malformed values are a Poll.
  - Reply: after the tracker and controller keys come `cal_saves`, `cal_miss` (counters, never one-shot flags: a 25 Hz poll can miss an edge), `stick`, `ax`,`ay` (inverse AimMap of the pose), `lim` (end-stop bitmask), `rx0..ry1` (reachable box), `home`, `ret`, `spd`, `vw`. `vw` is `StreamBroadcaster::viewer_count()` (`/stream` viewers, 0 to 2), read by the `/commands` handler in `websockets.h` when it builds the reply, not by the controller.
  - Handshake guard on `/commands` and `/stream` (`remote::origin_allowed`; notes s9): an Origin, when present, must equal `http(s)://` + Host, and Host must be an IPv4 literal. Non-browser LAN clients are not authenticated (known follow-up).
  - `/stream` is push: `StreamBroadcaster` queues one send per idle viewer (max 2) onto the httpd task; busy viewers skip frames. `close_fn` removes viewers and must close the socket. The HUD sends a 1-byte keepalive every 2 s (push-only viewers would be LRU-purged) and reconnects on close.
- **Stats lines**: `telemetry::log_stats` in `src/telemetry.cpp` (window arithmetic: `stats::WindowedCounter`) logs `[stream]`, `[detect]` and `[net]` every 5 s (separate lines because the ROM printf behind Arduino logging truncates a line after about `jpeg_b_avg`). Non-obvious fields:
  - `*_win_*`, `det_frames`, `jpeg_b_avg` cover the last 5 s window (0 if no frames); the since-boot `_avg` fields hide slowdowns.
  - `idle0`/`idle1`: percent of the window each core spent in its IDLE task, from the kernel run-time counters (`ulTaskGetIdleRunTimeCounterForCore` over wall time). Core 0 saturation shows here before the task watchdog (it watches IDLE0 only) reboots the board.
  - `[net]`: how long `/stream` sends block the httpd task, which also serves `/commands` (notes s13). `snd_max_ms` is the longest send in the window, `snd_slow` the sends over 200 ms, `snd_busy` the percent of the window spent sending, `snd_fail` sends that errored (the viewer is dropped). A `snd_max_ms` around 1000-1600 is a TCP retransmit timeout: the video froze for every viewer and HUD replies stalled.
  - `motor_late`: motor wake-ups lagging more than 1 tick. `*_stack_free`: high-water marks in bytes (`det_stack_free` sampled every 100 frames). `heap_min`/`psram_min` drifting down means a leak.
- **Tracking fixtures** (`ESP32/bench/fixtures/`): `<name>.gray` is concatenated 1200-byte frames (40x30, 8-bit). `<name>.expect` is `key=value` lines (`#` comments) with the keys `track_bench.cpp` parses; a malformed line fails the clip, a missing `.expect` only warns. `real_*.gray` are recorded on the board and committed; `tools/tracking/make_synthetic_clips.py` writes only the four synthetic ones (CI regenerates and `cmp`s them).
  - To add a real clip: `tools/tracking/record_stream.py`, then `jpeg_to_gray.py` (usage in their docstrings), then write its `.expect`.
  - Detector thresholds come from these clips (`tracking_types.h`, notes s5). Do not loosen them without re-running every clip.

## Pin constraints

`include/pins.h` is the only place GPIOs are defined (turret and camera). Its `static_assert`s reject flash/PSRAM pins (6-11, 16, 17), camera collisions, duplicates, non-ADC1 joystick axes and the stepper on GPIO0. The map is verified on hardware; don't change it without a reason.

- With the camera attached exactly 8 GPIOs are free (0, 2, 12, 13, 14, 15, 32, 33) and the turret uses all 8. The stepper sits on strapping pins 12/15/2, which is safe only because ULN2003 inputs pull low.
- **GPIO12 high at reset selects 1.8V flash**: boot loop with `invalid header: 0xffffffff`, uploads fail with `Wrong boot mode detected (0x2b)`. Seen when the ULN2003's `+`/`-` were reversed (it then energizes every coil and the motor gets hot). In the ROM's `boot:0x..` value, bit 5 is GPIO12, bit 4 is GPIO0.
- Wiring is on a breadboard via the Freenove GPIO Extension Board (USB 5V/GND on the left rails, 3.3V/GND on the right). Motors run from the left 5V rail.
- **Camera XCLK is 12 MHz, not 20 MHz** (`camera.h`): 20 MHz cripples WiFi. Measurements: notes s3.
- **JPEG quality is 18** (`camera.h`, was 12): bigger frames at 12 doubled the decode time and loaded WiFi. Watch `jpeg_b_avg`. Measurements: notes s3.

## Web UI pipeline

- Edit `web/web_ui.html` only. `ESP32/lib/network/include/index_html.h` is **generated and gitignored**: `web/update_index_html.py` (a PlatformIO `pre:` script) writes it via `web/html_header.py` before compilation, only when the content changed. Gzip uses `mtime=0` so the output is deterministic.
- The HUD never sends `fire:*`. `web/test_web_ui.py` checks the page has no external resources, balanced markup and only known command prefixes. The design reference is `docs/design/hud-mockup.html` (spec: `docs/superpowers/specs/2026-10-26-hud-ux-overhaul-design.md`).
- `web/compress_HTML_to_C_header.py` / `decompress_C_header_to_HTML.py` are standalone manual converters, not part of the build.
- **`/commands` is request/reply:** the HUD polls with `" "` and the board answers **every** message with one JSON reply. All HUD sends go through `App.sendCommand()`, and the next poll is scheduled only when no message is awaiting its reply, so exactly one poll loop runs. Never schedule a poll from a reply without that check: loops double every round trip and httpd saturates core 0 until the watchdog reboots the board (the Stage 4 poll storm, notes s8).
