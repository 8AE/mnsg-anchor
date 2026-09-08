#!/usr/bin/env python3
"""Reject compiler-generated indirect dispatch in the native enemy collector."""

import argparse
from pathlib import Path
import struct
import sys


SYMBOL = "anchor_collision_append_enemies"


def function_bytes(data: bytes) -> tuple[int, bytes]:
    """Locate the exact function using the linked ELF32 big-endian symtab."""
    def read(offset: int, size: int) -> bytes:
        if offset < 0 or size < 0 or offset + size > len(data):
            raise ValueError("truncated or invalid ELF range")
        return data[offset:offset + size]

    if read(0, 16)[:7] != b"\x7fELF\x01\x02\x01":
        raise ValueError("expected ELF32 big-endian version 1")
    header = struct.unpack(">HHIIIIIHHHHHH", read(16, 36))
    if header[1] != 8:
        raise ValueError("expected MIPS machine type")
    section_offset, section_stride, section_count = header[5], header[10], header[11]
    if section_stride != 40 or not section_count:
        raise ValueError("missing or unsupported ELF section table")
    sections = [struct.unpack(">10I", read(section_offset + index * 40, 40))
                for index in range(section_count)]
    matches = []
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        offset, size, link, stride = section[4], section[5], section[6], section[9]
        if stride != 16 or size % stride or link >= len(sections):
            raise ValueError("invalid ELF symbol table")
        strings = sections[link]
        if strings[1] != 3:  # SHT_STRTAB
            raise ValueError("symbol table has no string table")
        names = read(strings[4], strings[5])
        symbols = read(offset, size)
        for position in range(0, size, stride):
            name, address, length, info, _, index = struct.unpack_from(">IIIBBH", symbols, position)
            if name >= len(names):
                raise ValueError("invalid ELF symbol name offset")
            end = names.find(b"\0", name)
            if end < 0:
                raise ValueError("unterminated ELF symbol name")
            if names[name:end] != SYMBOL.encode():
                continue
            if info & 15 != 2 or not (0 < index < len(sections)):
                raise ValueError(f"{SYMBOL} is not a defined function")
            code = sections[index]
            relative = address - code[3]
            if (code[1] != 1 or not code[2] & 4 or relative < 0 or
                    length == 0 or address % 4 or length % 4 or
                    relative + length > code[5]):
                raise ValueError(f"invalid code range for {SYMBOL}")
            matches.append((address, read(code[4] + relative, length)))
    if len(matches) != 1:
        raise ValueError(f"expected one defined {SYMBOL}; found {len(matches)}")
    return matches[0]


def check_dispatch(data: bytes) -> None:
    address, code = function_bytes(data)
    for offset in range(0, len(code), 4):
        word = struct.unpack_from(">I", code, offset)[0]
        opcode, operation, register = word >> 26, word & 63, (word >> 21) & 31
        if opcode == 0 and (operation == 9 or (operation == 8 and register != 31)):
            instruction = "jalr" if operation == 9 else "jr"
            raise ValueError(
                f"unsafe indirect dispatch in {SYMBOL} at 0x{address + offset:08X}: "
                f"{instruction} ${register}; keep enemy selection free of jump tables")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", nargs="?", type=Path, default=Path("build/mod.elf"))
    args = parser.parse_args()
    try:
        check_dispatch(args.elf.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        print(f"Collision dispatch check failed ({args.elf}): {error}", file=sys.stderr)
        return 1
    print(f"Collision dispatch check passed: {SYMBOL}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
