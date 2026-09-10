#include "anchor_boss_arenas.h"

typedef struct
{
    int id;
    unsigned short room;
    const char *name;
} BossArena;

/* Room identity and display labels are selected locally, never supplied by
 * a peer as arbitrary warp coordinates or native dialog text. */
static const BossArena s_arenas[] = {
    {ANCHOR_BOSS_ARENA_CONGO, ANCHOR_BOSS_ROOM_CONGO, "Congo's Arena"},
    {ANCHOR_BOSS_ARENA_DHARUMANYO, ANCHOR_BOSS_ROOM_DHARUMANYO, "Dharumanyo's Arena"},
    {ANCHOR_BOSS_ARENA_TSURAMI, ANCHOR_BOSS_ROOM_TSURAMI, "Tsurami's Arena"},
    {ANCHOR_BOSS_ARENA_CONTROL_MACHINE, ANCHOR_BOSS_ROOM_CONTROL_MACHINE,
     "Control Machine's Arena"},
    {ANCHOR_BOSS_ARENA_KASHIWAGI, ANCHOR_BOSS_ROOM_KASHIWAGI,
     "Kashiwagi's Arena"},
    {ANCHOR_BOSS_ARENA_THAISAMBA, ANCHOR_BOSS_ROOM_THAISAMBA,
     "Thaisamba's Arena"},
    {ANCHOR_BOSS_ARENA_BALBERRA, ANCHOR_BOSS_ROOM_BALBERRA,
     "Balberra's Arena"},
    {ANCHOR_BOSS_ARENA_DETOILE, ANCHOR_BOSS_ROOM_DETOILE,
     "D'Etoile's Arena"}
};

static const BossArena *find_arena(int arena)
{
    for (unsigned int i = 0; i < sizeof(s_arenas) / sizeof(s_arenas[0]); ++i)
        if (s_arenas[i].id == arena)
            return &s_arenas[i];
    return 0;
}

int anchor_boss_arena_room(int arena)
{
    const BossArena *entry = find_arena(arena);
    return entry ? entry->room : -1;
}

const char *anchor_boss_arena_name(int arena)
{
    const BossArena *entry = find_arena(arena);
    return entry ? entry->name : 0;
}

int anchor_boss_arena_for_room(unsigned short room)
{
    for (unsigned int i = 0; i < sizeof(s_arenas) / sizeof(s_arenas[0]); ++i)
        if (s_arenas[i].room == room)
            return s_arenas[i].id;
    return 0;
}

int anchor_boss_arena_for_impact_stage(unsigned int stage)
{
    /* Intro cutscenes 0x0239..0x023D pair one-to-one with the five bosses. */
    if (stage >= ANCHOR_BOSS_IMPACT_INTRO_FIRST &&
        stage <= ANCHOR_BOSS_IMPACT_INTRO_LAST)
        return ANCHOR_BOSS_ARENA_KASHIWAGI +
               (int)(stage - ANCHOR_BOSS_IMPACT_INTRO_FIRST);
    /* Boss stages 0x0220..0x0223. */
    if (stage >= ANCHOR_BOSS_ROOM_KASHIWAGI &&
        stage <= ANCHOR_BOSS_ROOM_DETOILE)
        return ANCHOR_BOSS_ARENA_KASHIWAGI +
               (int)(stage - ANCHOR_BOSS_ROOM_KASHIWAGI);
    /* High-speed minigames 0x021C..0x021F precede bosses one..four. */
    if (stage >= ANCHOR_BOSS_IMPACT_STAGE_FIRST && stage <= 0x021Fu)
        return ANCHOR_BOSS_ARENA_KASHIWAGI +
               (int)(stage - ANCHOR_BOSS_IMPACT_STAGE_FIRST);
    return 0;
}
