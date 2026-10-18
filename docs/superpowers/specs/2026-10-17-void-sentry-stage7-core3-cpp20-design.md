# Void Sentry Stage 7: Toolchain Upgrade (Arduino Core 3.3.12, C++20)

Date: 2026-10-17. Status: design approved in chat section by section.

## Problem

The firmware is built on Arduino core 2.0.17 (ESP-IDF 4.4, GCC 8.4). GCC 8.4 cannot do C++20, so the whole project is held at C++17 (CLAUDE.md: "moving to C++20/23 means Arduino core 3.x via pioarduino, planned as its own stage"). ESP-IDF 4.4 is also out of support. The pure libraries pass pointer + length pairs everywhere, which a newer standard can turn into single, checked types.

## Goal

Run the same firmware on Arduino core 3.3.12 with C++20 everywhere, with no regression against the Stage 4 numbers, and use C++20 where it clearly improves existing code.

## Decisions

| Decision | Choice | Why | Rejected |
|---|---|---|---|
| Target | pioarduino `platform-espressif32` **55.03.312** (Arduino core 3.3.12, ESP-IDF 5.5.5, GCC 14), pinned by its release URL | Stable; C++20 complete, most of C++23; libraries support it | 61.04.00-RC1 (core 4.0.0 RC on IDF 6.1: release candidate, more churn, library support unknown) |
| Language | **C++20** everywhere after cut-over, plus targeted adoption (below) | Each use fixes a real weakness; no sweeping rewrite | C++23 (weaker cppcheck support; `std::expected` not needed now); flag flip only |
| Migration | **Side by side, then cut over**: a second firmware env `freenove_esp32_wrover_v3` builds the same sources on core 3; the code compiles on both (C++17) until cut-over; API differences live in one header `ESP32/include/platform_compat.h` (`#if ESP_ARDUINO_VERSION_MAJOR >= 3`); at cut-over (same PR) v3 becomes `freenove_esp32_wrover`, the old env and the compat branches are deleted, and `gnu++20` is switched on | Rollback is one env away until merge; A/B benchmarks on the same board | Straight replacement |
| Partitions | `min_spiffs.csv` if the core 3 binary does not fit the default 1.25 MB app slot with at least 15% headroom | Core 3 binaries are larger; `min_spiffs` gives 1.875 MB per app slot and keeps OTA; NVS stays at `0x9000`, size `0x5000` in both tables, so saved WiFi networks and the calibration survive (ESP-IDF 5 reads the same NVS format) | `huge_app` (drops OTA) |
| Camera library | Use the esp32-camera bundled with core 3; remove the `espressif/esp32-camera` lib_dep from the v3 env | Avoid two copies of the driver; re-verify `CONFIG_CAMERA_CORE0`, XCLK 12 MHz, JPEG quality 18 and `esp_jpg_decode` | Keeping the separate dependency |
| Servo | Keep ESP32Servo (3.x supports core 3's new LEDC API); verify attach, limits and rate limit on hardware | Smallest change | Own LEDC driver |

## Expected changes (to check during porting)

- FreeRTOS: `xTaskGetIdleTaskHandleForCPU` / `xTaskGetCurrentTaskHandleForCPU` (idle telemetry in `main.cpp`) renamed to `...ForCore` in IDF 5.
- WiFi: event IDs and `arduino_event_info_t` payloads (disconnect reason), `WiFi.begin` / `softAP` / `scanNetworks` behaviour, `setAutoReconnect`, `persistent`.
- `esp_http_server`: defaults (stack, LRU, `max_uri_handlers`), `httpd_ws_*` and `httpd_queue_work` signatures, the wildcard matcher.
- Camera config struct fields and `esp_camera` pin names; `esp_jpg_decode` availability.
- `esp_freertos_hooks`, `esp_task_wdt` (not called directly; watchdog config may differ), `esp_wifi_restore`, `esp_random` / `esp_fill_random`.
- Tooling: native env and cppcheck to `c++20` / `gnu++20`; `.clang-format` `Standard: c++20`; CI's Ubuntu GCC supports C++20.

## C++20 adoption (after cut-over; behaviour identical, existing native tests are the proof)

| Feature | Where | Why |
|---|---|---|
| `std::span<const uint8_t>` | byte APIs taking pointer + length: `checksum::crc32`, `prov::sha256`, blob `deserialize_*`, the JPEG check, `choose_networks` | Pointer and length travel as one value |
| `std::string_view` | text APIs taking `const char*` + length: `remote::parse`, `origin_allowed` / `is_ipv4_host`, `form_field`, validators | Same, for text, without copies |
| `std::bit_cast` | calibration blob floats (today `memcpy`) | Type-checked, `constexpr`-able reinterpretation |
| `std::rotr` (`<bit>`) | SHA-256 | Replaces hand-written rotates |
| `constexpr` tables | CRC-32 table, SHA-256 constants | Computed at compile time, placed in flash |
| `using enum` | long switches over `CommandKind`, `PlanState`, `ActionKind` | Less noise |
| Designated initializers | our option structs and test fixtures | Field names visible; no silent reordering |

ESP-IDF C structs keep their `*_DEFAULT_CONFIG()` macros. Out of scope: C++23, `std::expected`, rewriting device classes, concepts/coroutines/modules for their own sake.

## Verification

- **Host (CI):** native suite, ASan + UBSan, tracking bench, cppcheck pass on C++17 before cut-over and C++20 after; both firmware envs build in CI during the side-by-side phase; cachegrind instructions per frame on the tracking bench within 2% of before.
- **Board, A/B on the same board and scene, 5 min each with one HUD:**

| Metric | Gate (core 3 vs core 2 / Stage 4) |
|---|---|
| Capture fps, `dropped_no_slot` | >= 16 fps, 0 |
| `dec_win_us` + `trk_win_us`, empty scene | within +-10% |
| `idle0` / `idle1` | not more than 5 points lower |
| Ping avg and 14 KB page fetch (XCLK table method) | no worse than 19 ms / 0.07-0.13 s |
| `heap_min`, `psram_min` over 20 min | no downward drift |
| `motor_late` per s; motor and detect stack free | no worse; stacks keep >= 25% free |
| Flash | fits the chosen partition with >= 15% headroom |

- **Functional:** boot to a saved network; setup hotspot and captive page; HUD stream and `/commands`; HUD jog with dead man; origin and Host guards (PC scripts); servo limits and stepper motion; calibration saved and surviving a reboot.
- **Rollback:** until merge, flashing `freenove_esp32_wrover` (core 2) restores the old firmware.

## Success criteria

Core 3.3.12 is the only firmware env; C++20 everywhere with the adoptions above; every gate passes; the PR records the A/B numbers.

## Out of scope

Arduino core 4.0, C++23, new features, the logging rework (parked), LAN authentication.
