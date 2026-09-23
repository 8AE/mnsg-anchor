"""Link native imports for host fixtures; never substitute the sync logic.

Clip words are a checked-in USA file_13 memory read (8020B170/B260).
Native callbacks are inert symbols: fixtures exercise real capture/apply/hooks,
not an emulation of the complete game or its renderer.
"""
import json
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
clips = json.loads((root / "tests/impact_boss_clips.json").read_text())
sources = [root / "src/bosses/impact/anchor_impact_kashiwagi.c",
           root / "src/bosses/impact/anchor_impact_taisamba.c",
           root / "src/bosses/impact/anchor_impact_balberra.c",
           root / "src/bosses/impact/anchor_impact_detoile.c",
           root / "src/bosses/impact/anchor_impact_catalog.c",
           root / "src/bosses/impact/anchor_impact_visual_catalog.c"]
text = "\n".join(p.read_text() for p in sources)
for name in sorted(set(re.findall(r'extern void (func_\w+)\(void \*, void \*\);', text))):
    print(f"void {name}(void *t, void *o) {{ (void)t; (void)o; }}")
for name in sorted(set(re.findall(r'extern unsigned int (D_\w+)\[\];', text))):
    words = clips.get(name.split('_')[1], [0, 0])
    print(f"unsigned int {name}[] = {{{words[0]}u,{words[1]}u}};")
for name in sorted(set(re.findall(r'extern unsigned char (D_\w+)\[\];', text))):
    print(f"unsigned char {name}[16];")
