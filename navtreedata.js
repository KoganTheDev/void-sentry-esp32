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
      [ "What it does", "index.html#autotoc_md220", null ],
      [ "Highlights", "index.html#autotoc_md221", null ],
      [ "Architecture", "index.html#autotoc_md222", null ],
      [ "Performance", "index.html#autotoc_md223", null ],
      [ "Hardware and wiring", "index.html#autotoc_md224", null ],
      [ "Getting started", "index.html#autotoc_md225", null ],
      [ "Using it", "index.html#autotoc_md226", null ],
      [ "Testing and CI", "index.html#autotoc_md227", null ],
      [ "Project layout", "index.html#autotoc_md228", null ],
      [ "Roadmap status", "index.html#autotoc_md229", null ],
      [ "License", "index.html#autotoc_md230", null ],
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
    [ "Refactor audit (Stage 8a)", "md__2github_2workspace_2docs_2refactor-audit.html", [
      [ "Baseline (2026-10-18, main 1eab59c)", "md__2github_2workspace_2docs_2refactor-audit.html#autotoc_md21", null ],
      [ "Docs", "md__2github_2workspace_2docs_2refactor-audit.html#autotoc_md22", null ],
      [ "Tests", "md__2github_2workspace_2docs_2refactor-audit.html#autotoc_md23", null ],
      [ "Duplication", "md__2github_2workspace_2docs_2refactor-audit.html#autotoc_md24", null ],
      [ "Architecture and DSA", "md__2github_2workspace_2docs_2refactor-audit.html#autotoc_md25", null ],
      [ "Not proposed", "md__2github_2workspace_2docs_2refactor-audit.html#autotoc_md26", null ]
    ] ],
    [ "Void Sentry Stage 0 Tooling Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md28", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md29", null ],
      [ "File structure", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md31", [
        [ "Task 1: Generate index_html.h at build time (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md33", null ],
        [ "Task 2: Format-on-edit hook (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md35", null ],
        [ "Task 3: One-time reformat (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md37", null ],
        [ "Task 4: CI workflow (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md39", null ],
        [ "Task 5: Push, PR, green CI (main session, not delegated)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md41", null ]
      ] ]
    ] ],
    [ "Void Sentry Stage 1 (Camera + Push Stream) Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md43", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md44", null ],
      [ "File structure", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md46", [
        [ "Task 1: FrameSlots, FrameLease, is_complete_jpeg (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md48", null ],
        [ "Task 2: ViewerSet (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md50", null ],
        [ "Task 3: native_sanitize env + CI job (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md52", null ],
        [ "Task 4: FrameStore, camera task, Camera and Controller rework (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md54", null ],
        [ "Task 5: StreamBroadcaster + stats line (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md56", null ],
        [ "Task 6: HUD push client (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md58", null ],
        [ "Task 7: Hardware benchmarks, sanitizer proof, PR (main session, with the user)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md60", null ]
      ] ]
    ] ],
    [ "Stage 2 Motion Detection and Persistent Tracking Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md62", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md63", null ],
      [ "File map", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md65", [
        [ "Task 1: Tracking types and MotionDetector", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md67", null ],
        [ "Task 2: Tracker and direction_for", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md69", null ],
        [ "Task 3: Device integration (decoder, detection glue, /commands, controller, stats)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md71", null ],
        [ "Task 4: HUD target overlay", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md73", null ],
        [ "Task 5: Fixture tools, clip bench, CI job", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md75", null ],
        [ "Task 6 (controller + user, hardware): real clips and on-device verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md77", null ]
      ] ]
    ] ],
    [ "Stage 3 Motion Control Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md79", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md80", null ],
      [ "File map", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md82", [
        [ "Task 1: AimMap and calibration blob", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md84", null ],
        [ "Task 2: StepPlanner", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md86", null ],
        [ "Task 3: AimPolicy, ButtonClassifier, CalibrationSession", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md88", null ],
        [ "Task 4: Turret motor task", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md90", null ],
        [ "Task 5: Controller modes, aiming, calibration, NVS, /commands, HUD", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md92", null ],
        [ "Task 6 (controller + user, hardware): verify on the board", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md94", null ]
      ] ]
    ] ],
    [ "Stage 4 Dual-Core Task Layout Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md96", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md97", [
        [ "Task 1: Multi-subscriber FrameStore and detection task", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md98", null ],
        [ "Task 2: Pin httpd to core 0 and set priorities", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md99", null ],
        [ "Task 3 (controller + user): hardware verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md100", null ]
      ] ]
    ] ],
    [ "Stage 4b HUD Remote Control Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md102", null ],
      [ "Interfaces (fixed; every task uses exactly these)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md103", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md104", [
        [ "Task 1: Pure lib/remote with native tests", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md105", null ],
        [ "Task 2: HUD (web/web_ui.html only)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md106", null ],
        [ "Task 3: Firmware integration (after Tasks 1 and 2 are merged)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md107", null ],
        [ "Task 4 (controller + user, later): hardware verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage4b-hud-remote-control.html#autotoc_md108", null ]
      ] ]
    ] ],
    [ "Stage 5 Portfolio Polish Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md110", null ],
      [ "Facts list (measured on hardware unless noted; use these exact values)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md111", [
        [ "Task 1: README.md (repo root)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md112", null ],
        [ "Task 2: docs/engineering-notes.md and Doxygen name", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md113", null ],
        [ "Task 3 (controller): review, push, PR, CI", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md114", null ],
        [ "Task 4 (with the user, later): record the demo GIF and add it to the README slot", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage5-portfolio.html#autotoc_md115", null ]
      ] ]
    ] ],
    [ "Stage 6 On-Device WiFi Provisioning Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md117", null ],
      [ "Interfaces (fixed; every task uses exactly these)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md118", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md119", [
        [ "Task 1: lib/checksum, types, NetworkList, choose, blobs (pure)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md120", null ],
        [ "Task 2: SHA-256, PinGuard, form, WifiPlan (pure; after Task 1)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md121", null ],
        [ "Task 3: setup page and HUD link (web only; parallel with Tasks 1-2)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md122", null ],
        [ "Task 4: firmware integration (after Tasks 1-3)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md123", null ],
        [ "Task 5 (controller + user): hardware verification and cleanup", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage6-wifi-provisioning.html#autotoc_md124", null ]
      ] ]
    ] ],
    [ "Stage 7 Toolchain Upgrade Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md126", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md127", [
        [ "Task 1: core 3 env side by side", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md128", null ],
        [ "Task 2 (controller): A/B on the board", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md129", null ],
        [ "Task 3: cut-over to one env and C++20", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md130", null ],
        [ "Task 4: targeted C++20 adoption (pure libs; after Task 3)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md131", null ],
        [ "Task 5 (controller + user): final board check and PR", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-17-void-sentry-stage7-core3-cpp20.html#autotoc_md132", null ]
      ] ]
    ] ],
    [ "Void Sentry rebuild: roadmap and Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html", [
      [ "Context", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md134", null ],
      [ "Roadmap", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md135", null ],
      [ "Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md136", [
        [ "Hardware and pin map (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md137", null ],
        [ "Language and toolchain (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md138", null ],
        [ "WiFi credentials (done for now)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md139", null ],
        [ "Native tests (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md140", null ],
        [ "Formatting (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md141", null ],
        [ "CI (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md142", null ],
        [ "Generated web UI header (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md143", null ],
        [ "Removed", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md144", null ],
        [ "Definition of done", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md145", null ]
      ] ],
      [ "Non-goals for Stage 0", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md146", null ]
    ] ],
    [ "Void Sentry Stage 1: frame ownership and push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md148", null ],
      [ "Problems this fixes", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md149", null ],
      [ "Design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md150", [
        [ "FrameSlots: slot bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md151", null ],
        [ "FrameStore: device wrapper", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md152", null ],
        [ "Camera task", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md153", null ],
        [ "Controller integration", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md154", null ],
        [ "ViewerSet: viewer bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md155", null ],
        [ "Push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md156", null ],
        [ "Browser HUD (web/web_ui.html)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md157", null ],
        [ "Diagnostics", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md158", null ]
      ] ],
      [ "Benchmarks: how each success criterion is measured", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md159", null ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md160", null ],
      [ "Non-goals", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md161", null ]
    ] ],
    [ "Void Sentry Stage 2: Motion Detection and Persistent Tracking", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md163", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md164", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md165", [
        [ "Pure library lib/tracking (namespace tracking, no Arduino/ESP-IDF headers, native + sanitizer tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md166", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md167", null ]
      ] ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md168", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md169", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md170", null ]
    ] ],
    [ "Void Sentry Stage 3: Motion Control (Fixed Camera, Absolute Aiming)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md172", null ],
      [ "Why absolute aiming (not visual servoing)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md173", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md174", null ],
      [ "Coordinates and limits", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md175", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md176", [
        [ "Pure library lib/aiming (namespace aiming; no Arduino/ESP-IDF headers, no heap; native + ASan/UBSan t...", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md177", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md178", null ]
      ] ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md179", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md180", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md181", null ]
    ] ],
    [ "Void Sentry Stage 4: Dual-Core Task Layout", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html", [
      [ "Problem (measured in Stage 3)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md183", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md184", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md185", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md186", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md187", null ]
    ] ],
    [ "Void Sentry Stage 4b: HUD Remote Control", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html", [
      [ "Problem", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md189", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md190", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md191", null ],
      [ "Success criteria", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md192", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage4b-hud-remote-control-design.html#autotoc_md193", null ]
    ] ],
    [ "Void Sentry Stage 5: Portfolio Polish", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html", [
      [ "Problem", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md195", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md196", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md197", null ],
      [ "Success criteria", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md198", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage5-portfolio-design.html#autotoc_md199", null ]
    ] ],
    [ "Void Sentry Stage 6: On-Device WiFi Provisioning", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html", [
      [ "Problem", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md201", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md202", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md203", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md204", [
        [ "Pure library lib/provisioning (namespace prov; no Arduino/ESP-IDF headers, no heap; native + ASan/UBS...", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md205", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md206", null ]
      ] ],
      [ "Error handling", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md207", null ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md208", null ],
      [ "Success criteria", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md209", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage6-wifi-provisioning-design.html#autotoc_md210", null ]
    ] ],
    [ "Void Sentry Stage 7: Toolchain Upgrade (Arduino Core 3.3.12, C++20)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html", [
      [ "Problem", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md212", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md213", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md214", null ],
      [ "Expected changes (to check during porting)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md215", null ],
      [ "C++20 adoption (after cut-over; behaviour identical, existing native tests are the proof)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md216", null ],
      [ "Verification", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md217", null ],
      [ "Success criteria", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md218", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md219", null ]
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
        [ "Variables", "functions_vars.html", null ],
        [ "Enumerations", "functions_enum.html", null ]
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
"dir_9d6c47cbd51ed166aa2ff2bac1e2ca3f.html",
"md__2github_2workspace_2docs_2superpowers_2specs_22026-10-17-void-sentry-stage7-core3-cpp20-design.html#autotoc_md218"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';