#!/usr/bin/env python3
"""Convert binary P6 PPM screenshots into PNG using only Python's standard library."""
import struct
import sys
import zlib

def chunk(kind, payload):
    data = kind + payload
    return struct.pack(">I", len(payload)) + data + struct.pack(">I", zlib.crc32(data) & 0xFFFFFFFF)

def convert(src, dst):
    with open(src, "rb") as f:
        if f.readline() != b"P6\n":
            raise ValueError("Expected binary P6 PPM file")
        dimensions = f.readline().split()
        if len(dimensions) != 2:
            raise ValueError("Invalid PPM dimensions")
        width, height = map(int, dimensions)
        if f.readline() != b"255\n":
            raise ValueError("Expected 8-bit RGB PPM data")
        pixels = f.read()
    if width < 1 or height < 1 or len(pixels) != width * height * 3:
        raise ValueError("PPM dimensions or pixel count invalid")
    stride = width * 3
    scanlines = b"".join(b"\0" + pixels[y*stride:(y+1)*stride] for y in range(height))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(scanlines, 6))
    png += chunk(b"IEND", b"")
    with open(dst, "wb") as f:
        f.write(png)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: ppm_to_png.py <input.ppm> <output.png>")
    convert(sys.argv[1], sys.argv[2])
