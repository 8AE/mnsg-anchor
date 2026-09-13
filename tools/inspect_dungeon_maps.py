#!/usr/bin/env python3
"""Verify dungeon room rows and extract the native minimap dot from a local US ROM."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from extract_flute_icon import Pic0000Decoder, encode_png, rgba5551_to_rgba32

ROOT = Path(__file__).resolve().parents[1]

def inspect(rom_path, output):
    rom = rom_path.read_bytes()
    if hashlib.sha256(rom).hexdigest() != 'e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c':
        raise ValueError('Expected the verified decompressed US ROM')
    def at(address, size):
        offset = address - 0x80209A00 + 0x5C5910
        return rom[offset:offset+size]
    pointers = struct.unpack('>5I', at(0x80209B9C, 20))
    rows = []
    for dungeon, address in enumerate(pointers):
        seen = set()
        while True:
            room, floor, rule = at(address, 3)
            if room == 255:
                break
            if room not in seen:
                rows.append([dungeon, room, floor, rule])
                seen.add(room)
            address += 3
    compiled = [[int(n, 0) for n in match] for match in re.findall(
        r'\{(\d+), (0x[0-9A-F]+), (\d+), (\d+)\}',
        (ROOT/'include/anchor_dungeon_rooms.inc').read_text())]
    assert rows == compiled, 'Room table differs from the native ROM'
    assert [rom[0x675FD0 + i*0x54] for i in range(5)] == [3, 4, 4, 4, 2]
    assert struct.unpack('>f', at(0x8020C820, 4))[0] == 305.0
    assert at(0x80209BC8, 12).hex() == 'c00308080000000003000300'
    assert at(0x80209CD0, 16).hex() == '00400020100000000a000000007f0000'
    # File 0x7F's resource list maps resource 0x8004 to segment 0x0A000000.
    assert rom[0x663CC:0x663D4].hex() == '000080040a000000'
    packed = rom[0x7E6C00:0x7E7100]
    decoder = Pic0000Decoder(packed)
    pixels = decoder.decode()
    assert (decoder.width, decoder.height) == (64, 32)
    rgba = b''.join(rgba5551_to_rgba32(pixels[y*64+x]) for y in range(24, 32) for x in range(24, 32))
    result = {'romSha256': hashlib.sha256(rom).hexdigest(), 'roomRows': rows,
        'roomTablePointers': [f'0x{v:08X}' for v in pointers],
        'dot': {'resource': '0x8004', 'romAddress': '0x007E6C00', 'file': '0x007F',
            'segmentedAddress': '0x0A000000', 'bankSymbol': 'D_80209CA0',
            'descriptorSymbol': 'D_80209BC8', 'crop': [24, 24, 8, 8],
            'rgba32Sha256': hashlib.sha256(rgba).hexdigest()}}
    if output:
        output.mkdir(parents=True, exist_ok=True)
        (output/'minimap-dot.png').write_bytes(encode_png(8, 8, rgba))
        (output/'dungeon-map-rom-evidence.json').write_text(json.dumps(result, indent=2)+'\n')
    print(f'Verified {len(rows)} dungeon rooms, five floor counts, native dot descriptor, bank, and game-derived crop.')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    inspect(args.rom, args.output)
