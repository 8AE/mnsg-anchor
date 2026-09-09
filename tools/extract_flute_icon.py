#!/usr/bin/env python3
"""Extract the pause-menu flute icon from a decompressed MNSG US ROM.

The pause overlay registers packed resource 0x8016 as a 64x32 texture and
draws the 24x24 rectangle at source coordinates (40, 0).  Resource 0x8016 is
stored in the game's PIC0000 format at ROM offset 0x007EB740 in the
decompressed US ROM.

Usage:
    python3 tools/extract_flute_icon.py /path/to/mnsg.us.decompressed.z64
"""

from __future__ import annotations

import argparse
import binascii
import hashlib
import struct
import zlib
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parent.parent
DEFAULT_RGBA_OUT = ROOT_DIR / "icons" / "flute_icon.rgba"
DEFAULT_PNG_OUT = ROOT_DIR / "icons" / "flute_icon.png"

RESOURCE_ID = 0x8016
RESOURCE_ROM_OFFSET = 0x007EB740
RESOURCE_PACKED_SIZE = 0x2C0
RESOURCE_PACKED_SHA256 = (
    "050a835bcb95e925cbecf23c6cfdd79cbea6925d6ceacc6eec93c32926b496fc"
)
SOURCE_WIDTH = 64
SOURCE_HEIGHT = 32
CROP_X = 40
CROP_Y = 0
CROP_WIDTH = 24
CROP_HEIGHT = 24
FLUTE_RGBA32_SHA256 = (
    "87e4a85e700457c8a4cc240deb7a66d74ff1fb1d7cff0c8ce108bc0220dc52d2"
)


class BitReader:
    """MSB-first bit reader matching native func_80014CC4."""

    def __init__(self, data: bytes) -> None:
        self.data = data
        self.bit = 0

    def read(self, count: int) -> int:
        value = 0
        for _ in range(count):
            if self.bit >= len(self.data) * 8:
                raise EOFError(f"PIC0000 bitstream ended at bit {self.bit}")
            byte = self.data[self.bit >> 3]
            value = (value << 1) | ((byte >> (7 - (self.bit & 7))) & 1)
            self.bit += 1
        return value


class Pic0000Decoder:
    """Decoder for the RGB/RGBA branch of MNSG's native PIC0000 codec."""

    def __init__(self, packed: bytes) -> None:
        self.bits = BitReader(packed)
        self.transparent_key = 0x80000000
        self.bpp = 0
        self.width = 0
        self.height = 0
        self.pixels: list[int] = []

        # Native func_80015FDC initializes a 128-node circular LRU palette.
        self.palette = [0] * 128
        self.prev = [(index - 1) % 128 for index in range(128)]
        self.next = [(index + 1) % 128 for index in range(128)]
        self.current = 0

    def parse_header(self) -> None:
        magic = bytes(self.bits.read(8) for _ in range(3))
        if magic != b"PIC":
            raise ValueError(f"resource does not have a PIC header: {magic!r}")

        marker = self.bits.read(8)
        if marker != 0x1A:
            key_text = bytes([marker] + [self.bits.read(8) for _ in range(3)])
            try:
                self.transparent_key = int(key_text.decode("ascii"), 16)
            except (UnicodeDecodeError, ValueError) as exc:
                raise ValueError(f"invalid PIC transparent-color key {key_text!r}") from exc
            while self.bits.read(8) != 0x1A:
                pass

        # Skip the NUL-terminated metadata string, an ignored byte, and two
        # format nibbles before the bpp/width/height fields.
        while self.bits.read(8) != 0:
            pass
        self.bits.read(8)
        self.bits.read(4)
        self.bits.read(4)
        self.bpp = self.bits.read(16)
        self.width = min(self.bits.read(16), 0x200)
        self.height = min(self.bits.read(16), 0x200)
        self.pixels = [0] * (self.width * self.height)

    def decode_run(self) -> int:
        count_bits = 1
        if self.bits.read(1):
            count_bits += 1
            while self.bits.read(1):
                count_bits += 1
        return self.bits.read(count_bits) + (1 << count_bits) - 1

    def insert_raw_color(self, raw: int) -> int:
        if self.bpp == 15:
            canonical = ((raw >> 5) & 0x3FF) | ((raw & 0x1F) << 10)
            result = canonical << 1
        elif self.bpp == 16:
            result = ((raw & 0xFFC0) >> 5) | ((raw & 0x3E) << 10) | (raw & 1)
            canonical = result
        else:
            return raw

        self.current = self.next[self.current]
        self.palette[self.current] = canonical
        return result

    def select_palette_color(self, node: int) -> int:
        old_current = self.current
        if node != old_current:
            old_prev = self.prev[node]
            old_next = self.next[node]
            self.prev[old_next] = old_prev
            self.next[old_prev] = old_next

            current_next = self.next[old_current]
            self.prev[current_next] = node
            self.next[node] = current_next
            self.next[old_current] = node
            self.prev[node] = old_current
            self.current = node

        value = self.palette[node]
        return value if self.bpp == 16 else value << 1

    def decode_color(self) -> int:
        if self.bpp not in (15, 16):
            return self.bits.read(self.bpp)
        if self.bits.read(1) == 0:
            return self.insert_raw_color(self.bits.read(self.bpp))
        return self.select_palette_color(self.bits.read(7))

    def get_internal(self, x: int, y: int) -> int:
        pixel = self.pixels[y * self.width + x]
        return (
            (((pixel & 0x003E) >> 1) << 11)
            | (((pixel & 0x07C0) >> 6) << 6)
            | (((pixel & 0xF800) >> 11) << 1)
            | (pixel & 1)
        )

    def put_internal(self, x: int, y: int, color: int) -> None:
        if x < 0 or x >= self.width or y < 0 or y >= self.height:
            return
        alpha = (color & 1) if self.bpp == 16 else 1
        pixel = (
            (((color & 0x003E) >> 1) << 11)
            | (((color & 0x07C0) >> 6) << 6)
            | (((color & 0xF800) >> 11) << 1)
            | alpha
        )
        self.pixels[y * self.width + x] = pixel & 0xFFFF

    def decode_diagonal(self, x: int, y: int, color: int) -> None:
        active = True
        while True:
            move = self.bits.read(2)
            if move == 0:
                if self.bits.read(1) == 0:
                    return
                x += 2 if self.bits.read(1) else -2
            elif move == 1:
                x -= 1
            elif move == 3:
                x += 1

            if x >= self.width:
                return
            y += 1
            if y >= self.height:
                active = False
            if active:
                self.put_internal(x, y, color)

    def decode(self) -> list[int]:
        self.parse_header()
        if self.bpp not in (15, 16):
            raise NotImplementedError(
                f"this extractor expects a 15/16-bpp PIC, got {self.bpp} bpp"
            )

        x = -1
        y = 0
        last = 0
        while True:
            repeats = self.decode_run() - 1
            while repeats:
                x += 1
                if x == self.width:
                    y += 1
                    x = 0
                    if y == self.height:
                        self.apply_transparency()
                        return self.pixels
                existing = self.get_internal(x, y)
                if existing:
                    last = existing if self.bpp == 16 else existing & 0xFFFE
                self.put_internal(x, y, last)
                repeats -= 1

            x += 1
            if x == self.width:
                y += 1
                x = 0
                if y == self.height:
                    self.apply_transparency()
                    return self.pixels

            last = self.decode_color()
            self.put_internal(x, y, last)
            if self.bits.read(1):
                self.decode_diagonal(x, y, last)

    def apply_transparency(self) -> None:
        if self.bpp != 15 or self.transparent_key == 0x80000000:
            return
        for y in range(self.height):
            for x in range(self.width):
                if (self.get_internal(x, y) & 0xFFFE) == self.transparent_key:
                    self.pixels[y * self.width + x] = 0


def rgba5551_to_rgba32(pixel: int) -> bytes:
    r5 = (pixel >> 11) & 31
    g5 = (pixel >> 6) & 31
    b5 = (pixel >> 1) & 31
    return bytes(
        (
            (r5 << 3) | (r5 >> 2),
            (g5 << 3) | (g5 >> 2),
            (b5 << 3) | (b5 >> 2),
            255 if pixel & 1 else 0,
        )
    )


def png_chunk(kind: bytes, data: bytes) -> bytes:
    checksum = binascii.crc32(kind + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", checksum)


def encode_png(width: int, height: int, rgba: bytes) -> bytes:
    stride = width * 4
    scanlines = b"".join(
        b"\0" + rgba[y * stride : (y + 1) * stride] for y in range(height)
    )
    png = b"\x89PNG\r\n\x1a\n"
    png += png_chunk(
        b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    )
    png += png_chunk(b"IDAT", zlib.compress(scanlines, 9))
    png += png_chunk(b"IEND", b"")
    return png


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(rom_path: Path) -> bytes:
    with rom_path.open("rb") as rom:
        rom.seek(RESOURCE_ROM_OFFSET)
        packed = rom.read(RESOURCE_PACKED_SIZE)

    packed_hash = sha256(packed)
    if packed_hash != RESOURCE_PACKED_SHA256:
        raise ValueError(
            f"resource 0x{RESOURCE_ID:04X} at ROM offset "
            f"0x{RESOURCE_ROM_OFFSET:08X} has SHA-256 {packed_hash}, expected "
            f"{RESOURCE_PACKED_SHA256}; use the decompressed US ROM"
        )

    decoder = Pic0000Decoder(packed)
    pixels = decoder.decode()
    if (decoder.bpp, decoder.width, decoder.height) != (
        15,
        SOURCE_WIDTH,
        SOURCE_HEIGHT,
    ):
        raise ValueError(
            "unexpected PIC0000 format: "
            f"{decoder.bpp} bpp, {decoder.width}x{decoder.height}"
        )

    crop = [
        pixels[y * decoder.width + x]
        for y in range(CROP_Y, CROP_Y + CROP_HEIGHT)
        for x in range(CROP_X, CROP_X + CROP_WIDTH)
    ]
    rgba = b"".join(rgba5551_to_rgba32(pixel) for pixel in crop)
    rgba_hash = sha256(rgba)
    if rgba_hash != FLUTE_RGBA32_SHA256:
        raise ValueError(
            f"decoded flute RGBA32 has SHA-256 {rgba_hash}, expected "
            f"{FLUTE_RGBA32_SHA256}"
        )
    return rgba


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="decompressed MNSG US ROM")
    parser.add_argument(
        "--rgba-out",
        type=Path,
        default=DEFAULT_RGBA_OUT,
        help=f"raw RGBA32 output (default: {DEFAULT_RGBA_OUT})",
    )
    parser.add_argument(
        "--png-out",
        type=Path,
        default=DEFAULT_PNG_OUT,
        help=f"PNG output (default: {DEFAULT_PNG_OUT})",
    )
    args = parser.parse_args()

    rgba = extract(args.rom)
    args.rgba_out.parent.mkdir(parents=True, exist_ok=True)
    args.png_out.parent.mkdir(parents=True, exist_ok=True)
    args.rgba_out.write_bytes(rgba)
    args.png_out.write_bytes(encode_png(CROP_WIDTH, CROP_HEIGHT, rgba))

    print(
        f"extracted resource 0x{RESOURCE_ID:04X} crop "
        f"({CROP_X}, {CROP_Y}, {CROP_WIDTH}, {CROP_HEIGHT})"
    )
    print(f"wrote {args.rgba_out} ({len(rgba)} bytes)")
    print(f"wrote {args.png_out}")
    print(f"RGBA32 SHA-256 {sha256(rgba)}")


if __name__ == "__main__":
    main()
