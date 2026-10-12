# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Void Sentry: ESP32-CAM (Freenove ESP32-WROVER, OV3660) pan/tilt turret that tracks motion via frame differencing and serves a web HUD. Arduino framework on PlatformIO.

## Commands

All PlatformIO commands run from `ESP32/`. `pio` is not on PATH on this machine; use `~/.platformio/penv/Scripts/pio.exe` (or the VS Code PlatformIO extension).

```bash
pio run -e freenove_esp32_wrover          # build the turret firmware
pio run -e freenove_esp32_wrover -t upload
pio run -e pin_check -t upload            # wiring bring-up test (no camera/WiFi), read serial after
pio device monitor                        # serial @ 115200, exception decoder enabled
pio test -e native                        # host-side unit tests, no board needed
pio test -e native_sanitize               # ASan+UBSan, Linux/CI only
pio test -e freenove_esp32_wrover -f test_wifi_manager   # one on-device suite (board connected)
pio check --severity=high --severity=medium   # cppcheck
python tools/tracking/make_synthetic_clips.py ESP32/bench/fixtures   # regenerate synthetic clip fixtures (from repo root)
g++ -std=c++17 -O2 -g -Wall -Wextra -I ESP32/include -I ESP32/lib/tracking/include     ESP32/lib/tracking/src/*.cpp ESP32/bench/track_bench.cpp -o track_bench   # clip regression/bench, plain g++
./track_bench [--repeat N] ESP32/bench/fixtures/*.gray   # exits non-zero if any .expect fails
python -m unittest discover -s tools/tracking -p "test_*.py"   # fixture tool tests
```

- Envs: `freenove_esp32_wrover` (firmware, excludes `src/pin_check/`), `pin_check` (only `src/pin_check/`), `native` (host gcc, tests under `test/native/`), `native_sanitize` (same tests under ASan+UBSan; Linux/CI only). Device envs ignore `test/native/*`; `native` only runs it.
- **Language standard is C++17 everywhere** (firmware via `build_unflags = -std=gnu++11`, native, cppcheck locally and in CI). The firmware toolchain is GCC 8.4 (Arduino core 2.0.17), so C++20 features are not available; moving to C++20/23 means Arduino core 3.x via pioarduino, planned as its own stage.
- `native` uses the first `g++` on PATH and needs GCC >= 8. This machine has WinLibs GCC 14.2 + LLVM 19 (clang-format, clangd) from winget, but an old MinGW 6.3 at `C:\MinGW\bin` may still shadow it. Check `g++ --version` if native builds fail on C++17. Code under test there must not include Arduino/ESP-IDF headers.
- On-device suites under `ESP32/test/` are Unity sketches (`setup()` calls `UNITY_BEGIN()`), flashed and read over serial. Several are placeholders or stale: `test_motion/test_servo.cpp` (targets a `Turret::Servo` class that no longer exists) and `test_joystick/test_joystick.cpp` (passes 150 as the button pin and includes `"Joystick.h"` with the wrong case for Linux).
- Never pipe `pio` into something that exits early (`grep -m1`, `head`): on Windows pio blocks forever writing to the closed pipe and the build looks hung. Redirect to a log file and grep the file. A clean firmware build takes about 1 minute.
- Serial port is the CH340 (COM3 or COM4 - the number changes when the board is re-plugged; check `pio device list`). To read the board without resetting it, open the port with DTR/RTS held low.
- Formatting: `.clang-format` (LLVM base, Allman braces for functions/classes/control statements, 4-space indent, 120 columns, `int* p`), clang-format **19** locally (WinLibs LLVM) and in CI (`pip install "clang-format==19.1.*"`). A Claude Code `PostToolUse` hook (`.claude/hooks/format_cpp.py`) formats firmware C/C++ files on edit. CI (`ci.yml`) only checks with `--dry-run --Werror` and never commits to the branch.
- CI (`ci.yml`): `format`, `web-ui-generator`, `build` (both firmware envs, with `secrets.h` copied from the example), `native-tests`, `native-sanitize` and `tracking-bench` (fixture tool tests, fixtures regenerated and `cmp`ed byte-for-byte, clip regression under ASan+UBSan, cachegrind instructions/frame, `perf stat` best effort). `ci.yml` also has `workflow_dispatch` for manual runs. cppcheck stays in `static_analysis.yml`.
- CI static analysis (`static_analysis.yml`) runs cppcheck with `--error-exitcode=1 --std=c++17` and blocks on warning/performance/portability findings. Use `// cppcheck-suppress <id>` inline when a finding is intentional.
- Doxygen (`ESP32/Doxyfile`) publishes to `gh-pages` on push to `main`. Public headers use Doxygen `@brief`/`@details` blocks; keep that style for new APIs.
- Board logs: Arduino's logger only shows ESP_LOGx from files that include `esp32-hal-log.h` (or Arduino.h); pure ESP-IDF files must include it or their logs are silently dropped.

## Sanitizers and profiling

Use them throughout the project, on the target where each one actually works:

- **ASan + UBSan, host only.** Any logic that can be native-tested must also pass under `-fsanitize=address,undefined -fno-omit-frame-pointer` (the `native_sanitize` env, run as a CI job on Ubuntu). New pure-logic modules are designed so they can run there. Never weaken a test to silence a sanitizer report; fix the bug.
- **Not on Windows.** The MinGW toolchains here (WinLibs GCC 14 and Clang 19) ship no `libasan` or `libubsan` (`cannot find -lasan`, verified). **Decision: CI is the sanitizer and host-benchmark runner.** No local WSL/Linux setup, so don't suggest installing one. Push and read the CI job output instead.
- **Not on the ESP32.** There's no ASan for Xtensa. On-device memory safety is covered by design (no per-frame allocation, bounded buffers) and by watching `heap_min` and `psram_min` in the serial stats line.
- **`perf`, host only (Linux, so it runs in CI).** It can't profile the ESP32. GitHub's shared runners make absolute timings noisy, so compare runs within the same job, or use instruction counts (`perf stat -e instructions`) rather than wall time. Use it for host-side benchmarks of algorithm code (for example Stage 2 frame-diff detection run on recorded frames): `perf stat` for totals, `perf record -g` + `perf report` for hot spots, built with `-O2 -g -fno-omit-frame-pointer`.
- **On-device profiling:** time code sections with `esp_timer_get_time()` (microseconds), use `ESP.getCycleCount()` for very short sections, and report per-stage averages in the periodic stats line rather than logging per frame (logging itself distorts timing).
- **Benchmarks are reproducible:** each perf-relevant change records its method and numbers (fps, latency median, `heap_min`) in the PR description.

## Required local setup

- `ESP32/include/secrets.h` is gitignored and **must exist** to build the firmware env: copy `include/secrets.h.example` and set `WIFI_SSID` / `WIFI_PASSWORD`. Never commit it, read it, or print its contents. This is enforced: `.claude/settings.json` denies Read/Edit on `**/secrets.h`, and `.claude/hooks/guard_wifi_credentials.py` blocks any shell command containing the substring `secret` (so ask the user for anything involving that file, including copying the template; put commit messages that mention it in a file and use `git commit -F`). The hook is an accident guard, not a security boundary. On-device WiFi provisioning (SoftAP setup page + NVS) was split out of Stage 1 into its own later design; `secrets.h` stays until then.
- The project's Claude Code hooks (`.claude/settings.json`) invoke `python`; on a machine that only has `python3` they exit 127, which fails open (the WiFi-credential shell guard then does nothing).

## Architecture

Frames are produced by a camera FreeRTOS task and pushed to viewers by a stream task. `loop()` runs `controller.run()` → `WifiManager::maintain()` → `log_stream_stats()` and yields 1 tick each pass. All objects are globals constructed in `src/main.cpp` and wired by reference (manual dependency injection).

- **`Controller`** (`lib/controller`) depends only on interfaces: `BaseDetectionModule` and `BaseMovementManager`, plus `Joystick` and `FrameStore` (not `Camera`). Joystick Z-press toggles `SystemControl` between `AI_MODE` (default) and `USER_MODE`. `USER_MODE` runs at 25 Hz without needing frames; `AI_MODE` runs once per new frame sequence.
- **Detection and tracking**: JPEG frames go `jpeg_gray` (1/8-scale decode to 40x30 gray: DC-only, no IDCT, ~7 ms measured versus ~71 ms at 1/4 scale, so one cell = 8x8 QVGA pixels) -> `tracking::MotionDetector` -> `tracking::Tracker`. The `lib/tracking` library is pure (no Arduino headers) and natively tested. `CameraDiffDetection` (`lib/detection`, implements `BaseDetectionModule`) is the glue: a PSRAM pipeline that runs decode + detect + track and publishes a track snapshot under a spinlock, which `/commands` reads for the HUD. Output is a *direction* (Left/Right/None, Up/Down/None), not coordinates; the movement layer steps relatively. AI mode does not move the turret until Stage 3 (`Controller::kAiAimEnabled = false`). `TestDetection` is a stub.
- **Movement** (`lib/motion`): `MovementManager` drives a 28BYJ-style stepper (yaw) and a servo (pitch) from `move_relative()`.
- **Camera frame ownership**: a camera FreeRTOS task (`lib/frame_store/src/camera_task.cpp`, core 1, prio 2) copies each driver frame into one of 4 fixed 32 KB PSRAM slots in `FrameStore` and returns the driver buffer immediately. Bookkeeping is the pure `frames::FrameSlots` (natively tested); readers hold move-only RAII `frames::FrameLease`s, and a leased slot is never rewritten (a new frame is dropped and counted instead). No per-frame allocation anywhere on the frame path. Frames are **JPEG** (needed for streaming), QVGA, `CAMERA_GRAB_LATEST`; incomplete JPEGs (no FF D8 ... FF D9) are dropped as `corrupt`. PSRAM is mandatory.
- **Network** (`lib/network`): `HttpServer` wraps ESP-IDF `esp_http_server` (not the Arduino WebServer). Endpoints: `/` (gzipped HUD page), `/stream` WebSocket, `/commands` WebSocket (replies to every message with JSON motion metrics). The HUD already sends `move:<dir>`, `move:stop`, `fire:on|off`, `mode:<m>` on `/commands`, but the firmware does not parse them yet, so the HUD's manual controls are not wired to the motors. Handlers are static C callbacks, so instances reach state through static members / `user_ctx`. `/stream` is a push WebSocket: `StreamBroadcaster` wakes on each published frame and, for each idle viewer (max 2, `frames::ViewerSet`), queues one send onto the httpd task (`httpd_queue_work` + `httpd_ws_send_frame_async`); busy viewers skip frames, which keeps latency low. `close_fn` removes viewers and must close the socket. WiFi modem sleep is disabled for latency, httpd `send_wait_timeout` is 2 s, and the HUD sends a 1-byte keepalive on `/stream` every 2 s (push-only viewers would otherwise be LRU-purged) and reconnects on close.
- `move_types.h` / `system_control_types.h` in `ESP32/include/` are shared value types (`MoveX`, `MoveY` wrap `enum class` with `to_string()` and implicit `uint8_t` conversion).
- **Benchmark stats**: `main.cpp` logs `[stream] up= captured= sent= dropped_no_slot= oversize= corrupt= viewers= copy_us_avg= heap= heap_min= psram= psram_min= decode_us_avg= track_us_avg= scene_changes= decode_fail= track= id=` every 5 s (`decode_us_avg`/`track_us_avg` are per-frame averages in microseconds, `track` is the tracker phase); `heap_min`/`psram_min` drifting down means a leak.
- **Tracking fixtures** (`ESP32/bench/fixtures/`): `<name>.gray` is concatenated 1200-byte frames (40x30, row-major, 8-bit gray). `<name>.expect` is `key=value` lines (`#` comments) with optional keys `lock_by_frame`, `max_ids`, `min_locked_frames`, `min_scene_changes`, `max_scene_changes`, `no_track_after_frame=N` (tracker Idle on every frame from N to the end - the ghost check); a malformed line fails the clip and a missing `.expect` only warns; `track_bench` fails when an expectation is violated. `seed_object` and `pause_leave` are the ghost clips: the detector's `fg_age_limit = 34` (a cell foreground for more than 34 consecutive frames goes back to the fast background rate) makes ghosts of a wrong background clear in ~3 s instead of ~14 s. The synthetic clips are generated by `tools/tracking/make_synthetic_clips.py` (deterministic, CI checks they are up to date). Record real clips with `tools/tracking/record_stream.py <board_ip> <seconds> <dir>` (needs `pip install websockets`) then `tools/tracking/jpeg_to_gray.py <dir> <out.gray>` (needs Pillow), and write a `.expect` for them.

## Pin constraints

`include/pins.h` is the only place GPIOs are defined (turret and camera). Its `static_assert`s reject flash/PSRAM pins (6-11, 16, 17), camera collisions, duplicates, non-ADC1 joystick axes and the stepper on GPIO0. The map is verified on hardware; don't change it without a reason.

- With the camera attached exactly 8 GPIOs are free (0, 2, 12, 13, 14, 15, 32, 33) and the turret uses all 8. The stepper sits on strapping pins 12/15/2, which is safe only because ULN2003 inputs pull low.
- **GPIO12 high at reset selects 1.8V flash** -> boot loop with `invalid header: 0xffffffff`, and uploads fail with `Wrong boot mode detected (0x2b)`. Seen on this build when the ULN2003's `+`/`-` were reversed (it then leaks current out of its IN pins and energizes every coil, so the motor gets hot). In the ROM's `boot:0x..` value, bit 5 is GPIO12, bit 4 is GPIO0.
- Wiring is on a breadboard via the Freenove GPIO Extension Board, which feeds USB 5V/GND to the left rails and 3.3V/GND to the right rails. Motors run from the left 5V rail.
- **Camera XCLK is 12 MHz, not the usual 20 MHz.** Measured on this board 2 m from the router (ping avg / 14 KB page fetch / capture fps): 20 MHz = 265 ms / 1.1-5.0 s / 27.8 fps (WiFi crippled); 16 MHz = 27 ms / 0.1-2.4 s erratic / 17.8 fps; 12 MHz = 19 ms / 0.07-0.13 s / 16.6 fps; 10 MHz = 17 ms / 0.14-0.19 s / 13.8 fps; camera off = 10 ms / 0.07 s. Higher XCLK means more camera clock/DMA activity that degrades WiFi; 12 MHz is the best fps that keeps WiFi healthy. Also: DHCP may give the board a new IP after reflashing - read the IP from the boot log, never assume it.

## Web UI pipeline

- Edit `web/web_ui.html` only. `ESP32/lib/network/include/index_html.h` is **generated and gitignored**: `web/update_index_html.py` (a PlatformIO `pre:` script) runs before compilation and writes it via `web/html_header.py`, only when the content changed. Gzip uses `mtime=0` so the output is deterministic.
- `python -m unittest discover -s web -p "test_*.py"` tests the generator (also run in CI).
- `web/compress_HTML_to_C_header.py` / `decompress_C_header_to_HTML.py` are standalone manual converters, not part of the build.
