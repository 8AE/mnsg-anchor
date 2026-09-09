#ifndef ANCHOR_BOSS_INVITE_WORLD_H
#define ANCHOR_BOSS_INVITE_WORLD_H

#include "anchor_boss_arenas.h"

/* A loaded boss visit remains an arena during its native introduction and
 * dialogue. Reception/teleport readiness is deliberately a separate check. */
int anchor_boss_invite_world_arena(void);
unsigned int anchor_boss_invite_world_visit(void);

/* The caller must additionally let an existing native/custom dialog finish. */
int anchor_boss_invite_world_can_prompt(void);

/* Request a native room load at the selected arena's default entrance. Call only after
 * the invitation dialog has completely closed and released player control. */
int anchor_boss_invite_world_warp(int arena);

/* Request an ordinary live-world load at an exact peer position. This uses the
 * destination room's native camera/player rotations and intentionally permits a
 * same-room reload. Rooms above 0x225, including the 0x226 World Map overlay,
 * are rejected before the native room-start table is indexed. */
int anchor_boss_invite_world_transfer_to(unsigned short room,
                                         short x, short y, short z);

#endif
