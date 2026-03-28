#!/usr/bin/env python3
"""
gen_icon_headers.py  –  convert raw RGBA32 icon files to C headers.

Usage:
    python3 tools/gen_icon_headers.py

Reads  icons/{name}_icon.rgba  (200×200 × 4 bytes each) and writes
include/icon_{name}.h  containing:

    #define ICON_{NAME}_WIDTH  200
    #define ICON_{NAME}_HEIGHT 200
    static const unsigned char icon_{name}_data[160000] = { ... };
"""

import os
import struct

ICON_SIZE = 200
ICONS_DIR = os.path.join(os.path.dirname(__file__), '..', 'icons')
OUT_DIR   = os.path.join(os.path.dirname(__file__), '..', 'include')
NAMES     = ['goemon', 'ebisumaru', 'sasuke', 'yae']

for name in NAMES:
    in_path  = os.path.join(ICONS_DIR, f'{name}_icon.rgba')
    out_path = os.path.join(OUT_DIR,   f'icon_{name}.h')

    with open(in_path, 'rb') as f:
        data = f.read()

    expected = ICON_SIZE * ICON_SIZE * 4
    if len(data) != expected:
        raise ValueError(f'{in_path}: expected {expected} bytes, got {len(data)}')

    guard = f'ICON_{name.upper()}_H'
    lines = [
        f'#ifndef {guard}',
        f'#define {guard}',
        f'',
        f'#define ICON_{name.upper()}_WIDTH  {ICON_SIZE}',
        f'#define ICON_{name.upper()}_HEIGHT {ICON_SIZE}',
        f'',
        f'/* Raw RGBA32 pixel data, {ICON_SIZE}x{ICON_SIZE}. */',
        f'static const unsigned char icon_{name}_data[{expected}] = {{',
    ]

    # Emit 16 bytes per line
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        line = '    ' + ', '.join(f'0x{b:02x}' for b in chunk) + ','
        lines.append(line)

    lines += [
        '};',
        '',
        f'#endif /* {guard} */',
        '',
    ]

    with open(out_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f'  wrote {out_path}  ({len(data)} bytes)')

print('Done.')
