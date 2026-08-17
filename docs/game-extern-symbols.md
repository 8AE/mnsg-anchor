# Game Extern Symbols

This mod calls directly into Mystical Ninja Starring Goemon game code through
extern functions and data symbols. The notes below are based on the loaded
Ghidra decompilation and cross references for the current USA symbol set.

## Frame And Room State

### `func_80002040_2C40`

This is the frame-step dispatcher used by several `RECOMP_HOOK_RETURN` hooks.
Ghidra shows it copying a small callback table from `PTR_ARRAY_80058908`, then
calling the entry selected by `PTR_g_system_8015c5c8->stepw`.

The mod hooks its return so Anchor work runs after the normal game step:

- `item_sync_update` drains network packets and writes save flags.
- `anchor_character_frame_hook` keeps the selected character legal.
- `anchor_actors_update_cutscene_models` publishes local position/animation and
  updates all four remote cutscene models and nameplates.
- Race UI hooks apply pending race setup once save memory exists.

### `D_800C7AB2`

Current room/scene id. Actor manager references read this while deciding which
actors belong in the active room. The mod uses it as the local room id for
Anchor state and as a visibility filter so remote models are only drawn when a
player is in the same room.

## Save Data And Character State

### `D_8015C608_15D208`

Main save-data flag/item block at VRAM `0x8015C608`. The mod treats this as the
authoritative save store:

- `+0x000 .. +0x303` contains packed one-bit flags.
- Positive offsets such as `+0x94`, `+0x204`, and `+0x20a` hold 32-bit or
  16-bit item/spawn fields depending on the game field.
- Negative offsets such as `-0x28` are nearby stats fields; this mod uses
  `hp_max > 0` as the save-loaded check.

Ghidra confirms `func_8000B640_C240` clears/seeds this block for a new file and
`func_8000B5D0_C1D0` mirrors it into the backup block at `D_8015C910`.

### `D_8015C5D8_15D1D8`

Runtime save/control mirror. `func_8000B5D0_C1D0` copies data into this region
from `D_8015C66C`. The character patch uses:

- `D_8015C5D8[1]`: current character id.
- `D_8015C5D8[0x2c / 4]`: dirty/changed flag set by the original cycler.

### `func_801DD5C0_5994D0`

Applies a live character change to the player object. Decompiled behavior:

- Writes the selected character to `player + 0x60` and global `D_8015C5DC`.
- Marks the actor's change flag at `*(player + 0x5c) + 0x69`.
- Clears movement/action fields and some state timers.
- Sets the player object's callback with `FUN_8003522c(player, FUN_801e0944)`.

The mod calls this after choosing the next enabled character so the in-memory
player object, runtime save mirror, and animation/state callback all agree.

## Race Startup And Direct Launch

### `func_8000B640_C240`

Initializes a new save file. Ghidra shows it clearing `D_8015C608` for `0x304`
bytes, setting default health/item/character values, setting the default spawn
room to Goemon's house (`0x1d1`), writing default spawn coordinates, then
copying the block to `D_8015C910`.

The direct race-start path calls this first so later race setup writes land on
a known clean save layout without showing the game's file-select menu.

### `func_8000B5D0_C1D0`

Marks the freshly initialized save as started/loaded. It clears a global system
field, copies the short runtime save/control block into `D_8015C5D8`, mirrors
`D_8015C608` into `D_8015C910`, and sets `D_8015C5D8` to `1`.

The mod hooks this function's return and also calls it during direct race start
so pending race flags can be applied as soon as save memory is live.

### `g_system + 0x3B040`

Adventure Diary slot marker used by the file-select path. The old race autoload
hook wrote `-1` here immediately after `func_8000B5D0_C1D0`; preserving that
write is what keeps a direct race start in No Adventure Diary mode. If the
direct launcher only initializes and starts the save block, the game binds the
session to save slot one instead.

### `func_8000607C_6C7C`

Writes the engine destination fields: room, player x/y/z, camera rotation,
player rotation, and two related load fields. Direct race start uses this
instead of only touching the save spawn fields so the scene transition sees the
same destination state as the original No Adventure Diary flow.

### `func_8000B364_BF64` and `func_8000383C_443C`

Scene and graphics setup used by the game's No Adventure Diary start path after
the save block has been initialized. `func_8000B364_BF64` syncs the active stage
from the destination stage and kicks scene-load state, while
`func_8000383C_443C` resets render state and refreshes the graphics/texture
buffer for the next scene. The direct race launcher runs these before entering
warp step `12`; skipping them left the renderer in the menu/loading
configuration and could make loaded areas appear overbright. Step `7` is part
of the game's own start callback context, but using it from the recomp UI path
can fall back into the title/demo sequence.

### `func_80003478_4078`

Gameplay-control/UI state initializer. Ghidra shows it incrementing
`g_system + 0x3ADDE`, clearing `g_system + 0x3ADDF`, and resetting the substate
at `g_system + 0x3ADE0/0x3ADE1`. Direct race start calls this after
`func_8000383C_443C` because that render reset clears UI/task slots; without
this follow-up, gameplay can load but the in-game HUD, pause menu, and minimap
controls remain disabled.

### `g_system + 0x3AE16`

Load-source/gameplay-state field used by the start and transition paths.
`func_80005EDC_6ADC`, the original No Adventure Diary start function, sets this
to `1` before entering step `7`; the normal step path later clears it before
gameplay. Direct race start uses warp step `12` instead, so it explicitly keeps
this field at `0`. Leaving it set to `1` allows the scene to load but prevents
normal gameplay UI processing from running, which matches the missing HUD,
pause, and minimap behavior.

### `func_8003521C_35E1C` and `func_801CD890_660740`

Gameplay callback handoff used by the old race file-select autoload path.
`func_8003521C_35E1C` stores a callback pointer in the active scheduler task's
`+0x0c` slot; the race launcher installs `func_801CD890_660740` after the
direct scene setup and warp-step request. Without this handoff the scene can
load and player movement can work, but the normal in-game callback path that
drives HUD, pause, minimap, and related gameplay UI does not run.

### `func_8000B2A0_BEA0`

Reads the spawn fields from save data into the global destination fields:
destination room, player rotation, player x/y/z, and camera rotation. The race
spawn hook runs before this function so `apply_race_start_location()` can write
the selected start location into the fields the game is about to consume.

### `D_8006B780_6C380`

Room-indexed debug/start-position table. The race UI treats each room entry as
five signed shorts:

1. X position
2. Y position
3. Z position
4. Camera rotation
5. Player rotation

These values populate race start choices and are copied into the save spawn
fields when a race starts.

### `func_80003728_4328`

Engine step switcher. Ghidra shows it writing `g_system->stepw`, clearing
`stepw_end`, marking the system as transitioning, and resetting the step
substate:

```c
PTR_g_system_8015c5c8->stepw = param_1;
PTR_g_system_8015c5c8->stepw_end = 0;
PTR_g_system_8015c5c8->field15_0x3adca = 2;
FUN_80003628(0);
```

The debug transport menu and direct race start use step `12`, the normal
warp/load state, after writing the desired destination room and coordinates.

### `D_8015C5C8_15D1C8`

Global game-system pointer. Direct race start writes the selected destination
stage through this pointer and also updates the static game-system destination
fields before entering the warp/load step.

## Remote Cutscene-Style Model Tasks

The renderer follows the opening cutscene's task architecture but does not call
an overlay-local character creator and does not create another playable actor.
It uses pristine shared Goemon/Ebisumaru/Sasuke/Yae display data on a plain
child task, with a slot-private action slice only when an appearance change
requires the native model-replacement walker to mutate display pointers.

### `D_801FC604_5B8514`

Ghidra shows this as the live player task. Anchor reads its final action byte at
`+0xCC` for publishing and uses the task only as the parent of independent
remote render children. When the pointer changes, mod-side handles are
discarded without touching the destroyed task tree.

### `D_801FC60C_5B851C`

Current local player model/display object. Anchor reads:

- `+0x08/+0x0C/+0x10`: world position;
- `+0x14/+0x16/+0x18`: rotation;
- `+0x28`: current animation frame.

The frame count is obtained with `func_8001B5AC`. Character, action, frame,
frame count, and rotations are sent together so the receiving client binds the
same character/action record and predicts the one frame between packets.

### `D_8020D1C0_5C90D0` and `D_8020D1D0_5C90E0`

Engine-maintained camera offset and radius. They are read only for nameplate
projection and are not involved in model creation.

### `func_80034E08_35A08`

Allocates and inserts an engine task. Anchor uses it to create one plain child
task per visible Goemon/Ebisumaru/Sasuke/Yae remote, matching the standalone
ownership pattern traced in the opening.

### `func_8000DBF0_E7F0`

Allocates one kind-2 model/display record and initializes its transform,
animation context, and segment bindings. Anchor calls it with a null display
pointer so the task begins hidden, then binds validated model data after every
required segment base is ready.

This is a generic display allocator used by cutscenes. It does not initialize a
player task, controls, collision, inventory, or gameplay behavior.

It also does **not** initialize display-record byte `+0x05`. Ghidra shows that
the lower free-list allocator `func_80035D8C` clears only bit 7 of `+0x04`, so a
reused kind-2 record can retain an unrelated `+0x05` value. The stock clothed
player renderer corrects that in `func_801CC30C` by writing `2` to `+0x05`
immediately after setting animation context `0xC01FC680`. Anchor reproduces
that one render-object field assignment after its generic cutscene allocation;
it does not call the player initializer.

### `func_8020D6BC_5C8B8C`

Gameplay stage-resource dispatcher. Its return hook is the only place the
remote renderer stages broad Goemon/Ebisumaru/Sasuke/Yae data and performs the
initial raw action-file DMA. No broad resource loading occurs in the per-frame
hook.

### `func_80013B14_14714` and `func_800141C4_14DC4`

`func_80013B14` loads or reuses a broad file in the scene resource registry;
`func_800141C4` returns the resident base or `-1`. The renderer loads and
validates these immutable broad files:

| Character | Broad file |
| --- | ---: |
| Goemon | `0x120` |
| Ebisumaru | `0x124` |
| Sasuke | `0x128` |
| Yae | `0x12C` |

The registered base is bound to object segment 9 at `+0x40`.

### `func_80001D68_2968`, `func_80001D94_2994`, and `func_80001640_2240`

The first two functions return a file's ROM bounds; the third performs the
blocking DMA. At the stage hook, Anchor copies the complete raw Goemon
`0x123`, Ebisumaru `0x127`, Sasuke `0x12B`, and Yae `0x12F` action-model files
into persistent mod-owned buffers. Those bases are bound to object segment 8
at `+0x38`.

### `D_80203F34_5BFE44`

Four immutable character action arrays; each action record is `0x1C` bytes.
The remote renderer selects Goemon entry 0, Ebisumaru entry 1, Sasuke entry 2,
or Yae entry 3. It uses the record's display pointer, animation speed, aux
selector, and aux resource table.

The renderer never calls `func_801DAC7C` or `func_801DAD68`, never installs
`D_80203B90` behavior callbacks, and never mutates the live player task. The
table is model metadata only.

### `D_80204020_5BFF30` and `D_80204028_5BFF38`

Immutable per-character file-id tables. The former selects the broad file; the
latter selects the raw action-model file. They are indexed only with validated
remote character ids 0 through 3.

### `D_80203FF0_5BFF00`, `D_80203FF8_5BFF08`, and `func_800145B4_151B4`

The first descriptor table identifies the two action-start auxiliary display
segments. The second contains the segment ids used for timed per-frame
face/part updates. The renderer loads each nonzero resource into a slot-owned
double buffer with `func_800145B4` and binds the validated segment base without
borrowing the local player's buffers.

Ghidra confirms that an action-start bind alone is insufficient:

- `func_801DAF54` binds row zero and the action's `+0x14` row, then resets the
  two expression cursors;
- `func_801CBAF8` calls `func_801DABDC` every active player frame;
- `func_801DB060` reads the current model frame and the `+4` threshold in each
  8-byte auxiliary row;
- `func_801DB1D4` advances the row, treats resource id `0xFF` as the sequence
  wrap marker, and calls `func_801DC87C`;
- `func_801DC87C` loads the changed resource into the alternate buffer and
  rebinds the segment from `D_80203FF8`.

The remote renderer reproduces the two persistent cursors and advances each by
at most one row per rendered frame. This distinction matters when a remote
player first appears with an animation already in progress: scanning forward
through many rows in one update is not stock behavior and can escape the valid
sequence, bind an unrelated resource, and send an invalid display list to
RT64. Resource ids and stored sizes are now checked before `func_800145B4` is
allowed to write to a buffer.

The renderer does not call these playable-task functions. The buffers are
`0x1000` bytes each, matching the four stock buffers allocated by
`func_801DC630`.

### `D_80167FC0_168BC0` and renderer-visible face memory

Ghidra identifies `D_80167FC0_168BC0` as the 48-entry resident scene-resource
registry. `func_80013B14` finds the first entry whose file id is zero and uses
that entry's data pointer as the next free address. It writes the aligned end
of the loaded file into the following entry, making the zero-id entry a bump
cursor for the next resident allocation.

This distinction fixes the persistent corrupted-face bug. `recomp_alloc`
serves memory beginning at `0x81000000`, beyond the original 8 MiB RDRAM
window. The CPU-side model walker can read action metadata and geometry from
that extended memory, which is why the remote body rendered correctly.
Face/part resources are different: their texture addresses reach RT64 through
generated N64 display-list commands. Ghidra shows `func_800196F0` emitting each
object segment base by adding `0x80000000` to the bound KSEG0 pointer. A normal
base such as `0x80305500` therefore becomes physical `0x00305500`; an extended
base such as `0x81000000` becomes `0x01000000`, outside the renderer's normal
8 MiB window. With extended-RDRAM display-list mode not enabled by the base
recomp, that address samples unrelated memory, producing the multicolored or
missing faces seen on both Goemon and Ebisumaru.

The stock player does not allocate these buffers from the general 40 KiB game
heap. `func_801DC630` starts at `0x80305500`, advances the resident scene arena
four times by `0x1000` for its two-channel double buffers, and then carves its
larger action-model buffers. Anchor now follows that persistent-arena pattern:

- after all four remote characters and their broad dependencies are resident, it
  finds the scene registry's zero-id cursor;
- it reserves `25 * 2 * 2 * 0x1000 = 0x64000` bytes, enough for both double-
  buffered channels of every remote slot;
- it advances the external cursor so any later scene resource begins after
  the reserved range;
- it rejects a reservation whose end would cross `0x80800000`, and keeps all
  four character caches unavailable rather than rendering with unsafe addresses;
- it invalidates every old face-buffer pointer when a new stage rebuilds the
  registry.

The action files remain in mod memory because they are consumed by the CPU-side
model path. Only the auxiliary buffers whose pixels become renderer-visible
texture addresses require original-RDRAM residency.

### Sudden Impact appearance state

Goemon's gold hair is not implied by action `0x82` or `0x83` alone. The stock
activation callback `func_801F5314_5B1224` sets player-work byte `+0x84` to `1`
at animation frame 10. `func_801DD498_5993A8` then selects
`D_80204048_5BFF58[1]` and calls `func_8001C3E0_1CFE0`, which recursively
replaces display pointers in the currently bound model. When the ability timer
at player-work `+0x86` expires, `func_801E78DC_5A37EC` temporarily sets `+0x84`
to `2`, applies the inverse table, and clears the byte.

Anchor samples the authoritative `+0x84 == 1` state after the local game frame
and sends it as `suddenImpact` in the same `MNSG_PLAYER_POS` packet as action,
animation phase, and rotation. An appearance edge bypasses the normal movement
rate limit. The compact C-facing snapshot exposes the value as `si`.

The remote renderer never runs the ability callback or timer. Instead, it
copies the current Goemon action range from the pristine whole-file cache into
a slot-private buffer, binds that range with the same base arithmetic used by
`func_801DC70C_59861C`, and applies replacement table 1 with
`func_8001C3E0_1CFE0`. Returning to normal simply rebinds the pristine shared
cache. Keeping the mutable range private prevents one transformed Goemon from
changing every remote Goemon that shares the action-file cache.

Aux loads and model-segment rebinds run from each remote model's ordinary task
callback. They are not performed from the return hook after the selected game
step has completed. This preserves the engine's pre-render ordering and avoids
changing face memory after a display list using that memory may already have
been submitted to RT64.

### `func_8001B5AC_1C1AC`

Resolves the bound segmented display pointer and returns its clip frame count.
The receiver snaps to each authoritative remote frame and derives the
intervening frame step from consecutive packets, including zero for a paused
animation.

### `func_80034EF8_35AF8`

Ghidra shows this destructor recursively freeing child tasks and then calling
`func_800350C4`. The latter splices the task out through its `+0x04` backlink
without validating that link. `func_80034A10` clears the backlink when a task
is freed, so calling the destructor again after owner teardown can follow the
engine's `0x80000000` invalid-link sentinel and crash.

Anchor therefore does not call this destructor. Each remote task remains a
child of the current player-owner task, is hidden and reused when a peer leaves,
and is reclaimed exactly once by the engine when the owner tree is destroyed.
Before reusing a retained child, Anchor verifies that its backlink still points
back to the task; otherwise it discards the stale mod-side handles without
touching game memory.

## Corrected Opening-Resource Boundary

The opening's standalone file-`0x4D9` Ebisumaru is intentionally the
naked/fundoshi variant and its selector exposes only nine special opening
clips. The opening Goemon controller drives the already-live local player model
and never creates a standalone Goemon display object.

Therefore the remote implementation keeps the opening's plain task/display
architecture but not those presentation assets. Immutable clothed
Goemon/Ebisumaru/Sasuke/Yae render data is bound to independent objects; no
playable actor code runs.

## Hook Boundary

- `RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")` stages all four characters after
  normal stage resource loading.
- `RECOMP_HOOK_RETURN("func_80002040_2C40")` publishes the final local state,
  queues complete remote snapshots, and draws nameplates. Each remote
  cutscene-style child task applies its queued transform/action/frame and aux
  resource changes during the engine's next scheduled pre-render update.
- Overlay-local `file_18` creators are neither patched nor called while
  another overlay occupies their VRAM addresses.
