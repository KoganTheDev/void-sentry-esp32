# Refactor audit (Stage 8a)

Scope: data structures and algorithms, architecture and duplication, test coverage, docs against code. Behaviour and the HUD protocol stay identical. The vendored `lib/tjpgd` is out of scope.

**How to use this:** tick `[x] keep` on the rows you want. Each group then lands as its own PR, in the order of the section headings: Docs, Tests, Duplication, Architecture and DSA. A DSA change is kept only with a measured win, or with a simpler or safer invariant at equal cost.

## Baseline (2026-10-18, main 1eab59c)

| Metric | Value | Source |
|---|---|---|
| Native tests | 226 in 12 suites | `pio test -e native` |
| Pure-library line / branch coverage | **99.3%** (1203/1212) / **88.0%** | new CI `coverage` job; floor set to 95% |
| Per library (lines) | aiming 98.7, checksum 100, frames 100, provisioning 99.8, remote 100, tracking 98.4 | same |
| Tracking bench | 32.7k-102.4k instructions/frame (7 clips) | CI cachegrind |
| Board (Stage 7 soak) | capture 16.7 fps, decode 9.1 ms, track 0.54 ms, idle0 80%, heap flat, viewer 16.6 fps | serial stats, 20 min |
| Docs | CLAUDE.md 3679 words, README 1668, engineering-notes 3384 | `wc -w` |

## 1. Docs

The mismatches (D1-D6) are wrong statements, so they are fixed whatever you tick here. D7-D10 are the trim.

| ID | Finding | Change | Keep |
|---|---|---|---|
| D1 | The test count is 132 in `README.md:30,132` and `docs/engineering-notes.md:169`; it is really 226. The pure-library list there leaves out provisioning, checksum and the shared types. | Fix it, and stop hard-coding counts: point at the CI job instead. | fixed |
| D2 | CLAUDE.md says the `test_joystick` include has the wrong case; that was fixed in 6cbfd48 (the pin-150 problem is still real). | Fix it (moot if T1 deletes the suite). | fixed |
| D3 | `main.cpp:24` says "run-time stats are off in the precompiled SDK" (no longer true on core 3); `main.cpp:52` says "one line" (it logs two). | Fix the comments. | fixed |
| D4 | CLAUDE.md says "5 wrong PINs lock out" but leaves out the escalation (60 s doubling, capped at 1 h, `pin_guard.h:20-28`). | Fix it. | fixed |
| D5 | The task table has no stack sizes (camera 4096, motor 3072, stream 4096, httpd 8192, detect 4096), and its stream row is out of order. | Fix it, in one place (see D7). | fixed |
| D6 | Doxyfile: `INPUT = . ../`, recursive, with an empty `EXCLUDE`, so it scans `.pio`, `managed_components`, tjpgd and the generated HTML headers. `USE_MDFILE_AS_MAINPAGE = README.md` points at a file not in `ESP32/`. | Make INPUT our sources and headers only, add EXCLUDE entries, set the mainpage to `../README.md`, and check the gh-pages build. | fixed |
| D7 | Facts are duplicated across docs: the task table appears 3 times (CLAUDE.md, README, engineering notes), the XCLK measurements 3 times, the calibration flow twice, and the detector rules twice. CLAUDE.md also copies the stats printf format, the CI job list and the env setup that `platformio.ini` already shows. | Single-source rule: README is for people (what it is, how to use it, headline results). `engineering-notes.md` holds the measurements and design history. CLAUDE.md holds agent rules, commands and gotchas, with one-line pointers instead of copies. Doxygen covers the APIs. Target: CLAUDE.md about 50% shorter, with every removed sentence either moved to a named file or dropped for a stated reason (listed in the PR). | [ ] keep |
| D8 | Comments that only restate the code: `main.cpp:142,156,184,203` ("Initialize Joystick", ...), the duplicated "Initializing components..." log (`main.cpp:139-140`), `turret_server.cpp:44,49`, the vague "Increase stack for JSON/String handling" (`turret_server.cpp:40`), `camera.h:69,73`. | Delete them, or replace them with the reason (for example why httpd needs 8 KB). | [ ] keep |
| D9 | Mixed logging: `turret_server.cpp:70,74` uses `Serial.println` next to `ESP_LOGx`. | Use `ESP_LOGx` only. | [ ] keep |
| D10 | Very long lines in CLAUDE.md (the WiFi, Network, stats and fixtures items are 300-450 words each). | Split them into short bullets during D7. | [ ] keep |

## 2. Tests

| ID | Finding | Change | Keep |
|---|---|---|---|
| T1 | Stale on-device suites: `test_controller` and `test_detection` are `TEST_ASSERT_TRUE(true)` placeholders; `test_motion/test_servo.cpp` targets a deleted `Turret::Servo`; `test_joystick` passes 150 as a pin; `test/camera_diff_detection/` has no `test_` prefix (never runs) and calls a deleted function. | Delete the placeholders and dead suites. Keep the two live ones (`test_wifi_manager`, `test_turret_server`). Rewrite `test_joystick` with the real pin only if it tests something the native suite can't. | [ ] keep |
| T2 | `lib/detection/src/jpeg_gray.cpp` is pure (it calls only TJpgDec) but shares a library with Arduino code, so it has no native test. The JPEG path is the biggest per-frame cost. | Move `jpeg_gray` and `lib/tjpgd` usage into a pure `lib/jpeg_gray`. Add native tests on 2-3 small fixture JPEGs (generated with Pillow, committed): output size, a known gray value, and corrupt-input failure. Add it to the gated list. | [ ] keep |
| T3 | Uncovered lines in the pure libs, all defensive paths: the label overflow in `motion_detector.cpp:193-195` (more than 600 provisional labels), the reverse union branch at `:149`, the soft-limit hard stop at `step_planner.cpp:132-134`, and the oversized body in `form_has_key` at `form.cpp:94`. Branch coverage is lowest in calibration_blob, tracker, tracking_types and blobs (9-12 missed each). | Add one test per uncovered line (a checkerboard mask for the label overflow), then raise the floor to the new measured value. The branch gaps get tests only where they are a real input class (corrupt blobs, tracker gate edges), not to chase 100%. | [ ] keep |

## 3. Duplication

| ID | Finding | Change | Keep |
|---|---|---|---|
| U1 | A FreeRTOS mutex RAII `Lock` class is defined 3 times (`frame_store.cpp:12`, `stream_broadcaster.cpp:15`, `wifi_manager.cpp:18`, which differ only in a null check). The spinlock snapshot getter pattern (enter, copy, exit, return) repeats in `turret.cpp:103-133` (4 times), `camera_diff_detection.cpp:211-224` and `controller.cpp:153,160`. | One `ESP32/include/sync.h` with `MutexLock` and `SpinGuarded<T>` (`load()`/`store()` under its own `portMUX_TYPE`). Getters become one-liners and the lock can't be forgotten. | [ ] keep |
| U2 | Blob framing (magic, version, payload, CRC-32) is written in two styles: `aiming/calibration_blob.cpp` (templated put/get) and `provisioning/blobs.cpp` (raw memcpy plus `header_ok`). The NVS read sequence (length, read, size check) repeats in `cal_store.cpp:21` and `wifi_store.cpp:26,45`. | One framing helper in `lib/checksum` (`frame_blob`/`unframe_blob` over `std::span`) with **byte-identical output**, pinned by golden-bytes tests written before the change. One NVS read helper for both stores. On the board: the saved calibration and WiFi networks still load. | [ ] keep |
| U3 | `bounded_len` is copied verbatim in `network_list.cpp:11` and `choose.cpp:9`. | Move it into `prov_types.h`. | [ ] keep |
| U4 | `wifi_http.cpp` boilerplate: every API handler starts with `if (!api_host_ok(req)) return ESP_OK;`; the chunked JSON-array writer appears twice (`:127-140`, `:152-164`); the "Connection: close + JSON error" exit appears 5 or more times; `api_host_ok` reads the Host header separately from `handshake_guard.h:45`. | Add a `json_array` writer helper, a `reply_error()` helper and one `read_header()` shared with the handshake guard. Responses must stay byte-identical, checked by `s6_api.py` on the board. | [ ] keep |
| U5 | Task start plus FrameStore subscription repeats in `StreamBroadcaster::start` (`:62-74`) and `CameraDiffDetection::start_task` (`:136-146`). | A small `start_subscriber_task()` helper. A low gain (2 sites), so it is optional. | [ ] keep |

## 4. Architecture and DSA

| ID | Finding | Change | Keep |
|---|---|---|---|
| A1 | `src/main.cpp` (209 lines) mixes five jobs: the idle-sampling tick hook, the global objects, about 70 lines of stats with 7 function-local `prev_*` statics for the 5 s windows, the init chain, and `loop()`. | Split telemetry into `src/telemetry.{h,cpp}` (tick hook and stats lines). The window arithmetic becomes a pure, natively tested `WindowedCounter` (delta and rate since the last sample). `main.cpp` keeps only wiring, `setup()` and `loop()`. The stats line format stays identical. | [ ] keep |
| A2 | CPU load comes from a 1 kHz IRAM tick hook that samples "is the idle task running" per core (`main.cpp:23-38`), because run-time stats were off on core 2. The core 3 build now has `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y`. | Spike first: read the idle-task run-time counters per core (`uxTaskGetSystemState`, or `ulTaskGetIdleRunTimeCounterForCore` if present) and compare with the hook over 5 minutes. If they agree within 2 points, replace the hook: the result is exact rather than sampled, the code in the tick interrupt goes away, and per-task CPU % (httpd, motor, detect) becomes available as a new stats field. Drop A2 if the counters are coarse or cost more. | [ ] keep |
| A3 | `MotionDetector` makes two passes over the 1200 cells per normal frame: `build_mask` (`motion_detector.cpp:110`) then `update_background` (`:83`). They can be fused safely: every path that skips the update (scene change, warm-up) overwrites the whole background and `fg_age` anyway, so updating in place during the mask pass changes no output. The mean and the mask still use the old background value, read first in the same iteration. | One fused pass. Keep it only if CI cachegrind shows at least 3% fewer instructions/frame on the bench clips, with every `.expect` unchanged (bit-identical outputs, checked by also running the old and new versions on all clips in a test). | [ ] keep |
| A4 | Checked and fine, no change: union-find labelling with path halving (`motion_detector.cpp:134`), top-K insertion (K up to 16 of 64 blobs), the linear scans in FrameSlots (4 slots), ViewerSet (2), NetworkList (3) and the FrameStore subscribers (2), `choose_networks` (3 x 24, once per scan), the RemoteInbox latest-value mailbox, and the scan dedup (24^2 strcmp, once per UI request). A heap, a hash map or a bitmap would add code with no measurable gain at these sizes. | none | n/a |

## Not proposed
- A multi-target tracker (Hungarian assignment). It needs a feature, not a refactor.
- An event bus or a new task layout. The Stage 4 task layout is measured, and a moderate scope keeps it.
