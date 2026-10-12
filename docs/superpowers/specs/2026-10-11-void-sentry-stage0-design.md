# Void Sentry rebuild: roadmap and Stage 0 design

- **Date:** 2026-10-11
- **Status:** Stage 0 in progress on `feature/stage0-foundation`
- **Goal:** bring Void Sentry back to a working, well-tested state, one subsystem at a time. It's both a portfolio piece and a learning project, so each stage should leave clean, tested code that can be explained in an interview.

## Context

The last code change was in April 2026. When work resumed, the project was broken in several ways at once:

- **Pin map:** it drove the stepper on GPIO 16/17, which is the WROVER's PSRAM, and assigned GPIO 33 twice.
- **Detection:** the whole JPEG-to-greyscale and frame-diff pipeline was commented out, so the turret never tracked.
- **Memory:** every frame was copied with `malloc` and the result was never checked.
- **HUD:** it sends commands the firmware never parses.
- **Tests:** placeholders, plus one stale suite.
- **Secrets:** `secrets.h` was missing, so the firmware didn't build.

## Roadmap

The order is bottom-up. Each stage depends only on the stages before it, so a regression has a single new variable, and the order matches wiring one hardware part at a time. Each stage gets its own spec, plan and build cycle.

| Stage | Scope | Demo at the end |
|---|---|---|
| 0. Foundation | Verified pin map, C++17, native test env, `secrets.h` guard, formatting, CI | `pio test -e native` and CI green; wiring verified with `pin_check` |
| 1. Camera + stream | Frame ownership without allocating per frame; `/stream` to the HUD; on-device WiFi provisioning (SoftAP setup page + NVS) that replaces `secrets.h` | Live video in the browser, WiFi set up from a phone |
| 2. Detection | Choose the JPEG-to-greyscale strategy (decoding at 1/8 scale is the leading option); restore the frame diff; test it natively on recorded frames | Motion box drawn on the HUD |
| 3. Motion | Stepper and servo limits, calibration, coil release when idle | Turret moves on command |
| 4. Control loop | Controller modes, joystick, parsing HUD `move:` and `mode:` commands; camera/stream and control on separate FreeRTOS tasks (dual-core) | Manual and automatic tracking end to end |
| 5. Portfolio polish | README with a demo GIF, architecture diagram, CI badges, write-up | Something worth linking on a CV |
| Later | Toolchain upgrade: Arduino core 3.x via pioarduino, C++20/23 | Same behaviour on the new toolchain |

## Stage 0 design

### Hardware and pin map (done)

The parts are a Freenove ESP32-WROVER CAM on the Freenove GPIO Extension Board, a 28BYJ-48 stepper with a ULN2003 driver (yaw), an SG90 servo (pitch), and a 2-axis joystick with a push button. With the camera attached, exactly 8 GPIOs are free, and the turret uses all of them:

| Signal | GPIO | Reason |
|---|---|---|
| Joystick X / Y | 32 / 33 | ADC1, which keeps working while WiFi is on |
| Joystick button | 0 | Pulled up; it's also the BOOT button |
| Servo | 14 | Not a strapping pin |
| Stepper IN1-IN4 | 13, 12, 15, 2 | 12, 15 and 2 are strapping pins. That's safe only because ULN2003 inputs pull low |

- **One source of truth:** `ESP32/include/pins.h` holds every GPIO, including the camera bus. `static_assert`s reject flash and PSRAM pins (6-11, 16, 17), collisions with the camera, duplicates, joystick axes that aren't on ADC1, and the stepper on GPIO 0.
- **Verified on hardware:** the `pin_check` environment tests each part on its own. Boot, PSRAM, stepper, servo, joystick axes and button all pass, and uploads and cold boots work with everything connected.
- **Known failure mode:** if the ULN2003's `+` and `-` are reversed, it leaks current out of its IN pins. GPIO 12 is then high at reset, which selects 1.8V flash, and the board boot-loops with `invalid header: 0xffffffff`. This is documented in CLAUDE.md and on the wiring sheet.

### Language and toolchain (done)

- **C++17 everywhere:** firmware (`build_unflags = -std=gnu++11`, GCC 8.4), the native tests, and cppcheck both locally and in CI.
- **Host compiler:** WinLibs GCC 14.2 with LLVM 19, which also provides `clang-format` and `clangd`. The old MinGW 6.3 was removed from PATH.

### WiFi credentials (done for now)

- **Template:** `include/secrets.h.example` is committed. The real `secrets.h` is gitignored.
- **Claude Code guard:** Read and Edit deny rules on `**/secrets.h`, plus a `PreToolUse` hook that blocks shell commands containing `secret`. The hook guards against accidents; it isn't a security boundary.
- **Replacement:** Stage 1's on-device provisioning removes `secrets.h` entirely.

### Native tests (done)

- **`[env:native]`** runs Unity tests on the host (`pio test -e native`). It covers `MoveX`/`MoveY`, `SystemControl` and the pin checker.
- **Rule:** code under native test must not include Arduino or ESP-IDF headers. Later stages move logic behind interfaces so it can be tested here.

### Formatting (to do)

- **Format on edit:** a Claude Code `PostToolUse` hook runs `clang-format -i` on each C/C++ file edited under `ESP32/` (`src`, `include`, `lib`, `test`).
- **CI checks, it doesn't rewrite:** `clang-format --dry-run --Werror` replaces the auto-commit job.
- **Same version everywhere:** CI pins clang-format 19 with `pip install "clang-format==19.1.*"`, matching the local LLVM 19.
- **One-time reformat:** a single commit reformats the whole codebase so the check starts green.

### CI (to do)

- **Builds:** `freenove_esp32_wrover` and `pin_check`. CI copies the template to `secrets.h` first, so the build has placeholder values.
- **Tests:** `pio test -e native` on the Ubuntu runner's GCC.
- **Static analysis:** keep `static_analysis.yml` (cppcheck with `--error-exitcode=1`) and remove the duplicate `pio check` from the lint workflow.
- **Formatting:** the check-only job above.

### Generated web UI header (to do)

- **Generate it whole:** `web/update_index_html.py` writes the entire `index_html.h` from `web/web_ui.html` (gzip, byte array, length), instead of regex-patching a committed file.
- **Stop tracking it:** `index_html.h` becomes gitignored and is removed from git. CI builds generate it the same way.

### Removed

- **SD card:** the `lib/storage` library and its test are deleted. They were never used, and there are no free GPIOs for an SD card.

### Definition of done

- [x] Wiring verified with `pin_check`
- [x] `pins.h` holds every GPIO, with compile-time checks
- [x] Firmware builds in C++17; SD card code removed; `secrets.h` template and guard in place
- [ ] Format hook active; one-time reformat committed
- [ ] CI builds both firmware envs, runs native tests, runs cppcheck, and checks formatting, all green on the PR
- [ ] `index_html.h` generated at build time, not tracked
- [ ] PR `feature/stage0-foundation` to `main` reviewed and merged

## Non-goals for Stage 0

- **Product behaviour:** the camera, detection, motor logic, controller and HUD commands all belong to Stages 1-4.
- **The stale on-device test** (`test_motion/test_servo.cpp`) stays until Stage 3 replaces it.
- **Toolchain upgrade:** Arduino core 3.x and C++20/23 are a later stage.
