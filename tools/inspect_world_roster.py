#!/usr/bin/env python3
"""Inventory all three placed-actor sources in the decompressed US ROM.

Output contains derived identities/locations, not game code or assets. Native
metadata+0 is an already-resident source list; +8 and the +12 partition grid
resolve through the room wave. Normal and sorted partition slots precede the
appended resident slots, matching the multiplayer roster builder.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROM_SHA256 = 'e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c'


def inventory(rom):
    if hashlib.sha256(rom).hexdigest() != ROM_SHA256:
        raise ValueError('Expected the verified decompressed US ROM')
    u32 = lambda offset: struct.unpack_from('>I', rom, offset)[0]
    u16 = lambda offset: struct.unpack_from('>H', rom, offset)[0]

    def resident(pointer):
        offset = pointer - 0x8020d2a0 + 0x5c8770
        if not 0x5c8770 <= offset < 0x5f6840:
            raise ValueError(f'Resident pointer outside common actor data: {pointer:#x}')
        return offset

    def wave_pointer(pointer, file_id):
        if pointer >> 24 != 8:
            return resident(pointer)
        if not file_id:
            raise ValueError('Segmented pointer without an actor-data wave')
        start = u32(0x57fd4 + file_id*4) & 0x7fffffff
        end = u32(0x57fd4 + (file_id+1)*4) & 0x7fffffff
        offset = start + (pointer & 0xffffff)
        if not start <= offset < end:
            raise ValueError(f'Actor-data pointer outside wave{file_id}: {pointer:#x}')
        return offset

    def source_list(pointer, resolve):
        if not pointer:
            return []
        offset = resolve(pointer)
        result = []
        for _ in range(256):
            definition = u32(offset+12)
            if not definition:
                return result
            result.append((offset, resolve(definition)))
            offset += 20
        raise ValueError('Unterminated actor source list')

    rooms, families, resident_rows = [], {}, []
    for room in range(800):
        pointer = u32(0x5ec7d0 + room*4)
        if not pointer:
            continue
        metadata = resident(pointer)
        file_id = u16(metadata+20)
        resolve = lambda pointer: wave_pointer(pointer, file_id)
        normal = source_list(u32(metadata+8), resolve)
        grouped = {}
        grid, config = u32(metadata+12), u32(metadata+16)
        if grid and config:
            config = resolve(config)
            x, y, z = struct.unpack_from('>3H', rom, config+20)
            if x*y*z > 4096:
                raise ValueError(f'Partition grid exceeds native roster limit: room{room:#x}')
            grid = resolve(grid)
            for cell in range(x*y*z):
                grouped.update(source_list(u32(grid+cell*4), resolve))
        fixed = source_list(u32(metadata), resident)
        records, seen = [], set()
        for kind, sources in [('normal', normal), ('partition', sorted(grouped.items())),
                              ('resident', fixed)]:
            for source, definition in sources:
                if source in seen:
                    continue
                seen.add(source)
                entity = u16(definition)
                row = {'index': len(records), 'entity': entity, 'sourceKind': kind,
                       'sourceRom': source, 'definitionRom': definition,
                       'position': list(struct.unpack_from('>3h', rom, source))}
                records.append(row)
                family = families.setdefault(entity, {'entity': entity, 'count': 0, 'rooms': set(),
                    'file': u16(0x5e4ca6+entity*2), 'entry': u32(0x5e3c8c+entity*4),
                    'category': rom[0x5e54b4+entity]})
                family['count'] += 1
                family['rooms'].add(room)
                if kind == 'resident':
                    resident_rows.append(dict(row, room=room))
        if len(records) > 256:
            raise ValueError(f'Combined roster exceeds256: room{room:#x}')
        rooms.append({'room': room, 'actorDataFile': file_id, 'actors': records})
    for family in families.values():
        family['rooms'] = sorted(family['rooms'])
    return {'romSha256': ROM_SHA256, 'metadataSlots': 800,
            'roomsWithMetadata': len(rooms),
            'roomsWithActorDataFile': sum(bool(r['actorDataFile']) for r in rooms),
            'roomsWithActors': sum(bool(r['actors']) for r in rooms),
            'maxRoster': max(len(r['actors']) for r in rooms),
            'actorCount': sum(len(r['actors']) for r in rooms),
            'entityTypes': len(families), 'residentActors': resident_rows,
            'families': sorted(families.values(), key=lambda row: row['entity']), 'rooms': rooms}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = inventory(args.rom.read_bytes())
    if args.output:
        args.output.write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({key: value for key, value in result.items()
                      if key not in ('families', 'rooms')}, indent=2))
