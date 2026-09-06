#include "anchor_boss_invite_world.h"
#include "item_sync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef union AlignedBytes
{
    unsigned long long alignment;
    unsigned char bytes[0xd0000];
} AlignedBytes;
static AlignedBytes s_system;
static unsigned char s_task[0xf0], s_object[0x98], s_work[0x80];
static void *s_backlink;
unsigned char *D_8015C5C8_15D1C8 = s_system.bytes;
unsigned short D_800C7AB2;
short D_8006B780_6C380[0x200 * 5];
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
static int s_saved, s_health, s_destination_calls, s_step_calls;
static short s_destination[7];
static int s_last_field91;

/* Independently read from the USA native room/default-start tables. */
static const struct {
    int arena;
    unsigned short room;
    const char *name;
    short start[5];
} arenas[] = {
    {1, 0x0016, "Congo's Arena", {60, -70, 171, 512, 16}},
    {2, 0x0049, "Dharumanyo's Arena", {145, -70, -99, 768, 16}},
    {3, 0x0071, "Tsurami's Arena", {0, -71, 318, 512, 16}},
    {4, 0x0155, "Control Machine's Arena", {27, 239, 131, 0, 16}}
};

void anchor_boss_invite_world_begin_load(void);
void anchor_boss_invite_world_finish_load(void);

int item_sync_save_is_loaded(void) { return s_saved; }
unsigned int item_sync_local_player_health(void) { return (unsigned int)s_health; }

void func_8000607C_6C7C(unsigned short room, short x, short y, short z,
                        short camera, short player, short field90, int field91)
{
    /* Destination preparation must happen before changing the main step. */
    assert(s_system.bytes[0x3add4] == 13);
    assert(s_step_calls == 0);
    s_destination[0] = (short)room;
    s_destination[1] = x;
    s_destination[2] = y;
    s_destination[3] = z;
    s_destination[4] = camera;
    s_destination[5] = player;
    s_destination[6] = field90;
    s_last_field91 = field91;
    ++s_destination_calls;
}

void func_80003728_4328(unsigned char step)
{
    assert(s_destination_calls == 1);
    assert(step == 12);
    s_system.bytes[0x3add4] = step;
    ++s_step_calls;
}

static void pointer_at(void *base, unsigned int offset, void *value)
{
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

static void ready(unsigned short room)
{
    memset(&s_system, 0, sizeof(s_system));
    memset(s_task, 0, sizeof(s_task));
    memset(s_work, 0, sizeof(s_work));
    D_8015C5C8_15D1C8 = s_system.bytes;
    D_800C7AB2 = room;
    s_system.bytes[0x3add4] = 13;
    s_system.bytes[0x3adde] = 1;
    D_801FC604_5B8514 = s_task;
    D_801FC60C_5B851C = s_object;
    s_backlink = s_task;
    pointer_at(s_task, 4, &s_backlink);
    pointer_at(s_task, 0x18, s_object);
    pointer_at(s_task, 0x5c, s_work);
    for (unsigned int i = 0; i < sizeof(arenas) / sizeof(arenas[0]); ++i)
        memcpy(&D_8006B780_6C380[arenas[i].room * 5], arenas[i].start,
               sizeof(arenas[i].start));
    s_saved = 1;
    s_health = 5;
    s_destination_calls = s_step_calls = 0;
    anchor_boss_invite_world_begin_load();
    assert(!anchor_boss_invite_world_can_prompt());
    anchor_boss_invite_world_finish_load();
}

static void check_gate(unsigned int offset, unsigned char value)
{
    ready(0x130);
    s_system.bytes[offset] = value;
    assert(!anchor_boss_invite_world_can_prompt());
    for (int arena = 1; arena <= 4; ++arena)
        assert(!anchor_boss_invite_world_warp(arena));
    assert(s_destination_calls == 0 && s_step_calls == 0);
}

int main(void)
{
    unsigned int visit;
    int step;

    ready(0x16);
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_CONGO);
    visit = anchor_boss_invite_world_visit();
    assert(visit != 0);
    /* Announce the entry during Congo's introduction, while deferring a
     * received invitation until scripted movement/input has completed. */
    s_system.bytes[0x3ae20] = 3;
    s_system.bytes[0x3ae23] = 1;
    s_system.bytes[0xcf8a2] = 1;
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_CONGO);
    assert(!anchor_boss_invite_world_can_prompt());
    assert(anchor_boss_invite_world_visit() == visit);
    ready(0x16);
    assert(anchor_boss_invite_world_visit() != visit);
    assert(!anchor_boss_invite_world_warp(ANCHOR_BOSS_ARENA_CONGO));
    assert(s_destination_calls == 0);
    ready(0x1a);
    assert(anchor_boss_invite_world_arena() == 0); /* Congo approach. */

    for (step = 0; step < 18; ++step)
        if (step != 13)
            check_gate(0x3add4, (unsigned char)step);
    check_gate(0x3adde, 0);
    check_gate(0x3adde, 2);
    check_gate(0x3ae16, 1);
    check_gate(0x3ae17, 1);
    check_gate(0x3ae20, 1);
    check_gate(0x3ae20, 2);
    check_gate(0x3ae22, 1);
    check_gate(0x3ae23, 1);
    check_gate(0x3ae26, 1);
    check_gate(0x3ae27, 1);
    check_gate(0x3ae29, 1);
    check_gate(0xcf8a2, 1);

    ready(0x130); s_saved = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); s_health = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); s_work[0x69] = 1;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); s_backlink = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); D_801FC60C_5B851C = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); D_801FC604_5B8514 = (void *)0x80000000ul;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); D_800C7AB2 = 0x131;
    assert(!anchor_boss_invite_world_can_prompt());
    ready(0x130); D_8015C5C8_15D1C8 = 0;
    assert(!anchor_boss_invite_world_can_prompt());

    for (unsigned int i = 0; i < sizeof(arenas) / sizeof(arenas[0]); ++i) {
        int arena = arenas[i].arena;
        assert(anchor_boss_arena_room(arena) == arenas[i].room);
        assert(strcmp(anchor_boss_arena_name(arena), arenas[i].name) == 0);
        ready(arenas[i].room);
        assert(anchor_boss_invite_world_arena() == arena);
        assert(!anchor_boss_invite_world_warp(arena));
        assert(!s_destination_calls);
        visit = anchor_boss_invite_world_visit();
        s_system.bytes[0x3ae20] = 3;
        assert(anchor_boss_invite_world_arena() == arena);
        assert(!anchor_boss_invite_world_can_prompt());
        ready(arenas[i].room);
        assert(anchor_boss_invite_world_visit() != visit);

        /* Accepting from another boss room uses the chosen destination. */
        unsigned short source = arenas[(i + 1) % 4].room;
        ready(source);
        assert(anchor_boss_invite_world_can_prompt());
        assert(anchor_boss_invite_world_warp(arena));
        assert(s_destination_calls == 1 && s_step_calls == 1);
        assert(D_800C7AB2 == source); /* Native loader owns current-room writes. */
        assert(s_destination[0] == arenas[i].room);
        assert(memcmp(&s_destination[1], arenas[i].start, sizeof(arenas[i].start)) == 0);
        assert(s_destination[6] == 0 && s_last_field91 == 0);
        assert(!anchor_boss_invite_world_can_prompt());
        assert(!anchor_boss_invite_world_warp(arena));
        assert(s_destination_calls == 1 && s_step_calls == 1);
    }

    ready(0x009d); /* Gourmet Submarine is not the dragon/Control Machine room. */
    assert(anchor_boss_invite_world_arena() == 0);
    const int invalid_arenas[] = {-1, 0, 5, 0x155, 0x7fffffff};
    for (unsigned int i = 0; i < sizeof(invalid_arenas) / sizeof(invalid_arenas[0]); ++i) {
        assert(anchor_boss_arena_room(invalid_arenas[i]) == -1);
        assert(!anchor_boss_arena_name(invalid_arenas[i]));
        assert(!anchor_boss_invite_world_warp(invalid_arenas[i]));
    }
    assert(!s_destination_calls && !s_step_calls);

    puts("boss invitation world: entry, intro, lifecycle gates and native warp passed");
    return 0;
}
