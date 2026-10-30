"""Soak test: one HUD-like client held open for N seconds, with the serial stats lines logged alongside.

The client polls /commands (port 80) at about 25 Hz and holds one /stream viewer (port 81) with the 2 s keepalive.

Usage:
    python soak.py 192.168.1.50 COM3 300

Output:
    polls=...   number of /commands round trips, with median / p95 / max round trip in ms
    stream ...  frames received, fps, and `stalls(3s)`: times no message arrived for 3 s
    serial stats  the board's [stream] / [detect] lines (and any E ( / W ( lines) seen during the run;
                  watch heap_min, psram_min and the stack_free marks for drift

Needs pyserial and websockets (the PlatformIO penv Python has them). Serial is opened with DTR/RTS low, so the board
is not reset. Find the board's IP in its boot log.
"""
import argparse
import asyncio
import json
import threading
import time

import serial
import websockets


def read_serial(port, baud, lines, stop):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, baud, 0.2
    s.dtr = False
    s.rts = False
    s.open()
    while not stop.is_set():
        line = s.readline()
        if line:
            lines.append(line.decode("utf-8", "replace").rstrip())
    s.close()


async def poller(host, secs, stats):
    async with websockets.connect(f"ws://{host}/commands", max_size=None) as c:
        end = time.time() + secs
        while time.time() < end:
            t0 = time.perf_counter()
            await c.send(" ")
            json.loads(await c.recv())
            stats["rtt"].append((time.perf_counter() - t0) * 1000)
            await asyncio.sleep(max(0, 0.04 - (time.perf_counter() - t0)))


async def streamer(host, stream_port, secs, stats):
    async with websockets.connect(f"ws://{host}:{stream_port}/stream", max_size=None) as s:
        end = time.time() + secs
        last_ka = time.time()
        while time.time() < end:
            try:
                m = await asyncio.wait_for(s.recv(), 3)
            except asyncio.TimeoutError:
                stats["stalls"] += 1
                continue
            if isinstance(m, bytes) and m[:2] == b"\xff\xd8":
                stats["frames"] += 1
            if time.time() - last_ka > 2:
                await s.send(b"k")
                last_ka = time.time()


async def run(host, stream_port, secs):
    stats = {"rtt": [], "frames": 0, "stalls": 0}
    await asyncio.gather(poller(host, secs, stats), streamer(host, stream_port, secs, stats))
    rtt = sorted(stats["rtt"])
    print(
        f"polls={len(rtt)} ({len(rtt) / secs:.1f}/s) rtt median={rtt[len(rtt) // 2]:.1f} ms "
        f"p95={rtt[int(len(rtt) * 0.95)]:.1f} ms max={rtt[-1]:.1f} ms"
    )
    print(f"stream frames={stats['frames']} ({stats['frames'] / secs:.1f} fps) stalls(3s)={stats['stalls']}")


def main():
    ap = argparse.ArgumentParser(description="Hold /commands and /stream open and log the serial stats lines.")
    ap.add_argument("host", help="board IP address")
    ap.add_argument("serial_port", help="serial port, e.g. COM3")
    ap.add_argument("seconds", type=float, help="soak duration")
    ap.add_argument("--stream-port", type=int, default=81, help="port of /stream (default 81)")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    lines = []
    stop = threading.Event()
    t = threading.Thread(target=read_serial, args=(args.serial_port, args.baud, lines, stop), daemon=True)
    t.start()
    asyncio.run(run(args.host, args.stream_port, args.seconds))
    stop.set()
    t.join(2)
    print("--- serial stats ---")
    for line in lines:
        if "[stream]" in line or "[detect]" in line or "E (" in line or "W (" in line:
            print(line)


if __name__ == "__main__":
    main()
