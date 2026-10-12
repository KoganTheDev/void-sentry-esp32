"""Tests for the tracking fixture tools (run: python -m unittest discover -s tools/tracking -p "test_*.py")."""
import random
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import make_synthetic_clips  # noqa: E402


class SyntheticClipTests(unittest.TestCase):
    def test_frame_size_and_count(self):
        frames = make_synthetic_clips.square_cross(random.Random(1))
        self.assertEqual(90, len(frames))
        self.assertTrue(all(len(f) == 4800 for f in frames))

    def test_deterministic(self):
        a = make_synthetic_clips.square_cross(random.Random(1))
        b = make_synthetic_clips.square_cross(random.Random(1))
        self.assertEqual(a, b)

    def test_square_position(self):
        frame = make_synthetic_clips.square_cross(random.Random(1))[10]
        self.assertEqual(190, frame[26 * 80 + 2])
        self.assertEqual(190, frame[33 * 80 + 9])
        self.assertNotEqual(190, frame[26 * 80 + 10])


class JpegToGrayTests(unittest.TestCase):
    def test_qvga_jpeg_becomes_80x60(self):
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
        self.assertEqual(4800, len(data))
        self.assertGreater(data[30 * 80 + 50], 200)  # inside the white square (cells 40-59 x 20-39)
        self.assertLess(data[5 * 80 + 5], 30)  # black corner


if __name__ == "__main__":
    unittest.main()
