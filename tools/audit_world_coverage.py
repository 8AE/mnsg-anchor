#!/usr/bin/env python3
"""Run production placement classifiers against every US-ROM placed record.

This reports classifier decisions, not universal runtime coverage. Native NPC
promotion hooks, boss adapters, static scenery, save-derived pickups and dynamic
children need separate evidence. No ROM code/assets are written to the report.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

from inspect_world_roster import inventory

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r'^[^\n;]*\b' + re.escape(name) +
                      r'\s*\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Missing production function: ' + name)
    # Ignore braces inside comments and literals while preserving exact source.
    tokens = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|'
                        r"'(?:\\.|[^'\\])*'|[{}]", re.S)
    depth = 1
    for token in tokens.finditer(source, match.end()):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return source[match.start():token.end()]
    raise ValueError('Unclosed production function: ' + name)


def classifier_source(world, enemy):
    struct = re.search(r'typedef struct \{[^}]*\} WorldActor;', world)
    if not struct:
        raise ValueError('Missing WorldActor definition')
    # Big-endian reads are essential: the classifier mixes byte, halfword and
    # word reads of the same definition. Host-endian casts would invent variants.
    prefix = r'''
#include <stdio.h>
#include <string.h>
#include "world/anchor_world.h"
#include "world/anchor_world_crane.h"
#include "world/anchor_world_bridge.h"
#include "world/anchor_world_gate64.h"
#include "world/anchor_world_doll.h"
#define U8(p,o) (((const unsigned char *)(p))[o])
#define U16(p,o) ((unsigned short)((U8(p,o)<<8)|U8(p,(o)+1)))
#define U32(p,o) (((unsigned int)U16(p,o)<<16)|U16(p,(o)+2))
#define recomp_printf(...) ((void)0)
'''
    globals_ = r'''
static WorldActor s_actors[ANCHOR_WORLD_MAX];
static unsigned int s_count, s_room, s_hash, s_signature, s_visit, s_next_visit;
static unsigned int s_old_signature, s_old_room;
static unsigned char s_dead[32], s_previous_dead[32];
'''
    main = r'''
int main(void) {
    unsigned int room, count, index, value;
    unsigned char sources[ANCHOR_WORLD_MAX][12];
    unsigned char definitions[ANCHOR_WORLD_MAX][16];
    while (scanf("%u %u", &room, &count) == 2) {
        if (room >= 800 || count > ANCHOR_WORLD_MAX) return 2;
        memset(s_actors,0,sizeof(s_actors));
        s_room=room;s_hash=mix(0x7931u,room);s_visit=0;s_old_room=0xffff;
        for (unsigned int i=0;i<count;++i) {
            if (scanf("%u", &index)!=1 || index!=i) return 3;
            for (unsigned int j=0;j<28;++j) {
                if (scanf("%u", &value)!=1 || value>255) return 4;
                if (j<12) sources[i][j]=(unsigned char)value;
                else definitions[i][j-12]=(unsigned char)value;
            }
            anchor_world_roster_add(i,sources[i],definitions[i]);
        }
        anchor_world_roster_end(count);
        for (unsigned int i=0;i<count;++i)
            printf("%u %u %u %u %u %u\n",room,i,s_actors[i].entity,
                s_actors[i].kind,s_actors[i].bridge_member,
                is_regular_enemy(s_actors[i].entity));
    }
    return ferror(stdin) ? 5 : 0;
}
'''
    return '\n'.join([prefix, struct.group(), globals_,
                      function(world, 'mix'),
                      function(world, 'anchor_world_roster_add'),
                      function(world, 'anchor_world_roster_end'),
                      function(enemy, 'is_regular_enemy'), main])


def audit(rom, compiler):
    roster = inventory(rom)
    world = (ROOT/'src/world/anchor_world.c').read_text()
    enemy = (ROOT/'src/world/enemy_sync.c').read_text()
    inputs, expected = [], {}
    for room in roster['rooms']:
        inputs.append(f"{room['room']} {len(room['actors'])}")
        for actor in room['actors']:
            src, definition = actor['sourceRom'], actor['definitionRom']
            data = rom[src:src+12] + rom[definition:definition+16]
            if len(data) != 28:
                raise ValueError('Truncated placement')
            inputs.append(' '.join(map(str, [actor['index'], *data])))
            expected[room['room'], actor['index']] = actor['entity']
    with tempfile.TemporaryDirectory(prefix='mnsg-coverage-') as directory:
        path = Path(directory)
        (path/'classifier.c').write_text(classifier_source(world, enemy))
        subprocess.run([compiler, '-std=c99', '-Wall', '-Wextra', '-Werror',
                        '-fsanitize=undefined', '-I'+str(ROOT/'include'),
                        str(path/'classifier.c'), '-o', str(path/'classifier')],
                       check=True, capture_output=True, text=True)
        process = subprocess.run([str(path/'classifier')], check=True,
                                 input='\n'.join(inputs)+'\n',
                                 capture_output=True, text=True)
    decisions, totals = {}, Counter()
    by_family = defaultdict(Counter)
    for line in process.stdout.splitlines():
        room, index, entity, kind, bridge, enemy_covered = map(int,line.split())
        key = room, index
        if key in decisions or expected.get(key) != entity:
            raise ValueError('Classifier identity mismatch')
        status = ('placed_world' if kind else 'coupled_bridge_member' if bridge
                  else 'regular_enemy' if enemy_covered else 'needs_other_evidence')
        decisions[key] = {'room': room, 'index': index, 'entity': entity,
                          'kind': kind, 'classification': status}
        totals[status] += 1
        by_family[entity][status] += 1
    if decisions.keys() != expected.keys():
        raise ValueError('Missing classifier output')
    families = [dict(family, classifications=dict(by_family[family['entity']]))
                for family in roster['families']]
    return {
        'romSha256': roster['romSha256'],
        'sourceSha256': {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
                         for name in ('src/world/anchor_world.c','src/world/enemy_sync.c',
                                      'include/world/anchor_world.h',
                                      'include/world/anchor_world_crane.h',
                                      'include/world/anchor_world_bridge.h',
                                      'include/world/anchor_world_gate64.h',
                                      'include/world/anchor_world_doll.h',
                                      'tools/audit_world_coverage.py',
                                      'tools/inspect_world_roster.py')},
        'actors': len(decisions), 'entityTypes': len(families),
        'classificationCounts': dict(totals),
        'limitations': [
            'A classifier match does not prove its complete native lifecycle is synchronized.',
            'needs_other_evidence includes runtime NPC promotion, bosses, static or save-derived actors and actual gaps.',
            'Dynamic children, native resources, collision and live multiplayer are not exercised.'
        ],
        'families': families,
        'placements': list(decisions.values())
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default=os.environ.get('HOST_CC','cc'))
    args = parser.parse_args()
    result = audit(args.rom.read_bytes(), args.cc)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items()
                      if k not in ('families','placements')},indent=2))
