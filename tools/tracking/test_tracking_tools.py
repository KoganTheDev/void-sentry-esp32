"""Tests for the tracking fixture tools (run: python -m unittest discover -s tools/tracking -p "test_*.py")."""
import random
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import make_synthetic_clips  # noqa: E402

W, H = 40, 30


class SyntheticClipTests(unittest.TestCase):
    def test_frame_size_and_count(self):
        frames = make_synthetic_clips.square_cross(random.Random(1))
        self.assertEqual(90, len(frames))
        self.assertTrue(all(len(f) == W * H for f in frames))

    def test_deterministic(self):
        a = make_synthetic_clips.square_cross(random.Random(1))
        b = make_synthetic_clips.square_cross(random.Random(1))
        self.assertEqual(a, b)

    def test_square_position(self):
        frame = make_synthetic_clips.square_cross(random.Random(1))[20]
        self.assertEqual(190, frame[13 * W + 2])  # top-left of the 4x4 square at x0=2, y0=13
        self.assertEqual(190, frame[16 * W + 5])  # bottom-right
        self.assertNotEqual(190, frame[13 * W + 6])
        self.assertNotEqual(190, frame[17 * W + 2])

    def test_square_bounces_inside_the_grid(self):
        frames = make_synthetic_clips.square_cross(random.Random(1))
        for f in range(20, 80):
            row = frames[f][13 * W:14 * W]
            xs = [x for x in range(W) if row[x] == 190]
            self.assertEqual(4, len(xs), f)  # always fully inside the grid
            self.assertEqual(xs[0] + 3, xs[-1], f)
        self.assertEqual(36, max(x for x in range(W) if frames[54][13 * W + x] == 190) - 3)  # right wall at frame 54


class GhostClipTests(unittest.TestCase):
    def test_all_clips_have_whole_40x30_frames(self):
        expected = {"square_cross": 90, "light_switch": 60, "seed_object": 150, "pause_leave": 150}
        self.assertEqual(set(expected), set(make_synthetic_clips.CLIPS))
        for name, make in make_synthetic_clips.CLIPS.items():
            frames = make(random.Random(1))
            self.assertEqual(expected[name], len(frames), name)
            self.assertTrue(all(len(f) == W * H for f in frames), name)

    def test_seed_object_is_in_the_first_frame_and_gone_at_the_end(self):
        frames = make_synthetic_clips.seed_object(random.Random(1))
        self.assertEqual(190, frames[0][13 * W + 2])
        self.assertEqual(190, frames[20][13 * W + 3])  # moved right by one
        self.assertEqual(190, frames[20][13 * W + 6])
        self.assertNotEqual(190, frames[70][13 * W + 2])
        self.assertLess(max(frames[149]), 100)

    def test_pause_leave_is_static_then_gone(self):
        frames = make_synthetic_clips.pause_leave(random.Random(1))
        self.assertLess(max(frames[19]), 100)
        self.assertEqual(190, frames[40][13 * W + 22])
        self.assertEqual(190, frames[73][13 * W + 22])
        self.assertLess(max(frames[74]), 100)


class JpegToGrayTests(unittest.TestCase):
    def test_qvga_jpeg_becomes_40x30(self):
        try:
            from PIL import Image
        except ImportError:
            self.skipTest("Pillow not installed")
        import jpeg_to_gray

        img = Image.new("RGB", (320, 240), (0, 0, 0))
        for y in range(80, 160):
            for x in range(160, 240):
                img.putpixel((x, y), (255, 255, 255))
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "00000.jpg"
            img.save(path, quality=90)
            data = jpeg_to_gray.jpeg_to_gray_bytes(path)
        self.assertEqual(W * H, len(data))
        self.assertGreater(data[15 * W + 25], 200)  # inside the white square (cells 20-29 x 10-19)
        self.assertLess(data[2 * W + 2], 30)  # black corner


if __name__ == "__main__":
    unittest.main()
