#ifndef ANCHOR_BOSS_INVITE_WORLD_H
#define ANCHOR_BOSS_INVITE_WORLD_H

#include "anchor_boss_arenas.h"

/* A loaded boss visit remains an arena during its native introduction and
 * dialogue. Reception/teleport readiness is deliberately a separate check. */
int anchor_boss_invite_world_arena(void);
unsigned int anchor_boss_invite_world_visit(void);

/* The first native Impact stage of the active sequence (0x021C..0x0224), or 0.
 * The invite carries it so a guest replays the Impact cutscene from its
 * beginning, even if the sender has already advanced to a later stage. */
unsigned int anchor_boss_invite_world_stage(void);

/* The native "load from start" fields recorded at the sequence's first entry.
 * Reproducing them resumes a guest at the sender's cutscene checkpoint. */
unsigned int anchor_boss_invite_world_field90(void);
unsigned int anchor_boss_invite_world_field91(void);

/* Record the dedicated native Impact-stage entry (used by the story, the
 * consecutive-boss mode and this mod's own accept warp). The first Impact boss
 * arena is announced from the Impact cutscene start rather than the ordinary
 * world room, so this must run on every Impact entry. */
void anchor_boss_invite_world_note_impact(void);

/* The caller must additionally let an existing native/custom dialog finish. */
int anchor_boss_invite_world_can_prompt(void);

/* Request a native room load at the selected arena's default entrance. Call only after
 * the invitation dialog has completely closed and released player control. */
int anchor_boss_invite_world_warp(int arena);

/* Request a native load of the exact Impact stage carried by an accepted
 * invitation (0x021C..0x0224), reproducing the sender's load-from-start
 * fields. Same caller preconditions as the arena warp. */
int anchor_boss_invite_world_warp_stage(unsigned int stage, unsigned int field90,
                                        unsigned int field91);

/* Request an ordinary live-world load at an exact peer position. This uses the
 * destination room's native camera/player rotations and intentionally permits a
 * same-room reload. Rooms above 0x225, including the 0x226 World Map overlay,
 * are rejected before the native room-start table is indexed. */
int anchor_boss_invite_world_transfer_to(unsigned short room,
                                         short x, short y, short z);

#endif
