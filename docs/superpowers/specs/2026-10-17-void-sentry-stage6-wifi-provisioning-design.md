# Void Sentry Stage 6: On-Device WiFi Provisioning

Date: 2026-10-17. Status: design approved in chat section by section; awaiting review of this written spec.

## Problem

The WiFi network is compiled into the firmware from a gitignored credentials header (`ESP32/include/secrets.h`, copied from `secrets.h.example`):

- A fresh clone does not build until someone creates that file, and CI copies the template to build at all.
- Moving the board to another network (home, college, a demo) means editing the file and reflashing.
- `setup()` blocks for up to 30 s in `WifiManager::connect()`, and its timeout compares against time since boot (`millis() > 30000`), not time since the connect started.
- If the network is unreachable there is no fallback: the board runs without a network and the HUD is unreachable.

## Goal

Both equally (user's answer): the user can switch the board between networks from a phone without reflashing, and anyone who clones the repo can flash it and set it up from a phone with no credentials file and no recompiling.

## Decisions

| Decision | Choice | Why | Rejected |
|---|---|---|---|
| Approach | Own implementation on the existing `esp_http_server`: SoftAP + DNS captive portal + setup page; pure host-tested logic | One server, one place for origin checks, no new tasks; supports several networks and a PIN | ESP-IDF `wifi_provisioning` (needs Espressif's phone app, one network); `WiFiManager` library (second web server, one network, no PIN, blocking portal) |
| Saved networks | Up to 3 in NVS; at boot, join the strongest saved network in range, then the next | Moving between known places needs no setup after the first time | One network (re-setup at every move) |
| Setup entry | Automatic when no saved network can be joined (first boot, new place, 2 min without any known network); in normal mode the HUD links to a WiFi page to add and forget networks | User's choice: automatic fallback plus management from the HUD; no button gesture (the joystick button is GPIO 0, the BOOT strapping pin, and its long press already means calibration) | Button gesture after boot |
| Hotspot | WPA2, SSID `VoidSentry-XXXX` (last 2 MAC bytes, hex), IP `192.168.4.1`; password: 10 random characters from the hardware RNG (`esp_random`), generated on first boot, stored in NVS, printed on serial whenever setup mode starts | The repo is public and the MAC is broadcast, so a password derived from the MAC would be computable by anyone; a random stored one is not | Open hotspot; MAC-derived password |
| PIN | Chosen at first setup (4-8 digits); required to add or forget networks from the setup page and the HUD; stored as a random 16-byte salt + SHA-256(salt + PIN); constant-time compare; 5 wrong tries -> 60 s lockout, doubling on each further lockout (cap 1 h), reset on success | Stops neighbours and LAN guests from repointing the board; online rate limiting is the real defence for a short PIN, and the hash never leaves the device | No PIN |
| PIN recovery | Typing `wifi-reset` in the serial monitor erases the saved networks and the PIN (physical access) | Physical access is the authority; no conflict with button gestures | Button hold |
| Credentials header | Removed from the build: `main.cpp` stops including it, CI drops the copy step, `secrets.h.example` is deleted from the repo. The user's local file is left alone (the user deletes it). CLAUDE.md rules and the guard hook stay until the user has moved their network into NVS, then are relaxed in a follow-up commit | A fresh clone builds as-is | Optional dev seed from the header |
| Boot | Non-blocking: WiFi is a state machine driven from `loop()`; the turret, joystick and detection are live within about 1 s of boot in every network state | `setup()` no longer waits up to 30 s | Blocking connect |
| Transport | Plain HTTP. Documented limitation: on the home network another device that knows the WiFi password could in principle read a network password typed into the HUD's WiFi page | HTTPS on this board needs certificates and a lot of RAM | HTTPS |

## Architecture

### Pure library `lib/provisioning` (namespace `prov`; no Arduino/ESP-IDF headers, no heap; native + ASan/UBSan tested)

- **`NetworkList`**: fixed array of up to 3 `{ssid[33], pass[65], last_ok_seq}`. `add` with an existing SSID replaces it; when full, the entry with the oldest `last_ok_seq` (the one longest without a successful join) is dropped. `forget(ssid)`, `mark_joined(ssid)`, iteration.
- **`choose_networks(scan, list) -> ordered candidates`**: saved networks present in the scan, strongest RSSI first; hidden networks (empty SSID in the scan) are ignored; ties keep list order.
- **`WifiPlan`** (connection state machine; events with `now_ms`, returns actions): states `Boot -> Scanning -> Joining(i) -> Connected`, `Connected -> Lost`, `Lost -> Connected` on reconnect, `Lost -> Setup` after 120 s, `Scanning/Joining -> Setup` when no candidate joins, `Setup -> Joining` when a background rescan (every 30 s) finds a saved network, and `Setup -> Joining` when a network is added. A successful join from setup mode stops the hotspot (`StopHotspot`) without a reboot, so the turret keeps running; a failed one returns to `Setup` with the reason. Join timeout 15 s per candidate. Actions: `StartScan`, `Join(index)`, `StartHotspot`, `StopHotspot`, `None`. Wrap-safe time arithmetic.
- **`PinGuard`**: `set(pin, salt)`, `check(pin, now_ms) -> Ok | Wrong | LockedOut(seconds_left)`, lockout rules above. Uses the library's own **SHA-256** (checked against the NIST FIPS 180-2 vectors), so host and board run the same code.
- **Form decoding**: `application/x-www-form-urlencoded` bodies up to 256 B, `%XX` and `+` decoding, field lookup; validation: SSID 1-32 bytes, password empty (open network) or 8-63 characters, PIN 4-8 ASCII digits.
- **Blobs**: `nets` and `pin` serialized with magic, version and CRC-32; unknown version, bad CRC, bad lengths rejected (treated as empty).
- **CRC-32** moves from `lib/aiming` (`calibration_blob.h`) into a tiny shared pure library `lib/checksum`, used by both; `aiming::crc32` callers are updated (no behaviour change, existing calibration blobs stay valid).

### Device side

- **`WifiManager`** (rewritten, `lib/network`): thin glue that feeds `WifiPlan` with WiFi events and scan results and executes its actions with the Arduino `WiFi` API (STA, or AP+STA in setup mode so background scans and test joins work while the hotspot is up). Called from `loop()` (`maintain()`), never blocks.
- **NVS** namespace `wifi`: keys `nets`, `pin`, `appw` (hotspot password).
- **DNS**: Arduino core `DNSServer` answering every name with `192.168.4.1`, polled from `loop()` only in setup mode. No new FreeRTOS tasks; the Stage 4 task layout is unchanged.
- **HTTP** (own file, registered on the existing `HttpServer`): `GET /wifi` (separate gzipped page built from `web/wifi_setup.html` by the existing generator), `GET /api/wifi/status` (mode, current SSID, IP, `pin_set`), `GET /api/wifi/scan` (cached scan: SSID, RSSI, secured), `GET /api/wifi/saved` (SSIDs only), `POST /api/wifi/add` (`ssid`, `password`, `pin`; on first setup `pin` sets the PIN), `POST /api/wifi/forget` (`ssid`, `pin`). Responses are JSON; passwords are never returned or logged. POSTs require the same Origin / IPv4-Host check as the WebSockets (blocks cross-site form posts). In setup mode, unknown URLs get a 302 to `/wifi` so phone captive-portal probes open the page.
- **HUD**: a "WiFi" link to `/wifi` and the current SSID in the status area.
- **Serial**: `wifi-reset` line read from `loop()`.
- **Stats line**: the `[detect]` line gains `wifi=<sta|setup|joining|lost>`.

## Error handling

| Case | Behaviour |
|---|---|
| Wrong password / network gone | Join times out after 15 s or fails with the ESP32 disconnect reason; the reason (`wrong password`, `not found`, `timeout`) is shown on the page; the next candidate is tried, or setup mode stays |
| Corrupt or unknown NVS blob | Treated as empty: setup mode, one log line, no crash |
| Too many wrong PINs | HTTP 429 with seconds left; the page shows a countdown |
| Network lost | Auto-reconnect; after 120 s without any saved network the hotspot opens while known networks keep being retried every 30 s; it leaves setup mode by itself when one joins |
| Oversized or malformed form body | HTTP 400, nothing stored |

## Testing

- **Host (native, plus ASan + UBSan in CI):** NetworkList add/replace/evict/forget; choose_networks ordering, ties, hidden SSIDs; every WifiPlan transition and timeout boundary (15 s, 120 s, 30 s) including a `uint32_t` wrap; PinGuard hash, wrong/ok, lockout, doubling, cap, reset; SHA-256 NIST vectors; form decoding (escapes, `+`, truncated `%`, oversize, missing fields) and validation bounds; blob round trips and each rejection; CRC-32 unchanged (existing calibration blob test still passes).
- **On the board, with the user:** first boot from erased NVS opens the hotspot and prints its password; the phone opens the captive page; save the network and set a PIN, it joins and the HUD loads on the home network; reboot joins without setup; a second saved network: the stronger is chosen; router off for more than 2 min -> hotspot, router back -> normal; wrong PIN lockout; `wifi-reset` clears everything; Stage 4 numbers unchanged.

## Success criteria

| Criterion | Target |
|---|---|
| Fresh clone | Builds with no credentials header; CI green without the copy step |
| Setup time | From erased NVS to the HUD on the home network in under 2 minutes from a phone |
| Boot | Turret and joystick live within about 1 s of boot in every network state |
| No regression | Detection windows unchanged; `idle0` at least 60% with one HUD |
| Logic | All new pure code natively tested; native, ASan + UBSan and CI pass |

## Out of scope

HTTPS, LAN authentication of the HUD and WebSockets (beyond the PIN for WiFi changes), mDNS names, enterprise (802.1X) networks, BLE provisioning.
