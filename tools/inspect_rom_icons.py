#!/usr/bin/env python3
"""Verify the runtime icon recipes against a local decompressed US ROM.

Optionally export the original sheets and displayed crops for documentation.
No extraction runs during the mod build; the mod loads its pixels at runtime.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
from extract_flute_icon import Pic0000Decoder, encode_png, rgba5551_to_rgba32

ROOT = Path(__file__).resolve().parents[1]

def inspect(rom_path: Path, output: Path | None = None) -> list[dict]:
    rom = rom_path.read_bytes()
    if rom[:4] != bytes.fromhex('80371240'):
        raise ValueError('Expected a big-endian .z64 ROM')
    if output:
        output.mkdir(parents=True, exist_ok=True)
    records = []
    sheets = {}
    for line in (ROOT/'include/anchor_rom_icon_defs.inc').read_text().splitlines():
        match = re.fullmatch(r'ICON\((\w+), (.*)\)', line)
        if not match:
            continue
        name = match[1]
        rid, address, size, sw, sh, x, y, w, h, flip, mirror = [int(v.strip().rstrip('u'), 0) for v in match[2].split(',')]
        if rid not in sheets:
            packed = rom[address:address+size]
            decoder = Pic0000Decoder(packed)
            pixels = decoder.decode()
            if (decoder.width, decoder.height) != (sw, sh) or decoder.bpp not in (15, 16):
                raise ValueError(f'{name}: unexpected PIC dimensions or format')
            sheets[rid] = pixels
            if output:
                rgba = b''.join(rgba5551_to_rgba32(v) for v in pixels)
                (output/f'resource-{rid:04x}.png').write_bytes(encode_png(sw, sh, rgba))
        if x+w > sw or y+h > sh:
            raise ValueError(f'{name}: crop outside source sheet')
        pixels = sheets[rid]
        width = w * (2 if mirror else 1)
        columns = list(range(w))
        if mirror:
            columns += columns[::-1]
        rgba = b''.join(rgba5551_to_rgba32(pixels[(y+(h-1-row if flip else row))*sw+x+col]) for row in range(h) for col in columns)
        if output:
            (output/f'{name.lower()}.png').write_bytes(encode_png(width,h,rgba))
        records.append({'name':name,'resource':f'0x{rid:04X}','romAddress':f'0x{address:08X}','packedSize':size,'crop':[x,y,w,h],'width':width,'height':h,'flipY':bool(flip),'mirrorX':bool(mirror),'rgba32Sha256':hashlib.sha256(rgba).hexdigest()})
    if output:
        (output/'icon-provenance.json').write_text(json.dumps(records,indent=2)+'\n')
    return records

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    rows = inspect(args.rom, args.output)
    print(f'Verified {len(rows)} runtime crops from {len({r["resource"] for r in rows})} original ROM sheets.')
