# Stage 6 On-Device WiFi Provisioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Replace the compiled-in WiFi credentials with up to 3 networks saved in NVS, a WPA2 setup hotspot with a captive setup page, a PIN-protected WiFi page reachable from the HUD, and a non-blocking boot.

**Architecture:** A pure library `lib/provisioning` (namespace `prov`) holds every decision: saved-network list, choosing a network, the connection state machine (`WifiPlan`), PIN hashing and lockout, form decoding, NVS blob formats. Device glue in `lib/network` executes `WifiPlan` actions with the Arduino `WiFi` API from `loop()`, stores blobs with `Preferences`, polls `DNSServer` in setup mode and serves `/wifi` + `/api/wifi/*` on the existing `esp_http_server`. CRC-32 moves to a shared `lib/checksum`.

**Tech Stack:** ESP32 Arduino core 2.0.17 (ESP-IDF 4.4), C++17, PlatformIO, Unity; vanilla JS page; Python page generator.

**Spec:** `docs/superpowers/specs/2026-10-17-void-sentry-stage6-wifi-provisioning-design.md`

## Global Constraints

- **C++17 only.** `lib/provisioning` and `lib/checksum`: no Arduino or ESP-IDF headers, no heap, natively tested (must pass under ASan + UBSan in CI).
- **Credentials:** never read, print, copy or edit the gitignored WiFi credentials header in `ESP32/include/` or its `.example` template, and never run a shell command containing the word that file is named after (a hook blocks it). Use the Edit/Write tools for files that mention it; for a commit message that mentions it, write the message to a file and use `git commit -F`. Deleting the template is left to the user (Task 5).
- **Never log or return a WiFi password or the PIN.** Logs may name SSIDs.
- **Locking:** no blocking calls or logging inside portMUX critical sections; shared WiFi state between the httpd task (core 0) and `loop()` (core 1) uses a FreeRTOS mutex (NVS writes are allowed under it).
- **`pio`:** `~/.platformio/penv/Scripts/pio.exe`, run from `ESP32/`. Never pipe it into `head` / `grep -m1`; redirect to a log file.
- **Native env:** needs WinLibs GCC 14 first on PATH: `export PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin | head -1):$PATH"`.
- **Style:** no em dashes. Device classes use a leading underscore and `this->`; pure libs follow `ESP32/lib/aiming` and `ESP32/lib/remote`. Doxygen `@brief` on public items. clang-format 19 clean.
- **Commits:** trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Board:** implementers do not upload; the controller does hardware runs.
- **Numbers (verbatim from the spec):** max 3 networks; SSID 1-32 bytes; password empty or 8-63 characters; PIN 4-8 ASCII digits; salt 16 bytes; SHA-256 hash 32 bytes; 5 wrong PINs -> 60 s lockout, doubling per further lockout, cap 3600 s, reset on success; join timeout 15 s per candidate; lost -> setup after 120 s; setup-mode rescan every 30 s; form body max 256 B; hotspot SSID `VoidSentry-XXXX` (last 2 MAC bytes, uppercase hex), IP `192.168.4.1`, password 10 random characters from `esp_random`; NVS namespace `wifi`, keys `nets`, `pin`, `appw`.

## Interfaces (fixed; every task uses exactly these)

`ESP32/lib/checksum/include/crc32.h`:
```cpp
namespace checksum
{
/** @brief CRC-32 (IEEE 802.3, reflected, init 0xFFFFFFFF, final xor 0xFFFFFFFF). */
uint32_t crc32(const uint8_t* data, size_t len);
}
```
`aiming::crc32` stays callable with the same results (`namespace aiming { using checksum::crc32; }` in `calibration_blob.h`); the aiming implementation is removed.

`ESP32/lib/provisioning/include/prov_types.h`:
```cpp
namespace prov
{
constexpr size_t kMaxNetworks = 3;
constexpr size_t kSsidMax = 32;
constexpr size_t kPassMax = 64;   // buffer bound; valid passwords are empty or 8-63 chars
constexpr size_t kSaltLen = 16;
constexpr size_t kHashLen = 32;

struct NetworkEntry
{
    char ssid[kSsidMax + 1] = {};
    char pass[kPassMax + 1] = {};
    uint32_t last_ok_seq = 0; // larger = more recently added or joined
};

struct PinRecord
{
    bool set = false;
    uint8_t salt[kSaltLen] = {};
    uint8_t hash[kHashLen] = {};
};

struct ScanResult
{
    char ssid[kSsidMax + 1] = {};
    int8_t rssi = -127;
    bool secured = true;
};

struct Candidates
{
    uint8_t idx[kMaxNetworks] = {}; // indices into NetworkList, strongest first
    size_t count = 0;
};
}
```

`network_list.h`:
```cpp
class NetworkList
{
public:
    enum class AddResult { Added, Replaced, Evicted, Invalid };
    AddResult add(const char* ssid, size_t ssid_len, const char* pass, size_t pass_len);
    bool forget(const char* ssid, size_t ssid_len);
    void mark_joined(size_t index);             // last_ok_seq = ++seq
    int find(const char* ssid, size_t ssid_len) const; // -1 if absent
    size_t size() const;
    const NetworkEntry& at(size_t index) const;
    bool restore(const NetworkEntry& e);        // for deserialization; false if full or invalid
    uint32_t seq() const;
    void clear();
};
```
Rules: `add` validates (`valid_ssid`, `valid_password` from `form.h` semantics: SSID 1-32 bytes, password 0 or 8-63 bytes) and returns `Invalid` otherwise; an existing SSID (byte-exact) is replaced in place (`Replaced`); when full, the entry with the smallest `last_ok_seq` (ties: lowest index) is overwritten (`Evicted`); every add sets the entry's `last_ok_seq = ++seq`. `restore` keeps `seq() >= max(last_ok_seq)`.

`choose.h`:
```cpp
Candidates choose_networks(const ScanResult* scan, size_t scan_count, const NetworkList& list);
```
Saved networks present in the scan (byte-exact SSID), ordered by their strongest RSSI (several APs with one SSID: take the max), strongest first; ties keep list order; scan entries with an empty SSID are ignored.

`blobs.h`:
```cpp
constexpr size_t kNetsBlobSize = 4 + 1 + 1 + kMaxNetworks * (kSsidMax + 1 + kPassMax + 1 + 4) + 4;
constexpr size_t kPinBlobSize = 4 + 1 + 1 + kSaltLen + kHashLen + 4;
std::array<uint8_t, kNetsBlobSize> serialize_nets(const NetworkList& list);
std::optional<NetworkList> deserialize_nets(const uint8_t* data, size_t len);
std::array<uint8_t, kPinBlobSize> serialize_pin(const PinRecord& rec);
std::optional<PinRecord> deserialize_pin(const uint8_t* data, size_t len);
```
Layout: little-endian magic (`0x4E575356` "VSWN" for nets, `0x50575356` "VSWP" for pin), version byte `1`, count byte (nets) or set byte (pin), fixed-size fields (unused entries zeroed), CRC-32 (`checksum::crc32`) over everything before it. Reject: wrong length, magic, version, CRC, count > 3, an SSID or password field not NUL-terminated within its buffer, an entry that fails validation.

`sha256.h`: `std::array<uint8_t, 32> sha256(const uint8_t* data, size_t len);`

`form.h`:
```cpp
constexpr size_t kMaxFormBody = 256;
/** Finds key=value in an x-www-form-urlencoded body, decodes %XX and '+', writes value + NUL.
 *  False if the body is longer than kMaxFormBody, the key is missing, an escape is malformed,
 *  or the decoded value (plus NUL) does not fit out_cap. */
bool form_field(const char* body, size_t len, const char* key, char* out, size_t out_cap, size_t* out_len);
bool valid_ssid(const char* s, size_t len);      // 1-32 bytes
bool valid_password(const char* s, size_t len);  // 0, or 8-63 bytes of printable ASCII (32-126)
bool valid_pin(const char* s, size_t len);       // 4-8 ASCII digits
```

`pin_guard.h`:
```cpp
enum class PinCheck { Ok, Wrong, LockedOut, NotSet };
struct PinResult { PinCheck status = PinCheck::NotSet; uint32_t seconds_left = 0; };
class PinGuard
{
public:
    static constexpr uint32_t kMaxFailures = 5;
    static constexpr uint32_t kBaseLockoutMs = 60000;
    static constexpr uint32_t kMaxLockoutMs = 3600000;
    explicit PinGuard(const PinRecord& rec = PinRecord{});
    bool set_pin(const char* pin, size_t len, const uint8_t (&salt)[kSaltLen]); // false if !valid_pin
    PinResult check(const char* pin, size_t len, uint32_t now_ms);
    const PinRecord& record() const;
};
```
`hash = sha256(salt || pin)`; constant-time compare. While locked: `LockedOut` with seconds left (rounded up), nothing evaluated. The 5th consecutive wrong PIN starts a lockout of `min(60 s * 2^k, 3600 s)` where k = lockouts since the last success, and resets the failure count. `Ok` resets failures and k. Lockout state is RAM only. Wrap-safe time.

`wifi_plan.h`:
```cpp
enum class PlanState { Boot, Scanning, Joining, Connected, Lost, Setup };
enum class ActionKind { None, StartScan, Join, StartHotspot, StopHotspot };
enum class JoinFail { None, Timeout, WrongPassword, NotFound, Other };
struct Action { ActionKind kind = ActionKind::None; uint8_t network = 0; }; // Join: index into NetworkList
class WifiPlan
{
public:
    static constexpr uint32_t kJoinTimeoutMs = 15000;
    static constexpr uint32_t kLostToSetupMs = 120000;
    static constexpr uint32_t kSetupRescanMs = 30000;
    Action start(bool have_saved, uint32_t now_ms);
    Action on_scan_done(const Candidates& c, uint32_t now_ms);
    Action on_joined(uint32_t now_ms);
    Action on_join_failed(JoinFail why, uint32_t now_ms);
    Action on_disconnected(uint32_t now_ms);
    Action on_network_added(uint32_t now_ms);
    Action tick(uint32_t now_ms);
    PlanState state() const;
    bool hotspot_up() const;
    JoinFail last_failure() const;
};
```
Transitions: `start`: saved -> `Scanning` + `StartScan`; none -> `Setup` + `StartHotspot`. `on_scan_done`: candidates -> `Joining` + `Join(c.idx[0])` (remember the rest); none -> `Setup` + `StartHotspot` if the hotspot is not up, else `None`. `on_join_failed` / join timeout in `tick` (15 s after the Join): record the reason, try the next remembered candidate (`Join`), else `Setup` (+ `StartHotspot` if not up). `on_joined` (from Joining or Lost): `Connected`; `StopHotspot` if the hotspot is up, else `None`. `on_disconnected` from Connected: `Lost` (the Arduino auto-reconnect retries; plan issues nothing). `tick` in Lost for 120 s: `Setup` + `StartHotspot`. `tick` in Setup every 30 s since the last scan: `Scanning`-like rescan (`StartScan`, state stays Setup with the hotspot up; its `on_scan_done` with candidates -> Joining). `on_network_added`: from Setup or Lost -> `StartScan`; from Connected/Joining/Scanning -> `None` (stored only; used on the next loss or boot). Events that do not apply to the current state return `None` and change nothing. All time arithmetic wrap-safe (`uint32_t(now - t)`).

Device-side (Task 4) interfaces:
- `ESP32/lib/network/include/wifi_store.h`: `class WifiStore { bool load(prov::NetworkList&, prov::PinRecord&); bool save_nets(const prov::NetworkList&); bool save_pin(const prov::PinRecord&); String hotspot_password(); /* creates and stores on first call */ void erase_all(); }` (Arduino `Preferences`, namespace `wifi`).
- `ESP32/lib/network/include/wifi_manager.h`: `class WifiManager` (instance, not static): `void begin();` (non-blocking: loads NVS, `WiFi.mode`, starts the plan), `void maintain();` (from `loop()`: WiFi events/scan polling, plan `tick`, executes actions, `DNSServer::processNextRequest()` in setup mode, serial `wifi-reset`), and thread-safe accessors for the HTTP layer under a mutex: `Status status()` (`mode` string `sta|setup|joining|lost`, current SSID, IP string, `pin_set`, last failure string), `size_t scan(prov::ScanResult* out, size_t cap)` (cached; requests a new async scan if older than 10 s), `size_t saved(char (*ssids)[prov::kSsidMax + 1], size_t cap)`, `AddOutcome add(ssid, pass, pin, now)` and `ForgetOutcome forget(ssid, pin, now)` with outcomes `Ok | BadInput | WrongPin | LockedOut` (plus `seconds_left` for LockedOut; a full list evicts, so there is no Full outcome) mapped by the HTTP layer to 200 / 400 / 403 / 429.
- `ESP32/lib/network/include/wifi_http.h`: `void register_wifi_routes(httpd_handle_t server, WifiManager& wifi);` registers `/wifi`, `/api/wifi/status`, `/api/wifi/scan`, `/api/wifi/saved`, `POST /api/wifi/add`, `POST /api/wifi/forget`, and a catch-all `GET /*` (setup mode: 302 to `http://192.168.4.1/wifi`; otherwise 404). Requires `config.uri_match_fn = httpd_uri_match_wildcard` in `HttpServer::start`; the catch-all is registered last so exact routes win. POSTs call `handshake_origin_ok` (`handshake_guard.h`) first; GETs do not.

## Review Focus

1. **Special characters survive end to end:** an SSID or password containing `&`, `=`, `%`, `+`, spaces or UTF-8 bytes is stored byte-exact (form decode -> NetworkList -> blob -> NVS -> join). Tests: Task 1 blob round trip with such bytes; Task 2 form decode of `p%26ss%3D%2B%25+x` -> `p&ss=+% x` and a UTF-8 SSID.
2. **Adding a network while connected must not drop the current connection:** `on_network_added` in Connected returns `None`. Test in Task 2.
3. **Captive-portal probes are hostnames:** GET requests are never origin-checked and the catch-all redirects them in setup mode; only POSTs are origin-checked (Task 4).
4. **A failed setup join leaves setup usable:** after `on_join_failed` with no more candidates in Setup, the hotspot stays up (no second `StartHotspot`, no `StopHotspot`). Test in Task 2.
5. **Lockout cannot be bypassed by alternating add and forget:** one `PinGuard` instance guards both endpoints (Task 4); Task 2 tests that failures accumulate across calls regardless of caller.

### Task 1: `lib/checksum`, types, NetworkList, choose, blobs (pure)

**Files:** create `ESP32/lib/checksum/include/crc32.h`, `ESP32/lib/checksum/src/crc32.cpp`; modify `ESP32/lib/aiming/include/calibration_blob.h`, `ESP32/lib/aiming/src/calibration_blob.cpp` (remove its crc32 implementation, use checksum's); create `ESP32/lib/provisioning/include/{prov_types.h,network_list.h,choose.h,blobs.h}`, `ESP32/lib/provisioning/src/{network_list.cpp,choose.cpp,blobs.cpp}`, `ESP32/test/native/test_provisioning_store/test_provisioning_store.cpp`.

`network_list.cpp` needs the validation rules; implement them as file-local helpers matching `form.h`'s rules exactly (Task 2 owns `form.h`; do not create it).

- [ ] Tests first (Unity, style of `ESP32/test/native/test_remote`): crc32 known value (`"123456789"` -> `0xCBF43926`) and the existing calibration tests still pass; NetworkList add/replace/evict (fill 3, mark_joined one, add a 4th: the oldest non-joined is evicted; ties lowest index), invalid SSID/password rejected, forget, find byte-exact (`"Home"` != `"home"`); choose ordering by RSSI, duplicate SSIDs take max, ties keep list order, empty scan SSIDs ignored, no matches -> count 0; blob round trips (including SSID/password bytes `&=%+ ` and UTF-8 `0xD7 0xA9`), each rejection case listed under `blobs.h`, an all-zero buffer, `seq()` restored.
- [ ] Implement; run `pio test -e native` (all suites pass); clang-format; commit "Add pure provisioning store: network list, chooser, NVS blobs; share CRC-32".

### Task 2: SHA-256, PinGuard, form, WifiPlan (pure; after Task 1)

**Files:** create `ESP32/lib/provisioning/include/{sha256.h,pin_guard.h,form.h,wifi_plan.h}`, `ESP32/lib/provisioning/src/{sha256.cpp,pin_guard.cpp,form.cpp,wifi_plan.cpp}`, `ESP32/test/native/test_provisioning_logic/test_provisioning_logic.cpp`.

- [ ] Tests first: SHA-256 NIST vectors (`""` -> `e3b0c442...b855`, `"abc"` -> `ba7816bf...15ad`, the 448-bit `"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"` -> `248d6a61...06c1`, and one million `'a'` -> `cdc76e5c...c7d1` fed as one buffer built in a static array); PinGuard: NotSet before set, invalid pins rejected by set_pin, Ok/Wrong, 4 wrongs then Ok resets, 5 wrongs -> LockedOut 60 s (seconds_left 60 at once, 1 at +59.5 s, unlocked at +60 s), second lockout 120 s, cap 3600 s, Ok after lockout resets k, wrap-safe across `UINT32_MAX`, record() round trip; form: plain field, `+` -> space, `%26`, `p%26ss%3D%2B%25+x` -> `p&ss=+% x`, UTF-8 `%D7%A9`, key prefix not confused (`xssid=` vs `ssid=`), missing key, malformed `%4`, `%G1`, body of 257 bytes rejected, value exactly filling out_cap-1 accepted and one more rejected, validation bounds (SSID 0/1/32/33, password 0/7/8/63/64 and a control char, PIN 3/4/8/9 and a letter); WifiPlan: every transition above, join timeout at exactly 15000 ms (14999 no), lost -> setup at 120000, setup rescan at 30000, `on_network_added` in Connected -> None (Review Focus 2), failed setup join keeps the hotspot (Review Focus 4), second candidate tried after the first fails, timers wrap-safe.
- [ ] Implement; run native; clang-format; commit "Add pure provisioning logic: SHA-256, PIN guard, form decoding, WiFi plan".

### Task 3: setup page and HUD link (web only; parallel with Tasks 1-2)

**Files:** create `web/wifi_setup.html`; modify `web/update_index_html.py`, `web/html_header.py` (generate a second header `ESP32/lib/network/include/wifi_html.h` with symbols `WIFI_PAGE_GZ` / `WIFI_PAGE_GZ_LEN`, same gzip `mtime=0` determinism and write-if-changed behaviour; keep `index_html.h` unchanged), `web/test_*.py` (cover the second header), `.gitignore` (ignore `wifi_html.h` like `index_html.h`), `web/web_ui.html` (a "WiFi" link to `/wifi` and the current SSID fetched from `GET /api/wifi/status` on load and every 10 s; do not touch the `/commands` code).

- [ ] `wifi_setup.html`: self-contained (inline CSS/JS, no external resources - the hotspot has no internet), mobile-first, matches the HUD's dark style. Shows status (`GET /api/wifi/status`: mode, SSID, IP, last failure); nearby networks (`GET /api/wifi/scan`, refresh button; tap fills the SSID field); saved networks (`GET /api/wifi/saved`) each with a Forget button; an Add form (SSID, password with show/hide, PIN; label "Choose a PIN (4-8 digits)" when `pin_set` is false, else "PIN"). POSTs use `application/x-www-form-urlencoded` (`URLSearchParams`) to `/api/wifi/add` and `/api/wifi/forget`. Responses: 200 ok, 400 bad input message, 403 wrong PIN, 429 `{"locked_s":N}` with a live countdown that disables the buttons. After an add while in setup mode, poll status every 2 s and show "Joining <ssid>...", then success with the new IP or the failure reason. Never store or log passwords or the PIN (no localStorage, no console.log of form data).
- [ ] Verify: `node --check` on the extracted scripts of both pages; `python -m unittest discover -s web -p "test_*.py"`; no em dashes. Commit "Add WiFi setup page and HUD WiFi link; generate wifi_html.h".

### Task 4: firmware integration (after Tasks 1-3)

**Files:** create `ESP32/lib/network/include/wifi_store.h`, `ESP32/lib/network/src/wifi_store.cpp`, `ESP32/lib/network/include/wifi_http.h`, `ESP32/lib/network/src/wifi_http.cpp`; rewrite `ESP32/lib/network/include/wifi_manager.h`, `ESP32/lib/network/src/wifi_manager.cpp`; modify `ESP32/lib/network/src/turret_server.cpp` (+ header) to set the wildcard matcher and call `register_wifi_routes`, raise `max_uri_handlers` if needed; `ESP32/src/main.cpp` (drop the credentials include and the blocking connect; `wifi.begin()` before `http_server.start(...)`, `wifi.maintain()` in `loop()`, `wifi=` field appended to the `[detect]` stats line); `.github/workflows/ci.yml` (remove the step that copies the credentials template, with the Edit tool); the two on-device sketches `ESP32/test/test_wifi_manager/test_wifi_manager.cpp` and `ESP32/test/test_turret_server/test_turret_server.cpp` (drop the credentials include; test_wifi_manager checks that `begin()` returns at once and the plan reaches Setup or Connected within 40 s; test_turret_server starts the server after `begin()`); `CLAUDE.md` (Architecture: provisioning + WiFi; Required local setup: the header is no longer needed to build - keep the guard rules until the user removes the file; stats line `wifi=`; Commands: `wifi-reset`).

- [ ] Behaviour per the spec and the Interfaces above: AP+STA while the hotspot is up; `WiFi.setSleep(false)` and auto-reconnect kept; join failures mapped from the disconnect reason (`WIFI_REASON_AUTH_FAIL`/`AUTH_EXPIRE`/`4WAY_HANDSHAKE_TIMEOUT` -> WrongPassword, `NO_AP_FOUND` -> NotFound); successful join -> `mark_joined` + `save_nets`; hotspot SSID from `WiFi.macAddress()`; the hotspot password printed on serial each time setup mode starts (and only there); `wifi-reset` erases `nets` and `pin` and restarts; one `PinGuard` for both endpoints; first add with no PIN set requires a valid PIN and sets it (salt from `esp_fill_random`), then saves both; JSON escaping of SSIDs in responses (quotes, backslashes, control chars).
- [ ] Verify: firmware and pin_check builds SUCCESS; `pio check --severity=high --severity=medium` no defects; native suite passes; clang-format clean; `grep` shows no remaining include of the credentials header in `ESP32/` sources. Commit "Provision WiFi on the device: saved networks, setup hotspot and captive page, PIN; non-blocking boot" (write the message to a file and use `git commit -F` if it mentions the header).

### Task 5 (controller + user): hardware verification and cleanup

- [ ] The user removes the template and their local header (`git rm ESP32/include/<header>.example`, delete the local header) after their network is saved in NVS; then CLAUDE.md's guard rules and the hook are relaxed in a follow-up commit (asked first).
- [ ] Hardware checks from the spec's Testing section.
