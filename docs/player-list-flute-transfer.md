# Online player map icons and flute transfer

The Online Players HUD shows Yae's flute beside each eligible remote player.
Clicking the icon asks the game's normal room loader to transfer the local
player to that remote player's current room and position. The local player's
own row never has a transfer button.

## Native icon provenance

The flute is the original pause-menu art, not a redraw. The pause overlay uses
this native path:

```text
func_8021433C_6732EC
  -> func_80211870_670820
  -> func_80213988_672938
  -> func_80213424_6723D4
  -> func_800106F0_112F0
```

`func_80211870_670820` registers a 64x32, 16-bit texture at segmented address
`0x0A012000` in file `0x7F`. When Yae owns the flute (`eq_flute`, save offset
`0xC0`), `func_80213424_6723D4` draws a 24x24 sprite from source coordinates
`u=40`, `v=0`. The backing data is packed resource `0x8016` in the game's
`PIC0000` format.

The player list now resolves resource `0x8016` through the game's own resource
table at runtime and requires its verified US ROM address `0x007EB740` and
packed size `0x2C0`. It then calls native `func_800144E8_150E8`, the same
resource/PIC0000 load-and-decode path used by the game, into a temporary 64x32
RGBA5551 sheet. Only the native `(40, 0, 24, 24)` crop is converted to RGBA32
for RecompUI's one-time texture upload. No flute pixels or extracted flute
asset are compiled into or packaged with the mod.

RecompUI cannot consume an N64 segmented texture address or native texture
registration directly: its only raw texture API copies a caller-provided
RGBA32 buffer. The runtime conversion is therefore the boundary between the
game's ROM-backed asset and the HUD renderer. If the expected resource address,
size, or decoded output length does not match, the transfer action stays hidden
instead of leaving an invisible clickable button.

`tools/extract_flute_icon.py` remains only as an offline provenance checker for
the verified address, crop, and resulting pixels. It is not called by the build
or by the runtime path.

## Native roster icon provenance

The four roster portraits now come from the same face atlas used by the
player marker on the stock Japan map. The map code selects model handles
`0x48000040`, `0x48000120`, `0x48000200`, and `0x480002E0` for Goemon,
Ebisumaru, Sasuke, and Yae. Their display lists all bind segmented texture
`0x08000460` and their UVs select four cells in one atlas.

At runtime the HUD resolves packed resource `0x868C` and requires the verified
US ROM address `0x013F08D0` and packed size `0x4A0`. The native PIC decoder
expands it to a 32x64 RGBA5551 sheet. The character-ID-ordered 16x16 crops are:

| Character | Atlas crop |
| --- | --- |
| Goemon | `(0, 48, 16, 16)` |
| Ebisumaru | `(16, 48, 16, 16)` |
| Sasuke | `(0, 32, 16, 16)` |
| Yae | `(16, 32, 16, 16)` |

As with the flute, RecompUI receives only a one-time runtime conversion of
those native texels. The stock marker quads map their top edge to the larger
texture-T coordinate, so each face cell is flipped vertically during that
conversion to preserve the map's displayed orientation. The previous 200x200
extracted character images and their generated C headers are no longer built
or packaged. If the atlas identity or decoded length does not match, the roster
remains readable and uses a transparent placeholder instead of an invalid
texture.

## Transfer lifecycle

The roster and DBG/NET controls share one persistent mouse-capturing HUD
context so all of its buttons remain clickable. The flute image and its
textless button are sibling elements because RecompUI buttons cannot contain
an image child. The real button is positioned transparently over the image.
The panel shrink-wraps to its widest row; labels retain their intrinsic
single-line width so each flute action remains directly beside its player text.
Local metadata is merged through the same normalized character schema as
received peer metadata, so switching characters updates the self portrait
immediately. If the initial roster arrives one frame before local character
publication, the HUD retries on the next frame instead of caching a blank face
for the normal one-second refresh interval.

The click callback records only the row's client ID. On a later game frame the
mod resolves that ID again under the network-state lock instead of trusting the
player list's one-second display cache. A target is accepted only when it is:

- still online, remote, and in a loaded save;
- backed by a locally received movement sample no more than five seconds old;
- in ordinary gameplay rather than the `0x226` World Map overlay;
- outside scripted/cutscene movement;
- bound to a live interaction session and player lifetime; and
- representable by the native signed 16-bit room-position interface.

The local side waits for pause, the mod-owned invitation dialog, scripted
input, room-start work, death, or an existing transition to clear. It then calls
`func_8000607C_6C7C` with the remote XYZ and the destination room's native
camera/player rotations, followed by `func_80003728_4328(12)`. Step 12 performs
a normal scene load even when both players are already in the same room. This
avoids directly moving a partially initialized player object and does not alter
the saved respawn location.

## Validation

Python tests cover stable roster IDs, local-row exclusion, fresh-target
resolution, expiry, scripted-state rejection, World Map rejection, session and
player-lifetime requirements, and signed coordinate bounds. Host C tests cover
the exact flute and map-face resource identities, decoded-length checks, crop
coordinates, the Japan-map vertical texture orientation, RGBA5551 conversion,
immediate local character changes, native lifecycle gates, exact peer
coordinates, destination-native rotations, same-room reloads, cross-room
loads, and room-table bounds. Both release and debug packages compile the same
path.

These checks do not certify the live game UI. A two-client test should click
each remote flute button in same-room and cross-room cases, confirm the icon and
hitbox align at different UI scales, and verify that a request made while a
pause or dialog is active waits and then loads the remote location. Also test a
remote entering the World Map, a cutscene, disconnecting, and changing rooms at
the moment of the click.
