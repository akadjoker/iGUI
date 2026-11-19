#!/usr/bin/env python3
"""Converts the P6 pictures the tests write (ROUND9_SHOTS, ROUND10_SHOTS) to PNG, without any library but zlib.

    tools/ppm2png.py picture.ppm [more.ppm ...]     writes picture.png next to each
"""
import struct
import sys
import zlib


def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def convert(src):
    data = open(src, "rb").read()
    magic, size, maxval, pixels = data.split(b"\n", 3)
    assert magic == b"P6" and maxval == b"255", "not a P6 picture with 8 bits"
    width, height = map(int, size.split())
    rows = b"".join(b"\x00" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
    dst = src[:-4] + ".png" if src.endswith(".ppm") else src + ".png"
    open(dst, "wb").write(png)
    print("wrote", dst)


for path in sys.argv[1:]:
    convert(path)
