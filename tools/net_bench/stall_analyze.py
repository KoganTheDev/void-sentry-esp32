"""Summarise a stall_capture.py timeline.

Usage:
    python stall_analyze.py stall.jsonl

Output, in order:
    frames    count, fps, gaps over 400 ms (how often), gaps over 1 s, longest gap, share of time frozen
    poll rtt  /commands round trip: median, p95, p99, max in ms
    ping      packets lost, median / p95 / max in ms
    frame bytes, stream socket closes
    then the board's [net] lines (snd_max_ms ...), idle0/idle1/rssi from [detect], viewers/heap_min/jpeg_b_avg from
    [stream], and any [W]/[E]/STREAM lines, each prefixed with the capture time in seconds
Gaps that line up with long poll round trips while ping stays low mean the board's httpd task was blocked (notes s13).
"""
import argparse
import json
import re


def q(v, p):
    return v[min(len(v) - 1, int(p * len(v)))]


def main():
    ap = argparse.ArgumentParser(description="Summarise a stall_capture.py JSONL timeline.")
    ap.add_argument("capture", help="JSONL file written by stall_capture.py")
    args = ap.parse_args()

    with open(args.capture) as f:
        E = [json.loads(l) for l in f]
    fr = [e for e in E if e["k"] == "frame"]
    po = [e for e in E if e["k"] == "poll"]
    pi = [e for e in E if e["k"] == "ping"]
    dur = fr[-1]["t"] - fr[0]["t"]
    gaps = [fr[i]["t"] - fr[i - 1]["t"] for i in range(1, len(fr))]
    big = sorted(g for g in gaps if g > 0.4)
    print(
        f"frames {len(fr)} in {dur:.0f} s = {len(fr) / dur:.1f} fps | gaps >400 ms: {len(big)} "
        f"(one per {dur / max(1, len(big)):.1f} s), >1 s: {sum(g > 1 for g in big)}, longest {max(gaps) * 1000:.0f} ms, "
        f"frozen time {sum(big):.1f} s = {100 * sum(big) / dur:.0f}%"
    )
    rt = sorted(p["rtt"] for p in po)
    print(f"poll rtt median {q(rt, .5):.0f} ms, p95 {q(rt, .95):.0f}, p99 {q(rt, .99):.0f}, max {rt[-1]:.0f}")
    pm = [p["ms"] for p in pi]
    pv = sorted(m for m in pm if m is not None)
    print(f"ping lost {pm.count(None)}/{len(pm)}, median {q(pv, .5)} ms, p95 {q(pv, .95)}, max {pv[-1]}")
    sz = sorted(e["n"] for e in fr)
    print(f"frame bytes median {q(sz, .5)}, p95 {q(sz, .95)}, max {sz[-1]}")
    closes = [e for e in E if e["k"] == "close"]
    print(f"stream socket closes: {len(closes)} {[c.get('why', '')[:70] for c in closes][:4]}")
    for e in E:
        if e["k"] != "serial":
            continue
        l = e["line"]
        if "[net]" in l:
            print(f"{e['t']:6.1f} {l[l.find('[net]'):]}")
        elif "[detect]" in l:
            m = re.search(r"idle0=(\d+) idle1=(\d+) rssi=(-?\d+)", l)
            if m:
                print(f"{e['t']:6.1f}   idle0={m.group(1)} idle1={m.group(2)} rssi={m.group(3)}")
        elif "[stream]" in l:
            m = re.search(r"viewers=(\d+).*heap_min=(\d+).*jpeg_b_avg=(\d+)", l)
            if m:
                print(f"{e['t']:6.1f}   viewers={m.group(1)} heap_min={m.group(2)} jpeg_b_avg={m.group(3)}")
        elif "[W]" in l or "[E]" in l or "STREAM" in l:
            print(f"{e['t']:6.1f}   {l[:160]}")


if __name__ == "__main__":
    main()
