"""Isolation test: does a stuck /stream viewer delay /commands?

One /stream viewer connects with a tiny receive buffer and never reads, so its TCP window fills and the board's sends
to it block, like a retransmit wait. Meanwhile a /commands client polls at about 25 Hz and records round trips.

Usage:
    python stuck_viewer.py 192.168.1.50 60

Output: the handshake status line, then `polls N median .. p95 .. p99 .. max ..` in ms and `over 500 ms`, the number of
polls slower than 500 ms. If /stream shares a task with /commands, those numbers jump; with /stream on its own server
(port 81) they should stay near a normal round trip.

Needs websockets (the PlatformIO penv Python has it).
"""
import argparse
import asyncio
import base64
import os
import socket
import statistics
import time

import websockets


def stuck_stream(host, stream_port):
    s = socket.create_connection((host, stream_port))
    s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)  # small window: fills after ~1-2 frames
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(
        (
            f"GET /stream HTTP/1.1\r\nHost: {host}:{stream_port}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
        ).encode()
    )
    print("handshake:", s.recv(200).split(b"\r\n")[0].decode(), flush=True)
    return s  # never read again


async def run(host, stream_port, secs):
    s = stuck_stream(host, stream_port)
    rtts = []
    async with websockets.connect(f"ws://{host}/commands") as c:
        end = time.time() + secs
        while time.time() < end:
            t = time.perf_counter()
            await c.send(" ")
            await asyncio.wait_for(c.recv(), 10)
            rtts.append((time.perf_counter() - t) * 1000)
            await asyncio.sleep(0.04)
    s.close()
    rtts.sort()
    print(
        f"polls {len(rtts)} median {statistics.median(rtts):.0f} ms p95 {rtts[int(.95 * len(rtts))]:.0f} "
        f"p99 {rtts[int(.99 * len(rtts))]:.0f} max {rtts[-1]:.0f} | over 500 ms: {sum(r > 500 for r in rtts)}"
    )


def main():
    ap = argparse.ArgumentParser(description="A /stream viewer that never reads, while /commands is polled.")
    ap.add_argument("host", help="board IP address")
    ap.add_argument("seconds", type=float, help="test duration")
    ap.add_argument("--stream-port", type=int, default=81, help="port of /stream (default 81)")
    args = ap.parse_args()
    asyncio.run(run(args.host, args.stream_port, args.seconds))


if __name__ == "__main__":
    main()
