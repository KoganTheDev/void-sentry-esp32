"""Tests for the mock board simulation (run: python -m unittest discover -s tools/hud_mock -p "test_*.py")."""
import io
import json
import re
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import mock_board as mb  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "web"))
from hud_build import build_hud_page  # noqa: E402

# The shared reply contract: the ordered keys of a full /commands reply as the firmware emits them. See
# ReplyContractTests for how both sides are tied to it.
REPO = Path(__file__).resolve().parents[2]
SPEC_KEYS = json.loads((Path(__file__).parent / "reply_keys.json").read_text(encoding="utf-8"))


class FixedTarget(mb.Target):
    """A tracker output that the test sets directly: state, centroid, and where the person is."""

    def __init__(self, state="locked", x=160, y=120):
        self.state, self.x, self.y = state, x, y

    def sample(self, t):
        if self.state == "idle":
            return mb.TrackSample("idle", 1)
        return mb.TrackSample(self.state, 1, self.x, self.y, self.x - 22, self.y - 37, 44, 74, 0.0, 0.0, 1800,
                              self.state != "coasting")  # fmt: skip


def board(state="locked", x=160, y=120, **kw):
    t = FixedTarget(state, x, y)
    return mb.Board(target=t, **kw), t


def r(b, msg, t):
    return json.loads(b.handle(msg, t))


def calibrate_two_points(b, tgt, t, p1=(60, 50), p2=(260, 190)):
    """Run a full HUD calibration; returns the time after it. Uses a jog between the points so the poses differ."""
    r(b, "mode:user", t)
    r(b, "cal:start", t)
    tgt.x, tgt.y = p1
    r(b, "aim:%d,%d" % p1, t)
    t += 3.0
    r(b, "cal:point", t)
    t += 0.5
    r(b, " ", t)  # the record happens on the first pass with the pan stopped, before the target moves on
    tgt.x, tgt.y = p2
    r(b, "aim:%d,%d" % p2, t)
    t += 3.0
    r(b, "cal:point", t)
    t += 0.5
    return r(b, " ", t), t


class ParserTests(unittest.TestCase):
    def test_moves(self):
        self.assertEqual(mb.Cmd("move", pan=-1, tilt=1), mb.parse("move:up-left"))
        self.assertEqual(mb.Cmd("move", pan=1, tilt=-1), mb.parse("move:down-right"))
        self.assertEqual(8, sum(mb.parse("move:" + d).kind == "move" for d in
                                "up down left right up-left up-right down-left down-right".split()))  # fmt: skip
        self.assertEqual("stop", mb.parse("move:stop").kind)

    def test_exact_and_case_sensitive(self):
        for bad in ("MOVE:up", "move:Up", "move:up ", " move:up", "move:", "mode:AI", "speed:Fast", "cal:Start"):
            self.assertEqual("poll", mb.parse(bad).kind, bad)

    def test_speed_mode_fire_cal(self):
        self.assertEqual("slow", mb.parse("speed:slow").speed)
        self.assertEqual("speed", mb.parse("speed:fast").kind)
        self.assertEqual("poll", mb.parse("speed:warp").kind)
        self.assertEqual("ai", mb.parse("mode:ai").mode)
        self.assertEqual("user", mb.parse("mode:user").mode)
        self.assertEqual("fire", mb.parse("fire:on").kind)
        self.assertEqual("fire", mb.parse("fire:off").kind)
        self.assertEqual(["cal_start", "cal_point", "cal_cancel"],
                         [mb.parse(m).kind for m in ("cal:start", "cal:point", "cal:cancel")])  # fmt: skip

    def test_aim(self):
        self.assertEqual(mb.Cmd("aim", x=319, y=239), mb.parse("aim:319,239"))
        self.assertEqual(mb.Cmd("aim", x=0, y=0), mb.parse("aim:0,0"))
        self.assertEqual(mb.Cmd("aim", x=7, y=7), mb.parse("aim:7,007"))
        for bad in ("aim:320,10", "aim:10,240", "aim:-1,5", "aim:+1,5", "aim:1, 5", "aim:1,2,3", "aim:1", "aim:,",
                    "aim:1234,5", "aim:1.5,2", "aim:١,2", "aim:a,b", "aim:"):  # fmt: skip
            self.assertEqual("poll", mb.parse(bad).kind, bad)

    def test_unknown_and_empty_are_polls(self):
        self.assertEqual("poll", mb.parse("").kind)
        self.assertEqual("poll", mb.parse(" ").kind)
        self.assertEqual("poll", mb.parse("hello").kind)


class ReplyTests(unittest.TestCase):
    def test_exact_keys_in_spec_order(self):
        b, _ = board()
        self.assertEqual(SPEC_KEYS, list(json.loads(b.handle(" ", 1.0))))
        self.assertEqual(SPEC_KEYS, list(mb.REPLY_KEYS))

    def test_types(self):
        b, _ = board()
        d = r(b, " ", 1.0)
        for k in ("detected", "calibrated", "stick", "ret"):
            self.assertIn(d[k], (0, 1))
        for k in ("x", "y", "width", "height", "pixels", "id", "bx", "by", "bw", "bh", "cal", "cal_saves", "cal_miss",
                  "ax", "ay", "lim", "rx0", "ry0", "rx1", "ry1", "home", "vw", "auth"):  # fmt: skip
            self.assertIs(int, type(d[k]), k)
        for k in ("vx", "vy", "pan", "tilt"):
            self.assertIs(float, type(d[k]), k)
        for k in ("state", "mode", "cal_state", "cal_error", "spd"):
            self.assertIs(str, type(d[k]), k)

    def test_one_reply_per_message_for_every_kind(self):
        b, _ = board()
        for i, m in enumerate(["", " ", "move:up", "move:stop", "speed:fast", "mode:user", "fire:on", "cal:start",
                               "cal:point", "cal:cancel", "aim:1,1", "garbage"]):  # fmt: skip
            json.loads(b.handle(m, 1.0 + i * 0.01))

    def test_viewer_count(self):
        n = [2]
        b = mb.Board(target=FixedTarget(), viewers=lambda: n[0])
        self.assertEqual(2, r(b, " ", 1.0)["vw"])
        n[0] = 0
        self.assertEqual(0, r(b, " ", 1.1)["vw"])

    def test_starts_calibrated_or_not(self):
        self.assertEqual(1, r(board()[0], " ", 1.0)["calibrated"])
        self.assertEqual(0, r(board(uncalibrated=True)[0], " ", 1.0)["calibrated"])


class AccessTests(unittest.TestCase):
    """remote::gate in the mock: a viewer gets every reply, but its control commands change nothing."""

    CONTROL = ("move:right", "move:stop", "mode:user", "fire:on", "speed:fast", "cal:start", "cal:point",
               "cal:cancel", "aim:300,200")  # fmt: skip

    def test_auth_is_the_last_key_and_echoes_the_level(self):
        b, _ = board()
        for level in (mb.AUTH_VIEWER, mb.AUTH_AUTHED, mb.AUTH_OPEN):
            d = json.loads(b.handle(" ", 1.0, level))
            self.assertEqual("auth", list(d)[-1])
            self.assertEqual(level, d["auth"])
        self.assertEqual(mb.AUTH_OPEN, r(b, " ", 1.1)["auth"])  # the default: no gating

    def test_requires_auth_is_everything_but_a_poll(self):
        for m in self.CONTROL:
            self.assertTrue(mb.requires_auth(mb.parse(m)), m)
        for m in (" ", "", "garbage", "aim:999,1"):
            self.assertFalse(mb.requires_auth(mb.parse(m)), m)

    def test_viewer_commands_are_dropped_but_answered(self):
        b, _ = board(state="idle")
        before = r(b, " ", 1.0)
        t = 1.0
        for m in self.CONTROL:
            t += 0.05
            d = json.loads(b.handle(m, t, mb.AUTH_VIEWER))
            self.assertEqual(mb.AUTH_VIEWER, d["auth"], m)
        d = json.loads(b.handle(" ", t + 2.0, mb.AUTH_VIEWER))
        self.assertEqual(("ai", "idle", "normal", before["pan"], before["tilt"]),
                         (d["mode"], d["cal_state"], d["spd"], d["pan"], d["tilt"]))  # fmt: skip

    def test_authed_and_open_sessions_control(self):
        for level in (mb.AUTH_AUTHED, mb.AUTH_OPEN):
            b, _ = board(state="idle")
            self.assertEqual("user", json.loads(b.handle("mode:user", 1.0, level))["mode"])
            b.handle("move:right", 1.0, level)
            self.assertGreater(json.loads(b.handle(" ", 1.2, level))["pan"], 1.0)


class TargetTests(unittest.TestCase):
    def test_states_over_one_cycle(self):
        t = mb.Target()
        self.assertEqual("tentative", t.sample(0.1).state)
        self.assertEqual("locked", t.sample(0.7).state)
        self.assertEqual("locked", t.sample(15.9).state)
        self.assertEqual("coasting", t.sample(16.2).state)
        self.assertEqual("idle", t.sample(17.5).state)
        self.assertEqual("idle", t.sample(19.9).state)

    def test_id_increments_each_appearance(self):
        t = mb.Target()
        self.assertEqual(1, t.sample(5).id)
        self.assertEqual(1, t.sample(18).id)
        self.assertEqual(2, t.sample(20.1).id)
        self.assertEqual(3, t.sample(40.1).id)

    def test_box_and_frame(self):
        t = mb.Target()
        for i in range(0, 1600):
            s = t.sample(i * 0.01)
            if s.visible:
                self.assertEqual((44, 74), (s.bw, s.bh))
                self.assertTrue(0 <= s.bx and s.bx + s.bw <= 320 and 0 <= s.by and s.by + s.bh <= 240, (i, s))

    def test_stay_never_leaves(self):
        t = mb.Target(stay=True)
        for i in range(0, 4000):
            s = t.sample(i * 0.01)
            self.assertEqual(("locked", 1, True), (s.state, s.id, s.visible), i)

    def test_detected_only_locked_or_coasting(self):
        t = mb.Target()
        self.assertFalse(t.sample(0.1).detected)
        self.assertTrue(t.sample(5).detected)
        self.assertTrue(t.sample(16.5).detected)
        self.assertFalse(t.sample(18).detected)
        idle = t.sample(18)
        self.assertEqual((0, 0, 0, 0, 0), (idle.x, idle.y, idle.bw, idle.bh, idle.pixels))


class JogTests(unittest.TestCase):
    def test_dead_man(self):
        b, _ = board(state="idle")
        r(b, "mode:user", 1.0)
        r(b, "move:right", 1.0)
        r(b, " ", 1.2)
        moved = r(b, " ", 1.25)["pan"]
        self.assertGreater(moved, 1.0)  # about 0.25 s * 0.6 * 25 deg/s
        # no resend: the jog expires 300 ms after the move
        later = r(b, " ", 1.6)["pan"]
        again = r(b, " ", 3.0)["pan"]
        self.assertEqual(later, again)
        self.assertLess(later, 0.6 * 25 * 0.4)

    def test_resent_jog_keeps_moving_and_stop_ends_it(self):
        b, _ = board(state="idle")
        r(b, "mode:user", 1.0)
        t = 1.0
        for _ in range(10):
            r(b, "move:right", t)
            t += 0.1
        pan = r(b, "move:stop", t)["pan"]
        self.assertGreater(pan, 10.0)
        self.assertEqual(pan, r(b, " ", t + 1.0)["pan"])

    def test_directions_and_speeds(self):
        for speed, mag in (("slow", 0.25), ("normal", 0.6), ("fast", 1.0)):
            b, _ = board(state="idle")
            r(b, "mode:user", 1.0)
            self.assertEqual(speed, r(b, "speed:" + speed, 1.0)["spd"])
            r(b, "move:up-right", 1.0)
            d = r(b, " ", 1.2)
            self.assertAlmostEqual(mag * 25 * 0.2, d["pan"], delta=0.6)
            self.assertAlmostEqual(90 + mag * 40 * 0.2, d["tilt"], delta=0.8)
        b, _ = board(state="idle")
        r(b, "mode:user", 1.0)
        r(b, "move:down-left", 1.0)
        d = r(b, " ", 1.2)
        self.assertLess(d["pan"], 0)
        self.assertLess(d["tilt"], 90)

    def test_no_jog_in_ai_mode(self):
        b, _ = board(state="idle")
        self.assertEqual("ai", r(b, " ", 1.0)["mode"])
        r(b, "move:right", 1.0)
        self.assertEqual(0.0, r(b, " ", 1.2)["pan"])

    def test_soft_limits_and_lim_bits(self):
        b, _ = board(state="idle")
        r(b, "mode:user", 1.0)
        t = 1.0
        for _ in range(60):  # hold right for 6 s at 15 deg/s: past 90
            r(b, "move:right", t)
            t += 0.1
        d = r(b, " ", t)
        self.assertEqual(90.0, d["pan"])
        self.assertEqual(2, d["lim"])
        for _ in range(60):
            r(b, "move:up", t)
            t += 0.1
        d = r(b, " ", t)
        self.assertEqual(160.0, d["tilt"])
        self.assertEqual(2 | 8, d["lim"])

    def test_stick_option_ignores_hud_jog(self):
        b, _ = board(state="idle", stick_mode=True)
        r(b, "mode:user", 1.0)
        r(b, "move:right", 1.0)
        self.assertEqual(1, r(b, " ", 1.1)["stick"])
        self.assertEqual(0.0, r(b, " ", 1.2)["pan"])
        self.assertEqual(0, r(b, " ", 2.0)["stick"])  # dead man passed

    def test_stick_is_zero_without_the_option(self):
        b, _ = board(state="idle")
        r(b, "mode:user", 1.0)
        r(b, "move:right", 1.0)
        self.assertEqual(0, r(b, " ", 1.1)["stick"])


class ModeTests(unittest.TestCase):
    def test_mode_switch(self):
        b, _ = board()
        self.assertEqual("user", r(b, "mode:user", 1.0)["mode"])
        self.assertEqual("ai", r(b, "mode:ai", 1.1)["mode"])

    def test_mode_dropped_while_calibrating_never_deferred(self):
        b, _ = board()
        r(b, "mode:user", 1.0)
        self.assertEqual("calibrate", r(b, "cal:start", 1.1)["mode"])
        self.assertEqual("calibrate", r(b, "mode:ai", 1.2)["mode"])
        self.assertEqual("calibrate", r(b, "mode:user", 1.3)["mode"])
        d = r(b, "cal:cancel", 1.4)
        self.assertEqual("user", d["mode"])  # restored, and the dropped mode:ai was not applied later
        self.assertEqual("user", r(b, " ", 3.0)["mode"])


class CalibrationTests(unittest.TestCase):
    def test_start_and_cancel_restore_mode_and_map(self):
        b, _ = board(uncalibrated=True)
        r(b, "mode:user", 1.0)
        d = r(b, "cal:start", 1.0)
        self.assertEqual(("calibrate", "point1", 0), (d["mode"], d["cal_state"], d["cal"]))
        self.assertEqual("calibrate", r(b, "cal:start", 1.1)["mode"])  # ignored while calibrating
        d = r(b, "cal:cancel", 1.2)
        self.assertEqual(("user", "idle", 0, 0), (d["mode"], d["cal_state"], d["cal"], d["calibrated"]))
        self.assertEqual("user", r(b, "cal:cancel", 1.3)["mode"])  # ignored outside Calibrate
        self.assertEqual("user", r(b, "cal:point", 1.4)["mode"])

    def test_cancel_returns_to_ai_when_started_from_ai(self):
        b, _ = board()
        r(b, "cal:start", 1.0)
        self.assertEqual("ai", r(b, "cal:cancel", 1.1)["mode"])

    def test_happy_path(self):
        b, tgt = board(uncalibrated=True)
        r(b, "cal:start", 1.0)
        tgt.x, tgt.y = 60, 50
        r(b, "aim:60,50", 1.0)
        d = r(b, "cal:point", 4.0)
        self.assertEqual(("point1", 0), (d["cal_state"], d["cal"]))  # the record waits for the next control pass
        d = r(b, " ", 4.3)
        self.assertEqual(("point2", 1), (d["cal_state"], d["cal"]))
        tgt.x, tgt.y = 260, 190
        r(b, "aim:260,190", 4.4)
        r(b, "cal:point", 8.0)
        d = r(b, " ", 8.3)
        self.assertEqual(("ai", "idle", 0, 1, 1, 0, ""),
                         (d["mode"], d["cal_state"], d["cal"], d["calibrated"], d["cal_saves"], d["cal_miss"],
                          d["cal_error"]))  # fmt: skip
        # The fitted map sends the pixel of point 1 back to the pose recorded there.
        pan, tilt = b.map.aim(60, 50)
        dm = mb.default_map().aim(60, 50)
        self.assertAlmostEqual(dm[0], pan, delta=0.6)
        self.assertAlmostEqual(dm[1], tilt, delta=0.6)

    def test_pose_is_the_stopped_pose(self):
        b, tgt = board(uncalibrated=True)
        r(b, "mode:user", 1.0)
        r(b, "cal:start", 1.0)
        t = 1.0
        for _ in range(10):  # jog right for 1 s
            r(b, "move:right", t)
            t += 0.1
        r(b, "cal:point", t)  # jog zeroed here, pose recorded after the pan stopped
        d = r(b, " ", t + 0.5)
        self.assertEqual("point2", d["cal_state"])
        self.assertGreater(d["pan"], 10.0)
        self.assertEqual(d["pan"], r(b, " ", t + 3.0)["pan"])  # and the held HUD jog did not resume

    def test_too_close_fails_then_restarts_at_point1(self):
        b, tgt = board()
        r(b, "mode:user", 1.0)
        r(b, "cal:start", 1.0)
        tgt.x, tgt.y = 100, 100
        r(b, "cal:point", 2.0)
        tgt.x, tgt.y = 150, 130  # 50 px in x, 30 px in y: too close
        r(b, "cal:point", 3.0)
        d = r(b, " ", 3.5)
        self.assertEqual(("failed", 2, mb.CAL_TOO_CLOSE, "calibrate", 0), (d["cal_state"], d["cal"], d["cal_error"],
                                                                         d["mode"], d["cal_saves"]))  # fmt: skip
        self.assertEqual("points too close (need 80 px apart in x and 60 px in y)", d["cal_error"])
        r(b, "cal:point", 4.0)  # restart: no point is recorded, even with a target
        d = r(b, " ", 4.5)
        self.assertEqual(("point1", 0, ""), (d["cal_state"], d["cal"], d["cal_error"]))
        self.assertEqual(0, d["cal_miss"])
        r(b, "cal:point", 5.0)
        d = r(b, " ", 5.5)
        self.assertEqual(("point2", 1), (d["cal_state"], d["cal"]))

    def test_too_close_in_y_only_also_fails(self):
        b, tgt = board()
        r(b, "mode:user", 1.0)
        r(b, "cal:start", 1.0)
        tgt.x, tgt.y = 40, 100
        r(b, "cal:point", 2.0)
        tgt.x, tgt.y = 280, 140  # far in x, 40 px in y
        r(b, "cal:point", 3.0)
        self.assertEqual("failed", r(b, " ", 3.5)["cal_state"])

    def test_no_target_counts_a_miss_and_changes_nothing(self):
        b, tgt = board(state="idle")
        r(b, "mode:user", 1.0)
        r(b, "cal:start", 1.0)
        r(b, "cal:point", 2.0)
        d = r(b, " ", 2.5)
        self.assertEqual(("point1", 0, 1), (d["cal_state"], d["cal"], d["cal_miss"]))
        tgt.state = "tentative"  # tentative is not a target either
        r(b, "cal:point", 3.0)
        self.assertEqual(2, r(b, " ", 3.5)["cal_miss"])
        tgt.state = "coasting"  # coasting counts
        r(b, "cal:point", 4.0)
        d = r(b, " ", 4.5)
        self.assertEqual(("point2", 1, 2), (d["cal_state"], d["cal"], d["cal_miss"]))

    def test_point_while_pending_is_ignored(self):
        b, _ = board()
        r(b, "mode:user", 1.0)
        r(b, "cal:start", 1.0)
        r(b, "cal:point", 2.0)
        r(b, "cal:point", 2.0)  # a second press before the first was applied
        self.assertEqual(1, r(b, " ", 3.0)["cal"])

    def test_full_calibration_via_aim_helper(self):
        b, tgt = board(uncalibrated=True)
        d, _ = calibrate_two_points(b, tgt, 1.0)
        self.assertEqual(("ai", 1, 1), (d["mode"], d["calibrated"], d["cal_saves"]))


class AimTests(unittest.TestCase):
    def test_aim_moves_to_mapped_pose(self):
        b, _ = board(uncalibrated=True)
        r(b, "mode:user", 1.0)
        r(b, "aim:260,60", 1.0)
        d = r(b, " ", 5.0)
        pan, tilt = mb.default_map().aim(260, 60)
        self.assertAlmostEqual(pan, d["pan"], delta=0.11)
        self.assertAlmostEqual(tilt, d["tilt"], delta=0.11)
        self.assertAlmostEqual(260, d["ax"], delta=1)
        self.assertAlmostEqual(60, d["ay"], delta=1)

    def test_slew_rate_limits_motion(self):
        b, _ = board(uncalibrated=True)
        r(b, "mode:user", 1.0)
        r(b, "aim:319,120", 1.0)  # about 32.8 deg away
        d = r(b, " ", 1.5)
        self.assertAlmostEqual(25 * 0.5, d["pan"], delta=0.6)

    def test_aim_ignored_in_ai_and_when_malformed(self):
        b, _ = board(state="idle")
        r(b, "aim:300,100", 1.0)  # AI mode: a poll
        self.assertEqual(0.0, r(b, " ", 5.0)["pan"])
        r(b, "mode:user", 5.0)
        for bad in ("aim:320,100", "aim:100,240", "aim:-5,5", "aim:5", "aim:1,2,3"):
            r(b, bad, 5.0)
        d = r(b, " ", 9.0)
        self.assertEqual((0.0, 90.0), (d["pan"], d["tilt"]))

    def test_aim_cancels_the_jog(self):
        b, _ = board(uncalibrated=True)
        r(b, "mode:user", 1.0)
        r(b, "move:left", 1.0)
        r(b, "aim:160,120", 1.05)  # centre: pose (0, 90), the jog must not drag it left afterwards
        d = r(b, " ", 3.0)
        self.assertAlmostEqual(0.0, d["pan"], delta=0.11)

    def test_aim_works_in_calibrate(self):
        b, _ = board(uncalibrated=True)
        r(b, "cal:start", 1.0)
        r(b, "aim:300,200", 1.0)
        d = r(b, " ", 6.0)
        self.assertGreater(d["pan"], 20)
        self.assertLess(d["tilt"], 80)

    def test_aim_clamps_to_soft_limits_and_reach_box(self):
        b, tgt = board(uncalibrated=True)
        # A calibration where the pan is wildly off-centre: pixel 60 -> -80 deg, pixel 260 -> -20 deg (0.3 deg/px)
        r(b, "mode:user", 1.0)
        r(b, "cal:start", 1.0)
        tgt.x, tgt.y = 60, 50
        b.turret.pan, b.turret.tilt = -80.0, 120.0
        r(b, "cal:point", 2.0)
        r(b, " ", 2.5)
        tgt.x, tgt.y = 260, 190
        b.turret.pan, b.turret.tilt = -20.0, 60.0
        r(b, "cal:point", 3.0)
        r(b, " ", 3.5)
        d = r(b, " ", 3.6)
        self.assertEqual(1, d["calibrated"])
        # pan = 0.3*x - 98, so pan in -90..90 means x in 26.7..626.7 -> reach box starts at x=27, ends at the frame
        self.assertEqual((27, 319), (d["rx0"], d["rx1"]))
        self.assertEqual((0, 239), (d["ry0"], d["ry1"]))
        r(b, "mode:user", 3.7)
        r(b, "aim:0,120", 3.7)  # outside the reach box: lands at the pan limit
        d = r(b, " ", 12.0)
        self.assertEqual(-90.0, d["pan"])
        self.assertEqual(1, d["lim"] & 1)
        self.assertLessEqual(abs(d["ax"] - 27), 1)  # the aim pixel is where the turret stopped: the box edge

    def test_reach_box_function(self):
        self.assertEqual((0, 0, 319, 239), mb.default_map().reach_box())
        m = mb.AimMap(0.3, -98.0, -0.2, 120.0)  # tilt 20..160 -> y from -200 to 500; whole frame
        self.assertEqual((27, 0, 319, 239), m.reach_box())
        far = mb.AimMap(0.1, 200.0, -0.2, 90.0)  # pan 200..232: never reachable; degenerate box, rx0 <= rx1
        rx0, _, rx1, _ = far.reach_box()
        self.assertLessEqual(rx0, rx1)
        flipped = mb.AimMap(-0.3, 98.0, 0.2, 40.0)  # decreasing pan; box still ordered
        rx0, ry0, rx1, ry1 = flipped.reach_box()
        self.assertTrue(rx0 <= rx1 and ry0 <= ry1)

    def test_inverse_map_roundtrip_and_unclamped(self):
        m = mb.default_map()
        self.assertEqual((160, 120), m.inverse(0.0, 90.0))
        x, y = m.inverse(*m.aim(250, 70))
        self.assertEqual((250, 70), (x, y))
        ax, _ = m.inverse(90.0, 90.0)
        self.assertGreater(ax, 319)  # outside the frame, not clamped


class NarrowLimitsTests(unittest.TestCase):
    def test_reach_box_is_smaller_than_the_frame(self):
        b, _ = board(state="idle", uncalibrated=True, narrow_limits=True)
        d = r(b, " ", 1.0)
        # default map: pan 0.20625 deg/px -> x in 160 +- 97; tilt 0.2083 deg/px -> y in 120 +- 72 (48..192)
        self.assertEqual((64, 48, 256, 192), (d["rx0"], d["ry0"], d["rx1"], d["ry1"]))
        full = r(board(state="idle", uncalibrated=True)[0], " ", 1.0)
        self.assertEqual((0, 0, 319, 239), (full["rx0"], full["ry0"], full["rx1"], full["ry1"]))

    def test_jog_hits_the_end_stops(self):
        b, _ = board(state="idle", narrow_limits=True)
        r(b, "mode:user", 1.0)
        r(b, "speed:fast", 1.0)
        t = 1.0
        for _ in range(12):  # 1.2 s at 25 deg/s: past +20 pan; 40 deg/s up: past +105 tilt
            r(b, "move:up-right", t)
            t += 0.1
        d = r(b, " ", t)
        self.assertEqual((20.0, 105.0, 2 | 8), (d["pan"], d["tilt"], d["lim"]))
        for _ in range(30):
            r(b, "move:down-left", t)
            t += 0.1
        d = r(b, " ", t)
        self.assertEqual((-20.0, 75.0, 1 | 4), (d["pan"], d["tilt"], d["lim"]))

    def test_aim_outside_the_reach_box_stops_at_the_limit(self):
        b, _ = board(state="idle", uncalibrated=True, narrow_limits=True)
        r(b, "mode:user", 1.0)
        r(b, "aim:319,239", 1.0)
        d = r(b, " ", 8.0)
        self.assertEqual((20.0, 75.0, 2 | 4), (d["pan"], d["tilt"], d["lim"]))
        self.assertLessEqual(abs(d["ax"] - 256), 1)  # the aim pixel sits on the reach box edge, not at the finger
        self.assertLessEqual(abs(d["ay"] - 192), 1)


class AiTests(unittest.TestCase):
    def test_ai_follows_target_with_deadband(self):
        b, tgt = board(uncalibrated=True)
        tgt.x, tgt.y = 260, 60
        d = r(b, " ", 1.0)
        d = r(b, " ", 5.0)
        pan, tilt = mb.default_map().aim(260, 60)
        self.assertAlmostEqual(pan, d["pan"], delta=0.11)
        self.assertAlmostEqual(tilt, d["tilt"], delta=0.11)
        tgt.x = 261  # 0.2 deg: inside the deadband, no new setpoint
        d2 = r(b, " ", 8.0)
        self.assertEqual(d["pan"], d2["pan"])
        tgt.x = 200  # 12 deg: follow
        d3 = r(b, " ", 12.0)
        self.assertNotEqual(d["pan"], d3["pan"])

    def test_home_countdown_then_return(self):
        b, tgt = board(uncalibrated=True)
        tgt.x, tgt.y = 260, 60
        r(b, " ", 1.0)
        r(b, " ", 5.0)
        tgt.state = "idle"
        d = r(b, " ", 5.5)
        self.assertEqual(0, d["ret"])
        self.assertAlmostEqual(2000 - 500, d["home"], delta=60)
        d = r(b, " ", 6.5)
        self.assertAlmostEqual(500, d["home"], delta=60)
        d = r(b, " ", 7.5)  # 2.5 s after the last sighting: Home fired
        self.assertEqual(0, d["home"])
        self.assertEqual(1, d["ret"])
        d = r(b, " ", 60.0)
        self.assertEqual((0, 0.0, 90.0), (d["ret"], d["pan"], d["tilt"]))
        self.assertEqual(0, d["home"])

    def test_home_not_pending_without_a_prior_target_or_outside_ai(self):
        b, _ = board(state="idle")
        self.assertEqual(0, r(b, " ", 5.0)["home"])
        b, tgt = board(uncalibrated=True)
        r(b, " ", 1.0)
        r(b, "mode:user", 1.1)
        tgt.state = "idle"
        self.assertEqual(0, r(b, " ", 2.0)["home"])

    def test_target_returning_cancels_home(self):
        b, tgt = board(uncalibrated=True)
        tgt.x, tgt.y = 260, 60
        r(b, " ", 1.0)
        tgt.state = "idle"
        r(b, " ", 3.1)  # past the delay: homing
        tgt.state = "locked"
        d = r(b, " ", 3.5)
        self.assertEqual((0, 0), (d["ret"], d["home"]))


class RenderTests(unittest.TestCase):
    def test_jpeg_and_darkness(self):
        from PIL import Image, ImageStat

        person = mb.TrackSample("locked", 1, 160, 120, 138, 83, 44, 74, visible=True)
        for dark in (False, True):
            data = mb.FrameRenderer(dark=dark).render(person)
            self.assertEqual(b"\xff\xd8", data[:2])
            self.assertEqual(b"\xff\xd9", data[-2:])
            img = Image.open(io.BytesIO(data))
            self.assertEqual((320, 240), img.size)
            mean = sum(ImageStat.Stat(img.convert("L")).mean)
            self.assertEqual(dark, mean < 20, mean)

    def test_person_changes_the_frame(self):
        rend = mb.FrameRenderer()
        away = mb.TrackSample("idle", 1)
        there = mb.TrackSample("locked", 1, 160, 120, 138, 83, 44, 74, visible=True)
        self.assertNotEqual(rend.render(away), rend.render(there))


class WifiTests(unittest.TestCase):
    def test_pin_and_lists(self):
        w = mb.WifiState()
        self.assertEqual(403, w.post("/api/wifi/add", {"ssid": "X", "password": "password1", "pin": "0000"})[0])
        self.assertEqual(403, w.post("/api/wifi/forget", {"ssid": "HomeNet"})[0])
        self.assertEqual((200, {"ok": True}), w.post("/api/wifi/add", {"ssid": "New", "pin": "1234"}))
        self.assertIn("New", w.saved)
        self.assertEqual(400, w.post("/api/wifi/add", {"ssid": "X", "password": "short", "pin": "1234"})[0])
        self.assertEqual((200, {"ok": True}), w.post("/api/wifi/forget", {"ssid": "New", "pin": "1234"}))
        self.assertEqual({"networks"}, set(w.scan()))
        self.assertEqual({"mode", "ssid", "ip", "pin_set", "failure"}, set(w.status()))

class ReplyContractTests(unittest.TestCase):
    """reply_keys.json ties the mock and the firmware together, so drift on either side fails CI.

    Mock side: its full reply must have exactly these keys in this order. Firmware side: test_reply_json.cpp holds the
    golden reply that format_reply must reproduce byte for byte (the native test enforces that), and the keys parsed
    out of that golden string must equal the fixture. The HUD side: every key the built HUD page reads off a reply (`m.<key>`)
    must be in the fixture, so the page cannot depend on a field the board never sends.
    """

    def test_mock_reply_has_the_contract_keys_in_order(self):
        b, _ = board()
        self.assertEqual(SPEC_KEYS, list(r(b, " ", 1.0).keys()))

    def test_firmware_golden_reply_has_the_contract_keys_in_order(self):
        src = (REPO / "ESP32" / "test" / "native" / "test_reply_json" / "test_reply_json.cpp").read_text(encoding="utf-8")
        start = src.index("const char* expected =", src.index("void test_golden_typical_reply"))
        golden = src[start : src.index(";", start)]
        self.assertEqual(SPEC_KEYS, re.findall(r'\\"(\w+)\\":', golden))

    def test_hud_reads_only_contract_keys(self):
        html = build_hud_page(REPO / "web")
        read = set(re.findall(r"\bm\.([a-z_][a-z_0-9]*)\b", html))
        self.assertTrue(read, "no reply reads found: the pattern no longer matches the built HUD page")
        self.assertEqual(set(), read - set(SPEC_KEYS))


if __name__ == "__main__":
    unittest.main()
