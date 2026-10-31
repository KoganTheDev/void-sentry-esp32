# HUD end-to-end checks

`hud_e2e.py` drives the real HUD (built from `web/hud/`) in a headless Chrome or Edge (raw Chrome DevTools Protocol over
websockets, no Playwright) against eight `tools/hud_mock/mock_board.py` instances, each started with the flags one
scenario needs (`--stall-after`, `--busy`, `--dark`, `--uncalibrated`, `--stick`). It runs 32 checks: help and keys,
manual jog, snapshot download, fire and auto-fire, the calibration wizard, frozen/busy video, low light, joystick
override, third-viewer refusal, the WiFi page and link loss.

```bash
python tools/hud_e2e/hud_e2e.py                      # autodetects Edge/Chrome/Chromium
python tools/hud_e2e/hud_e2e.py --browser /usr/bin/google-chrome --out /tmp/hud_e2e
```

Options: `--browser PATH`, `--out DIR` (browser profile and downloads, default a temp dir), `--base-port` (default
8090, uses 8 consecutive ports), `--debug-port` (default 9334). Needs `pip install websockets pillow`. Prints one
`PASS`/`FAIL` line per check and `N/M passed`; exits 1 if any check failed. CI runs it in the `hud-e2e` job.
