# Network benchmarks

Host-side scripts that measure the `/stream` and `/commands` behaviour of a running board (method of notes s13). They
need `pyserial` and `websockets`; the PlatformIO penv Python has both (`~/.platformio/penv/Scripts/python.exe`). Serial
is opened with DTR/RTS low, so the board is not reset. Find the board's IP in its boot log. `/commands` is on port 80
and `/stream` on port 81 (`--stream-port` changes it). Each script's docstring and `--help` list the output fields.

| Script | Measures |
|---|---|
| `stall_capture.py` | One timeline of `/stream` frame arrivals, `/commands` round trips, ping and the board's serial stats, written as JSONL |
| `stall_analyze.py` | Summary of a capture: frame gaps, poll RTT percentiles, ping loss, plus the board's `[net]`, `[detect]`, `[stream]` lines |
| `soak.py` | Long run of one HUD-like client (25 Hz poll + one stream viewer): fps, poll RTT, 3 s stalls, serial stats for `heap_min` drift |
| `stuck_viewer.py` | Isolation: a `/stream` viewer that never reads, while `/commands` is polled |
| `read_serial.py` | Plain serial read without resetting the board |

```bash
PY=~/.platformio/penv/Scripts/python.exe
$PY tools/net_bench/stall_capture.py 192.168.1.50 COM3 180 stall.jsonl && $PY tools/net_bench/stall_analyze.py stall.jsonl
$PY tools/net_bench/soak.py 192.168.1.50 COM3 180
$PY tools/net_bench/stuck_viewer.py 192.168.1.50 40
```

## Pass criteria (notes s13)

- Soak, 3 min: no frame gap over 400 ms, `snd_fail` 0, poll RTT median about 60-70 ms and max well under 500 ms, `heap_min` steady (about 86 KB while streaming).
- Stuck viewer, 40 s: no `/commands` poll over 500 ms (median about 70 ms, max about 250 ms). The stuck viewer itself is dropped after the 4 s send timeout.
- A soak on a lossy link may still show stalls (TCP repairs losses in order). Judge by `snd_max_ms` and the poll RTT staying bounded, and compare runs at similar RSSI.
