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
pio test -e freenove_esp32_wrover -f test_joystick   # one on-device suite (board connected)
pio check --severity=high --severity=medium   # cppcheck
```

- Envs: `freenove_esp32_wrover` (firmware, excludes `src/pin_check/`), `pin_check` (only `src/pin_check/`), `native` (host gcc, tests under `test/native/`). Device envs ignore `test/native/*`; `native` only runs it.
- **Language standard is C++17 everywhere** (firmware via `build_unflags = -std=gnu++11`, native, cppcheck locally and in CI). The firmware toolchain is GCC 8.4 (Arduino core 2.0.17), so C++20 features are not available; moving to C++20/23 means Arduino core 3.x via pioarduino, planned as its own stage.
- `native` uses the first `g++` on PATH and needs GCC >= 8. This machine has WinLibs GCC 14.2 + LLVM 19 (clang-format, clangd) from winget, but an old MinGW 6.3 at `C:\MinGW\bin` may still shadow it. Check `g++ --version` if native builds fail on C++17. Code under test there must not include Arduino/ESP-IDF headers.
- On-device suites under `ESP32/test/` are Unity sketches (`setup()` calls `UNITY_BEGIN()`), flashed and read over serial. Several are placeholders or stale (`test_motion/test_servo.cpp` targets a `Turret::Servo` class that no longer exists).
- Never pipe `pio` into something that exits early (`grep -m1`, `head`): on Windows pio blocks forever writing to the closed pipe and the build looks hung. Redirect to a log file and grep the file. A clean firmware build takes about 1 minute.
- Serial port is COM3 (CH340). To read the board without resetting it, open the port with DTR/RTS held low.
- Formatting: `.clang-format` (LLVM base, Allman braces for functions/classes/control statements, 4-space indent, 120 columns, `int* p`). CI runs `clang-format -i` on `ESP32/src`, `ESP32/include`, `ESP32/lib` and **auto-commits the result back to the branch** ("GITHUB-ACTIONS: auto-formmated code"). Pull after pushing, or format locally first to avoid the extra commit.
- CI static analysis (`static_analysis.yml`) runs cppcheck with `--error-exitcode=1 --std=c++17` and blocks on warning/performance/portability findings. Use `// cppcheck-suppress <id>` inline when a finding is intentional.
- Doxygen (`ESP32/Doxyfile`) publishes to `gh-pages` on push to `main`. Public headers use Doxygen `@brief`/`@details` blocks; keep that style for new APIs.

## Required local setup

- `ESP32/include/secrets.h` is gitignored and **must exist** to build the firmware env: copy `include/secrets.h.example` and set `WIFI_SSID` / `WIFI_PASSWORD`. Never commit it, read it, or print its contents. This is enforced: `.claude/settings.json` denies Read/Edit on `**/secrets.h`, and `.claude/hooks/guard_wifi_credentials.py` blocks any shell command containing the substring `secret` (so ask the user for anything involving that file, including copying the template; put commit messages that mention it in a file and use `git commit -F`). The hook is an accident guard, not a security boundary. Planned: replace it with on-device WiFi provisioning (SoftAP setup page + NVS) in Stage 1.

## Architecture

Single-threaded Arduino `loop()`: `camera.capture()` → `controller.run()` → `WifiManager::maintain()`. All objects are globals constructed in `src/main.cpp` and wired by reference (manual dependency injection).

- **`Controller`** (`lib/controller`) depends only on interfaces: `BaseDetectionModule` and `BaseMovementManager`, plus `Joystick` and `Camera`. Joystick Z-press toggles `SystemControl` between `AI_MODE` (default) and `USER_MODE`. Note: `run()` currently calls `detect_object()` every frame even in `USER_MODE` and then overwrites the result with joystick input (known, marked TODO).
- **Detection** (`lib/detection`): implement `BaseDetectionModule::detect_object(camera_buffer_t) -> std::tuple<MoveX, MoveY>` and optionally `get_motion_data()` (feeds the web HUD). `CameraDiffDetection` is the real implementation; `TestDetection` is a stub. Output is a *direction* (Left/Right/None, Up/Down/None), not coordinates; the movement layer steps relatively.
- **Movement** (`lib/motion`): `MovementManager` drives a 28BYJ-style stepper (yaw) and a servo (pitch) from `move_relative()`.
- **Camera frame ownership**: `Camera::capture()` copies the driver frame (`esp_camera_fb_get`) into a heap buffer it owns, immediately returns the driver buffer, and frees the previous copy. Consumers get a `const camera_buffer_t&` via `get_frame_buffer()` that is only valid until the next `capture()`. Frames are **JPEG** (needed for streaming), QVGA, 2 frame buffers in PSRAM, `CAMERA_GRAB_LATEST`. PSRAM is mandatory (`-DBOARD_HAS_PSRAM`; `begin()` fails without it).
- **Network** (`lib/network`): `HttpServer` wraps ESP-IDF `esp_http_server` (not the Arduino WebServer). Endpoints: `/` (gzipped HUD page), `/stream` WebSocket (binary JPEG of the latest frame, sent per client request), `/commands` WebSocket (replies to every message with JSON motion metrics). The HUD already sends `move:<dir>`, `move:stop`, `fire:on|off`, `mode:<m>` on `/commands`, but the firmware does not parse them yet, so the HUD's manual controls are not wired to the motors. Handlers are static C callbacks, so instances reach state through static members / `user_ctx`. `websockets.h` is header-only (TODO to split).
- `move_types.h` / `system_control_types.h` in `ESP32/include/` are shared value types (`MoveX`, `MoveY` wrap `enum class` with `to_string()` and implicit `uint8_t` conversion).

## Pin constraints

`include/pins.h` is the only place GPIOs are defined (turret and camera). Its `static_assert`s reject flash/PSRAM pins (6-11, 16, 17), camera collisions, duplicates, non-ADC1 joystick axes and the stepper on GPIO0. The map is verified on hardware; don't change it without a reason.

- With the camera attached exactly 8 GPIOs are free (0, 2, 12, 13, 14, 15, 32, 33) and the turret uses all 8. The stepper sits on strapping pins 12/15/2, which is safe only because ULN2003 inputs pull low.
- **GPIO12 high at reset selects 1.8V flash** -> boot loop with `invalid header: 0xffffffff`, and uploads fail with `Wrong boot mode detected (0x2b)`. Seen on this build when the ULN2003's `+`/`-` were reversed (it then leaks current out of its IN pins and energizes every coil, so the motor gets hot). In the ROM's `boot:0x..` value, bit 5 is GPIO12, bit 4 is GPIO0.
- Wiring is on a breadboard via the Freenove GPIO Extension Board, which feeds USB 5V/GND to the left rails and 3.3V/GND to the right rails. Motors run from the left 5V rail.

## Web UI pipeline

- Edit `web/web_ui.html` only. `ESP32/lib/network/include/index_html.h` is **generated and gitignored**: `web/update_index_html.py` (a PlatformIO `pre:` script) runs before compilation and writes it via `web/html_header.py`, only when the content changed. Gzip uses `mtime=0` so the output is deterministic.
- `python -m unittest discover -s web -p "test_*.py"` tests the generator (also run in CI).
- `web/compress_HTML_to_C_header.py` / `decompress_C_header_to_HTML.py` are standalone manual converters, not part of the build.
