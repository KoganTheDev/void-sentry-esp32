# Stage 4b HUD Remote Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Parse the HUD's `/commands` messages so the HUD can jog the turret in manual mode and switch AI/manual, with a dead-man timeout and the joystick as the override.

**Architecture:** A pure `lib/remote` (parser + `RemoteInbox`) is natively tested. The httpd handler parses each message and hands it to a `remote::BaseRemoteSink`. `Controller` implements the sink: it stores the command in a spinlocked `RemoteInbox` and consumes it in `run()`. The HUD resends a held direction every 100 ms.

**Tech Stack:** ESP32 Arduino core 2.0.17 (ESP-IDF 4.4), C++17, PlatformIO, Unity; vanilla JS HUD.

**Spec:** `docs/superpowers/specs/2026-10-17-void-sentry-stage4b-hud-remote-control-design.md`

## Global Constraints

- **C++17 only.** No heap on the per-message path. `lib/remote` includes no Arduino or ESP-IDF headers.
- **Locking:** no blocking calls or logging inside portMUX critical sections.
- **Credentials:** never read or print the gitignored WiFi credentials header in `ESP32/include/`. Never run a shell command containing the word that file is named after (a hook blocks it).
- **`pio`:** `~/.platformio/penv/Scripts/pio.exe`, run from `ESP32/`. Never pipe it into `head` / `grep -m1`; log to a file instead.
- **Native env:** needs WinLibs GCC 14 first on PATH: `export PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin | head -1):$PATH"`.
- **Style:** no em dashes. Device classes use a leading underscore and `this->`. Doxygen `@brief` on headers. clang-format 19 clean (`.clang-format`).
- **Commits:** trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Board:** do not upload; the controller does hardware runs.

## Interfaces (fixed; every task uses exactly these)

`ESP32/lib/remote/include/remote_command.h`, namespace `remote`:

```cpp
enum class CommandKind : uint8_t { Poll, Move, Stop, Mode, Fire };
enum class RemoteMode : uint8_t { Ai, User };

struct Command
{
    CommandKind kind = CommandKind::Poll;
    int8_t pan = 0;   // Move only: -1 left, 0, +1 right
    int8_t tilt = 0;  // Move only: -1 down, 0, +1 up (up = larger tilt angle)
    RemoteMode mode = RemoteMode::Ai; // Mode only
    bool fire_on = false;             // Fire only
};

/** Exact, case-sensitive match on the whole buffer (not NUL-terminated; len bytes). */
Command parse(const char* data, size_t len);

/** Receives parsed commands; implemented by the controller (device) and fakes (tests). */
class BaseRemoteSink
{
public:
    virtual ~BaseRemoteSink() = default;
    virtual void submit(const Command& cmd) = 0;
};
```

`ESP32/lib/remote/include/remote_inbox.h`, namespace `remote`:

```cpp
constexpr uint32_t kJogTimeoutMs = 300;
constexpr float kJogSpeed = 0.6f;

struct Jog { float pan = 0.0f; float tilt = 0.0f; };

class RemoteInbox
{
public:
    void submit(const Command& cmd, uint32_t now_ms);
    bool jog_active(uint32_t now_ms) const;   // a Move arrived and (now_ms - its time) < kJogTimeoutMs (wrap-safe)
    Jog jog(uint32_t now_ms) const;           // kJogSpeed * direction while active, else {0, 0}
    std::optional<RemoteMode> take_mode_request(); // the latest Mode command, returned once
};
```

Parse table: `move:up` (0,+1), `move:down` (0,-1), `move:left` (-1,0), `move:right` (+1,0), `move:up-left` (-1,+1), `move:up-right` (+1,+1), `move:down-left` (-1,-1), `move:down-right` (+1,-1), `move:stop` -> Stop, `mode:ai`, `mode:user` -> Mode, `fire:on`, `fire:off` -> Fire. Anything else (`" "`, empty, nullptr, unknown, trailing bytes, `MOVE:UP`, `move:`) -> Poll.

Inbox rules: Move stores the direction and `now_ms`; Stop ends the jog at once; Mode stores a pending request (a later Mode replaces an untaken one); Poll and Fire change nothing. `move:` with direction (0,0) cannot be produced by the parser.

## Review Focus

1. **Dead man.** A held HUD key followed by a dropped connection must stop the turret within 300 ms (no Stop ever arrives).
2. **Joystick wins.** A deflected stick overrides an active HUD jog in User and Calibrate; in Calibrate while the button is held the jog is (0,0) regardless.
3. **Mode from the HUD while calibrating** is ignored and never leaves a pending request that fires after calibration ends (it is taken and dropped).
4. **Malformed input** (oversized frame, binary frame, partial `move:`) never reads past `len` and never allocates.
5. **No per-message heap and no leak** in `CommandsWebSocketHandler`, including the send-failure path.

### Task 1: Pure `lib/remote` with native tests

**Files:** create `ESP32/lib/remote/include/remote_command.h`, `ESP32/lib/remote/include/remote_inbox.h`, `ESP32/lib/remote/src/remote_command.cpp`, `ESP32/lib/remote/src/remote_inbox.cpp`, `ESP32/test/native/test_remote/test_remote.cpp`.

- [ ] Implement exactly the interfaces above (Doxygen `@brief` on public items; style like `ESP32/lib/aiming`).
- [ ] Tests (Unity, style like `ESP32/test/native/test_aim_policy`):
  - every row of the parse table, including all four diagonals
  - Poll for `" "`, empty, nullptr with len 0, `move:`, `move:upp`, `move:up ` (trailing space), `MOVE:UP`, `mode:calibrate`, `fire:maybe`, and a 200-byte buffer of `x`
  - parse reads only `len` bytes: pass `"move:upXYZ"` with len 7 -> Move (0,+1)
  - inbox: no jog before any Move; jog = (0.6 * dir) right after a Move; still active at now+299, inactive at now+300; a fresh Move restarts the window; Stop clears immediately; wrap-safe across `UINT32_MAX` (Move at 0xFFFFFF00, check at 0x00000010)
  - inbox: `take_mode_request` returns the mode once then nullopt; a second Mode before taking replaces the first; Poll and Fire leave jog and mode untouched
- [ ] Verify: `pio test -e native` all pass (from `ESP32/`, with WinLibs on PATH); clang-format clean. Commit.

### Task 2: HUD (`web/web_ui.html` only)

The firmware protocol is fixed by the parse table above. The `/commands` send path already goes through `App.sendCommand(msg)`, which keeps exactly one poll loop; never bypass it and never schedule polls from replies (see the comment block in the file and CLAUDE.md "Web UI pipeline").

- [ ] Track held directions from keys (WASD/arrows) and on-screen buttons (mouse and touch) in one set; compute the combined direction (`up`, `up-left`, ...; opposite keys cancel). On change, send `move:<dir>` or `move:stop` (when nothing is held or the result cancels to zero).
- [ ] While a direction is held, resend `move:<dir>` every 100 ms (one `setInterval`, cleared on stop).
- [ ] Send `move:stop` and clear the held set on `window` `blur` and on `document` `visibilitychange` to hidden.
- [ ] Mode button: enable it; `toggleMode()` sends `mode:user` when the current firmware mode is `ai`, `mode:ai` when it is `user`, and does nothing in `calibrate`. Update its title. The `m` key already calls `toggleMode()`.
- [ ] When the firmware mode is `ai`, dim the arrow controls (CSS class) so it is visible that moves are ignored; do not block sending.
- [ ] Keep the help modal text in sync (WASD/arrows jog in manual mode, M toggles mode).
- [ ] Verify: extract the `<script>` blocks and run `node --check` on them; `python -m unittest discover -s web -p "test_*.py"` passes. Commit.

### Task 3: Firmware integration (after Tasks 1 and 2 are merged)

**Files:** `ESP32/lib/network/include/websockets.h`, `ESP32/lib/network/include/turret_server.h`, `ESP32/lib/network/src/turret_server.cpp`, `ESP32/lib/controller/include/controller.h`, `ESP32/lib/controller/src/controller.cpp`, `ESP32/src/main.cpp`, `CLAUDE.md`.

- [ ] `CommandsWebSocketHandler`: take a `remote::BaseRemoteSink*` (nullable). Receive into a fixed 64-byte stack buffer; a frame longer than 64 bytes or not TEXT returns `ESP_FAIL` (the server closes the session). `remote::parse` the payload and `submit` it when the sink is non-null. Build the JSON reply in a fixed stack buffer (same keys and order as today) and send it; no `malloc` anywhere in the handler, and nothing to leak on a send failure. Simplify `WebsocketHandler` accordingly (it has one subclass).
- [ ] `HttpServer::start(FrameStore&, BaseDetectionModule*, const BaseStatusProvider*, remote::BaseRemoteSink* remote = nullptr)`.
- [ ] `Controller` implements `remote::BaseRemoteSink`: `submit()` stores into a `remote::RemoteInbox` under its own portMUX with `millis()` (safe from the httpd task). In `run()`:
  - take the mode request every pass; if present and the mode is not Calibrate and differs from the current mode, switch exactly as the short-press toggle does (detection reset, policy reset, jog stop, log). Factor the toggle into one private `set_mode(ControlMode)` used by both.
  - `jog_from_joystick` becomes the jog arbiter for User/Calibrate: Calibrate with the button held -> (0,0); a deflected stick (`get_speed_x()` or `get_speed_y()` non-zero) -> the stick; else an active remote jog; else (0,0). Keep the 25 Hz cadence.
  - In Ai mode, remote jogs are ignored (read nothing).
- [ ] `main.cpp`: pass `&controller` as the remote sink.
- [ ] `CLAUDE.md`: update the Network paragraph (the HUD's controls and mode button now work, protocol, dead man 300 ms, joystick wins, calibration joystick-only) and add `lib/remote` to Architecture.
- [ ] Verify: firmware and pin_check builds SUCCESS, `pio check --severity=high --severity=medium` clean, native suite passes, clang-format clean. Commit.

### Task 4 (controller + user, later): hardware verification

- [ ] Manual mode: HUD arrows/WASD jog; diagonal works; release stops within about 0.3 s; closing the tab mid-jog stops within 0.3 s.
- [ ] HUD mode button toggles AI/manual; ignored in calibration; the joystick wins over a held HUD key.
- [ ] `idle0` with one HUD at least 60%; detection windows unchanged.
