# Opening Cutscene Goemon and Ebisumaru Spawn Trace

## Scope and corrected result

This document traces only the Goemon and Ebisumaru presentation used by the
new-file opening in static overlay `file_18` (ROM
`0x676C80..0x694CA0`, VRAM `0x8020D2A0`). It does not treat later room actors
or ordinary NPCs as either character.

The corrected result is asymmetric:

- Ebisumaru is a standalone cutscene task with a model/display object created
  directly from file `0x4D9`.
- Goemon has a cutscene-specific controller task, but that task does **not**
  create a Goemon model. It moves the already-live player model objects through
  globals `D_801FC60C` and `D_801FC61C`, and the director changes Goemon's
  animation through the live player task `D_801FC604`.

Consequently, there is no standalone opening-Goemon file/model tuple that can
be copied into `func_8000DBF0`. A recreation that forbids all player-renderer
assets can reproduce standalone Ebisumaru, but cannot create a second visible
Goemon from this opening path.

The earlier identification of `func_802135E4` as Goemon and
`func_80213D4C` as Ebisumaru was wrong. Runtime screenshots showed those exact
resources as a food/throwable prop and a male NPC. Rechecking the director
proves that both are later scene events, while the real character tasks are
created together during opening initialization.

## Evidence boundary

The trace uses:

- the supplied boot-to-new-file function log;
- Ghidra MCP decompilation of stable task, resource, model, and player-action
  helpers;
- `file_18` instructions from the exact US ROM at the symbol-map ROM offsets,
  necessary because several overlays reuse the same VRAM range;
- the observed remote-render output, which matches the two previously selected
  resource roots exactly.

The relevant stable Ghidra results are:

- `func_80013AC4` walks a zero-terminated list and loads each file through
  `func_80013B14`;
- `func_8000DBF0` creates and binds a model/display record;
- `func_801DAC7C`/`func_801DAD68` select an action on the live player task;
- `func_8001B5AC` resolves a model pointer and returns its frame count.

## Opening initialization path

The opening entry creates a director whose update function is
`func_8020D998_677378`. During its initialization branch, that director does
the following:

```text
func_8020D998_677378
  func_8020D818_6771F8             load initial opening resource list
    func_80013AC4
      files 0x71, 0x4D9, 0x4AB, 0x4DA
  func_80212684_67C064             create Goemon controller
    callback func_80211F60_67B940
  func_80212DA4_67C784             create standalone Ebisumaru
    callback func_802126FC_67C0DC
```

The exact director call sites are:

| Operation | VRAM call site | ROM call site |
| --- | ---: | ---: |
| Load initial opening files | `0x8020DA6C` | `0x67744C` |
| Create Goemon controller | `0x8020DA90` | `0x677470` |
| Create Ebisumaru | `0x8020DAAC` | `0x67748C` |

The director passes these initial positions:

- Goemon controller: `(90.0, 0.0, -80.0)`;
- Ebisumaru: `(90.0, 0.0, -90.0)`.

It retains the Goemon controller at director work offset `+0x10` and the
Ebisumaru task at `+0x14`. Later director states repeatedly operate on these
same two tasks.

## Goemon: controller around the live player renderer

`func_80212684_67C064` allocates a child task with callback
`func_80211F60_67B940`. It stores interpolation/controller state, including the
initial coordinates, but never calls `func_8000DBF0` and never creates a model
record.

`func_80211F60_67B940` updates the controller's position, rotation, and scale.
It then writes those values into the two existing player display objects held
by:

- `D_801FC60C`: the live player's primary model/display object;
- `D_801FC61C`: the related secondary player display object.

The opening director changes Goemon's animations by calling
`func_801DAC7C` on `D_801FC604`, the live player task. Ghidra decompilation of
`func_801DAD68`, which `func_801DAC7C` wraps, shows it indexing the selected
character's player action table and rebinding the live player's model record.

This is the decisive distinction:

```text
Goemon opening task
  owns controller state
  does not own/create a Goemon model
  drives the existing player renderer

Ebisumaru opening task
  owns controller state
  creates and owns its cutscene model
```

Calling `func_80212684` a standalone Goemon model creator is therefore
incorrect. Spawning another copy of its callback would move the local player
objects, not render an independent remote Goemon.

## Ebisumaru: standalone cutscene model

`func_80212DA4_67C784` is the standalone Ebisumaru creator. It:

1. allocates a child task through `func_80034E08` with callback
   `func_802126FC_67C0DC`;
2. calls `func_8000DBF0` with the exact model arguments below;
3. stores the initial coordinates in its controller work area;
4. allocates a related secondary kind-2 display record used by its update
   callback.

The verified model arguments are:

| Field | Value |
| --- | ---: |
| Resource file | `0x4D9` |
| Flagged model pointer | `0x68000B0C` |
| Animation context | `0xC006D898` |
| Rotation | `0, 0, 0` |
| Scale | `0.1, 0.1, 0.1` |
| Secondary file id | `0` |

The pointer `0x68000B0C` resolves through resource slot 8 to offset `0xB0C`
inside file `0x4D9`. It is not the `0x18000554` pointer from file `0x277`
that produced the male NPC in the remote-player screenshot.

### Ebisumaru animation selection

The director calls `func_80212A04_67C3E4` to change Ebisumaru's animation.
That function selects one of nine model/animation pointers in file `0x4D9`,
sets a per-animation frame step, and resets the model frame. Its initial pointer
is `0x68000B0C`.

`func_802126FC_67C0DC` advances and wraps the selected model frame, applies
controller interpolation, and copies the resulting transform and render state
to the attached records each frame. A remote recreation may map the sender's
normalized animation phase onto this fixed cutscene clip by writing model
frame `+0x28` and bounding it with `func_8001B5AC`.

## The two previously misidentified functions

### `func_802135E4_67CFC4`

This creator is first called much later at director call site `0x8020E0D4`.
Its callback `func_80213540_67CF20` applies gravity, velocity, ground collision,
and bounce damping. The type-0 resource tuple is:

- file `0x288`;
- pointer `0x180000AC`;
- context `0x8006D920`.

That tuple is the food/throwable-looking object in the screenshot, not Goemon.
The nearby `GOEMON_CREAT1` debug text describes the surrounding scene event;
it is not sufficient evidence that the created model is Goemon.

### `func_80213D4C_67D72C`

This creator is called later at `0x8020E428` and produces the male NPC shown in
the screenshot. Its initial tuple is file `0x277`, pointer `0x18000554`, and
context `0x8006D898`. Its variant/interpolation helpers operate on that scene
NPC; they are not the standalone Ebisumaru animation helpers.

## Implementation consequence

The remote renderer must use file `0x4D9`, pointer `0x68000B0C`, and context
`0xC006D898` for standalone cutscene Ebisumaru.

For Goemon, the opening trace offers only two truthful choices:

1. keep Goemon nameplate-only while preserving the prohibition on all
   playable-renderer data; or
2. authorize use of Goemon's render assets to create an independent plain
   model task, while still avoiding the playable actor constructor, player
   manager, and gameplay actor behavior.

Using `0x288/0x180000AC` is not a third choice; it is the wrong prop proven by
both its ballistic callback and the runtime screenshot.

## Correct address reference

| Purpose | VRAM | ROM |
| --- | ---: | ---: |
| Opening state machine | `0x8020D998` | `0x677378` |
| Initial resource-list wrapper | `0x8020D818` | `0x6771F8` |
| Goemon controller update | `0x80211F60` | `0x67B940` |
| Goemon controller creator | `0x80212684` | `0x67C064` |
| Ebisumaru update | `0x802126FC` | `0x67C0DC` |
| Ebisumaru animation selector | `0x80212A04` | `0x67C3E4` |
| Ebisumaru creator | `0x80212DA4` | `0x67C784` |
| Wrong ballistic prop creator | `0x802135E4` | `0x67CFC4` |
| Wrong scene-NPC creator | `0x80213D4C` | `0x67D72C` |
