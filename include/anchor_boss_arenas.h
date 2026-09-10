#ifndef ANCHOR_BOSS_ARENAS_H
#define ANCHOR_BOSS_ARENAS_H

/* Stable wire IDs. Congo remains 1 for existing invitation clients. */
#define ANCHOR_BOSS_ARENA_CONGO 1
#define ANCHOR_BOSS_ARENA_DHARUMANYO 2
#define ANCHOR_BOSS_ARENA_TSURAMI 3
#define ANCHOR_BOSS_ARENA_CONTROL_MACHINE 4
#define ANCHOR_BOSS_ARENA_KASHIWAGI 5
#define ANCHOR_BOSS_ARENA_THAISAMBA 6
#define ANCHOR_BOSS_ARENA_BALBERRA 7
#define ANCHOR_BOSS_ARENA_DETOILE 8
#define ANCHOR_BOSS_ARENA_IMPACT_FIRST ANCHOR_BOSS_ARENA_KASHIWAGI
#define ANCHOR_BOSS_ARENA_IMPACT_LAST ANCHOR_BOSS_ARENA_DETOILE

#define ANCHOR_BOSS_ROOM_CONGO 0x0016u
#define ANCHOR_BOSS_ROOM_DHARUMANYO 0x0049u
#define ANCHOR_BOSS_ROOM_TSURAMI 0x0071u
#define ANCHOR_BOSS_ROOM_CONTROL_MACHINE 0x0155u

/* The four giant-robot Impact bosses occupy stages 0x0220..0x0223 (Kashiwagi,
 * Thaisamba, Balberra, D'Etoile). Their intro cutscene stages are 0x0239..0x023C
 * and their high-speed minigames 0x021C..0x021F. All are joined through the
 * dedicated native Impact-stage transition rather than the ordinary room
 * loader. Stage 0x0224 is an unused fifth slot and the boss rush mode uses its
 * own stage 0x0260, so neither is treated as an Impact arena. */
#define ANCHOR_BOSS_ROOM_KASHIWAGI 0x0220u
#define ANCHOR_BOSS_ROOM_THAISAMBA 0x0221u
#define ANCHOR_BOSS_ROOM_BALBERRA 0x0222u
#define ANCHOR_BOSS_ROOM_DETOILE 0x0223u
#define ANCHOR_BOSS_IMPACT_ROOM_BASE 0x0220u

/* The complete Impact stage range: 0x021C..0x021F are the pre-boss minigames
 * and 0x0220..0x0223 the bosses. 0x0239..0x023C are the intro cutscene stages,
 * one per boss (0x0239 is Kashiwagi's, where Goemon blows the triton shell and
 * enters Impact). The invite carries the exact stage so a guest loads the same
 * cutscene, minigame or boss stage as the sender. */
#define ANCHOR_BOSS_IMPACT_STAGE_FIRST 0x021Cu
#define ANCHOR_BOSS_IMPACT_STAGE_LAST 0x0223u
#define ANCHOR_BOSS_IMPACT_INTRO_FIRST 0x0239u
#define ANCHOR_BOSS_IMPACT_INTRO_LAST 0x023Cu
#define ANCHOR_BOSS_IMPACT_INTRO_STAGE ANCHOR_BOSS_IMPACT_INTRO_FIRST
#define ANCHOR_BOSS_IMPACT_STAGE_VALID(stage)                                \
    (((unsigned int)(stage) >= ANCHOR_BOSS_IMPACT_STAGE_FIRST &&             \
      (unsigned int)(stage) <= ANCHOR_BOSS_IMPACT_STAGE_LAST) ||             \
     ((unsigned int)(stage) >= ANCHOR_BOSS_IMPACT_INTRO_FIRST &&             \
      (unsigned int)(stage) <= ANCHOR_BOSS_IMPACT_INTRO_LAST))

/* Unknown IDs return -1 / NULL; ordinary rooms return arena ID zero. */
int anchor_boss_arena_room(int arena);
const char *anchor_boss_arena_name(int arena);
int anchor_boss_arena_for_room(unsigned short room);

/* Map one specific Impact stage (intro, minigame or boss) to its arena ID, or
 * 0 when the stage is not part of the Impact sequence. */
int anchor_boss_arena_for_impact_stage(unsigned int stage);

#endif
