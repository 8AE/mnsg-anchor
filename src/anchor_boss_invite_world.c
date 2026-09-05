#include "anchor_boss_invite_world.h"
#include "item_sync.h"

#ifndef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
#include "modding.h"
#else
#include <string.h>
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned short D_800C7AB2;
extern short D_8006B780_6C380[];
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern void func_8000607C_6C7C(unsigned short room, short x, short y, short z,
                               short camera_rot, short player_rot,
                               short field90, int field91);
extern void func_80003728_4328(unsigned char step);

/* Full-system offsets. 8001F940 is the ordinary world input/pause dispatcher;
 * it checks the native transition, pause and sequence gates below. The main
 * dispatcher selects it through step 13 -> 80002EFC -> 8001F8B0, substate 1.
 * Step 3 is a different overlay at the same address as gameplay functions. */
#define SYS_STEP 0x3ADD4u
#define SYS_WORLD_SUBSTATE 0x3ADDEu
#define SYS_SEQUENCE_GATE 0x3AE16u
#define SYS_PLAYER_CONTROL 0x3AE20u
#define SYS_PLAYER_LOCK 0x3AE22u
#define SYS_SCRIPTED_INPUT 0x3AE23u
#define SYS_PAUSED 0x3AE26u
#define SYS_TRANSITION_PENDING 0x3AE29u
#define SYS_ROOM_START_BUSY 0xCF8A2u
#define STEP_WORLD 13u
#define STEP_WARP 12u

static unsigned int s_visit;
static unsigned short s_loaded_room;
static int s_room_loaded;

static int native_pointer_valid(const void *pointer)
{
#ifdef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
    return pointer != 0 && (unsigned long)pointer != 0x80000000ul;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    /* These are the real player's original native records, never mod-owned
     * remote tasks. Exclude the teardown sentinel and check before reading. */
    return address >= 0x80001000u && address < 0x80800000u;
#endif
}

static void *native_pointer_at(const void *base, unsigned int offset)
{
#ifdef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
    /* Native task offsets are aligned for four-byte MIPS pointers. Host
     * fixtures can use eight-byte pointers without introducing alignment UB. */
    void *result;
    memcpy(&result, (const unsigned char *)base + offset, sizeof(result));
    return result;
#else
    return *(void *const *)((const unsigned char *)base + offset);
#endif
}

static int player_is_alive(void)
{
    unsigned char *task = D_801FC604_5B8514;
    unsigned char *work;
    void *backlink;
    if (!item_sync_save_is_loaded() || item_sync_local_player_health() == 0 ||
        !native_pointer_valid(task) ||
        !native_pointer_valid(D_801FC60C_5B851C))
        return 0;
    backlink = native_pointer_at(task, 4);
    if (!native_pointer_valid(backlink) || native_pointer_at(backlink, 0) != task ||
        native_pointer_at(task, 0x18) != D_801FC60C_5B851C)
        return 0;
    work = native_pointer_at(task, 0x5c);
    return native_pointer_valid(work) && work[0x69] == 0;
}

/* Stage resources are rebuilt before player/actor setup. Clear the previous
 * room at entry so frame-end consumers cannot announce a stale player, and
 * count a visit at return even when a death/reload keeps the same room ID. */
RECOMP_HOOK("func_8020D6BC_5C8B8C")
void anchor_boss_invite_world_begin_load(void)
{
    s_room_loaded = 0;
}

RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")
void anchor_boss_invite_world_finish_load(void)
{
    s_loaded_room = D_800C7AB2;
    s_visit = s_visit == 0x7fffffffu ? 1u : s_visit + 1u;
    s_room_loaded = 1;
}

static int room_is_current(void)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    return system && s_room_loaded && s_loaded_room == D_800C7AB2 &&
           system[SYS_STEP] == STEP_WORLD &&
           system[SYS_WORLD_SUBSTATE] == 1u;
}

int anchor_boss_invite_world_arena(void)
{
    return room_is_current() && D_800C7AB2 == ANCHOR_BOSS_ROOM_CONGO &&
                   player_is_alive()
               ? ANCHOR_BOSS_ARENA_CONGO : 0;
}

unsigned int anchor_boss_invite_world_visit(void)
{
    return s_visit;
}

int anchor_boss_invite_world_can_prompt(void)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    return room_is_current() && player_is_alive() &&
           *(short *)(system + SYS_SEQUENCE_GATE) == 0 &&
           *(unsigned short *)(system + SYS_PAUSED) == 0 &&
           system[SYS_TRANSITION_PENDING] == 0 &&
           system[SYS_ROOM_START_BUSY] == 0 &&
           (system[SYS_PLAYER_CONTROL] & 3u) == 0 &&
           system[SYS_PLAYER_LOCK] == 0 &&
           system[SYS_SCRIPTED_INPUT] == 0;
}

int anchor_boss_invite_world_warp(void)
{
    const short *start;
    if (!anchor_boss_invite_world_can_prompt() ||
        D_800C7AB2 == ANCHOR_BOSS_ROOM_CONGO)
        return 0;
    /* The native table defines x, y, z, camera rotation, player rotation.
     * US entry 0x16 is (60, -70, 171, 512, 16); read the actual resident entry
     * instead of duplicating those values or accepting network coordinates. */
    start = &D_8006B780_6C380[ANCHOR_BOSS_ROOM_CONGO * 5u];
    func_8000607C_6C7C(ANCHOR_BOSS_ROOM_CONGO,
                      start[0], start[1], start[2], start[3], start[4], 0, 0);
    /* The warp state calls 8000B364, which consumes this destination directly
     * and clears control state before loading the new world. It does not read
     * saved spawn fields. Preserve the player's saved respawn location and
     * let that native consumer change the current room at the correct time. */
    s_room_loaded = 0;
    func_80003728_4328(STEP_WARP);
    return 1;
}
