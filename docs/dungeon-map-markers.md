# Dungeon map and minimap markers

Dungeon pause maps add native character heads for online players in the same dungeon. The native room tables select each floor; nine native height rules handle rooms spanning floors. Heads occupy 18-pixel steps to the right, with the local head retaining the first position on its floor. Changing the selected map floor does not change the player's actual floor marker.

The C-right minimap adds colored native position dots for online players in the same gameplay room. Its rotation, scale, +13 origin correction, and rectangular/square bounds follow the native projection. Remote dots fade with the map background and remain visible while the local red dot blinks. The local marker draws in front. Six map updates separate roster refreshes; the first opening refreshes immediately.

Both views read `anchor_get_lobby_positions_json()`. There are no additional packets or changes to packet routing. The getter filters self and offline players. Dungeon views accept the existing pre-Japan-map location snapshot; minimaps require both current room and snapshot room to match the local gameplay room. Incomplete positions and unknown character IDs are skipped.

Native tasks own the display records. A pool per view reuses inactive records up to the maximum simultaneous visible roster. Constructor/state-zero hooks invalidate previous generations before any old record is accessed, including reused task addresses. The minimap appends its records after the native background: the native updater depends on `local_marker->next` continuing to point to that background.

The dot is the original resource `0x8004` crop `(24,24,8,8)`, selected by descriptor `D_80209BC8` and bank `D_80209CA0 + 0x30`. The loaded file `0x007F` maps the decoded sheet to `0x0A000000`; this is not the raw file's ROM bytes. Runtime recolors retain the game's shape, transparency, and shading. Sixteen colors are assigned without collisions among the first sixteen active peers; larger rosters reuse colors. Color slots stay stable for surviving peers during joins/leaves. No external images or ROM pixel arrays are included in the mod.

The private dot pixels use a 2 KiB reservation in the native scene resource arena below `0x80800000`. Reserve after character/projectile resources and before render scratch; advance the registry sentinel so later allocations cannot overwrite the dots. Rebuild the reservation at each stage load, then reuse it across minimap open/close cycles in that scene. Only the 8x8 crop is copied, with zero UV origin and S/T tile masks of three. Texture-bank descriptors remain CPU-only mod data.

The first implementation incorrectly put full recolored sheets in mod BSS (`0x8106D928` in the reported build). The stock graphics path has extended RDRAM addressing disabled, so this pointer was interpreted through the segment table and read unrelated bytes. The supplied 23:09 recording shows that failure. CPU-only sprite tests did not cover this; the regression now verifies the graphics address range, physical-address preservation, complete crop contents, texture stride/masks, arena bounds, and scene invalidation.

Native evidence was checked in Ghidra against `mnsg.us.0.z64` (gameplay/minimap) and `file_17_linked.elf` (pause map), then compared with recomp symbol tables and instructions. `tools/inspect_dungeon_maps.py` independently verifies the 116 room rows, floor counts, resource mapping and crop against the local decompressed US ROM.

Validation:

- `bash tests/run_dungeon_maps.sh`: layout-faithful 32-bit pointer fixtures under AddressSanitizer and UndefinedBehaviorSanitizer; floor boundaries, projection, filtering, colors, graphics-addressable crop storage, list integrity, scene lifecycle, reuse and allocation failure.
- `python3 -m unittest discover -s tests -p 'test_world_map_locations.py'`: existing lobby/map snapshot contract.
- `python3 tools/inspect_dungeon_maps.py /path/to/mnsg.us.decompressed.z64 --output /tmp/map-evidence`: ROM verification and original dot export.
- `bash build_mod.sh -j8`: release/debug MIPS compilation, collision-dispatch validation, and package generation.

A fresh two-client visual test of the corrected build remains necessary: verify solid colored dots while moving and reopening the minimap, change rooms to exercise arena rebuilding, and check the closer dungeon-floor heads. The supplied 23:09 recording and dungeon-map image show the previous build, not the corrected rendering.
