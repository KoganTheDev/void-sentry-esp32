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
      [ "System Architecture", "index.html#autotoc_md71", null ],
      [ "Technical Specifications", "index.html#autotoc_md72", null ],
      [ "Core Capabilities", "index.html#autotoc_md73", [
        [ "Software Setup", "index.html#autotoc_md74", null ]
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
    [ "Void Sentry rebuild: roadmap and Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html", [
      [ "Context", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md42", null ],
      [ "Roadmap", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md43", null ],
      [ "Stage 0 design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md44", [
        [ "Hardware and pin map (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md45", null ],
        [ "Language and toolchain (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md46", null ],
        [ "WiFi credentials (done for now)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md47", null ],
        [ "Native tests (done)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md48", null ],
        [ "Formatting (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md49", null ],
        [ "CI (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md50", null ],
        [ "Generated web UI header (to do)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md51", null ],
        [ "Removed", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md52", null ],
        [ "Definition of done", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md53", null ]
      ] ],
      [ "Non-goals for Stage 0", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-11-void-sentry-stage0-design.html#autotoc_md54", null ]
    ] ],
    [ "Void Sentry Stage 1: frame ownership and push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html", [
      [ "Goal", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md56", null ],
      [ "Problems this fixes", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md57", null ],
      [ "Design", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md58", [
        [ "FrameSlots: slot bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md59", null ],
        [ "FrameStore: device wrapper", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md60", null ],
        [ "Camera task", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md61", null ],
        [ "Controller integration", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md62", null ],
        [ "ViewerSet: viewer bookkeeping (pure C++17, native-tested)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md63", null ],
        [ "Push streaming", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md64", null ],
        [ "Browser HUD (web/web_ui.html)", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md65", null ],
        [ "Diagnostics", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md66", null ]
      ] ],
      [ "Benchmarks: how each success criterion is measured", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md67", null ],
      [ "Testing", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md68", null ],
      [ "Non-goals", "md__2github_2workspace_2docs_2superpowers_2specs_22026-10-12-void-sentry-stage1-camera-stream-design.html#autotoc_md69", null ]
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
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"system__control__types_8h_source.html"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';