#!/usr/bin/env python3
"""Verify NPC path instruction boundaries from the decompressed US ROM.

Produces only derived boundary masks, never game code/assets. These masks stop
network checkpoints from jumping the native path interpreter into operands.
"""
import argparse
import hashlib
from pathlib import Path
import struct

SHA256='e40bee20508c2e29e651dca4e47504e40f908f0a2186e34a582784bf5a64be4c'
STEPS={0:2,1:5,3:6,4:5,5:7,6:4,8:3,9:6,10:6,12:4,14:6,15:4,
       16:6,17:8,18:2,19:2,20:6,21:2,22:6,24:5,25:5,26:2,
       28:3,29:2,30:2,31:1,32:1,33:2,34:4,35:8,36:5,37:2,
       38:3,39:5,40:6,41:6,42:2,43:5}

def masks(rom):
    assert hashlib.sha256(rom).hexdigest()==SHA256
    u32=lambda o:struct.unpack_from('>I',rom,o)[0]
    u16=lambda o:struct.unpack_from('>H',rom,o)[0]
    off=lambda a:a-0x8020d2a0+0x5c8770
    result=[]
    for route in range(163):
        p=u32(off(0x80238980)+route*4);f=u16(off(0x80238c0c)+route*2)
        if f:
            start=u32(0x57fd4+f*4)&0x7fffffff
            end=u32(0x57fd4+(f+1)*4)&0x7fffffff
            base=start+(p&0xffffff)
        else:
            base=off(p);end=0x5f6840
        seen=set();pc=0
        while pc not in seen and 0<=pc<256:
            assert base+pc*2+2<=end,(route,pc)
            op=u16(base+pc*2);seen.add(pc)
            if op in (13,23):break
            if op==7:
                pc=u16(base+pc*2+2);continue
            if op not in STEPS:break
            pc=(pc+STEPS[op])&255
        b=bytearray(32)
        for pc in seen:b[pc//8]|=1<<(pc%8)
        result.append(b)
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom',type=Path);p.add_argument('--write',type=Path);a=p.parse_args()
    rows=masks(a.rom.read_bytes())
    content='/* Derived from verified native path opcode boundaries. Regenerate with tools/inspect_world_actors.py. */\nstatic const unsigned char world_path_pc[163][32] = {\n'
    content+=''.join('    {'+','.join(str(v) for v in row)+'}, /* '+str(i)+' */\n' for i,row in enumerate(rows))+'};\n'
    if a.write:a.write.write_text(content)
    else:assert Path('include/world/anchor_world_paths.inc').read_text()==content
    print('Verified 163 NPC path instruction masks')
