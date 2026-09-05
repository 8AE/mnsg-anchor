#ifndef ANCHOR_BOSS_INVITE_WORLD_H
#define ANCHOR_BOSS_INVITE_WORLD_H

#define ANCHOR_BOSS_ARENA_CONGO 1
#define ANCHOR_BOSS_ROOM_CONGO 0x0016u

/* A loaded Congo visit remains an arena during its native introduction and
 * dialogue. Reception/teleport readiness is deliberately a separate check. */
int anchor_boss_invite_world_arena(void);
unsigned int anchor_boss_invite_world_visit(void);

/* The caller must additionally let an existing native/custom dialog finish. */
int anchor_boss_invite_world_can_prompt(void);

/* Request a native room load at Congo's default entrance. Call only after
 * the invitation dialog has completely closed and released player control. */
int anchor_boss_invite_world_warp(void);

#endif
