#ifndef ANCHOR_BOSS_ARENAS_H
#define ANCHOR_BOSS_ARENAS_H

/* Stable wire IDs. Congo remains 1 for existing invitation clients. */
#define ANCHOR_BOSS_ARENA_CONGO 1
#define ANCHOR_BOSS_ARENA_DHARUMANYO 2
#define ANCHOR_BOSS_ARENA_TSURAMI 3
#define ANCHOR_BOSS_ARENA_CONTROL_MACHINE 4
#define ANCHOR_BOSS_ARENA_KASHIWAGI 5

#define ANCHOR_BOSS_ROOM_CONGO 0x0016u
#define ANCHOR_BOSS_ROOM_DHARUMANYO 0x0049u
#define ANCHOR_BOSS_ROOM_TSURAMI 0x0071u
#define ANCHOR_BOSS_ROOM_CONTROL_MACHINE 0x0155u

/* The five giant-robot Impact bosses occupy stages 0x0220..0x0224. Kashiwagi,
 * the first, is 0x0220. These stages are joined through the dedicated native
 * Impact-stage transition rather than the ordinary room loader. */
#define ANCHOR_BOSS_IMPACT_ROOM_BASE 0x0220u
#define ANCHOR_BOSS_ROOM_KASHIWAGI 0x0220u

/* The complete Impact stage range: 0x021C..0x021F are the pre-boss minigames
 * and 0x0220..0x0224 the bosses. 0x0239 is the Impact intro cutscene stage the
 * game loads before the sequence (Goemon blows the triton shell and enters
 * Impact). The invite carries the exact stage so a guest loads the same
 * cutscene, minigame or boss stage as the sender. */
#define ANCHOR_BOSS_IMPACT_STAGE_FIRST 0x021Cu
#define ANCHOR_BOSS_IMPACT_STAGE_LAST 0x0224u
#define ANCHOR_BOSS_IMPACT_INTRO_STAGE 0x0239u
#define ANCHOR_BOSS_IMPACT_STAGE_VALID(stage)                                \
    (((unsigned int)(stage) >= ANCHOR_BOSS_IMPACT_STAGE_FIRST &&             \
      (unsigned int)(stage) <= ANCHOR_BOSS_IMPACT_STAGE_LAST) ||             \
     (unsigned int)(stage) == ANCHOR_BOSS_IMPACT_INTRO_STAGE)

/* Unknown IDs return -1 / NULL; ordinary rooms return arena ID zero. */
int anchor_boss_arena_room(int arena);
const char *anchor_boss_arena_name(int arena);
int anchor_boss_arena_for_room(unsigned short room);

#endif
