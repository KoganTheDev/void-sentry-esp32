# Stage 7 Toolchain Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Move the firmware from Arduino core 2.0.17 (ESP-IDF 4.4, GCC 8.4, C++17) to Arduino core 3.3.12 (pioarduino 55.03.312, ESP-IDF 5.5.5, GCC 14) and C++20, with no regression, then use C++20 where it improves existing code.

**Architecture:** Side by side first (a second env on core 3; one compat header), A/B on the board, then cut over in the same branch (one env, C++20 everywhere), then targeted C++20 adoption in the pure libraries.

**Tech Stack:** PlatformIO, pioarduino platform-espressif32 55.03.312, ESP-IDF 5.5.5, GCC 14, Unity, cppcheck, clang-format 19.

**Spec:** `docs/superpowers/specs/2026-10-17-void-sentry-stage7-core3-cpp20-design.md`

## Global Constraints

- **Platform URL (exact):** `platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip`. Never a branch or `latest`.
- **Until cut-over (Tasks 1-2):** every source compiles on both envs as C++17; API differences only in `ESP32/include/platform_compat.h` under `#if ESP_ARDUINO_VERSION_MAJOR >= 3` (include `esp_arduino_version.h`). No `#if` on the core version anywhere else.
- **After cut-over (Tasks 3-4):** C++20 (`gnu++20` firmware, `gnu++20` native, `--std=c++20` cppcheck), one firmware env named `freenove_esp32_wrover`.
- **Behaviour is unchanged.** No new features. Existing native tests must keep passing unmodified except where an interface change (Task 4) forces a call-site edit; never weaken a test.
- **Credentials:** never read, print, copy or edit `ESP32/include/secrets.h` or its `.example`, and never run a shell command containing the substring "secret" (a hook blocks it). Never log or print WiFi passwords, the PIN or the hotspot password in reports.
- **`pio`:** `~/.platformio/penv/Scripts/pio.exe`, from `ESP32/`. Never pipe it into `head` / `grep -m1`; redirect to a log file. First core 3 build downloads the platform and toolchain (several hundred MB); allow time.
- **Native env:** WinLibs GCC 14 first on PATH: `export PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin | head -1):$PATH"`.
- **Style:** no em dashes; device classes leading underscore + `this->`; Doxygen `@brief`; clang-format 19 clean.
- **Commits:** trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Board:** implementers do not upload; the controller runs the board.

## Review Focus

1. **Camera driver duplication:** the v3 env must not link both the bundled esp32-camera and the `espressif/esp32-camera` lib_dep (duplicate symbols or, worse, two different driver versions silently mixed).
2. **Task affinity and priorities unchanged:** `CONFIG_CAMERA_CORE0`, httpd `core_id = 0`, motor prio 7, camera/stream prio 4, detect prio 3 must hold on core 3 (check the camera task's actual core; sdkconfig defaults may differ).
3. **Disconnect reasons and event payloads:** the WiFi join-failure mapping (AUTH_FAIL, NO_AP_FOUND, ...) must still classify correctly with IDF 5 reason codes.
4. **Flash layout:** if the partition table changes, NVS must stay at `0x9000` (size `0x5000`) so saved networks and calibration survive.
5. **Watchdog and idle telemetry:** IDF 5 task-watchdog defaults and the renamed `...ForCore` APIs must keep `idle0`/`idle1` meaningful and the IDLE0 watchdog active.

### Task 1: core 3 env side by side

**Files:** `ESP32/platformio.ini` (new env `freenove_esp32_wrover_v3` extending the firmware env: platform URL above, no `espressif/esp32-camera` lib_dep, `board_build.partitions = min_spiffs.csv` only if needed for >= 15% flash headroom), create `ESP32/include/platform_compat.h`, any source that needs a compat call; `.github/workflows/ci.yml` (build job also builds `freenove_esp32_wrover_v3`; cache the new platform).

- [ ] Build v3, fix every error through `platform_compat.h` (each wrapper with a one-line comment naming the API difference). Keep core 2 building with zero new warnings.
- [ ] Check Review Focus 1, 2, 4 and 5 from the build config, sdkconfig and map file; report the camera task core setting, `max_uri_handlers`/httpd stack defaults, task WDT settings and both envs' RAM/flash.
- [ ] Verify: both firmware envs and pin_check build SUCCESS; native suite passes; cppcheck clean on the default env; clang-format clean. Commit "Add Arduino core 3.3.12 env side by side (pioarduino 55.03.312)".

### Task 2 (controller): A/B on the board

- [ ] Flash core 2, 5 min stats with one viewer (PC WebSocket client), ping and 14 KB page fetch; flash core 3, same. Then functional checks (spec). Record numbers in the ledger; any gate failure goes back to Task 1 as a fix.

### Task 3: cut-over to one env and C++20

**Files:** `ESP32/platformio.ini` (v3 settings become `freenove_esp32_wrover`; delete the old env and `_v3`; `build_unflags` the core's default standard flag (check what core 3 passes, likely `-std=gnu++2b`) and set `-std=gnu++20`; native and native_sanitize `-std=gnu++20`; cppcheck `--std=c++20`), delete `ESP32/include/platform_compat.h` and inline its v3 branch, `.github/workflows/ci.yml` and `static_analysis.yml` (`--std=c++20`, one firmware env), `.clang-format` (`Standard: c++20`), `CLAUDE.md` (toolchain, C++20, platform URL, remove the "C++20 planned" notes).

- [ ] Verify: firmware + pin_check builds, native, cppcheck, clang-format. Commit "Cut over to Arduino core 3.3.12 and C++20".

### Task 4: targeted C++20 adoption (pure libs; after Task 3)

Per the spec table: `std::span<const uint8_t>` for byte APIs (`checksum::crc32`, `prov::sha256`, `prov::deserialize_nets/_pin`, `frames` JPEG check, `prov::choose_networks` as `std::span<const ScanResult>`), `std::string_view` for text APIs (`remote::parse`, `remote::origin_allowed`, `remote::is_ipv4_host`, `prov::form_field`, `prov::form_has_key`, `prov::valid_*`), `std::bit_cast` in the calibration blob, `std::rotr` in SHA-256, `constexpr` CRC-32 table and SHA-256 constants, `using enum` in long switches, designated initializers in our structs and test fixtures. Update every caller (device code included). No behaviour change.

- [ ] Verify: native (all pass), firmware build, cppcheck, clang-format; tracking bench cachegrind instruction count noted. Commit "Use C++20 where it helps: span, string_view, bit_cast, constexpr tables".

### Task 5 (controller + user): final board check and PR

- [ ] Flash, 20 min soak (heap drift), functional checks, PR with the A/B table.
