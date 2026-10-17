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
  [ "Smart Camera ESP32", "index.html", [
    [ "CLAUDE.md", "md__2github_2workspace_2CLAUDE.html", [
      [ "System Architecture", "index.html#autotoc_md137", null ],
      [ "Technical Specifications", "index.html#autotoc_md138", null ],
      [ "Core Capabilities", "index.html#autotoc_md139", [
        [ "Software Setup", "index.html#autotoc_md140", null ]
      ] ],
      [ "Commands", "md__2github_2workspace_2CLAUDE.html#autotoc_md1", null ],
      [ "Sanitizers and profiling", "md__2github_2workspace_2CLAUDE.html#autotoc_md2", null ],
      [ "Required local setup", "md__2github_2workspace_2CLAUDE.html#autotoc_md3", null ],
      [ "Architecture", "md__2github_2workspace_2CLAUDE.html#autotoc_md4", null ],
      [ "Pin constraints", "md__2github_2workspace_2CLAUDE.html#autotoc_md5", null ],
      [ "Web UI pipeline", "md__2github_2workspace_2CLAUDE.html#autotoc_md6", null ]
    ] ],
    [ "Void Sentry Stage 0 Tooling Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md8", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md9", null ],
      [ "File structure", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md11", [
        [ "Task 1: Generate index_html.h at build time (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md13", null ],
        [ "Task 2: Format-on-edit hook (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md15", null ],
        [ "Task 3: One-time reformat (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md17", null ],
        [ "Task 4: CI workflow (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md19", null ],
        [ "Task 5: Push, PR, green CI (main session, not delegated)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-11-void-sentry-stage0-tooling.html#autotoc_md21", null ]
      ] ]
    ] ],
    [ "Void Sentry Stage 1 (Camera + Push Stream) Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md23", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md24", null ],
      [ "File structure", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md26", [
        [ "Task 1: FrameSlots, FrameLease, is_complete_jpeg (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md28", null ],
        [ "Task 2: ViewerSet (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md30", null ],
        [ "Task 3: native_sanitize env + CI job (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md32", null ],
        [ "Task 4: FrameStore, camera task, Camera and Controller rework (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md34", null ],
        [ "Task 5: StreamBroadcaster + stats line (model: Sonnet)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md36", null ],
        [ "Task 6: HUD push client (model: Haiku)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md38", null ],
        [ "Task 7: Hardware benchmarks, sanitizer proof, PR (main session, with the user)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage1-camera-stream.html#autotoc_md40", null ]
      ] ]
    ] ],
    [ "Stage 2 Motion Detection and Persistent Tracking Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md42", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md43", null ],
      [ "File map", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md45", [
        [ "Task 1: Tracking types and MotionDetector", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md47", null ],
        [ "Task 2: Tracker and direction_for", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md49", null ],
        [ "Task 3: Device integration (decoder, detection glue, /commands, controller, stats)", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md51", null ],
        [ "Task 4: HUD target overlay", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md53", null ],
        [ "Task 5: Fixture tools, clip bench, CI job", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md55", null ],
        [ "Task 6 (controller + user, hardware): real clips and on-device verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md57", null ]
      ] ]
    ] ],
    [ "Stage 3 Motion Control Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md59", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md60", null ],
      [ "File map", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md62", [
        [ "Task 1: AimMap and calibration blob", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md64", null ],
        [ "Task 2: StepPlanner", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md66", null ],
        [ "Task 3: AimPolicy, ButtonClassifier, CalibrationSession", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md68", null ],
        [ "Task 4: Turret motor task", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md70", null ],
        [ "Task 5: Controller modes, aiming, calibration, NVS, /commands, HUD", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md72", null ],
        [ "Task 6 (controller + user, hardware): verify on the board", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage3-motion-control.html#autotoc_md74", null ]
      ] ]
    ] ],
    [ "Stage 4 Dual-Core Task Layout Implementation Plan", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html", [
      [ "Global Constraints", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md76", null ],
      [ "Review Focus", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md77", [
        [ "Task 1: Multi-subscriber FrameStore and detection task", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md78", null ],
        [ "Task 2: Pin httpd to core 0 and set priorities", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md79", null ],
        [ "Task 3 (controller + user): hardware verification", "md__2github_2workspace_2docs_2superpowers_2plans_22026-10-13-void-sentry-stage4-dual-core.html#autotoc_md80", null ]
      ] ]
    ] ],
    [ "Void Sentry rebuild: roadmap and Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html", [
      [ "Context", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md82", null ],
      [ "Roadmap", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md83", null ],
      [ "Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md84", [
        [ "Hardware and pin map (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md85", null ],
        [ "Language and toolchain (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md86", null ],
        [ "WiFi credentials (done for now)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md87", null ],
        [ "Native tests (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md88", null ],
        [ "Formatting (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md89", null ],
        [ "CI (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md90", null ],
        [ "Generated web UI header (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md91", null ],
        [ "Removed", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md92", null ],
        [ "Definition of done", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md93", null ]
      ] ],
      [ "Non-goals for Stage 0", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md94", null ]
    ] ],
    [ "Void Sentry Stage 1: frame ownership and push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md96", null ],
      [ "Problems this fixes", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md97", null ],
      [ "Design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md98", [
        [ "FrameSlots: slot bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md99", null ],
        [ "FrameStore: device wrapper", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md100", null ],
        [ "Camera task", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md101", null ],
        [ "Controller integration", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md102", null ],
        [ "ViewerSet: viewer bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md103", null ],
        [ "Push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md104", null ],
        [ "Browser HUD (web/web_ui.html)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md105", null ],
        [ "Diagnostics", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md106", null ]
      ] ],
      [ "Benchmarks: how each success criterion is measured", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md107", null ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md108", null ],
      [ "Non-goals", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md109", null ]
    ] ],
    [ "Void Sentry Stage 2: Motion Detection and Persistent Tracking", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md111", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md112", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md113", [
        [ "Pure library lib/tracking (namespace tracking, no Arduino/ESP-IDF headers, native + sanitizer tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md114", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md115", null ]
      ] ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md116", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md117", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage2-motion-tracking-design.html#autotoc_md118", null ]
    ] ],
    [ "Void Sentry Stage 3: Motion Control (Fixed Camera, Absolute Aiming)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md120", null ],
      [ "Why absolute aiming (not visual servoing)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md121", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md122", null ],
      [ "Coordinates and limits", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md123", null ],
      [ "Architecture", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md124", [
        [ "Pure library lib/aiming (namespace aiming; no Arduino/ESP-IDF headers, no heap; native + ASan/UBSan t...", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md125", null ],
        [ "Device side", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md126", null ]
      ] ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md127", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md128", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage3-motion-control-design.html#autotoc_md129", null ]
    ] ],
    [ "Void Sentry Stage 4: Dual-Core Task Layout", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html", [
      [ "Problem (measured in Stage 3)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md131", null ],
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md132", null ],
      [ "Decisions", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md133", null ],
      [ "Success criteria (on hardware)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md134", null ],
      [ "Out of scope", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-13-void-sentry-stage4-dual-core-design.html#autotoc_md135", null ]
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
"md__2github_2workspace_2docs_2superpowers_2plans_22026-10-12-void-sentry-stage2-motion-tracking.html#autotoc_md49"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';