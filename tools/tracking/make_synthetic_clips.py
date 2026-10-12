"""Generate deterministic synthetic 40x30 gray clips for the tracking bench (no camera needed).

Every clip is background 60 with +-3 noise and a 4x4 square of value 190 that moves 1 cell/frame. The grid is only
40 cells wide, so a square cannot run 60 frames in one direction: it bounces off the edges (x0 stays in 0..36)
and keeps moving at 1 cell/frame, which also exercises a velocity reversal in the tracker.
square_cross: the square enters at frame 10 at x0=2 and keeps moving until frame 69, then disappears (frames 70-89).
light_switch: background 60 for 30 frames, then the whole scene brightens to 140 (frames 30-59).
seed_object: 150 frames; the square sits at x0=2 in frames 0-19 (so it is in the detector's seed frame), moves
1 cell/frame in frames 20-69 and is gone from frame 70. Tests ghost clearing of a wrong seed background.
pause_leave: 150 frames; the square enters at x0=2 in frame 10, moves right until frame 29, is static at x0=22 in
frames 30-63 (2 s) and is gone from frame 64. Tests ghost clearing after a pause.
"""
import random
import sys
from pathlib import Path

W, H = 40, 30
SQUARE = 4
SQUARE_Y = 13  # rows 13-16: vertically centred
MAX_X0 = W - SQUARE  # 36: the right-most left edge that keeps the square in the grid


def noisy_background(rng, level):
    return bytearray(max(0, min(255, level + rng.randint(-3, 3))) for _ in range(W * H))


def bounce_x0(travel):
    """Left edge after `travel` cells of motion, reflecting between 0 and MAX_X0 (triangle wave, period 72)."""
    m = travel % (2 * MAX_X0)
    return m if m <= MAX_X0 else 2 * MAX_X0 - m


def draw_square(frame, x0, y0=SQUARE_Y):
    for y in range(y0, y0 + SQUARE):
        for x in range(x0, x0 + SQUARE):
            frame[y * W + x] = 190


def square_cross(rng):
    frames = []
    for f in range(90):
        frame = noisy_background(rng, 60)
        if 10 <= f < 70:
            draw_square(frame, bounce_x0(2 + (f - 10)))
        frames.append(bytes(frame))
    return frames


def light_switch(rng):
    return [bytes(noisy_background(rng, 60 if f < 30 else 140)) for f in range(60)]


def seed_object(rng):
    frames = []
    for f in range(150):
        frame = noisy_background(rng, 60)
        if f < 20:
            draw_square(frame, 2)
        elif f < 70:
            draw_square(frame, bounce_x0(2 + (f - 19)))
        frames.append(bytes(frame))
    return frames


def pause_leave(rng):
    frames = []
    for f in range(150):
        frame = noisy_background(rng, 60)
        if 10 <= f < 30:
            draw_square(frame, 2 + (f - 10))
        elif 30 <= f < 64:
            draw_square(frame, 22)
        frames.append(bytes(frame))
    return frames


CLIPS = {
    "square_cross": square_cross,
    "light_switch": light_switch,
    "seed_object": seed_object,
    "pause_leave": pause_leave,
}


def main(out_dir):
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for name, make in CLIPS.items():
        frames = make(random.Random(1))
        (out / f"{name}.gray").write_bytes(b"".join(frames))
        print(f"{name}: {len(frames)} frames")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "ESP32/bench/fixtures")
