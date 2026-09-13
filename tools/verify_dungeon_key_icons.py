#!/usr/bin/env python3
"""Verify every tracked dungeon key against US ROM actor definitions.

Compiles the real icon selector into a temporary host library and compares its
results to native actor color parameters, independently of notification text
and ky_s/ky_g/ky_d naming. No ROM data enters the mod package.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ROM_SHA256 = 'e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--evidence', type=Path)
    args = parser.parse_args()
    rom = args.rom.read_bytes()
    if hashlib.sha256(rom).hexdigest() != ROM_SHA256:
        parser.error('Expected the verified US decompressed ROM')
    # Native entity table maps actor 0x193 to file-43 entry 0x080001BC.
    assert int.from_bytes(rom[0x5E3C8C + 0x193 * 4:0x5E3C90 + 0x193 * 4], 'big') == 0x080001BC
    keys = {int(flag, 16): key for flag, key in re.findall(
        r'\{(0x[0-9A-Fa-f]+), 0, "(ky_[^"]+)"\}',
        (ROOT / 'src/item_sync.c').read_text())}
    colors = ('SILVER_KEY', 'GOLD_KEY', 'DIAMOND_KEY')
    enum_names = re.findall(r'^ICON\((\w+),', (ROOT / 'include/anchor_rom_icon_defs.inc').read_text(), re.M)
    enum_values = {name: i + 1 for i, name in enumerate(enum_names)}
    records = {}
    for offset in range(0, len(rom) - 16, 4):
        if rom[offset:offset + 2] != b'\x01\x93':
            continue
        flag = int.from_bytes(rom[offset + 8:offset + 10], 'big')
        if flag not in keys or rom[offset + 5] > 2:
            continue
        color = rom[offset + 5]
        assert flag not in records, f'Ambiguous native key flag {flag:#x}'
        records[flag] = {'key': keys[flag], 'flag': f'0x{flag:04X}',
                         'definitionRomAddress': f'0x{offset:08X}',
                         'nativeColor': color, 'icon': colors[color]}
    assert records.keys() == keys.keys(), 'Not every tracked dungeon key has a native definition'
    labels = dict(re.findall(r'\{"(ky_[^"]+)", "([^"]+)", 0\}',
                             (ROOT / 'src/ui/debug.c').read_text()))
    with tempfile.TemporaryDirectory() as directory:
        library = Path(directory) / 'key_icons.dylib'
        subprocess.run(['cc', '-shared', '-fPIC', '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'include'), str(ROOT / 'src/ui/anchor_check_icons.c'),
                        str(ROOT / 'src/utils/string_utils.c'), '-o', str(library)], check=True)
        selector = ctypes.CDLL(str(library)).anchor_icon_for_check
        selector.argtypes = [ctypes.c_char_p, ctypes.c_int]
        selector.restype = ctypes.c_int
        for row in records.values():
            key = row['key'].encode()
            assert selector(key, 1) == enum_values[row['icon']], row
            assert selector(key, 0) == 0, row
            assert selector(key, -1) == 0, row
            color = row['icon'].split('_')[0].lower()
            assert color in labels[row['key']].lower(), row
            assert not any(other.lower() in labels[row['key']].lower()
                           for other in ('Silver', 'Gold', 'Diamond') if other.lower() != color), row
    if args.evidence:
        args.evidence.write_text(json.dumps({'romSha256': ROM_SHA256,
            'nativeInitializer': 'func_80218A54_5D3F24 copies definition+4/+8 into actor+D0/+D4',
            'nativeKeyUpdate': 'file_43 func_0800058C_6F3A6C reads actor+D1 color and sets actor+D4 pickup flag',
            'keys': sorted(records.values(), key=lambda row: row['flag'])}, indent=2) + '\n')
    print(f'Verified {len(records)} dungeon key icons and labels against native ROM definitions')


if __name__ == '__main__':
    main()
