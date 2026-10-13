"""Record N seconds of JPEG frames from the turret's /stream WebSocket into a folder.

Usage: python tools/tracking/record_stream.py <board_ip> <seconds> <out_dir>
Read the board IP from the boot log (DHCP may change it after a reflash). Needs: pip install websockets
"""
import asyncio
import sys
import time
from pathlib import Path

import websockets


async def record(ip, seconds, out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    count = 0
    async with websockets.connect(f"ws://{ip}/stream", max_size=None) as ws:
        await ws.send(b" ")  # same 1-byte keepalive the HUD sends; also marks us as a viewer
        end = time.monotonic() + seconds
        last_keepalive = time.monotonic()
        while time.monotonic() < end:
            msg = await asyncio.wait_for(ws.recv(), timeout=5)
            if isinstance(msg, bytes) and msg[:2] == b"\xff\xd8":
                (out / f"{count:05d}.jpg").write_bytes(msg)
                count += 1
            if time.monotonic() - last_keepalive > 2:
                await ws.send(b" ")
                last_keepalive = time.monotonic()
    return count


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    n = asyncio.run(record(sys.argv[1], float(sys.argv[2]), sys.argv[3]))
    print(f"saved {n} frames to {sys.argv[3]}")
