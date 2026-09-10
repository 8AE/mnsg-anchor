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
short D_8006B780_6C380[0x226 * 5];
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
static int s_saved, s_health, s_dialog_busy;
static int s_destination_calls, s_step_calls;
static short s_destination[7];
static int s_last_field91;
static int s_impact_calls, s_impact_index;
static int s_impact_field90, s_impact_field91;

/* Independently read from the USA native room/default-start tables. The
 * Impact stage supplies its own start, so arena 5 carries a zero placeholder. */
static const struct {
    int arena;
    unsigned short room;
    const char *name;
    short start[5];
} arenas[] = {
    {1, 0x0016, "Congo's Arena", {60, -70, 171, 512, 16}},
    {2, 0x0049, "Dharumanyo's Arena", {145, -70, -99, 768, 16}},
    {3, 0x0071, "Tsurami's Arena", {0, -71, 318, 512, 16}},
    {4, 0x0155, "Control Machine's Arena", {27, 239, 131, 0, 16}},
    {5, 0x0220, "Kashiwagi's Arena", {0, 0, 0, 0, 0}},
    {6, 0x0221, "Thaisamba's Arena", {0, 0, 0, 0, 0}},
    {7, 0x0222, "Balberra's Arena", {0, 0, 0, 0, 0}},
    {8, 0x0223, "D'Etoile's Arena", {0, 0, 0, 0, 0}}
};

void anchor_boss_invite_world_begin_load(void);
void anchor_boss_invite_world_finish_load(void);

int item_sync_save_is_loaded(void) { return s_saved; }
unsigned int item_sync_local_player_health(void) { return (unsigned int)s_health; }
int anchor_dialog_busy(void) { return s_dialog_busy; }

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

void func_800061B0_6DB0(unsigned char index, unsigned short field90,
                        unsigned int field91)
{
    (void)index;
    (void)field90;
    (void)field91;
}

void func_80006140_6D40(unsigned char index, unsigned short field90,
                        unsigned int field91)
{
    /* Destination preparation must happen before changing the main step. */
    assert(s_system.bytes[0x3add4] == 13);
    assert(s_step_calls == 0 && s_destination_calls == 0);
    s_impact_index = (int)index;
    s_impact_field90 = (int)field90;
    s_impact_field91 = (int)field91;
    ++s_impact_calls;
}

void func_80003728_4328(unsigned char step)
{
    assert((s_destination_calls == 1 && s_impact_calls == 0) ||
           (s_destination_calls == 0 && s_impact_calls == 1));
    assert(step == 12);
    s_system.bytes[0x3add4] = step;
    ++s_step_calls;
}

static void pointer_at(void *base, unsigned int offset, void *value)
{
    memcpy((unsigned char *)base + offset, &value, sizeof(value));
}

/* Host fixtures are little-endian; the native stage is a big-endian halfword. */
static void set_dest_stage(unsigned short stage)
{
    s_system.bytes[0x3afe0] = (unsigned char)(stage & 0xff);
    s_system.bytes[0x3afe1] = (unsigned char)(stage >> 8);
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
    s_dialog_busy = 0;
    s_destination_calls = s_step_calls = 0;
    s_impact_calls = 0;
    s_impact_index = -1;
    s_impact_field90 = -1;
    s_impact_field91 = -1;
    anchor_boss_invite_world_begin_load();
    assert(!anchor_boss_invite_world_can_prompt());
    anchor_boss_invite_world_finish_load();
}

static void check_gate(unsigned int offset, unsigned char value)
{
    ready(0x130);
    s_system.bytes[offset] = value;
    assert(!anchor_boss_invite_world_can_prompt());
    for (int arena = 1; arena <= 8; ++arena)
        assert(!anchor_boss_invite_world_warp(arena));
    assert(!anchor_boss_invite_world_transfer_to(0x130, 123, -456, 789));
    assert(s_destination_calls == 0 && s_step_calls == 0 && s_impact_calls == 0);
}

static void check_transfer_blocked(void)
{
    assert(!anchor_boss_invite_world_transfer_to(0x130, 123, -456, 789));
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
    check_transfer_blocked();
    ready(0x130); s_health = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); s_work[0x69] = 1;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); s_backlink = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); D_801FC60C_5B851C = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); D_801FC604_5B8514 = (void *)0x80000000ul;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); s_dialog_busy = 1;
    assert(anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); D_800C7AB2 = 0x131;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();
    ready(0x130); D_8015C5C8_15D1C8 = 0;
    assert(!anchor_boss_invite_world_can_prompt());
    check_transfer_blocked();

    /* Peer transfer preserves exact signed coordinates and uses only the
     * destination room's native rotations. The native loader still owns the
     * current-room write, including a same-room reload. */
    ready(0x0049);
    assert(anchor_boss_invite_world_transfer_to(0x0049,
                                                -32768, 32767, -12345));
    assert(s_destination_calls == 1 && s_step_calls == 1);
    assert(D_800C7AB2 == 0x0049);
    assert(s_destination[0] == 0x0049);
    assert(s_destination[1] == -32768);
    assert(s_destination[2] == 32767);
    assert(s_destination[3] == -12345);
    assert(s_destination[4] == arenas[1].start[3]);
    assert(s_destination[5] == arenas[1].start[4]);
    assert(s_destination[6] == 0 && s_last_field91 == 0);

    ready(0x0130);
    assert(anchor_boss_invite_world_transfer_to(0x0016, 321, -222, 17));
    assert(s_destination_calls == 1 && s_step_calls == 1);
    assert(D_800C7AB2 == 0x0130);
    assert(s_destination[0] == 0x0016);
    assert(s_destination[1] == 321);
    assert(s_destination[2] == -222);
    assert(s_destination[3] == 17);
    assert(s_destination[4] == arenas[0].start[3]);
    assert(s_destination[5] == arenas[0].start[4]);
    assert(s_destination[6] == 0 && s_last_field91 == 0);

    /* 0x225 is the last index admitted by the live transfer API. */
    ready(0x0130);
    D_8006B780_6C380[0x225 * 5 + 3] = -512;
    D_8006B780_6C380[0x225 * 5 + 4] = 1023;
    assert(anchor_boss_invite_world_transfer_to(0x0225, 1, 2, 3));
    assert(s_destination[0] == 0x0225);
    assert(s_destination[1] == 1 && s_destination[2] == 2 &&
           s_destination[3] == 3);
    assert(s_destination[4] == -512 && s_destination[5] == 1023);

    /* Reject the World Map overlay and every larger unsigned room before any
     * room-indexed native-table read or destination mutation. */
    ready(0x0130);
    assert(!anchor_boss_invite_world_transfer_to(0x0226, 1, 2, 3));
    assert(!anchor_boss_invite_world_transfer_to(0xffff, 1, 2, 3));
    assert(s_destination_calls == 0 && s_step_calls == 0);

    for (unsigned int i = 0; i < sizeof(arenas) / sizeof(arenas[0]); ++i) {
        int arena = arenas[i].arena;
        assert(anchor_boss_arena_room(arena) == arenas[i].room);
        assert(strcmp(anchor_boss_arena_name(arena), arenas[i].name) == 0);
        ready(0x0130); /* Clear any prior Impact sequence. */
        ready(arenas[i].room);
        assert(anchor_boss_invite_world_arena() == arena);
        assert(!anchor_boss_invite_world_warp(arena));
        assert(!s_destination_calls && !s_impact_calls);
        visit = anchor_boss_invite_world_visit();
        s_system.bytes[0x3ae20] = 3;
        assert(anchor_boss_invite_world_arena() == arena);
        assert(!anchor_boss_invite_world_can_prompt());
        ready(arenas[i].room);
        if (arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST &&
            arena <= ANCHOR_BOSS_ARENA_IMPACT_LAST)
            /* A same-stage Impact reload continues the sequence. */
            assert(anchor_boss_invite_world_visit() == visit);
        else
            assert(anchor_boss_invite_world_visit() != visit);

        /* Accepting from another boss room uses the chosen destination. */
        unsigned short source = arenas[(i + 1) % (sizeof(arenas) / sizeof(arenas[0]))].room;
        ready(source);
        assert(anchor_boss_invite_world_can_prompt());
        assert(anchor_boss_invite_world_warp(arena));
        if (arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST &&
            arena <= ANCHOR_BOSS_ARENA_IMPACT_LAST) {
            /* The giant-robot stage uses the dedicated Impact entry and lets
             * native transition data supply the spawn position. */
            assert(s_impact_calls == 1 &&
                   s_impact_index == (int)(arenas[i].room -
                                           ANCHOR_BOSS_IMPACT_STAGE_FIRST));
            assert(s_destination_calls == 0 && s_step_calls == 1);
        } else {
            assert(s_destination_calls == 1 && s_step_calls == 1);
            assert(s_destination[0] == arenas[i].room);
            assert(memcmp(&s_destination[1], arenas[i].start, sizeof(arenas[i].start)) == 0);
            assert(s_destination[6] == 0 && s_last_field91 == 0);
        }
        assert(D_800C7AB2 == source); /* Native loader owns current-room writes. */
        assert(!anchor_boss_invite_world_can_prompt());
        assert(!anchor_boss_invite_world_warp(arena));
        assert(s_step_calls == 1 && s_impact_calls <= 1 && s_destination_calls <= 1);
    }

    /* The Impact arena is announced from the loaded stage even when the
     * on-foot player records are absent or the player is not "alive". */
    ready(0x0220);
    s_health = 0;
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_KASHIWAGI);
    ready(0x0220);
    D_801FC604_5B8514 = 0;
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_KASHIWAGI);
    /* Some Impact stages do not pass through the ordinary stage loader, so a
     * stale loaded-room token must not suppress the announcement. */
    ready(0x0130);
    D_800C7AB2 = ANCHOR_BOSS_ROOM_KASHIWAGI;
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_KASHIWAGI);

    /* The dedicated Impact entry is observed directly, so the cutscene start
     * announces the arena before the fight and without an ordinary room load. */
    ready(0x0130);
    set_dest_stage(ANCHOR_BOSS_ROOM_KASHIWAGI);
    anchor_boss_invite_world_note_impact();
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_KASHIWAGI);
    assert(anchor_boss_invite_world_stage() == ANCHOR_BOSS_ROOM_KASHIWAGI);
    unsigned int impact_visit = anchor_boss_invite_world_visit();
    assert(impact_visit != 0);
    anchor_boss_invite_world_note_impact();
    assert(anchor_boss_invite_world_visit() == impact_visit);

    /* A later Impact stage keeps the sequence's first stage so an accepted
     * invitation always replays the cutscene from its beginning. */
    set_dest_stage(0x0222);
    anchor_boss_invite_world_note_impact();
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_KASHIWAGI);
    assert(anchor_boss_invite_world_stage() == ANCHOR_BOSS_ROOM_KASHIWAGI);
    anchor_boss_invite_world_note_impact();
    assert(anchor_boss_invite_world_stage() == ANCHOR_BOSS_ROOM_KASHIWAGI);
    assert(anchor_boss_invite_world_visit() == impact_visit);

    /* A fresh sequence records its own first stage and visit. */
    ready(0x0130);
    set_dest_stage(0x0222);
    anchor_boss_invite_world_note_impact();
    assert(anchor_boss_invite_world_stage() == 0x0222);
    assert(anchor_boss_invite_world_visit() != impact_visit);

    /* The Impact intro cutscene stage (Goemon enters Impact) is part of the
     * same sequence and is announced from its own loaded room. */
    ready(0x0130);
    ready(ANCHOR_BOSS_IMPACT_INTRO_STAGE);
    assert(anchor_boss_invite_world_arena() == ANCHOR_BOSS_ARENA_KASHIWAGI);
    assert(anchor_boss_invite_world_stage() == ANCHOR_BOSS_IMPACT_INTRO_STAGE);

    /* The native load-from-start fields are captured at the sequence start. */
    ready(0x0130);
    s_system.bytes[0x3ae3a] = 0x34;
    s_system.bytes[0x3ae3b] = 0x12; /* field90 = 0x1234 */
    s_system.bytes[0x3ae3c] = 0x78;
    s_system.bytes[0x3ae3d] = 0x56;
    s_system.bytes[0x3ae3e] = 0x34;
    s_system.bytes[0x3ae3f] = 0x12; /* field91 = 0x12345678 */
    set_dest_stage(0x0220);
    anchor_boss_invite_world_note_impact();
    assert(anchor_boss_invite_world_field90() == 0x1234);
    assert(anchor_boss_invite_world_field91() == 0x12345678);

    /* The guest is sent to the exact stage and load-from-start fields. */
    ready(0x0130);
    assert(anchor_boss_invite_world_warp_stage(0x0222, 0x1234, 0x12345678));
    assert(s_impact_calls == 1 &&
           s_impact_index == (int)(0x0222 - ANCHOR_BOSS_IMPACT_STAGE_FIRST));
    assert(s_impact_field90 == 0x1234 && s_impact_field91 == 0x12345678);
    assert(s_destination_calls == 0 && s_step_calls == 1);
    ready(0x0130);
    assert(!anchor_boss_invite_world_warp_stage(0x0100, 0, 0));
    assert(!anchor_boss_invite_world_warp_stage(0x0225, 0, 0));
    assert(!s_impact_calls && !s_destination_calls && !s_step_calls);

    /* The guest is sent to the intro cutscene stage when the sequence began
     * there, before the sender reached the minigame. */
    ready(0x0130);
    assert(anchor_boss_invite_world_warp_stage(ANCHOR_BOSS_IMPACT_INTRO_STAGE,
                                               0, 0));
    assert(s_impact_calls == 1 &&
           s_impact_index == (int)(ANCHOR_BOSS_IMPACT_INTRO_STAGE -
                                   ANCHOR_BOSS_IMPACT_STAGE_FIRST));
    assert(s_destination_calls == 0 && s_step_calls == 1);

    ready(0x009d); /* Gourmet Submarine is not the dragon/Control Machine room. */
    assert(anchor_boss_invite_world_arena() == 0);
    const int invalid_arenas[] = {-1, 0, 9, 0x155, 0x7fffffff};
    for (unsigned int i = 0; i < sizeof(invalid_arenas) / sizeof(invalid_arenas[0]); ++i) {
        assert(anchor_boss_arena_room(invalid_arenas[i]) == -1);
        assert(!anchor_boss_arena_name(invalid_arenas[i]));
        assert(!anchor_boss_invite_world_warp(invalid_arenas[i]));
    }
    assert(!s_destination_calls && !s_step_calls);

    puts("boss invitation world: entry, lifecycle gates, peer transfer and native warp passed");
    return 0;
}
