#ifndef ANCHOR_BOSS_ARENAS_H
#define ANCHOR_BOSS_ARENAS_H

/* Stable wire IDs. Congo remains 1 for existing invitation clients. */
#define ANCHOR_BOSS_ARENA_CONGO 1
#define ANCHOR_BOSS_ARENA_DHARUMANYO 2
#define ANCHOR_BOSS_ARENA_TSURAMI 3
#define ANCHOR_BOSS_ARENA_CONTROL_MACHINE 4

#define ANCHOR_BOSS_ROOM_CONGO 0x0016u
#define ANCHOR_BOSS_ROOM_DHARUMANYO 0x0049u
#define ANCHOR_BOSS_ROOM_TSURAMI 0x0071u
#define ANCHOR_BOSS_ROOM_CONTROL_MACHINE 0x0155u

/* Unknown IDs return -1 / NULL; ordinary rooms return arena ID zero. */
int anchor_boss_arena_room(int arena);
const char *anchor_boss_arena_name(int arena);
int anchor_boss_arena_for_room(unsigned short room);

#endif
