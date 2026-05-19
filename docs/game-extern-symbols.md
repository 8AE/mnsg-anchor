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
- `anchor_actors_update_particle_markers` publishes local position and updates
  remote markers/nameplates.
- Race UI hooks apply pending race setup once save memory exists.

### `D_800C7AB2`

Current room/scene id. Actor manager references read this while deciding which
actors belong in the active room. The mod uses it as the local room id for
Anchor state and as a visibility filter so remote markers are only drawn when a
teammate is in the same room.

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

## Remote Marker Effect Tasks

### `D_801FC604_5B8514`

Current actor/task owner. Effect code and actor spawning read this pointer as
the active parent task. Anchor remote marker tasks are parented to it; if the
pointer changes during a room transition, the mod discards cached marker task
pointers and creates new ones.

### `D_801FC60C_5B851C`

Current player world object pointer. The mod only relies on the first known
position fields:

- `+0x08`: world X
- `+0x0c`: world Y
- `+0x10`: world Z

Those fields are used for local position publishing and for projecting remote
nameplates relative to the local player.

### `D_8020D1C0_5C90D0` And `D_8020D1D0_5C90E0`

Camera vector and horizontal radius. Camera routines write both globals; the
nameplate code uses the vector as the offset from player position to camera
position, then divides by the radius to derive a forward vector for simple 3D
to 2D projection.

### `func_80034E08_35A08`

Allocates and inserts an engine task. Decompilation shows this wrapper:

1. Calls `func_80034B58(task_list, update, flags)` to take a task from the
   global free list, clear it, attach it to `task_list`, store the update
   callback at task `+0x0c`, set flags at `+0x28`, and stamp it with a frame or
   time value.
2. Calls `func_80034D24(task)` to reorder the task in its linked list by
   priority/depth.

Anchor uses it to create a persistent effect task per visible remote player
slot.

### `func_80035EEC_36AEC`

Allocates a chain of particle/effect records from the game's effect pool. It
masks `count` to one byte, checks the remaining pool capacity for `kind`, calls
an allocator for each record, and appends them through task offsets `+0x18` and
`+0x1c`. It returns the first allocated particle, or `NULL` if allocation fails.

Anchor requests three kind-2 particles for each marker sparkle.

### `func_801EE4AC_5AA3BC`

Initializes the common effect-task header from a parent task:

- Clears bytes/halfwords at offsets `0x60..0x68`.
- Stores `effect_type` at `+0x64`.
- Stores the parent task pointer at `+0x84`.
- Copies parent state from parent `+0x5c` into task `+0x5c`.

Anchor calls it immediately after task allocation so the effect task behaves
like a normal child effect owned by the current scene task.

### `func_801EE750_5AA660`

Initializes particle draw/config state. Ghidra shows it calling a reset helper,
then setting particle `+0x30` to a render-mode table entry selected by `type`
ORed with the caller-provided `flags`.

Anchor passes `particle + 0x80` in the flags so the particle draws the display
list built into its own memory.

### `func_801EF684_5AB594`

Initializes the tiny state block paired with a particle. The game writes a
default config/vtable pointer, clears a state byte, stores `0xffff` in a
halfword, then runs a common reset helper. Anchor keeps one of these state
blocks in the effect task at `task + 0xa0 + particle_index * 8`.

### `func_801DC554_598464`

Builds and cache-writes a tiny display list:

1. `gSPDisplayList(texture)`
2. Optional primitive color
3. Environment color
4. `gSPEndDisplayList()`

Anchor uses it to recolor the stock sparkle texture and pulse marker alpha
without maintaining separate display-list assets.

### `D_80204DA0_5C0CB0` And `D_802049C0_5C08D0`

Stock effect assets. Cross references show `D_80204DA0` being passed into
particle init helpers and `D_802049C0` being used as a texture/display-list
asset by multiple effect routines. Anchor reuses them for lightweight remote
presence markers.
