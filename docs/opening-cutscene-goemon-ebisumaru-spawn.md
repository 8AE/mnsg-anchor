# Opening Cutscene Goemon and Ebisumaru Spawn Trace

## Scope

This document traces only the special Goemon and Ebisumaru objects used by the
new-file opening cutscene. They are not gameplay/player actors, and they are not
spawned by the room actor manager.

The trace was reconstructed from:

- the supplied boot-to-new-file function log;
- Ghidra decompilation of the stable engine functions;
- a dedicated Ghidra import of static overlay `file_18`, whose code occupies ROM
  `0x676C80..0x694CA0` and is loaded at VRAM `0x8020D2A0`;
- debug strings in that overlay, including `OPENING_START:` and
  `GOEMON_CREAT1`.

A separate import is important because several static overlays reuse the same
VRAM range. Looking at an unrelated overlay at the same VRAM address produces a
valid decompilation of the wrong code.

## Result

The opening overlay creates both characters as ordinary engine tasks with
attached model/display objects:

```text
new-file setup
  func_8000B640_C240
  func_8000B5D0_C1D0
  func_8000B3E4_BFE4
  func_80020440_21040                 select/load the static overlay pair
    func_800203D4_20FD4
    func_80211EA0_67B880             file_18 opening entry point
      func_80211D7C_67B75C           create opening director
        func_8020D2E0_676CC0         opening resource/setup work
        func_8020D540_676F20         opening camera/render setup
        func_8020D998_677378         per-frame opening state machine
          func_802135E4_67CFC4       create Goemon or a nearby scene prop
            func_80213540_67CF20     Goemon/prop movement callback
          func_80213D4C_67D72C       create Ebisumaru
            func_802139C4_67D3A4     Ebisumaru movement/animation callback
            func_80213B8C_67D56C     change Ebisumaru visual variant
            func_80213C84_67D664     set interpolation duration
            func_80213C90_67D670     set movement target
```

The supplied log contains this exact sequence: `func_80211EA0` through
`func_8020D998` begins at log lines 507-512, the Goemon creator/callback appears
at lines 680 and 685, and the Ebisumaru creator and helpers appear at lines
732-737.

## Entering the opening overlay

`func_80020440_21040` selects and loads a pair of static overlays through
`func_800203D4_20FD4`. For the new-file opening it dispatches
`func_80211EA0_67B880` in `file_18`.

`func_80211EA0_67B880` reads the opening-scene selector and chooses the opening
branch. That branch calls `func_80211D7C_67B75C`, which:

1. creates the opening director task with the engine task allocator;
2. installs `func_8020D998_677378` as its update callback;
3. initializes the cutscene resources, camera, and render state through
   `func_8020D2E0_676CC0` and `func_8020D540_676F20`.

`func_8020D998_677378` is a 34-state director. The two requested characters are
created in director state 10. The state owns every task described below and
explicitly destroys them before moving to state 11.

## Goemon creation

At state-10 counter value 1, the director calls:

```c
func_802135E4_67CFC4(
    director,
    0,                    // type 0: Goemon
    { 90.0, 20.0, -90.0 },
    { -1.8, 0.5, 0.0 }
);
```

The returned task is retained in the director's state work area at `+0x30`.
The director also plays sound `0x28A` at this point.

Ghidra shows `func_802135E4_67CFC4` doing the following for type 0:

1. Allocate a child task through `func_80034E08` and give it callback
   `func_80213540_67CF20`.
2. Create the model/display object through `func_8000DBF0` using:
   - resource file id `0x288`;
   - flagged model pointer `0x180000AC`, which resolves to offset `0xAC` in
     that resource;
   - animation/context pointer `0x8006D920`;
   - scale `0.1`;
   - the position supplied by the director.
3. Store velocity in task fields `+0x60`, `+0x64`, and `+0x68`.
4. Set all three model rotations to `0x8000` for type 0.

The nearby `GOEMON_CREAT1` debug string and the type-0 model branch identify
this object as the cutscene Goemon.

The same creator is called again at counter values 20 and 40 with type 1 and
type 2. Those objects use model pointers `0x480065F0` and `0x48003EC0` from the
same resource file and are scene props, not Ebisumaru. Their returned tasks are
stored at `+0x34` and `+0x38`. Treating the second call as “the second
character” is therefore incorrect.

### Goemon update behavior

`func_80213540_67CF20` is a small ballistic-motion callback:

```text
vertical_velocity -= 0.1
model.position     += velocity

if model.y < 0:
    model.y = 0
    velocity.x /=  2
    velocity.y /= -2
    velocity.z /=  2
```

This is a self-contained cutscene movement task. It does not initialize a
gameplay actor record and never supplies an actor id to the actor manager.

## Ebisumaru creation

At state-10 counter value `195` (`0xC3`), the director calls:

```c
ebi = func_80213D4C_67D72C(director, 80.0, 0.0, -90.0);
func_80213C84_67D664(ebi, 80.0);
func_80213B8C_67D56C(ebi, 1);
func_80213C90_67D670(ebi, 40.0, -90.0);
```

The returned Ebisumaru task is retained at director state-work offset `+0x18`.

`func_80213D4C_67D72C`:

1. creates a child task through `func_80034E08` with callback
   `func_802139C4_67D3A4`;
2. creates its model/display object through `func_8000DBF0` with scale `0.1`;
3. initially selects resource file `0x277` and flagged model pointer
   `0x18000554` (resource offset `0x554`), with animation/context pointer
   `0x8006D898`;
4. stores the destination coordinates `80.0, 0.0, -90.0` in task fields
   `+0x68`, `+0x74`, and `+0x80`;
5. initializes a secondary model/animation pointer from the created model.

The creator starts the render object at zeroed coordinates. The subsequent
interpolation and target calls move it into the shot.

### Visual-variant change

At counter value `275` (`0x113`), the director performs the cutscene's
Ebisumaru clothing/underwear change:

```c
func_80213B8C_67D56C(ebi, 2);
func_80213C90_67D670(ebi, 35.0, -75.0);
```

It also plays sound `0x28B`. Variant 2 changes the existing render object to
resource file `0x4D8`, flagged model pointer `0x18001A3C` (resource offset
`0x1A3C`), then resolves the new resource and resets the model's animation
frame.

The complete selector in `func_80213B8C_67D56C` is:

| Variant | Resource file | Model pointer | Animation step |
| ---: | ---: | ---: | ---: |
| 0 | `0x277` | `0x180009B4` | `0.2` |
| 1 | `0x277` | `0x18000554` | `0.3` |
| 2 | `0x4D8` | `0x18001A3C` | `0.2` |

The state-10 timing, the initial variant-1 selection, and the later variant-2
model/resource replacement match the on-screen Ebisumaru gag and distinguish
this task from the Goemon-adjacent props.

### Ebisumaru update behavior

`func_802139C4_67D3A4`:

- moves the task's current coordinates toward its stored targets using
  per-axis interpolation steps;
- advances the selected model animation by the variant's animation step and
  wraps/clamps it against the animation length;
- copies the task's rotation and interpolated coordinates into the attached
  model every frame.

`func_80213C84_67D664` sets the interpolation duration.
`func_80213C90_67D670` stores the next movement target, calculates the
per-frame deltas, and updates the facing angle.

## Shared model-resource path

Both creators ultimately call `func_8000DBF0`. Its decompilation shows that it
allocates a model/display object, writes its model and animation/context
pointers, position, rotation, scale, and resource ids, and then calls
`func_80014218_14E18` to resolve the required loaded resources.

The opening setup includes the resources used here in its load list, including
`0x288`, `0x277`, and `0x4D8`. The flagged model values passed by the creators
are resource-relative references, not gameplay character ids.

## Lifetime and cleanup

At state-10 counter value `450` (`0x1C2`), the director advances to state 11
and deletes:

- the Goemon task at director work `+0x30`;
- the two prop tasks at `+0x34` and `+0x38`;
- the Ebisumaru task at `+0x18`.

Deletion uses the normal task cleanup helper (`func_80034EF8_35AF8`). The
characters therefore exist only inside this opening-director state.

## Address reference

All overlay addresses below are from `file_18`:

| Purpose | VRAM | ROM |
| --- | ---: | ---: |
| Opening entry point | `0x80211EA0` | `0x67B880` |
| Create opening director | `0x80211D7C` | `0x67B75C` |
| Opening state machine | `0x8020D998` | `0x677378` |
| Goemon/prop creator | `0x802135E4` | `0x67CFC4` |
| Goemon/prop update | `0x80213540` | `0x67CF20` |
| Ebisumaru update | `0x802139C4` | `0x67D3A4` |
| Ebisumaru variant selector | `0x80213B8C` | `0x67D56C` |
| Ebisumaru interpolation duration | `0x80213C84` | `0x67D664` |
| Ebisumaru movement target | `0x80213C90` | `0x67D670` |
| Ebisumaru creator | `0x80213D4C` | `0x67D72C` |

Useful call sites inside `func_8020D998_677378` are:

| State-10 action | VRAM call site | ROM call site |
| --- | ---: | ---: |
| Create Goemon | `0x8020E0D4` | `0x677AB4` |
| Create type-1 prop | `0x8020E13C` | `0x677B1C` |
| Create type-2 prop | `0x8020E1F0` | `0x677BD0` |
| Create Ebisumaru | `0x8020E428` | `0x677E08` |
| Select Ebisumaru variant 2 | `0x8020E48C` | `0x677E6C` |

## Code that must not be used for these characters

The function log transitions out of `file_18` after the opening finishes. Only
then does it load the gameplay room and run the room actor manager. Those later
spawns are not part of the opening sequence.

In particular:

- do not use the local playable-character constructor or character-change
  callback;
- do not use the old playable-player or particle remote-rendering paths as a
  source of character model data; the current remote renderer instead
  reproduces the cutscene task/model arguments through stable engine helpers;
- do not route these objects through `actor_manager_spawn_actors`;
- do not use actor ids `0x2D3` or `0x2D4`: those later overlay initializers are
  the Lord of Oedo and Princess Yuki, not Goemon and Ebisumaru.

For an opening-cutscene hook or recreation, the correct boundary is the
`file_18` director and its four task/model functions described above.
