"""Convert a folder of QVGA JPEG frames (sorted by name) into one 80x60 gray clip (<name>.gray).

Pillow's draft() uses libjpeg DCT scaling, close to the ESP32's TJpgDec 1/4-scale decode.
Usage: python tools/tracking/jpeg_to_gray.py <jpeg_dir> <out.gray>
"""
import sys
from pathlib import Path

from PIL import Image

W, H = 80, 60


def jpeg_to_gray_bytes(path):
    with Image.open(path) as img:
        img.draft("L", (W, H))
        gray = img.convert("L")
        if gray.size != (W, H):
            gray = gray.resize((W, H), Image.BOX)
        return gray.tobytes()


def convert(jpeg_dir, out_path):
    files = sorted(Path(jpeg_dir).glob("*.jpg"))
    if not files:
        raise SystemExit(f"no .jpg files in {jpeg_dir}")
    data = b"".join(jpeg_to_gray_bytes(f) for f in files)
    Path(out_path).write_bytes(data)
    return len(files)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    print(f"{convert(sys.argv[1], sys.argv[2])} frames -> {sys.argv[2]}")
