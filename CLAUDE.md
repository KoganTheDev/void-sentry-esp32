# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Void Sentry: ESP32-CAM (Freenove ESP32-WROVER, OV3660) pan/tilt turret that tracks motion via frame differencing and serves a web HUD. Arduino framework on PlatformIO.

## Commands

All PlatformIO commands run from `ESP32/`. `pio` is not on PATH on this machine; use `~/.platformio/penv/Scripts/pio.exe` (or the VS Code PlatformIO extension).

```bash
pio run                                   # build (single env: freenove_esp32_wrover)
pio run -t upload                         # flash
pio device monitor                        # serial @ 115200, exception decoder enabled
pio test -f test_joystick                 # run one test suite (folder name under ESP32/test/)
pio check --severity=high --severity=medium   # cppcheck, same as CI
```

- **Tests run on the device, not natively.** Every suite under `ESP32/test/` is a Unity sketch (`setup()` calls `UNITY_BEGIN()`); `pio test` builds, flashes and reads results over serial, so a board must be connected. There is no `native` env.
- Formatting: `.clang-format` (LLVM base, Allman braces for functions/classes/control statements, 4-space indent, 120 columns, `int* p`). CI runs `clang-format -i` on `ESP32/src`, `ESP32/include`, `ESP32/lib` and **auto-commits the result back to the branch** ("GITHUB-ACTIONS: auto-formmated code"). Pull after pushing, or format locally first to avoid the extra commit.
- CI static analysis (`static_analysis.yml`) runs cppcheck with `--error-exitcode=1` and blocks on warning/performance/portability findings. Use `// cppcheck-suppress <id>` inline when a finding is intentional.
- Doxygen (`ESP32/Doxyfile`) publishes to `gh-pages` on push to `main`. Public headers use Doxygen `@brief`/`@details` blocks; keep that style for new APIs.

## Required local setup

- `ESP32/include/secrets.h` is gitignored and **must exist** to build: it defines `WIFI_SSID` and `WIFI_PASSWORD` (used by `main.cpp` and the wifi/server tests). Never commit it or print its contents.

## Architecture

Single-threaded Arduino `loop()`: `camera.capture()` → `controller.run()` → `WifiManager::maintain()`. All objects are globals constructed in `src/main.cpp` and wired by reference (manual dependency injection).

- **`Controller`** (`lib/controller`) depends only on interfaces: `BaseDetectionModule` and `BaseMovementManager`, plus `Joystick` and `Camera`. Joystick Z-press toggles `SystemControl` between `AI_MODE` (default) and `USER_MODE`. Note: `run()` currently calls `detect_object()` every frame even in `USER_MODE` and then overwrites the result with joystick input (known, marked TODO).
- **Detection** (`lib/detection`): implement `BaseDetectionModule::detect_object(camera_buffer_t) -> std::tuple<MoveX, MoveY>` and optionally `get_motion_data()` (feeds the web HUD). `CameraDiffDetection` is the real implementation; `TestDetection` is a stub. Output is a *direction* (Left/Right/None, Up/Down/None), not coordinates; the movement layer steps relatively.
- **Movement** (`lib/motion`): `MovementManager` drives a 28BYJ-style stepper (yaw) and a servo (pitch) from `move_relative()`.
- **Camera frame ownership**: `Camera::capture()` copies the driver frame (`esp_camera_fb_get`) into a heap buffer it owns, immediately returns the driver buffer, and frees the previous copy. Consumers get a `const camera_buffer_t&` via `get_frame_buffer()` that is only valid until the next `capture()`. Frames are **JPEG** (needed for streaming), QVGA, 2 frame buffers in PSRAM, `CAMERA_GRAB_LATEST`. PSRAM is mandatory (`-DBOARD_HAS_PSRAM`; `begin()` fails without it).
- **Network** (`lib/network`): `HttpServer` wraps ESP-IDF `esp_http_server` (not the Arduino WebServer). Endpoints: `/` (gzipped HUD page), `/stream` WebSocket (binary JPEG of the latest frame, sent per client request), `/commands` WebSocket (replies to every message with JSON motion metrics). The HUD already sends `move:<dir>`, `move:stop`, `fire:on|off`, `mode:<m>` on `/commands`, but the firmware does not parse them yet, so the HUD's manual controls are not wired to the motors. Handlers are static C callbacks, so instances reach state through static members / `user_ctx`. `websockets.h` is header-only (TODO to split).
- `move_types.h` / `system_control_types.h` in `ESP32/include/` are shared value types (`MoveX`, `MoveY` wrap `enum class` with `to_string()` and implicit `uint8_t` conversion).

## Pin constraints

`include/constants.h` (joystick, servo, stepper) and the `WroverPins` macros in `lib/detection/include/camera.h` share one GPIO budget. The camera bus occupies 4, 5, 18, 19, 21-23, 25-27, 34-36, 39. Avoid strapping pins 2 and 12. Check both files before reassigning any pin. (`constants.h` currently has GPIO 33 on both `JOYSTICK_PIN_Y` and `STEPPER_PIN2`.)

## Web UI pipeline

- Edit `web/web_ui.html` only. `web/update_index_html.py` is a PlatformIO `pre:` extra script that gzips it on every build and regex-replaces the byte array and `HTML_PAGE_GZ_LEN` in `ESP32/lib/network/include/index_html.h`. Never hand-edit the array in `index_html.h`.
- `index_html.h` is marked `git update-index --assume-unchanged` locally, so its regenerated contents don't show in `git status`. To commit a new UI build, run `git update-index --no-assume-unchanged ESP32/lib/network/include/index_html.h` first.
- `web/compress_HTML_to_C_header.py` / `decompress_C_header_to_HTML.py` are standalone manual converters, not part of the build.
