# Void Sentry Stage 2: Motion Detection and Persistent Tracking

Date: 2026-10-12. Status: approved in chat (approach A, sections 1-2, summary of 3-4).

## Goal

Find moving objects in the camera frames and keep a persistent lock on one target (stable id, short memory), shown on the web HUD. The turret does **not** move in AI mode in this stage; aiming (and the move/settle/re-acquire logic) is Stage 3.

Scene: indoor desk/room demo, targets 1-3 m away (a hand, a person crossing the room, a toy), indoor lighting.

## Decisions

| Decision | Choice | Why | Rejected |
|---|---|---|---|
| Pixel source | Decode the camera JPEG at 1/8 scale with `esp_jpg_decode(..., JPG_SCALE_8X, ...)` (ROM TJpgDec) straight into a 40x30 gray grid. At 1/8 scale each cell is one 8x8 JPEG block's DC coefficient (the block average): the IDCT is skipped entirely | Measured on this board (decode only, QVGA JPEG): none (full size) 60 ms, 1/2 scale 77 ms, 1/4 scale 71 ms, **1/8 scale 7 ms**. 1/2 and 1/4 still run the full IDCT on every block and then average, so they cost as much as a full decode; 1/8 is ten times cheaper and fits the 15 ms budget with room for detect + track. The block average is also a free noise filter, and there is no big RGB buffer and no per-frame malloc. Cost: a 40x30 grid, so each cell is 8x8 QVGA pixels (a hand at 2 m is only ~1.5x3 cells) | 1/4 scale 80x60 (71 ms, over budget at ~17 fps); camera in gray/YUV + software JPEG for the stream (encode cost, undoes Stage 1); full-size RGB888 decode (230 KB, 16x the pixels needed) |
| Background | Exponential moving average in `uint16_t` Q8 fixed point with a *conditional* rate: background cells learn 1/16 per frame, foreground cells 1/128, but only while the cell has been foreground for at most `fg_age_limit = 34` consecutive frames (~2 s); beyond that it learns at 1/16 again (during warm-up the background is a copy of the current frame) | Shows the whole object, not just its edges; sees slow motion; a moving object leaves no trail (a single 1/16 rate leaves a ghost trail of about 4 times the object width behind an object moving 1 cell/frame, found while planning at the former 80x60 grid). Cost: a stopped object takes a few seconds to be absorbed instead of 1-2 s. The age rule exists for ghosts: a cell that differs because the background itself is wrong (an object in the seed frame that then leaves, or an object that paused and left) would otherwise stay foreground for ~14 s and the tracker would lock onto the ghost; with the rule it clears in ~60 frames. A 4-cell object moving 1 cell/frame covers a cell for 4 frames, far below the limit, so the no-trail behaviour holds | Previous-frame diff (edges only, misses slow motion); single-rate EMA (trails); freezing the background under the tracked target (ghosts, YAGNI) |
| Target | Single persistent track: largest blob confirmed over 3 frames, then sticky | Ignores flicker; does not jump to a bigger distraction | Centroid of all motion (aims between two movers); largest blob per frame (jumps) |
| Prediction | Alpha-beta filter (constant velocity, fixed gains) | Steady-state Kalman with fixed gains; one target, position-only measurement | Full Kalman (covariance math buys nothing here) |
| Where it runs | In `loop()` via `Controller`, once per new frame (as today) | No new task or sync; dual-core split is Stage 4 | New detection task now |

## Architecture

```mermaid
flowchart LR
    FS[FrameStore lease<br/>QVGA JPEG] --> JG[JpegGray<br/>device adapter]
    JG -->|40x30 gray| MD[MotionDetector<br/>pure]
    MD -->|blobs, scene_change| TR[Tracker<br/>pure]
    TR -->|TrackState| CDD[CameraDiffDetection]
    CDD -->|snapshot| CMD[/commands JSON/]
    CMD --> HUD[HUD overlay]
```

### Pure library `lib/tracking` (namespace `tracking`, no Arduino/ESP-IDF headers, native + sanitizer tested)

Constants: `kGridW = 40`, `kGridH = 30`, `kCells = 1200` (one cell = 8x8 QVGA pixels). All state is fixed-size members; no heap anywhere.

`GrayFrame`: `std::array<uint8_t, kCells>`, row-major.

`DetectorConfig` (defaults): `diff_threshold = 20`, `bg_shift = 4` (1/16), `fg_shift = 7` (1/128), `fg_age_limit = 34` (consecutive foreground frames before a cell goes back to the 1/16 rate), `scene_change_fraction = 0.35f`, `scene_change_mean_shift = 25` (gray levels), `warmup_frames = 17` (~1 s at 17 fps), `min_blob_cells = 2` (rejects isolated noise cells; there is no morphological opening, because each cell is already an 8x8 block average and a 3x3 opening would erase hand-sized targets of ~1.5x3 cells), `max_blobs = 16`.

`MotionDetector::process(const GrayFrame&) -> const DetectionResult&`:
1. First frame or after reset: `bg = frame` (Q8), start warm-up, return no blobs.
2. Foreground mask: `|gray - bg| > diff_threshold`.
3. Scene-change guard: only if foreground cells exceed `scene_change_fraction * kCells` AND `|mean(frame) - mean(background)| > scene_change_mean_shift` (the background mean is of the rounded Q8 values, both means computed in the mask pass), set `scene_change = true`, reset `bg = frame`, restart warm-up, return no blobs.
4. During warm-up: copy the frame into the background (no 1/16 learning), reset the foreground ages, return no blobs. The scene-change check above stays active, so a light switch mid-warm-up still reseeds. Copying means auto-exposure settling and a lamp's afterglow end up in the background instead of becoming a ghost blob.

   Measured on real clips (see Testing): a person close to the camera covers 35-45% of the grid while the frame mean stays near the background mean (203 vs ~195, because auto-exposure holds it), so the fraction alone gave 5 false scene changes in `real_cross_room`, each blinding the tracker for ~1 s. A real light switch moves the mean by ~100-180 levels, far above 25. At switch-off the lamp glows for ~1 s; with the old 1/16 warm-up the background kept part of the glow and the fading spot became a ghost the tracker locked onto (13 cells at about (3.4, 12.8)).
5. Blobs: two-pass connected components, 8-connectivity, union-find on fixed arrays (`uint16_t` labels, `kCells` capacity). Per blob: bounding box (x0, y0, x1, y1 inclusive, cells), area (cells), centroid (float, cells). Drop blobs under `min_blob_cells`; keep the `max_blobs` largest, sorted by area descending.
6. Update bg using the raw mask from step 2 (computed against the old background, so this frame is unaffected): 1/16 for background cells, 1/128 for foreground cells.

`DetectionResult`: `scene_change`, `warming_up`, `foreground_cells`, `blob_count`, `std::array<Blob, 16> blobs`.

`TrackerConfig` (defaults): `confirm_frames = 3`, `coast_frames = 17` (~1 s at 17 fps), `alpha = 0.5f`, `beta = 0.2f`, `gate_margin_min = 2` (cells), `gate_margin_frac = 0.5f` (of target width/height), `velocity_decay = 0.8f`, `coast_gate_growth = 1` (cells per coasted frame).

`Tracker::update(const DetectionResult&) -> const TrackState&`, states `Idle`, `Tentative`, `Locked`, `Coasting`:
- **Idle:** if any blob, start `Tentative` on the largest (position = its centroid, velocity 0, box = its box, hits = 1).
- **Gate:** predicted box (`box shifted by velocity`) grown by `max(gate_margin_min, gate_margin_frac * box size)` on each side, plus `coast_gate_growth * frames_since_seen` while coasting.
- **Measurement:** union (bounding box, area sum, area-weighted centroid) of all blobs whose box overlaps the gate. None overlap = miss.
- **Tentative:** match increments hits; at `confirm_frames` go `Locked` and assign `id = ++last_id` (ids start at 1, never reused). A miss returns to `Idle` (no id consumed).
- **Locked:** match updates the alpha-beta filter (`pred = pos + vel; r = meas - pred; pos = pred + alpha*r; vel += beta*r`) and the box (box size smoothed with alpha, box re-centred on `pos`). Larger blobs outside the gate are ignored (stickiness). A miss goes `Coasting`.
- **Coasting:** each frame `pos += vel; vel *= velocity_decay; frames_since_seen++`. A match returns to `Locked` with the **same id**. After `coast_frames` misses go `Idle`.
- **Scene change:** a `Locked` target goes `Coasting`; `Tentative` goes `Idle`. Warm-up frames (no blobs) count as misses.

`TrackState`: `state`, `id` (0 when none), box (cells), `cx`, `cy`, `vx`, `vy` (cells, cells/frame), `frames_since_seen`, `area`. Helper to scale to the frame size (x8 for QVGA).

`direction_for(const TrackState&, deadzone_cells = 2.5) -> (MoveX, MoveY)`: Left/Right/Up/Down of the grid centre when `Locked` (or Coasting), None otherwise. Kept for Stage 3; uses `move_types.h` (header-only, native-safe).

### Device side

- **`JpegGray`** (`lib/tracking_device` or inside `lib/detection`): `bool decode(const uint8_t* jpeg, size_t len, tracking::GrayFrame& out)` using `esp_jpg_decode(len, JPG_SCALE_8X, reader, writer, ctx)`. The writer converts each RGB888 pixel to luma `(77R + 150G + 29B) >> 8` into `out`. Rejects frames whose scaled size is not 40x30. **Verify early** whether `esp_jpg_decode` allocates per call; if it does, measure heap behaviour and document (or switch to the ROM `jd_prepare`/`jd_decomp` with a static work buffer).
- **`CameraDiffDetection`** rebuilt: owns `JpegGray`, `MotionDetector`, `Tracker` (detector + tracker state, ~15 KB, allocated once in PSRAM by `begin()` so it does not cost internal RAM); `detect_object()` decodes, processes, updates, records timings, publishes a snapshot, and returns `direction_for(track)`. The dead commented-out code and the unused members (`_prev_frame`, `_curr_frame`, `_diff_buffer`, `find_motion`, `jpeg_to_greyscale`, static-local `calculate_fps`) are removed.
- **Cross-task snapshot:** today the `/commands` handler (httpd task) calls `get_motion_data()` while `loop()` writes it (a data race). The detection module publishes a small POD snapshot (track state, box and centroid in QVGA pixels, velocity, timings, counters) under a `portMUX`/mutex; the handler copies it out. `MotionData` is extended or replaced accordingly.
- **Controller:** in AI mode, detection runs per new frame as now, but `move_relative()` is not called (a single `constexpr bool kAiAimEnabled = false` in the controller; Stage 3 flips it).
- **`/commands` JSON** (keep existing keys for compatibility, add new ones): `{"detected":0|1,"x","y","width","height","pixels","state":"idle|tentative|locked|coasting","id","bx","by","bw","bh","vx","vy"}` (QVGA pixels; `detected` = Locked or Coasting).
- **HUD** (`web/web_ui.html`): draw the target box and `#id` on the overlay canvas (solid when Locked, dashed when Coasting, nothing otherwise), show the state text in the lock indicator.
- **Stats line** adds `decode_us_avg`, `track_us_avg` (detector + tracker), `scene_changes`, `track_state`, `track_id`.

## Testing

**Native + `native_sanitize` (Unity, `test/native/test_tracking*`), synthetic frames, exact assertions:**
- Detector: static scene -> no blobs after warm-up; a bright rectangle appearing -> one blob with the exact box/area; single-cell noise dropped by `min_blob_cells` while a 2-cell blob is reported; two separated rectangles -> two blobs sorted by area; a U-shape -> one blob (8-connectivity and union-find merge); full-frame brightness step -> `scene_change`, bg reset, warm-up; a static object blends into bg over time; a blob touching the grid border.
- Tracker: object moving at constant velocity -> `Locked` after exactly `confirm_frames`, id 1, centroid within 1 cell; a one-frame blob -> never locks, no id consumed; target pauses (no blobs) for 10 frames then reappears near prediction -> same id; gone for > `coast_frames` -> `Idle`, next lock gets id 2; locked target plus a bigger blob elsewhere -> stays on the target; target split into 2 blobs inside the gate -> one merged measurement; scene change while locked -> Coasting, then reacquire.
- `direction_for` deadzone and sign conventions.
- Clip regression (host): run the recorded clips through detector + tracker; assert no crash, no sanitizer report, and per-clip expectations recorded alongside the fixture (e.g. "locks within N frames, at most 1 id over the clip").

**Recorded fixtures:**
- `tools/record_stream.py`: connects to `ws://<ip>/stream`, saves N seconds of JPEG frames.
- `tools/jpeg_to_gray.py`: Pillow `draft("L", (40, 30))` (libjpeg DCT scaling, close to TJpgDec's) -> one `<clip>.gray` file of concatenated 1200-byte frames, plus `<clip>.json` (frame count, source, expectations).
- 3 clips of about 6 s each (~0.5 MB each), committed under `ESP32/bench/fixtures/`: hand waving and stopping, person crossing the room, light switched on/off. Two deterministic synthetic clips (`tools/tracking/make_synthetic_clips.py`) are committed too, so CI has fixtures before the real ones exist.
- Real clips recorded on the board and converted to 40x30 (`real_hand_pause`, 185 frames: face in the first frame, then a hand and arm waving close to the camera; `real_cross_room`, 176 frames: the user moving across very close to the camera; `real_light_switch`, 250 frames: lights off, on at ~78, off at ~215 with a ~1 s lamp afterglow). They are committed as binary and are not generated by the tools. Observed with the tuned detector: hand_pause first lock 20, 165 locked frames, 1 id; cross_room first lock 22, 152 locked frames, 1 id, 0 scene changes; light_switch 4 scene changes (each switch is a two-frame brightness ramp, so it resets twice), 0 ids.
- Clip regression runs in the bench program (`ESP32/bench/track_bench.cpp`, plain g++, also built with ASan + UBSan in CI), with per-clip expectations in `<clip>.expect`.
- Expectation keys: `lock_by_frame`, `max_ids`, `min_locked_frames`, `min_scene_changes`, `max_scene_changes` and `no_track_after_frame=N` (the tracker must be Idle on every frame from N to the end of the clip). A malformed line fails the clip; a missing `.expect` only warns.
- Ghost clips (synthetic, generated by `tools/tracking/make_synthetic_clips.py`): `seed_object` (4x4 square present in the seed frame, moves away from frame 20, gone from frame 70; must be Idle by frame 130) and `pause_leave` (square enters at frame 20, pauses 2 s at frames 40-73, gone from frame 74; must be Idle by frame 109, observed 104). Both fail without the foreground-age rule. Moving squares enter at frame 20, after the 17-frame warm-up (first detection at frame 18), because anything present during warm-up is absorbed into the background. The synthetic square is 4x4 and moves 1 cell/frame; the grid is only 40 cells wide, so in `square_cross` and `seed_object` it bounces off the grid edges (which also exercises a velocity reversal).

**Host benchmark (CI job `tracking-bench`):** the bench built `-O2 -g -fno-omit-frame-pointer` reports instructions per frame with `valgrind --tool=cachegrind` (deterministic, works on hosted runners); `perf stat` runs best-effort since hosted runners often lack hardware counters. Numbers go in the PR description.

## Success criteria (on hardware)

| Criterion | Target |
|---|---|
| Decode + detect + track time | `decode_us_avg + track_us_avg` <= 15 ms |
| Capture / stream rate | >= 16 fps capture, HUD fps unchanged vs Stage 1 |
| Memory | `heap_min`, `psram_min` flat over a 10 min soak; no per-frame allocation on the detection path |
| Lock time | hand entering the view is `Locked` within ~0.3 s (about 5 frames) |
| Persistence | same id after the hand pauses for 1 s and moves again |
| Robustness | light switched on/off: no false lock (scene change counted) |
| Logic | all native tests pass, also under ASan + UBSan in CI |

## Out of scope

Moving the turret in AI mode, ego-motion handling, multiple simultaneous tracks, the HUD's manual-control commands (`move:`, `fire:`, `mode:`), detection on core 0, the `/commands` per-message 256-byte malloc (noted, unchanged), WiFi provisioning.
