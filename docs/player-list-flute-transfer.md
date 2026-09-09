# Online player flute transfer

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

`tools/extract_flute_icon.py` reproduces the native RGB/RGBA decoder, validates
the packed resource, and extracts exactly that crop from the decompressed US
ROM. The checked-in 24x24 RGBA32 asset has SHA-256
`87e4a85e700457c8a4cc240deb7a66d74ff1fb1d7cff0c8ce108bc0220dc52d2`.
The mod bakes these pixels into `include/icon_flute.h`; it does not reuse the
pause overlay's native texture handle, whose lifetime and type are incompatible
with RecompUI textures.

## Transfer lifecycle

The roster and DBG/NET controls share one persistent mouse-capturing HUD
context so all of its buttons remain clickable. The flute image and its
textless button are sibling elements because RecompUI buttons cannot contain
an image child. The real button is positioned transparently over the image.
The panel shrink-wraps to its widest row; labels retain their intrinsic
single-line width so each flute action remains directly beside its player text.

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
the native lifecycle gates, exact peer coordinates, destination-native
rotations, same-room reloads, cross-room loads, and room-table bounds. Both
release and debug packages compile the same transfer path.

These checks do not certify the live game UI. A two-client test should click
each remote flute button in same-room and cross-room cases, confirm the icon and
hitbox align at different UI scales, and verify that a request made while a
pause or dialog is active waits and then loads the remote location. Also test a
remote entering the World Map, a cutscene, disconnecting, and changing rooms at
the moment of the click.
