# Title Cutscene Four-Character Spawn Trace

This note traces only the title-screen cutscene actors from overlay `.file_19`
(ROM `0x694CA0`, VRAM `0x8020D2A0`). It does not use a playable-character
constructor, player manager, controls, or playable behavior callback.

## Trace entry and director

The supplied log reaches `func_8021373C_69B13C`, which calls
`func_8020FAC0_6974C0`. Ghidra shows `func_8020FAC0` creating the title
director task with `func_80034E08_35A08` and using
`func_8020D640_695040` as its update callback.

The director's states `0x2A` and `0x2D` create indices 0 through 3 through a
single indexed actor constructor:

```text
func_8020D640_695040
  -> func_8020FD8C_69778C(parent, character_index)
       -> func_80034E08_35A08(parent, func_8020FC50_697650, 0)
       -> func_8000DBF0_E7F0(task, indexed model/resource tuple, ...)
```

`func_8000DBF0_E7F0` is the generic kind-2 display-object primitive. It writes
the model pointer at object `+0x2C`, animation context at `+0x30`, transform at
`+0x08..+0x24`, and segment file ids at `+0x34/+0x3C`, then registers the
segment bases. It does not create a playable actor.

## Indexed title actor data

`func_8020FD8C_69778C` reads three parallel tables at `0x80214770`,
`0x80214788`, and `0x80214794`. The first four entries are the four characters
spawned together by the title director:

| Title index | Character order | Model pointer | Segment-8 file | Segment-9 file |
| ---: | --- | ---: | ---: | ---: |
| 0 | Goemon | `0x08000BDC` | `0x287` | `0x180` |
| 1 | Ebisumaru | `0x080004B4` | `0x286` | `0x17D` |
| 2 | Sasuke | `0x08000758` | `0x2C1` | `0x17C` |
| 3 | Yae | `0x08000CFC` | `0x285` | `0x000` |

The character labels follow the game's stable character-id order, independently
confirmed by the four-entry gameplay render tables: 0 Goemon, 1 Ebisumaru,
2 Sasuke, and 3 Yae. The title table also has indices 4 and 5, but those are
separate presentation variants used later in the sequence and are not remote
player identities.

The constructor adds segment selector `0x60000000` to the indexed model
pointer, supplies the title animation context at `0x802146D8`, and uses the
same title-specific scale for all entries. `func_8020FEA0_6978A0` then selects
one of the title sequence's special clips.

## Remote-render boundary

The title data proves that Sasuke and Yae use the same safe standalone
task/display-object architecture already used for remote Goemon and Ebisumaru.
The title resources themselves are not suitable for exact remote gameplay
animation synchronization: their selector exposes title-presentation clips,
not the complete gameplay action-id space sent over the network.

Anchor therefore keeps the proven cutscene spawn architecture but binds the
game's immutable clothed render metadata for all four characters:

| Character id | Character | Broad file | Raw action-model file |
| ---: | --- | ---: | ---: |
| 0 | Goemon | `0x120` | `0x123` |
| 1 | Ebisumaru | `0x124` | `0x127` |
| 2 | Sasuke | `0x128` | `0x12B` |
| 3 | Yae | `0x12C` | `0x12F` |

Ghidra shows `func_801DC9C8_5983C8` indexing the four-entry broad-file table
and `func_801DC70C_59810C` indexing the parallel action-file table. The remote
renderer reads those immutable file ids plus the matching action record from
`D_80203F34_5BFE44`, then binds the received action, frame, and rotations to an
independent kind-2 object. It never calls the playable staging or action
callbacks that consume the same metadata.
