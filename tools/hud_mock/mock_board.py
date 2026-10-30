"""Fake Void Sentry board for HUD work: serve the HUD and speak the board's protocols, no hardware needed.

Usage (repo root; needs the `websockets` package, version 13 or newer, and Pillow):

    python tools/hud_mock/mock_board.py [--port 8080] [--host 127.0.0.1] [--dark] [--stall-after SECONDS]
                                        [--busy] [--uncalibrated] [--stick] [--narrow-limits]

then open http://127.0.0.1:8080 (the HUD) or http://127.0.0.1:8080/wifi (WiFi setup).

The real board serves /stream on its own port (81), so the HUD opens it there when it was served from the default
port 80. Served from any other port (as here) it opens /stream on that same port, so the mock needs one listener and
several mocks can run side by side.

What it serves:
    GET  /                    web/web_ui.html, read from disk on every request (edit, then reload)
    GET  /wifi                web/wifi_setup.html
    GET  /api/wifi/status|scan|saved   plausible JSON in the shapes documented in wifi_setup.html
    POST /api/wifi/add|forget          accept PIN 1234, 403 {"error":"wrong pin"} otherwise
    WS   /stream              one binary 320x240 JPEG per message at about 15 fps (a dim room and a walking
                              grey person). Max 2 viewers: a 3rd is accepted and closed at once, no frames, like
                              the board. Client messages (the HUD's 1-byte keepalive) are drained and ignored.
    WS   /commands            request/reply: exactly one JSON text reply per received text message, never an
                              unsolicited one. A binary frame or a message over 64 bytes closes the socket.

The protocol and the reply keys are the contract in docs/superpowers/specs/2026-10-26-hud-ux-overhaul-design.md.
The behaviour mirrors the firmware: lib/remote (parser, 300 ms dead man), lib/controller (modes, calibration button
semantics), lib/aiming (aim map, aim policy, calibration session).

Options:
    --dark             draw a very dark room (mean brightness under 20) to test the LOW LIGHT badge
    --stall-after N    stop sending frames N s after a viewer connects but keep the socket open (frozen video)
    --busy             accept every /stream viewer and close it at once, as the board does when full
    --uncalibrated     start with calibrated 0 and the default aim map (the default is a saved calibration)
    --narrow-limits    shrink the soft limits to pan -20..20 deg and tilt 75..105 deg: the reach box is clearly
                       smaller than the frame and the lim bits trigger after a short jog (out-of-reach and end-stop UI)
    --stick            the physical joystick "wins": while a HUD jog is held, stick is 1 and the jog is ignored

The simulation (classes Target, Turret, Board) is pure: it takes the time as an argument, so it is deterministic and
unit-tested (test_mock_board.py). Only the Server class touches sockets and the clock.

Why a small TCP front end instead of websockets' process_request alone: the websockets server only parses GET
requests, and the WiFi page needs POST with a form body. The front end answers plain HTTP itself and forwards
WebSocket upgrades byte for byte to an internal websockets server on a loopback port.
"""
import argparse
import asyncio
import io
import json
import math
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import NamedTuple, Optional
from urllib.parse import parse_qsl

REPO_ROOT = Path(__file__).resolve().parents[2]

# ---- constants mirrored from the firmware (lib/aiming/include/aim_map.h and friends) ----
FRAME_W, FRAME_H = 320, 240
PAN_MIN, PAN_MAX = -90.0, 90.0
TILT_MIN, TILT_MAX, TILT_HOME = 20.0, 160.0, 90.0


class Limits(NamedTuple):
    """Soft limits in degrees. The firmware's are FULL_LIMITS; --narrow-limits uses NARROW_LIMITS."""

    pan_min: float = PAN_MIN
    pan_max: float = PAN_MAX
    tilt_min: float = TILT_MIN
    tilt_max: float = TILT_MAX


FULL_LIMITS = Limits()
NARROW_LIMITS = Limits(-20.0, 20.0, 75.0, 105.0)  # makes the reach box smaller than the frame
MIN_CAL_DX, MIN_CAL_DY = 80.0, 60.0
CAL_TOO_CLOSE = "points too close (need 80 px apart in x and 60 px in y)"
DEADBAND_DEG = 1.0
HOME_DELAY_MS = 2000
DEAD_MAN_S = 0.3
SPEEDS = {"slow": 0.25, "normal": 0.6, "fast": 1.0}
PAN_RATE = 25.0  # deg/s at full deflection and when slewing to a setpoint
TILT_RATE = 40.0
PAN_STOP_S = 0.1  # pan_speed() stays non-zero this long after the jog is zeroed (the stepper decelerates)
CAL_RECORD_TIMEOUT_S = 1.0  # a calibration record waits at most this long for the pan to stop
LIMIT_EPS_DEG = 0.5
MAX_MESSAGE = 64
MAX_VIEWERS = 2
FPS = 15
STEP_S = 0.02  # simulation sub-step: dead man expiry and slewing stay accurate across big time jumps
MAX_CATCH_UP_S = 30.0

REPLY_KEYS = (
    "detected", "x", "y", "width", "height", "pixels", "state", "id", "bx", "by", "bw", "bh", "vx", "vy",
    "mode", "cal", "cal_state", "calibrated", "pan", "tilt", "cal_error",
    "cal_saves", "cal_miss", "stick", "ax", "ay", "lim", "rx0", "ry0", "rx1", "ry1", "home", "ret", "spd", "vw",
)  # fmt: skip


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


# ---------------------------------------------------------------------------------------------------------------
# Command parser (lib/remote/src/remote_command.cpp plus the new messages of the spec)
# ---------------------------------------------------------------------------------------------------------------
class Cmd(NamedTuple):
    kind: str  # poll move stop mode fire speed cal_start cal_point cal_cancel aim
    pan: int = 0
    tilt: int = 0
    mode: str = ""
    speed: str = ""
    x: int = 0
    y: int = 0


_MOVES = {
    "up": (0, 1), "down": (0, -1), "left": (-1, 0), "right": (1, 0),
    "up-left": (-1, 1), "up-right": (1, 1), "down-left": (-1, -1), "down-right": (1, -1),
}  # fmt: skip
_DIGITS = "0123456789"


def _plain_uint(s: str, max_digits: int = 3) -> Optional[int]:
    """Plain decimal, 1 to max_digits ASCII digits, no sign or spaces; else None."""
    if not 1 <= len(s) <= max_digits or any(c not in _DIGITS for c in s):
        return None
    return int(s)


def parse(message: str) -> Cmd:
    """Exact, case-sensitive match of one message; anything unknown or malformed is a poll."""
    if message.startswith("move:") and message[5:] in _MOVES:
        pan, tilt = _MOVES[message[5:]]
        return Cmd("move", pan=pan, tilt=tilt)
    if message == "move:stop":
        return Cmd("stop")
    if message in ("mode:ai", "mode:user"):
        return Cmd("mode", mode=message[5:])
    if message in ("fire:on", "fire:off"):
        return Cmd("fire")
    if message.startswith("speed:") and message[6:] in SPEEDS:
        return Cmd("speed", speed=message[6:])
    if message in ("cal:start", "cal:point", "cal:cancel"):
        return Cmd("cal_" + message[4:])
    if message.startswith("aim:"):
        parts = message[4:].split(",")
        if len(parts) == 2:
            x, y = _plain_uint(parts[0]), _plain_uint(parts[1])
            if x is not None and y is not None and x < FRAME_W and y < FRAME_H:
                return Cmd("aim", x=x, y=y)
    return Cmd("poll")


# ---------------------------------------------------------------------------------------------------------------
# Aim map (lib/aiming/src/aim_map.cpp)
# ---------------------------------------------------------------------------------------------------------------
@dataclass(frozen=True)
class AimMap:
    pan_a: float
    pan_b: float
    tilt_a: float
    tilt_b: float

    def aim(self, x: float, y: float, lim: Limits = FULL_LIMITS) -> tuple[float, float]:
        """Pan and tilt degrees for an image pixel, clamped to the soft limits."""
        return (
            clamp(self.pan_a * x + self.pan_b, lim.pan_min, lim.pan_max),
            clamp(self.tilt_a * y + self.tilt_b, lim.tilt_min, lim.tilt_max),
        )

    def inverse(self, pan: float, tilt: float) -> tuple[int, int]:
        """The image pixel the pose points at: rounded, not clamped (may be outside the frame)."""
        x = (pan - self.pan_b) / self.pan_a if abs(self.pan_a) > 1e-9 else FRAME_W / 2
        y = (tilt - self.tilt_b) / self.tilt_a if abs(self.tilt_a) > 1e-9 else FRAME_H / 2
        return math.floor(x + 0.5), math.floor(y + 0.5)

    def reach_box(self, lim: Limits = FULL_LIMITS) -> tuple[int, int, int, int]:
        """Image region whose (unclamped) aim lies inside the soft limits, clamped to the frame: rx0 ry0 rx1 ry1."""
        rx0, rx1 = _axis_reach(self.pan_a, self.pan_b, lim.pan_min, lim.pan_max, FRAME_W - 1)
        ry0, ry1 = _axis_reach(self.tilt_a, self.tilt_b, lim.tilt_min, lim.tilt_max, FRAME_H - 1)
        return rx0, ry0, rx1, ry1


def _axis_reach(a: float, b: float, lo: float, hi: float, n_max: int) -> tuple[int, int]:
    if abs(a) < 1e-9:
        return (0, n_max) if lo <= b <= hi else (0, 0)
    u, v = (lo - b) / a, (hi - b) / a
    if u > v:
        u, v = v, u
    r0, r1 = max(0, math.ceil(u)), min(n_max, math.floor(v))
    if r0 > r1:  # the reachable span lies outside the frame: degenerate box at the nearest edge
        edge = n_max if u > n_max else 0
        return edge, edge
    return r0, r1


def default_map() -> AimMap:
    """Camera next to the turret, 66 x 50 degree field of view, image centre = pan 0 / tilt 90."""
    pan_a = 66.0 / FRAME_W
    tilt_a = -50.0 / FRAME_H
    return AimMap(pan_a, -pan_a * (FRAME_W / 2.0), tilt_a, TILT_HOME - tilt_a * (FRAME_H / 2.0))


def fit(p1: tuple, p2: tuple) -> Optional[AimMap]:
    """Per-axis line through two (x, y, pan, tilt) points; None if too close in x or y."""
    dx, dy = p2[0] - p1[0], p2[1] - p1[1]
    if abs(dx) < MIN_CAL_DX or abs(dy) < MIN_CAL_DY:
        return None
    pan_a = (p2[2] - p1[2]) / dx
    tilt_a = (p2[3] - p1[3]) / dy
    return AimMap(pan_a, p1[2] - pan_a * p1[0], tilt_a, p1[3] - tilt_a * p1[1])


# A calibration "saved in NVS" from an earlier session (the default start state): close to, but not, the default map.
SAVED_MAP = fit((70.0, 50.0, -20.0, 103.0), (250.0, 190.0, 22.0, 67.0))


# ---------------------------------------------------------------------------------------------------------------
# The simulated tracked person
# ---------------------------------------------------------------------------------------------------------------
@dataclass(frozen=True)
class TrackSample:
    state: str  # idle tentative locked coasting
    id: int
    x: int = 0  # centroid
    y: int = 0
    bx: int = 0
    by: int = 0
    bw: int = 0
    bh: int = 0
    vx: float = 0.0  # pixels per frame
    vy: float = 0.0
    pixels: int = 0
    visible: bool = False  # the person is physically in the frame (what the video draws)

    @property
    def detected(self) -> bool:
        return self.state in ("locked", "coasting")


class Target:
    """A person walking a smooth loop, out of the frame 4 s of every 20 s. A pure function of the time."""

    CYCLE_S = 20.0
    ABSENT_S = 4.0
    TENTATIVE_S = 0.6
    COAST_S = 1.0
    BOX_W, BOX_H = 44, 74

    def center(self, t: float) -> tuple[float, float, float, float]:
        """Centroid and velocity (pixels per second) of the walk at time t."""
        wx, wy = 2 * math.pi / 13.0, 2 * math.pi / 9.0
        x = 160 + 105 * math.sin(wx * t)
        y = 120 + 60 * math.sin(wy * t + 0.8)
        return x, y, 105 * wx * math.cos(wx * t), 60 * wy * math.cos(wy * t + 0.8)

    def _sample_at(self, state: str, tid: int, t: float, visible: bool) -> TrackSample:
        x, y, vx, vy = self.center(t)
        bx, by = round(x - self.BOX_W / 2), round(y - self.BOX_H / 2)
        known_v = state in ("locked", "coasting")
        return TrackSample(
            state, tid, round(x), round(y), bx, by, self.BOX_W, self.BOX_H,
            vx / FPS if known_v else 0.0, vy / FPS if known_v else 0.0,
            int(self.BOX_W * self.BOX_H * 0.55), visible,
        )  # fmt: skip

    def sample(self, t: float) -> TrackSample:
        cycle = t % self.CYCLE_S
        start = t - cycle
        tid = int(t // self.CYCLE_S) + 1
        present_s = self.CYCLE_S - self.ABSENT_S
        if cycle < present_s:
            return self._sample_at("tentative" if cycle < self.TENTATIVE_S else "locked", tid, t, True)
        if cycle < present_s + self.COAST_S:  # left the frame: the tracker coasts on the last position
            return self._sample_at("coasting", tid, start + present_s, False)
        return TrackSample("idle", tid)  # the id stays until the next appearance


# ---------------------------------------------------------------------------------------------------------------
# Turret (lib/motion): jog or slew to a setpoint, soft limits
# ---------------------------------------------------------------------------------------------------------------
class Turret:
    def __init__(self, lim: Limits = FULL_LIMITS):
        self.lim = lim
        self.pan = 0.0
        self.tilt = TILT_HOME
        self.jog_pan = 0.0  # -1..1 of full deflection
        self.jog_tilt = 0.0
        self.target: Optional[tuple[float, float]] = None
        self._moving_until = 0.0

    def set_target(self, pan: float, tilt: float):
        self.target = (clamp(pan, self.lim.pan_min, self.lim.pan_max), clamp(tilt, self.lim.tilt_min, self.lim.tilt_max))
        self.jog_pan = self.jog_tilt = 0.0

    def home(self):
        self.set_target(0.0, TILT_HOME)

    def jog(self, pan: float, tilt: float):
        """A non-zero jog cancels the setpoint; a zero jog only stops the jog (a setpoint keeps slewing)."""
        self.jog_pan, self.jog_tilt = pan, tilt
        if pan != 0.0 or tilt != 0.0:
            self.target = None

    def stop(self):
        """jog(0, 0) as the firmware controller calls it on mode changes: stops everything, drops the setpoint."""
        self.jog_pan = self.jog_tilt = 0.0
        self.target = None

    def pan_speed_zero(self, t: float) -> bool:
        slewing = self.target is not None and abs(self.pan - self.target[0]) > 1e-3
        return self.jog_pan == 0.0 and not slewing and t >= self._moving_until

    def tick(self, t: float, h: float):
        if self.jog_pan != 0.0:
            self._moving_until = t + PAN_STOP_S
        if self.jog_pan != 0.0 or self.jog_tilt != 0.0:
            self.pan = clamp(self.pan + self.jog_pan * PAN_RATE * h, self.lim.pan_min, self.lim.pan_max)
            self.tilt = clamp(self.tilt + self.jog_tilt * TILT_RATE * h, self.lim.tilt_min, self.lim.tilt_max)
        elif self.target is not None:
            if abs(self.pan - self.target[0]) > 1e-3:
                self._moving_until = t + PAN_STOP_S
            self.pan += clamp(self.target[0] - self.pan, -PAN_RATE * h, PAN_RATE * h)
            self.tilt += clamp(self.target[1] - self.tilt, -TILT_RATE * h, TILT_RATE * h)

    def limits(self) -> int:
        bits = 0
        bits |= 1 if self.pan <= self.lim.pan_min + LIMIT_EPS_DEG else 0
        bits |= 2 if self.pan >= self.lim.pan_max - LIMIT_EPS_DEG else 0
        bits |= 4 if self.tilt <= self.lim.tilt_min + LIMIT_EPS_DEG else 0
        bits |= 8 if self.tilt >= self.lim.tilt_max - LIMIT_EPS_DEG else 0
        return bits


# ---------------------------------------------------------------------------------------------------------------
# The board: controller + inbox + calibration + AI policy
# ---------------------------------------------------------------------------------------------------------------
class Board:
    """Everything /commands needs. Time (seconds, any origin) is always passed in; nothing here reads a clock."""

    def __init__(self, target: Optional[Target] = None, uncalibrated: bool = False, stick_mode: bool = False,
                 viewers=lambda: 0, narrow_limits: bool = False):  # fmt: skip
        self.target = target or Target()
        self.stick_mode = stick_mode
        self._viewers = viewers
        self.lim = NARROW_LIMITS if narrow_limits else FULL_LIMITS
        self.turret = Turret(self.lim)
        self.mode = "ai"
        self.mode_before_cal = "ai"
        self.calibrated = not uncalibrated
        self.map = default_map() if uncalibrated else SAVED_MAP
        self.cal_saves = 0
        self.cal_miss = 0
        self.speed = "normal"
        self.stick = 0
        # calibration session (aiming::CalibrationSession)
        self.cal_state = "idle"  # idle point1 point2 failed
        self.cal_points = 0
        self.cal_error = ""
        self._cal_first: Optional[tuple] = None
        self._pending_record: Optional[float] = None  # deadline of a cal:point waiting for the pan to stop
        # remote inbox (remote::RemoteInbox)
        self._move: Optional[tuple[int, int, float]] = None
        # AI policy (aiming::AimPolicy)
        self._have_setpoint = False
        self._setpoint = (0.0, TILT_HOME)
        self._seen = False
        self._homed = False
        self._last_seen_ms = 0
        self._now = 0.0

    # ---- inbox ----
    def _jog_active(self, t: float) -> bool:
        return self._move is not None and t - self._move[2] < DEAD_MAN_S

    def _hud_jog(self, t: float) -> tuple[float, float]:
        if not self._jog_active(t):
            return 0.0, 0.0
        mag = SPEEDS[self.speed]
        return mag * self._move[0], mag * self._move[1]

    # ---- AI policy ----
    def _policy_reset(self):
        self._have_setpoint = False
        self._seen = False
        self._homed = False

    def _policy_update(self, s: TrackSample, t: float):
        now_ms = int(t * 1000)
        if s.detected:
            self._seen = True
            self._homed = False
            self._last_seen_ms = now_ms
            a = self.map.aim(s.x, s.y, self.lim)
            if (not self._have_setpoint or abs(a[0] - self._setpoint[0]) > DEADBAND_DEG
                    or abs(a[1] - self._setpoint[1]) > DEADBAND_DEG):  # fmt: skip
                self._setpoint = a
                self._have_setpoint = True
                self.turret.set_target(*a)
        elif s.state == "idle" and self._seen and not self._homed and now_ms - self._last_seen_ms >= HOME_DELAY_MS:
            self._homed = True
            self._have_setpoint = False
            self._setpoint = (0.0, TILT_HOME)
            self.turret.home()

    # ---- mode and calibration (lib/controller) ----
    def _set_mode(self, mode: str):
        self.mode = mode
        self._policy_reset()
        self.turret.stop()

    def _cal_start(self):
        self.mode_before_cal = self.mode
        self.mode = "calibrate"
        self._session_start()
        self._policy_reset()
        self.turret.stop()

    def _cal_cancel(self):
        self._session_cancel()
        self.mode = self.mode_before_cal
        self._pending_record = None
        self._policy_reset()
        self.turret.stop()

    def _session_start(self):
        self.cal_state, self.cal_points, self.cal_error, self._cal_first = "point1", 0, "", None

    def _session_cancel(self):
        self.cal_state, self.cal_points, self.cal_error, self._cal_first = "idle", 0, "", None

    def _record(self, t: float):
        """The short press while calibrating (Controller::on_short_press + CalibrationSession::record)."""
        self.turret.stop()
        s = self.target.sample(t)
        pose = (self.turret.pan, self.turret.tilt)
        if self.cal_state == "failed":  # the next record restarts at point 1 without recording
            self._session_start()
            return
        if self.cal_state not in ("point1", "point2") or not s.detected:
            self.cal_miss += 1  # ignored: no locked or coasting target to point at
            return
        point = (float(s.x), float(s.y), pose[0], pose[1])
        if self.cal_state == "point1":
            self._cal_first, self.cal_points, self.cal_state = point, 1, "point2"
            return
        self.cal_points = 2
        fitted = fit(self._cal_first, point)
        if fitted is None:
            self.cal_state, self.cal_error = "failed", CAL_TOO_CLOSE
            return
        self.map = fitted  # saved to NVS in the firmware
        self.calibrated = True
        self.cal_saves += 1
        self._session_cancel()
        self.mode = "ai"
        self._policy_reset()

    # ---- commands ----
    def _apply(self, c: Cmd, t: float):
        if c.kind == "move":
            self._move = (c.pan, c.tilt, t)
        elif c.kind == "stop":
            self._move = None
        elif c.kind == "speed":
            self.speed = c.speed
        elif c.kind == "mode":
            if self.mode != "calibrate" and c.mode != self.mode:  # dropped while calibrating
                self._set_mode(c.mode)
        elif c.kind == "cal_start":
            if self.mode != "calibrate":
                self._cal_start()
        elif c.kind == "cal_cancel":
            if self.mode == "calibrate":
                self._cal_cancel()
        elif c.kind == "cal_point":
            if self.mode == "calibrate" and self._pending_record is None:
                # Zero the jog now; the pose is recorded on the first pass where the pan has stopped, or after 1 s.
                self.turret.jog(0.0, 0.0)
                self._move = None
                self._pending_record = t + CAL_RECORD_TIMEOUT_S
        elif c.kind == "aim":
            if self.mode in ("user", "calibrate"):
                self._move = None  # cancels an active HUD jog
                self.turret.set_target(*self.map.aim(c.x, c.y, self.lim))
        # fire and poll: nothing

    def _tick(self, t: float):
        self.stick = 1 if self.stick_mode and self._jog_active(t) else 0
        if self._pending_record is not None and (self.turret.pan_speed_zero(t) or t >= self._pending_record):
            self._pending_record = None
            self._record(t)
        if self.mode in ("user", "calibrate") and self._pending_record is None:
            jog = (0.0, 0.0) if self.stick else self._hud_jog(t)
            self.turret.jog(*jog)
        elif self.mode == "ai":
            self._policy_update(self.target.sample(t), t)
        self.turret.tick(t, STEP_S)

    def advance(self, now: float):
        """Run the control loop up to `now` in 20 ms steps (a long gap is capped: nothing to see in between)."""
        if now - self._now > MAX_CATCH_UP_S:
            self._now = now - MAX_CATCH_UP_S
        while self._now + 1e-9 < now:
            h_end = min(self._now + STEP_S, now)
            self._now = h_end
            self._tick(h_end)

    def handle(self, message: str, now: float) -> str:
        """One /commands message in, exactly one JSON reply out."""
        self.advance(now)
        self._apply(parse(message), now)
        return self.reply(now)

    # ---- reply ----
    def home_ms(self, t: float) -> int:
        # Like AimPolicy: the timer also runs while a new candidate is only tentative.
        if self.mode != "ai" or not self._seen or self._homed or self.target.sample(t).state not in ("idle", "tentative"):
            return 0
        return max(0, HOME_DELAY_MS - (int(t * 1000) - self._last_seen_ms))

    def returning_home(self) -> int:
        far = abs(self.turret.pan) > 1.0 or abs(self.turret.tilt - TILT_HOME) > 1.0
        return 1 if self.mode == "ai" and self._homed and far else 0

    def reply_dict(self, t: float) -> dict:
        s = self.target.sample(t)
        ax, ay = self.map.inverse(self.turret.pan, self.turret.tilt)
        rx0, ry0, rx1, ry1 = self.map.reach_box(self.lim)
        d = {
            "detected": int(s.detected), "x": s.x, "y": s.y, "width": FRAME_W, "height": FRAME_H,
            "pixels": s.pixels, "state": s.state, "id": s.id, "bx": s.bx, "by": s.by, "bw": s.bw, "bh": s.bh,
            "vx": _f1(s.vx), "vy": _f1(s.vy),
            "mode": self.mode, "cal": self.cal_points, "cal_state": self.cal_state,
            "calibrated": int(self.calibrated), "pan": _f1(self.turret.pan), "tilt": _f1(self.turret.tilt),
            "cal_error": self.cal_error,
            "cal_saves": self.cal_saves, "cal_miss": self.cal_miss, "stick": self.stick,
            "ax": ax, "ay": ay, "lim": self.turret.limits(),
            "rx0": rx0, "ry0": ry0, "rx1": rx1, "ry1": ry1,
            "home": self.home_ms(t), "ret": self.returning_home(), "spd": self.speed, "vw": self._viewers(),
        }  # fmt: skip
        assert tuple(d) == REPLY_KEYS
        return d

    def reply(self, t: float) -> str:
        return json.dumps(self.reply_dict(t), separators=(",", ":"))


def _f1(v: float) -> float:
    return round(v, 1) + 0.0  # one decimal like the firmware's %.1f; +0.0 turns -0.0 into 0.0


# ---------------------------------------------------------------------------------------------------------------
# Video frames (Pillow)
# ---------------------------------------------------------------------------------------------------------------
class FrameRenderer:
    """Draws the dim room once and a grey person on top per frame."""

    def __init__(self, dark: bool = False):
        from PIL import Image, ImageDraw

        self._Image, self._ImageDraw = Image, ImageDraw
        k = 0.10 if dark else 1.0

        def c(r, g, b):
            return int(r * k), int(g * k), int(b * k)

        self._c = c
        bg = Image.new("RGB", (FRAME_W, FRAME_H))
        d = ImageDraw.Draw(bg)
        for y in range(FRAME_H):  # wall, slightly lighter towards the top
            d.line([(0, y), (FRAME_W, y)], fill=c(70 - y // 12, 68 - y // 12, 62 - y // 12))
        d.rectangle([0, 190, FRAME_W, FRAME_H], fill=c(48, 44, 40))  # floor
        d.line([(0, 190), (FRAME_W, 190)], fill=c(30, 28, 26), width=2)
        d.rectangle([24, 36, 96, 120], fill=c(110, 120, 130))  # window
        d.line([(60, 36), (60, 120)], fill=c(50, 50, 50), width=2)
        d.line([(24, 78), (96, 78)], fill=c(50, 50, 50), width=2)
        d.rectangle([236, 60, 300, 190], fill=c(52, 40, 30))  # door
        d.ellipse([288, 126, 294, 132], fill=c(150, 130, 60))
        d.rectangle([120, 150, 200, 158], fill=c(60, 50, 40))  # shelf
        self._bg = bg

    def render(self, s: TrackSample) -> bytes:
        img = self._bg.copy()
        if s.visible:
            c, d = self._c, self._ImageDraw.Draw(img)
            x0, y0, w, h = s.bx, s.by, s.bw, s.bh
            d.ellipse([x0 + 6, y0 + h - 8, x0 + w - 6, y0 + h], fill=c(30, 28, 26))  # shadow
            d.rounded_rectangle([x0 + 4, y0 + 22, x0 + w - 4, y0 + h - 22], radius=8, fill=c(140, 140, 142))  # torso
            d.rectangle([x0 + 10, y0 + h - 24, x0 + w - 10, y0 + h - 4], fill=c(110, 110, 114))  # legs
            d.ellipse([x0 + 12, y0, x0 + w - 12, y0 + 24], fill=c(170, 160, 150))  # head
        buf = io.BytesIO()
        img.save(buf, "JPEG", quality=80)
        return buf.getvalue()


# ---------------------------------------------------------------------------------------------------------------
# WiFi API (shapes from web/wifi_setup.html)
# ---------------------------------------------------------------------------------------------------------------
class WifiState:
    PIN = "1234"
    MAX_SAVED = 3

    def __init__(self):
        self.saved = ["HomeNet", "LabWiFi"]

    def status(self) -> dict:
        return {"mode": "sta", "ssid": self.saved[0] if self.saved else "", "ip": "192.168.1.57",
                "pin_set": True, "failure": ""}  # fmt: skip

    def scan(self) -> dict:
        nets = [("HomeNet", -48, True), ("LabWiFi", -61, True), ("CoffeeShop Guest", -72, False),
                ("Neighbor 5G", -80, True)]  # fmt: skip
        return {"networks": [{"ssid": s, "rssi": r, "secured": sec} for s, r, sec in nets]}

    def post(self, path: str, form: dict) -> tuple[int, dict]:
        if form.get("pin") != self.PIN:
            return 403, {"error": "wrong pin"}
        ssid = form.get("ssid", "")
        if not ssid or len(ssid.encode()) > 32:
            return 400, {"error": "bad ssid"}
        if path == "/api/wifi/forget":
            if ssid not in self.saved:
                return 400, {"error": "not saved"}
            self.saved.remove(ssid)
            return 200, {"ok": True}
        password = form.get("password")  # a missing key means an open network; a present one must be valid
        if password is not None and not 8 <= len(password) <= 63:
            return 400, {"error": "bad password"}
        if ssid not in self.saved:
            if len(self.saved) >= self.MAX_SAVED:
                return 400, {"error": "too many networks"}
            self.saved.append(ssid)
        return 200, {"ok": True}


# ---------------------------------------------------------------------------------------------------------------
# Server: HTTP front end + internal websockets server
# ---------------------------------------------------------------------------------------------------------------
_REASONS = {200: "OK", 400: "Bad Request", 403: "Forbidden", 404: "Not Found", 405: "Method Not Allowed"}


class Server:
    def __init__(self, args):
        self.args = args
        self.t0 = time.monotonic()
        self.viewers: set = set()
        self.wifi = WifiState()
        self.board = Board(uncalibrated=args.uncalibrated, stick_mode=args.stick, viewers=lambda: len(self.viewers),
                           narrow_limits=args.narrow_limits)
        self.renderer = FrameRenderer(dark=args.dark)
        self.inner_port = 0

    def clock(self) -> float:
        return time.monotonic() - self.t0

    # ---- websocket handlers (internal server) ----
    async def ws_handler(self, ws):
        path = ws.request.path.split("?")[0]
        if path == "/commands":
            await self.commands(ws)
        elif path == "/stream":
            await self.stream(ws)
        else:
            await ws.close(1008)

    async def commands(self, ws):
        async for message in ws:
            if isinstance(message, bytes) or len(message.encode()) > MAX_MESSAGE:
                await ws.close(1011)  # the board closes the session on a binary or oversized frame
                return
            await ws.send(self.board.handle(message, self.clock()))

    async def stream(self, ws):
        if self.args.busy or len(self.viewers) >= MAX_VIEWERS:
            return  # accepted, then closed at once with no frames
        self.viewers.add(ws)
        started = self.clock()

        async def drain():
            async for _ in ws:  # the HUD's keepalive "k"
                pass

        drain_task = asyncio.create_task(drain())
        try:
            next_at = time.monotonic()
            while not drain_task.done():
                if self.args.stall_after is not None and self.clock() - started >= self.args.stall_after:
                    await drain_task  # no more frames, socket stays open until the client leaves
                    break
                jpeg = await asyncio.to_thread(self.renderer.render, self.board.target.sample(self.clock()))
                await ws.send(jpeg)
                next_at += 1.0 / FPS
                await asyncio.sleep(max(0.0, next_at - time.monotonic()))
        except Exception:  # the client left mid-send
            pass
        finally:
            self.viewers.discard(ws)
            drain_task.cancel()

    # ---- HTTP front end ----
    def http_response(self, method: str, path: str, body: bytes) -> tuple[int, str, bytes]:
        if method == "POST":
            if path in ("/api/wifi/add", "/api/wifi/forget"):
                form = dict(parse_qsl(body.decode("utf-8", "replace"), keep_blank_values=True))
                status, obj = self.wifi.post(path, form)
                return status, "application/json", json.dumps(obj).encode()
            return 404, "text/plain", b"not found\n"
        if path in ("/", "/index.html"):
            return self.page("web_ui.html")
        if path == "/wifi":
            return self.page("wifi_setup.html")
        if path == "/api/wifi/status":
            return 200, "application/json", json.dumps(self.wifi.status()).encode()
        if path == "/api/wifi/scan":
            return 200, "application/json", json.dumps(self.wifi.scan()).encode()
        if path == "/api/wifi/saved":
            return 200, "application/json", json.dumps({"saved": self.wifi.saved}).encode()
        return 404, "text/plain", b"not found\n"

    @staticmethod
    def page(name: str) -> tuple[int, str, bytes]:
        try:
            return 200, "text/html; charset=utf-8", (REPO_ROOT / "web" / name).read_bytes()
        except OSError:
            return 404, "text/plain", f"web/{name} not found\n".encode()

    async def front(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        try:
            head = await asyncio.wait_for(reader.readuntil(b"\r\n\r\n"), 10)
            lines = head.decode("latin-1").split("\r\n")
            method, target, _ = lines[0].split(" ", 2)
            headers = {}
            for ln in lines[1:]:
                if ":" in ln:
                    k, v = ln.split(":", 1)
                    headers[k.strip().lower()] = v.strip()
            path = target.split("?")[0]
            if headers.get("upgrade", "").lower() == "websocket":
                await self.proxy(head, reader, writer)
                return
            length = min(int(headers.get("content-length", "0") or 0), 4096)
            body = await reader.readexactly(length) if length else b""
            if method not in ("GET", "HEAD", "POST"):
                status, ctype, payload = 405, "text/plain", b"method not allowed\n"
            else:
                status, ctype, payload = self.http_response("GET" if method == "HEAD" else method, path, body)
            out = (
                f"HTTP/1.1 {status} {_REASONS.get(status, 'OK')}\r\nContent-Type: {ctype}\r\n"
                f"Content-Length: {len(payload)}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
            ).encode()
            writer.write(out + (b"" if method == "HEAD" else payload))
            await writer.drain()
        except (asyncio.IncompleteReadError, asyncio.TimeoutError, ConnectionError, ValueError, asyncio.LimitOverrunError):
            pass
        finally:
            writer.close()

    async def proxy(self, head: bytes, reader, writer):
        """Forward a WebSocket upgrade, and everything after it, to the internal websockets server."""
        r2, w2 = await asyncio.open_connection("127.0.0.1", self.inner_port)
        w2.write(head)

        async def pipe(src, dst):
            try:
                while data := await src.read(65536):
                    dst.write(data)
                    await dst.drain()
            except (ConnectionError, OSError):
                pass
            finally:
                dst.close()

        await asyncio.gather(pipe(reader, w2), pipe(r2, writer))

    async def run(self):
        from websockets.asyncio.server import serve

        async with serve(self.ws_handler, "127.0.0.1", 0, compression=None, ping_interval=None) as inner:
            self.inner_port = inner.sockets[0].getsockname()[1]
            front = await asyncio.start_server(self.front, self.args.host, self.args.port)
            shown = "127.0.0.1" if self.args.host in ("0.0.0.0", "") else self.args.host
            print(f"mock board on http://{shown}:{self.args.port}/  (WiFi page: /wifi)", flush=True)
            async with front:
                await front.serve_forever()


def build_parser():
    ap = argparse.ArgumentParser(description="Fake Void Sentry board for HUD development.")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--dark", action="store_true", help="draw a very dark room (LOW LIGHT badge)")
    ap.add_argument("--stall-after", type=float, default=None, metavar="SECONDS",
                    help="stop sending frames this long after a viewer connects, keep the socket open")  # fmt: skip
    ap.add_argument("--busy", action="store_true", help="accept every /stream viewer and close it at once")
    ap.add_argument("--uncalibrated", action="store_true", help="start with calibrated 0 and the default map")
    ap.add_argument("--narrow-limits", action="store_true",
                    help="soft limits pan -20..20, tilt 75..105 (small reach box, easy to hit the end stops)")  # fmt: skip
    ap.add_argument("--stick", action="store_true", help="the joystick overrides HUD jogs (stick = 1 while held)")
    return ap


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        asyncio.run(Server(args).run())
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
