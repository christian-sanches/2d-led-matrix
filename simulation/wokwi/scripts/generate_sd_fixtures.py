#!/usr/bin/env python3
"""Regenerate small synthetic DAT v1 files for the Wokwi microSD card."""

from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1] / "sdcard" / "PRESETS"
HEADER = b"LEDMAT\r\n" + bytes((1, 20, 8, 8, 1, 0)) + struct.pack("<H", 192)


def encode(frames: list[bytes]) -> bytes:
    return HEADER + struct.pack("<I", len(frames)) + b"".join(frames)


def frame(pixel_fn) -> bytes:
    pixels = bytearray()
    for y in range(8):
        for x in range(8):
            pixels.extend(pixel_fn(x, y))
    assert len(pixels) == 192
    return bytes(pixels)


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    moving_bar = [
        frame(lambda x, y, bar=bar: (255, 70 + y * 20, 0) if x == bar else (0, 0, 8))
        for bar in range(8)
        for _ in range(4)
    ]
    blue = frame(lambda _x, _y: (0, 40, 255))
    white = frame(lambda _x, _y: (255, 255, 255))
    # Hold each color for half a second at the firmware's nominal 30 FPS.
    solid_colors = [blue] * 15 + [white] * 15
    (ROOT / "PRESET00.dat").write_bytes(encode(moving_bar))
    (ROOT / "PRESET01.dat").write_bytes(encode(solid_colors))
    print(f"Wrote synthetic presets to {ROOT}")


if __name__ == "__main__":
    main()
