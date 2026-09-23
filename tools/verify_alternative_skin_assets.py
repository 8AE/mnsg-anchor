#!/usr/bin/env python3
"""Audit the US ROM assets used by the alternative Ebisumaru mesh graft."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import struct


def word(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def main(rom_path: Path) -> None:
    rom = rom_path.read_bytes()
    if rom[:4] != bytes.fromhex("80371240"):
        raise ValueError("expected a big-endian US .z64 ROM")
    table = rom.index(b"Nisitenma-Ichigo") + 16

    def resource(file_id: int) -> bytes:
        # The native loader indexes the file table with file_id - 1.
        start = word(rom, table + (file_id - 1) * 4) & 0x7FFFFFFF
        end = word(rom, table + file_id * 4) & 0x7FFFFFFF
        return rom[start:end]

    broad = resource(0x124)
    action_file = resource(0x127)
    opening = resource(0x4D9)
    if (len(broad), len(action_file), len(opening)) != (
        0x7CF0, 0x2FBF0, 0x5D70
    ):
        raise ValueError("unexpected Ebisumaru resource layout")
    # func_80001C00 uses this expanded-size table to reserve the resident
    # resource. File 0x4D9 needs space beyond its raw mesh for four decoded
    # 0x1000-byte texture pages written by the model post-loader.
    expanded_table = 0x80054ACC - 0x80000000 + 0xC00
    expanded_start, expanded_end = struct.unpack_from(
        ">II", rom, expanded_table + (0x4D9 - 1) * 8
    )
    if expanded_end - expanded_start != 0x9D70:
        raise ValueError("opening file's resident size lacks texture pages")
    broad_start, broad_end = struct.unpack_from(
        ">II", rom, expanded_table + (0x124 - 1) * 8
    )
    if broad_end - broad_start != 0x114F0 or broad_end - broad_start > 0x18000:
        raise ValueError("playable broad file does not fit before opening mesh")

    source = Path(__file__).resolve().parents[1] / "src/player/alternative_ebisumaru/anchor_alternative_model.c"
    mapping = {
        int(clothed, 16): int(alternative, 16)
        for clothed, alternative in re.findall(
            r"\{(0x[0-9a-fA-F]+)u,\s*(0x[0-9a-fA-F]+)u\}",
            source.read_text(),
        )
    }
    if len(mapping) != 14 or len(set(mapping.values())) != 14:
        raise ValueError("expected fourteen distinct body-mesh pairs")

    pointers = []
    for offset in range(0x4798, 0x5D70, 4):
        value = word(opening, offset)
        if value >> 24 == 8:
            if (value & 0xFFFFFF) >= 0x9D70:
                raise ValueError(f"invalid opening pointer at {offset:#x}")
            pointers.append((offset, value))
    if len(pointers) != 72:
        raise ValueError(f"expected 72 opening display references, got {len(pointers)}")

    opening_root = word(opening, 0xB0C + 8) & 0xFFFFFF
    opening_displays: set[int] = set()

    def walk(data: bytes, root: int, bounds: tuple[int, int] | None = None) -> set[int]:
        seen: set[int] = set()
        displays: set[int] = set()

        def visit(offset: int) -> None:
            if (
                offset in seen
                or offset + 24 > len(data)
                or len(seen) >= 64
                or (bounds is not None and not (bounds[0] <= offset and offset + 24 <= bounds[1]))
            ):
                raise ValueError("invalid or cyclic model tree")
            seen.add(offset)
            displays.add(word(data, offset))
            left, right = struct.unpack_from(">bb", data, offset + 4)
            if left:
                visit(offset + left * 24)
            if right:
                visit(offset + right * 24)

        visit(root)
        return displays

    opening_displays = walk(opening, opening_root)
    if not set(mapping.values()) <= opening_displays:
        raise ValueError("mapped opening displays are absent from the actor")

    # D_80203F34[1] points at US overlay ROM 0x5BA900. Every playable action
    # must retain all body display references before the graft can be enabled.
    table_offset = 0x5BA900
    for action in range(0xE8):
        model = word(rom, table_offset + action * 0x1C)
        if model >> 24 != 7:
            raise ValueError(f"action {action:#x} has unexpected model segment")
        model_offset = model & 0xFFFFFF
        if model_offset + 12 > len(action_file):
            raise ValueError(f"action {action:#x} model is outside file 0x127")
        root = word(action_file, model_offset + 8)
        if root >> 24 != 7:
            raise ValueError(f"action {action:#x} has unexpected tree segment")
        slice_start_word = word(rom, table_offset + action * 0x1C + 0x0C)
        slice_end_word = word(rom, table_offset + action * 0x1C + 0x10)
        if slice_start_word >> 24 != 7 or (
            slice_end_word and slice_end_word >> 24 != 7
        ):
            raise ValueError(f"action {action:#x} has unexpected slice segment")
        slice_start = slice_start_word & 0xFFFFFF
        slice_end = (slice_end_word & 0xFFFFFF) if slice_end_word else len(action_file)
        if not (slice_start <= model_offset < slice_end <= len(action_file)):
            raise ValueError(f"action {action:#x} has invalid private slice")
        displays = walk(action_file, root & 0xFFFFFF, (slice_start, slice_end))
        if not set(mapping) <= displays:
            raise ValueError(f"action {action:#x} is missing body displays")

    print("Verified opening mesh, 72 display pointers, and all 232 playable actions")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path, help="decompressed US MNSG .z64")
    main(parser.parse_args().rom)
