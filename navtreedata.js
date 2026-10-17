/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Void Sentry", "index.html", [
    [ "CLAUDE.md", "md__2github_2workspace_2CLAUDE.html", [
      [ "What it does", "index.html#autotoc_md176", null ],
      [ "Highlights", "index.html#autotoc_md177", null ],
      [ "Architecture", "index.html#autotoc_md178", null ],
      [ "Performance", "index.html#autotoc_md179", null ],
      [ "Hardware and wiring", "index.html#autotoc_md180", null ],
      [ "Getting started", "index.html#autotoc_md181", null ],
      [ "Using it", "index.html#autotoc_md182", null ],
      [ "Testing and CI", "index.html#autotoc_md183", null ],
      [ "Project layout", "index.html#autotoc_md184", null ],
      [ "Roadmap status", "index.html#autotoc_md185", null ],
      [ "License", "index.html#autotoc_md186", null ],
      [ "Commands", "md__2github_2workspace_2CLAUDE.html#autotoc_md1", null ],
      [ "Sanitizers and profiling", "md__2github_2workspace_2CLAUDE.html#autotoc_md2", null ],
      [ "Required local setup", "md__2github_2workspace_2CLAUDE.html#autotoc_md3", null ],
      [ "Architecture", "md__2github_2workspace_2CLAUDE.html#autotoc_md4", null ],
      [ "Pin constraints", "md__2github_2workspace_2CLAUDE.html#autotoc_md5", null ],
      [ "Web UI pipeline", "md__2github_2workspace_2CLAUDE.html#autotoc_md6", null ]
    ] ],
    [ "Void Sentry engineering notes", "md__2github_2workspace_2docs_2engineering-notes.html", [
      [ "Contents", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md8", null ],
      [ "Fixed camera and absolute aiming", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md10", null ],
      [ "Decoding JPEG at 1/8 scale", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md11", null ],
      [ "Camera clock speed versus WiFi", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md12", null ],
      [ "Frame ownership without per-frame allocation", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md13", null ],
      [ "Detector rules learned from real clips", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md14", null ],
      [ "The motor task", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md15", null ],
      [ "Dual-core task layout", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md16", null ],
      [ "The poll storm: a debugging story", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md17", null ],
      [ "WebSocket origin check and DNS rebinding", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md18", null ],
      [ "How correctness is checked", "md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md19", null ]
    ] ],
    [ "Void Sentry Stage 0 Tooling Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md21", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md22", null ],
      [ "File structure", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md24", [
        [ "Task 1: Generate index_html.h at build time (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md26", null ],
        [ "Task 2: Format-on-edit hook (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md28", null ],
        [ "Task 3: One-time reformat (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md30", null ],
        [ "Task 4: CI workflow (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md32", null ],
        [ "Task 5: Push, PR, green CI (main session, not delegated)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md34", null ]
      ] ]
    ] ],
    [ "Void Sentry Stage 1 (Camera + Push Stream) Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md36", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md37", null ],
      [ "File structure", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md39", [
        [ "Task 1: FrameSlots, FrameLease, is_complete_jpeg (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md41", null ],
        [ "Task 2: ViewerSet (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md43", null ],
        [ "Task 3: native_sanitize env + CI job (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md45", null ],
        [ "Task 4: FrameStore, camera task, Camera and Controller rework (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md47", null ],
        [ "Task 5: StreamBroadcaster + stats line (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md49", null ],
        [ "Task 6: HUD push client (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md51", null ],
        [ "Task 7: Hardware benchmarks, sanitizer proof, PR (main session, with the user)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md53", null ]
      ] ]
    ] ],
    [ "Stage 2 Motion Detection and Persistent Tracking Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md55", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md56", null ],
      [ "File map", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md58", [
        [ "Task 1: Tracking types and MotionDetector", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md60", null ],
        [ "Task 2: Tracker and direction_for", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md62", null ],
        [ "Task 3: Device integration (decoder, detection glue, /commands, controller, stats)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md64", null ],
        [ "Task 4: HUD target overlay", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md66", null ],
        [ "Task 5: Fixture tools, clip bench, CI job", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md68", null ],
        [ "Task 6 (controller + user, hardware): real clips and on-device verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md70", null ]
      ] ]
    ] ],
    [ "Stage 3 Motion Control Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md72", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md73", null ],
      [ "File map", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md75", [
        [ "Task 1: AimMap and calibration blob", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md77", null ],
        [ "Task 2: StepPlanner", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md79", null ],
        [ "Task 3: AimPolicy, ButtonClassifier, CalibrationSession", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md81", null ],
        [ "Task 4: Turret motor task", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md83", null ],
        [ "Task 5: Controller modes, aiming, calibration, NVS, /commands, HUD", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md85", null ],
        [ "Task 6 (controller + user, hardware): verify on the board", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md87", null ]
      ] ]
    ] ],
    [ "Stage 4 Dual-Core Task Layout Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md89", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md90", [
        [ "Task 1: Multi-subscriber FrameStore and detection task", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md91", null ],
        [ "Task 2: Pin httpd to core 0 and set priorities", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md92", null ],
        [ "Task 3 (controller + user): hardware verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md93", null ]
      ] ]
    ] ],
    [ "Stage 4b HUD Remote Control Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md95", null ],
      [ "Interfaces (fixed; every task uses exactly these)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md96", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md97", [
        [ "Task 1: Pure lib/remote with native tests", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md98", null ],
        [ "Task 2: HUD (web/web_ui.html only)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md99", null ],
        [ "Task 3: Firmware integration (after Tasks 1 and 2 are merged)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md100", null ],
        [ "Task 4 (controller + user, later): hardware verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md101", null ]
      ] ]
    ] ],
    [ "Stage 5 Portfolio Polish Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md103", null ],
      [ "Facts list (measured on hardware unless noted; use these exact values)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md104", [
        [ "Task 1: README.md (repo root)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md105", null ],
        [ "Task 2: docs/engineering-notes.md and Doxygen name", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md106", null ],
        [ "Task 3 (controller): review, push, PR, CI", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md107", null ],
        [ "Task 4 (with the user, later): record the demo GIF and add it to the README slot", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md108", null ]
      ] ]
    ] ],
    [ "Void Sentry rebuild: roadmap and Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html", [
      [ "Context", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md110", null ],
      [ "Roadmap", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md111", null ],
      [ "Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md112", [
        [ "Hardware and pin map (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md113", null ],
        [ "Language and toolchain (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md114", null ],
        [ "WiFi credentials (done for now)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md115", null ],
        [ "Native tests (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md116", null ],
        [ "Formatting (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md117", null ],
        [ "CI (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md118", null ],
        [ "Generated web UI header (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md119", null ],
        [ "Removed", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md120", null ],
        [ "Definition of done", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md121", null ]
      ] ],
      [ "Non-goals for Stage 0", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md122", null ]
    ] ],
    [ "Void Sentry Stage 1: frame ownership and push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md124", null ],
      [ "Problems this fixes", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md125", null ],
      [ "Design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md126", [
        [ "FrameSlots: slot bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md127", null ],
        [ "FrameStore: device wrapper", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md128", null ],
        [ "Camera task", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md129", null ],
        [ "Controller integration", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md130", null ],
        [ "ViewerSet: viewer bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md131", null ],
        [ "Push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md132", null ],
        [ "Browser HUD (web/web_ui.html)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md133", null ],
        [ "Diagnostics", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md134", null ]
      ] ],
      [ "Benchmarks: how each success criterion is measured", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md135", null ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md136", null ],
      [ "Non-goals", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md137", null ]
    ] ],
    [ "Void Sentry Stage 2: Motion Detection and Persistent Tracking", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md139", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md140", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md141", [
        [ "Pure library lib/tracking (namespace tracking, no Arduino/ESP-IDF headers, native + sanitizer tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md142", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md143", null ]
      ] ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md144", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md145", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md146", null ]
    ] ],
    [ "Void Sentry Stage 3: Motion Control (Fixed Camera, Absolute Aiming)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md148", null ],
      [ "Why absolute aiming (not visual servoing)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md149", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md150", null ],
      [ "Coordinates and limits", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md151", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md152", [
        [ "Pure library lib/aiming (namespace aiming; no Arduino/ESP-IDF headers, no heap; native + ASan/UBSan t...", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md153", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md154", null ]
      ] ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md155", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md156", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md157", null ]
    ] ],
    [ "Void Sentry Stage 4: Dual-Core Task Layout", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html", [
      [ "Problem (measured in Stage 3)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md159", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md160", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md161", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md162", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md163", null ]
    ] ],
    [ "Void Sentry Stage 4b: HUD Remote Control", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html", [
      [ "Problem", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md165", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md166", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md167", null ],
      [ "Success criteria", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md168", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md169", null ]
    ] ],
    [ "Void Sentry Stage 5: Portfolio Polish", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html", [
      [ "Problem", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md171", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md172", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md173", null ],
      [ "Success criteria", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md174", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md175", null ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Enumerations", "globals_enum.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"aim__map_8h.html",
"md__2github_2workspace_2docs_2engineering-notes.html#autotoc_md13",
"structtracking_1_1TrackerConfig.html#aa433d82abf73bb807947d1da4936162d"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';