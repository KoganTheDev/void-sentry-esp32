"""Stream-stall capture: one timeline (host monotonic clock) of /stream frame arrivals, /commands poll round trips,
ICMP ping and the board's serial stats lines.

Usage:
    python stall_capture.py 192.168.1.50 COM3 180 stall.jsonl
    python stall_analyze.py stall.jsonl

Output: JSON lines in <out>, one event per line, `t` = seconds since start. Kinds:
    frame   a /stream binary message arrived (n = bytes)
    poll    a /commands round trip (sent = send time, rtt in ms, vw = viewer count from the reply)
    ping    one ICMP echo (sent = send time, ms = round trip or null if lost)
    serial  one serial line from the board
    open / close   the stream socket opened / dropped (why = reason); the capture reconnects like the HUD does
stall_analyze.py summarises it.

Needs pyserial and websockets (the PlatformIO penv Python has them). Serial is opened with DTR/RTS low, so the board
is not reset. The ping uses the Windows `ping -n/-w` flags.
"""
import argparse
import asyncio
import json
import re
import subprocess
import threading
import time

import serial
import websockets

T0 = time.monotonic()
events = []
lock = threading.Lock()


def now():
    return time.monotonic() - T0


def emit(kind, **kw):
    with lock:
        events.append({"t": round(now(), 4), "k": kind, **kw})


def read_serial(port, baud, stop):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, baud, 0.2
    s.dtr = False
    s.rts = False
    s.open()
    while not stop.is_set():
        line = s.readline()
        if line:
            emit("serial", line=line.decode("utf-8", "replace").rstrip())
    s.close()


def pinger(host, stop):
    # Windows ping: one echo per call with a 1 s timeout, about 4 per second.
    while not stop.is_set():
        t = now()
        r = subprocess.run(["ping", "-n", "1", "-w", "1000", host], capture_output=True, text=True)
        m = re.search(r"time[=<](\d+)ms", r.stdout)
        emit("ping", sent=round(t, 4), ms=int(m.group(1)) if m else None)
        time.sleep(max(0, 0.25 - (now() - t)))


async def poller(host, secs):
    async with websockets.connect(f"ws://{host}/commands") as c:
        end = now() + secs
        while now() < end:
            t = now()
            await c.send(" ")
            r = json.loads(await c.recv())
            emit("poll", sent=round(t, 4), rtt=round((now() - t) * 1000, 1), vw=r.get("vw"))
            await asyncio.sleep(0.04)


async def streamer(host, stream_port, secs):
    end = now() + secs
    while now() < end:  # like the HUD: reconnect when the socket drops, and log why it dropped
        try:
            async with websockets.connect(f"ws://{host}:{stream_port}/stream", max_size=None, ping_interval=None) as s:
                emit("open")
                last_ka = now()
                while now() < end:
                    try:
                        m = await asyncio.wait_for(s.recv(), 1.0)
                        if isinstance(m, bytes):
                            emit("frame", n=len(m))
                    except asyncio.TimeoutError:
                        pass
                    if now() - last_ka > 2:
                        await s.send(b"k")
                        last_ka = now()
        except websockets.exceptions.ConnectionClosed as e:
            emit("close", why=str(e))
        except OSError as e:
            emit("close", why=repr(e))
        await asyncio.sleep(0.5)


async def run(host, stream_port, secs):
    await asyncio.gather(poller(host, secs), streamer(host, stream_port, secs))


def main():
    ap = argparse.ArgumentParser(description="Capture one timeline of stream frames, poll RTTs, ping and serial stats.")
    ap.add_argument("host", help="board IP address")
    ap.add_argument("serial_port", help="serial port, e.g. COM3")
    ap.add_argument("seconds", type=float, help="capture duration")
    ap.add_argument("out", help="output JSONL file")
    ap.add_argument(
        "--stream-port",
        type=int,
        default=81,
        help="port of /stream: 81 on the current firmware (/commands is on 80); use 80 for older builds",
    )
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    stop = threading.Event()
    threads = [
        threading.Thread(target=read_serial, args=(args.serial_port, args.baud, stop), daemon=True),
        threading.Thread(target=pinger, args=(args.host, stop), daemon=True),
    ]
    for th in threads:
        th.start()
    asyncio.run(run(args.host, args.stream_port, args.seconds))
    stop.set()
    for th in threads:
        th.join(3)
    with open(args.out, "w") as f:
        for e in events:
            f.write(json.dumps(e) + "\n")
    print(f"{len(events)} events -> {args.out}")


if __name__ == "__main__":
    main()
