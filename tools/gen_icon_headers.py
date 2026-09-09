#!/usr/bin/env python3
"""
gen_icon_headers.py  -  convert raw RGBA32 icon files to C headers.

Usage:
    python3 tools/gen_icon_headers.py

Reads icons/{name}_icon.rgba and writes include/icon_{name}.h. Character
icons are 200x200; the ROM-derived flute icon is 24x24.

Character headers contain:

    #define ICON_{NAME}_WIDTH  200
    #define ICON_{NAME}_HEIGHT 200
    static const unsigned char icon_{name}_data[160000] = { ... };
"""

import hashlib
import os

ICONS_DIR = os.path.join(os.path.dirname(__file__), '..', 'icons')
OUT_DIR   = os.path.join(os.path.dirname(__file__), '..', 'include')
FLUTE_RGBA32_SHA256 = (
    '87e4a85e700457c8a4cc240deb7a66d74ff1fb1d7cff0c8ce108bc0220dc52d2'
)

ICON_SPECS = [
    ('goemon', 200, 200, None),
    ('ebisumaru', 200, 200, None),
    ('sasuke', 200, 200, None),
    ('yae', 200, 200, None),
    ('flute', 24, 24, FLUTE_RGBA32_SHA256),
]

for name, width, height, expected_sha256 in ICON_SPECS:
    in_path  = os.path.join(ICONS_DIR, f'{name}_icon.rgba')
    out_path = os.path.join(OUT_DIR,   f'icon_{name}.h')

    with open(in_path, 'rb') as f:
        data = f.read()

    expected = width * height * 4
    if len(data) != expected:
        raise ValueError(f'{in_path}: expected {expected} bytes, got {len(data)}')
    actual_sha256 = hashlib.sha256(data).hexdigest()
    if expected_sha256 is not None and actual_sha256 != expected_sha256:
        raise ValueError(
            f'{in_path}: expected SHA-256 {expected_sha256}, got {actual_sha256}'
        )

    guard = f'ICON_{name.upper()}_H'
    lines = [
        f'#ifndef {guard}',
        f'#define {guard}',
        f'',
        f'#define ICON_{name.upper()}_WIDTH  {width}',
        f'#define ICON_{name.upper()}_HEIGHT {height}',
        f'',
    ]

    if name == 'flute':
        lines += [
            '/*',
            ' * Raw RGBA32 pixel data, 24x24.',
            ' * MNSG packed resource 0x8016 (PIC0000), source sheet 64x32.',
            ' * Crop: x=40, y=0, width=24, height=24.',
            f' * RGBA32 SHA-256: {FLUTE_RGBA32_SHA256}',
            ' */',
        ]
    else:
        lines.append(f'/* Raw RGBA32 pixel data, {width}x{height}. */')

    lines.append(f'static const unsigned char icon_{name}_data[{expected}] = {{')

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
