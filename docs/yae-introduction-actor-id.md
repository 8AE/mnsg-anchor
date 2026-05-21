# Yae Introduction Actor ID

## Summary

The actor/resource ID that corresponds to Yae for the Zazen introduction cutscene path is `0x00cb`.

Do not use `0x0257` as Yae's actor ID. `0x0257` is the event trigger actor named `Yae Introduction Event Trigger`; it is the actor that starts the cutscene logic.

```c
#define ACTOR_ID_YAE 0x00cb
#define ACTOR_ID_YAE_INTRO_TRIGGER 0x0257
```

## Export Evidence

The actor dump entry for the trigger appears in `vanilla.mn64.export.txt` under `ROOM 167: 411 ... MACHI 9 - Zazen - Gate`:

```text
006: 0257 0000 0000 0000 0000 0000 0000 0000
  @ 257                     # Yae Introduction Event Trigger
```

The matching instance is placed in the same room:

```text
+06: 0000 0000 ffe8 0000 0000 0000 0800 0060 0000 0000
  @ 006  # 257 Yae Introduction Event Trigger
  @ x:              0000
  @ z:              0000
  @ y:             -0018
```

That same room's load metadata includes `00cb`:

```text
# !misc loading_files       00da,00cb,00cd,007d
```

In this room, `00da` is the room actor file. The `0x0257` trigger's table entry resolves into that room actor file, while `0x00cb` is the Yae/player actor resource made available for the cutscene.

Nearby Zazen rooms also consistently load `00cb` with the Zazen room actor files:

```text
# !misc loading_files       00db,00cb,00cd,007d
# !misc loading_files       00dc,00cb,00cd,007d
# !misc loading_files       00dd,00cb,00cd,007d
```

## Ghidra Cross-Check

The engine's actor spawn path treats the actor ID as an index into actor metadata/function tables. In Ghidra, `actor_manager_spawn_actors` reads each actor definition's first halfword as the actor ID and uses it to index actor tables:

```c
PTR_ARRAY_802287bc[*(ushort *)DAT_8015cde0->data]
SHORT_ARRAY_802297d6[*(ushort *)DAT_8015cde0->data]
```

The dynamic spawn helper `FUN_80217360` has the same shape. Its second parameter is the actor ID, and it stores that ID on the spawned actor object:

```c
int FUN_80217360(int parent_task, ushort actor_id, byte actor_type)
{
    ...
    FUN_8003555c(..., PTR_ARRAY_802287bc[actor_id], ..., SHORT_ARRAY_802297d6[actor_id]);
    ...
    *(ushort *)(spawned_actor + 0x5e) = actor_id;
    *(ushort *)(spawned_actor + 0x5c) = actor_id;
}
```

This matches the interpretation that `0x0257` is a trigger actor, while `0x00cb` is the Yae actor/resource used by the cutscene path.

## Practical Use

Use `0x00cb` when code needs to refer to Yae as a spawned actor/resource.

Use `0x0257` only when code needs to detect or reason about the Zazen gate event trigger that starts Yae's introduction sequence.
