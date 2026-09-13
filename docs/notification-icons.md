# Notification icons from the native ROM

Item toasts show a 24 DP icon to the left of their text. Cards reserve 44 DP
per stack slot so 24 DP sprites plus padding cannot overlap. The eight-slot
reuse, expiry, and Off / Important / All filters are retained. Unknown checks
and unavailable resources keep their text and hide the icon, including when a
slot previously displayed an icon. No images are imported into the mod package.

The 39 recipes in `include/anchor_rom_icon_defs.inc` load 25 original ROM sheets
through `func_80014698_15298` and `func_800144E8_150E8`. Resource ID, stored ROM
address, and packed size are checked before the native decoder writes. All
sheets decode to 4096 bytes; separate aligned scratch holds the largest packed
resource, Mr. Elly Fant at 0x870 bytes. RGBA5551 crops are converted once and
uploaded lazily to RecompUI, then reused for subsequent toasts.

## Supported checks

- All eight tracked equipment items: Chain Pipe, Meat Hammer, Bomb, Flute,
  Camera, Ice Kunai, Bazooka, and Fire Ryo.
- Silver/gold fortune doll pickup flags and all five dungeons' Mr. Elly Fant /
  Mr. Arrow pickup flags.
- Silver and gold weapons for each of the four characters. The awarded value,
  1 or 2, selects both the texture and the matching catalog name; gold no longer
  displays the first (silver) catalog label.
- Magic upgrades and their acquisition flags: Sudden Impact uses Goemon,
  Mini Ebisu uses Ebisumaru, Jetpack uses Sasuke, Mermaid uses Yae. These are
  exactly the ROM atlas cells used by the online player list.
- Native pause-menu quest icons (Triton Shell, Super Pass, Achilles Heel,
  Cucumber, training key), four Miracle items, dungeon key colors, fish, and
  the battery acquisition flag.
- All 29 tracked dungeon key pickups across Oedo Castle, Ghost Toys Castle,
  Festival Temple, Gourmet Submarine, and Musical Castle have explicit silver,
  gold, or diamond mappings. Keys are visible in Important and All modes.

Other events remain text-only. In particular the Japan-map unlock has no
verified standalone item crop in the inspected pause-menu sheets; it does not
borrow an unrelated item image. Notification visibility still follows the
existing config; weapon tiers appear in All mode.

## Native evidence

The pause overlay is **file 0x17**, not the other overlay sharing 0x8021xxxx.
`func_80211870_670820` registers its texture handles. Equipment draw functions
`func_8021302C_671FDC`, `func_802131A0_672150`,
`func_802132C8_672278`, and `func_80213424_6723D4` select 24x24 crops.
The weapon-tier helpers at 0x80212470 / 0x802124EC / 0x80212568 /
0x802125E4 branch on 0/1/2. Disassembly proves the texture handle and stack
arguments U/V; the decompiler omits arguments on several tail-merged paths.
The Triton draw at file-17 0x802135EC explicitly supplies **U=24**, not 32.

Fortune doll pause sprites occupy (0,0,24,24) and (24,0,24,24) in resource
0x800E, ROM 0x007E9570, packed 0x570 bytes.

Mr. Elly Fant (entity 0x86) and Mr. Arrow (0x87) dispatch through the native
actor table at 0x802287BC to file-26 functions 0x08000204 and 0x08000000.
`func_80216CE0_5D21B0` resolves descriptors through 0x80236984. Their descriptor
pointers are 0x80234F48 / 0x80234F58, selecting model files 0x193 / 0x194 and
model 0x08000040. The display lists bind textures **0x0A002980** and
**0x0A003980**. The resource binding records at ROM 0x67888 / 0x67890 resolve
these to **0x829D** / **0x829E**. The 64x32 sheets use their left 32x32 cell;
quad texture scale 0x8000 and UV endpoints 0x800 select that cell. Top vertices
use the larger T coordinate, so the crop is flipped vertically.

Roster head cells remain those documented in `player-list-flute-transfer.md`.
They select the magic owner by check key, independently of the local active
character or the teammate who sent the check.

Dungeon keys use native actor 0x193. The actor initializer
`func_80218A54_5D3F24` copies definition+4/+8 into actor+D0/+D4. File-43
`func_0800058C_6F3A6C` branches on the color byte at actor+D1 (0=silver,
1=gold, 2=diamond) and sets the pickup flag at actor+D4. The 29 matching ROM
definitions and their colors are recorded in `dungeon-key-icon-evidence.json`.
Historical display labels had conflicting colors or described unrelated events;
their key colors now agree with these native definitions. Room descriptions
were not independently reverified. Unknown `ky_` names remain text-only.

Silver and diamond use the 24x24 cells at (0,0) and (24,0) in resource 0x868E;
gold uses (32,0) in resource 0x868F. All three images are decoded from the ROM
by the same loader as the other toast icons.

## Verification

`tools/inspect_rom_icons.py ROM --output DIR` independently decodes and exports
all recipes, recording RGBA32 hashes. It is an inspection tool and is never
called by the build. The notification loader tests exercise every crop with
coordinate-dependent RGBA5551 data, output canaries, short buffers, invalid
resource identity, bad decoder end pointers, and check/tier selection.

`tools/verify_dungeon_key_icons.py ROM` compiles the actual icon selector into
a temporary host library, finds every tracked key in the ROM actor definitions,
and verifies both its icon and displayed color against the native color byte.

Release and debug packaging and native-resource tests do not establish an
in-game toast render. A live run should check mixed text/icon slot reuse,
silver-to-gold progression, all four magic heads, and eight simultaneous cards.
