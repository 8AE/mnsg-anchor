#include "anchor_boss_invite_world.h"
#include "anchor_dialog.h"
#include "item_sync.h"

#ifndef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
#include "modding.h"
#include "recomputils.h"
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

/* Dedicated native Impact-stage entry shared by the story and the
 * consecutive-boss mode. The first argument indexes the giant-robot stages
 * 0x0220..0x0224 (Kashiwagi is index 0). It seeds the packed "load from start"
 * fields, sets the Impact transition marker (system+0x3ae29 = 0x80) and the
 * destination stage. The caller owns the engine step change, matching the
 * ordinary warp below. */
extern void func_800061B0_6DB0(unsigned char index, unsigned short field90,
                               unsigned int field91);
/* Generic Impact-stage entry covering stages 0x021C..0x0224; index 0 is the
 * first stage. Used to load the exact stage named by an accepted invitation. */
extern void func_80006140_6D40(unsigned char index, unsigned short field90,
                               unsigned int field91);

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
#define SYS_DEST_STAGE 0x3AFE0u
#define SYS_IMPACT_FIELD90 0x3AE3Au
#define SYS_IMPACT_FIELD91 0x3AE3Cu
#define STEP_WORLD 13u
#define STEP_WARP 12u
#define LAST_TRANSFER_ROOM 0x225u

static unsigned int s_visit;
static unsigned short s_loaded_room;
static int s_room_loaded;
static unsigned short s_impact_stage;
static unsigned int s_impact_visit;
static int s_impact_active;
static unsigned int s_impact_field90;
static unsigned int s_impact_field91;

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
    /* An ordinary room load ends any active Impact sequence. An Impact stage
     * that does pass through this loader keeps the sequence (and its first
     * stage) for the whole cutscene/minigame/boss run. */
    if (!ANCHOR_BOSS_IMPACT_STAGE_VALID(D_800C7AB2))
    {
        s_impact_active = 0;
        s_impact_stage = 0;
    }
#ifndef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
    recomp_printf("[BossInvite] load room=0x%X impact=0x%X\n",
                  (unsigned int)D_800C7AB2, (unsigned int)s_impact_stage);
#endif
}

/* Record the first stage of an Impact sequence. Later stages in the same
 * sequence keep the first stage so an accepted invitation always replays the
 * Impact cutscene from its beginning. */
static void begin_impact_sequence(unsigned short stage)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    if (s_impact_active)
        return;
    s_impact_active = 1;
    s_impact_stage = stage;
    s_impact_field90 = system ? *(unsigned short *)(system + SYS_IMPACT_FIELD90) : 0;
    s_impact_field91 = system ? *(unsigned int *)(system + SYS_IMPACT_FIELD91) : 0;
    s_impact_visit = s_impact_visit == 0x7fffffffu ? 1u : s_impact_visit + 1u;
#ifndef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
    recomp_printf("[BossInvite] impact begin stage=0x%X f90=0x%X f91=0x%X\n",
                  (unsigned int)stage, s_impact_field90, s_impact_field91);
#endif
}

/* The dedicated Impact entries write the target stage to SYS_DEST_STAGE.
 * Observe them so an entry announces the arena at the cutscene start even if
 * the ordinary stage loader never runs for that stage. */
void anchor_boss_invite_world_note_impact(void)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    unsigned short stage;
    if (!system)
        return;
    stage = *(unsigned short *)(system + SYS_DEST_STAGE);
#ifndef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
    recomp_printf("[BossInvite] impact entry dest=0x%X\n", (unsigned int)stage);
#endif
    if (ANCHOR_BOSS_IMPACT_STAGE_VALID(stage))
        begin_impact_sequence(stage);
}

#ifndef ANCHOR_BOSS_INVITE_WORLD_HOST_TEST
RECOMP_HOOK_RETURN("func_80006140_6D40")
void anchor_boss_invite_world_impact_minigame_enter(void)
{
    anchor_boss_invite_world_note_impact();
}

RECOMP_HOOK_RETURN("func_800061B0_6DB0")
void anchor_boss_invite_world_impact_boss_enter(void)
{
    anchor_boss_invite_world_note_impact();
}

RECOMP_HOOK_RETURN("func_80006220_6E20")
void anchor_boss_invite_world_impact_boss_start_enter(void)
{
    anchor_boss_invite_world_note_impact();
}
#endif

static int room_loaded_matches(void)
{
    return s_room_loaded && s_loaded_room == D_800C7AB2;
}

static int room_is_current(void)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    return system && room_loaded_matches() &&
           system[SYS_STEP] == STEP_WORLD &&
           system[SYS_WORLD_SUBSTATE] == 1u;
}

/* Some Impact stages may not pass through the ordinary stage loader, so a
 * loaded Impact room also starts the sequence and records its first stage. */
static void refresh_impact_from_room(void)
{
    if (ANCHOR_BOSS_IMPACT_STAGE_VALID(D_800C7AB2))
        begin_impact_sequence(D_800C7AB2);
}

int anchor_boss_invite_world_arena(void)
{
    int arena;
    refresh_impact_from_room();
    /* Every Impact stage (intro, minigame and boss) belongs to one of the five
     * giant-robot encounters; map the recorded first stage to its boss. */
    if (s_impact_active)
        return anchor_boss_arena_for_impact_stage(s_impact_stage);
    if (room_loaded_matches())
    {
        arena = anchor_boss_arena_for_room(D_800C7AB2);
        return room_is_current() && player_is_alive() ? arena : 0;
    }
    return 0;
}

/* The first stage of the active Impact sequence, or 0. The invite carries it so
 * the guest replays the cutscene from its beginning. */
unsigned int anchor_boss_invite_world_stage(void)
{
    return s_impact_active ? s_impact_stage : 0;
}

unsigned int anchor_boss_invite_world_visit(void)
{
    return s_impact_active ? s_impact_visit : s_visit;
}

/* The native "load from start" fields the host's Impact entry used. Reproducing
 * them lets a guest resume at the same cutscene checkpoint rather than the
 * stage's default (post-cutscene) start. */
unsigned int anchor_boss_invite_world_field90(void)
{
    return s_impact_active ? s_impact_field90 : 0;
}

unsigned int anchor_boss_invite_world_field91(void)
{
    return s_impact_active ? s_impact_field91 : 0;
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

static const short *room_default_start(unsigned int room)
{
    if (room > LAST_TRANSFER_ROOM)
        return 0;
    return &D_8006B780_6C380[room * 5u];
}

static int request_live_world_transfer(unsigned short room, const short *start,
                                       short x, short y, short z)
{
    /* The mod-owned invitation dialog releases native pause/control bits
     * before its scenario/window lifetime has fully ended. Keep room loads
     * out until that higher-level owner is also gone. */
    if (!start || anchor_dialog_busy() ||
        !anchor_boss_invite_world_can_prompt())
        return 0;
    /* The peer supplies only the exact destination position. Camera and player
     * rotation remain native room-start data rather than network-controlled
     * values. room_default_start() has already bounded the table index. */
    func_8000607C_6C7C(room, x, y, z, start[3], start[4], 0, 0);
    /* The warp state calls 8000B364, which consumes this destination directly
     * and clears control state before loading the new world. It does not read
     * saved spawn fields. Preserve the player's saved respawn location and
     * let that native consumer change the current room at the correct time. */
    s_room_loaded = 0;
    func_80003728_4328(STEP_WARP);
    return 1;
}

int anchor_boss_invite_world_transfer_to(unsigned short room,
                                         short x, short y, short z)
{
    const short *start = room_default_start(room);
    /* Deliberately do not reject D_800C7AB2 == room. Step 12 owns the complete
     * same-room teardown/reload and is safer than moving player records by hand. */
    return request_live_world_transfer(room, start, x, y, z);
}

static int request_impact_transfer(unsigned int stage, unsigned int field90,
                                   unsigned int field91)
{
    /* Reproduce the sender's native Impact entry exactly, including its
     * "load from start" fields, so the recipient resumes at the sender's
     * cutscene checkpoint rather than the stage's post-cutscene default. */
    if (!ANCHOR_BOSS_IMPACT_STAGE_VALID(stage) || anchor_dialog_busy() ||
        !anchor_boss_invite_world_can_prompt())
        return 0;
    func_80006140_6D40((unsigned char)(stage - ANCHOR_BOSS_IMPACT_STAGE_FIRST),
                       (unsigned short)field90, field91);
    s_room_loaded = 0;
    func_80003728_4328(STEP_WARP);
    return 1;
}

int anchor_boss_invite_world_warp_stage(unsigned int stage, unsigned int field90,
                                        unsigned int field91)
{
    if (D_800C7AB2 == (unsigned short)stage)
        return 0;
    return request_impact_transfer(stage, field90, field91);
}

int anchor_boss_invite_world_warp(int arena)
{
    const short *start;
    int room = anchor_boss_arena_room(arena);
    if (room < 0)
        return 0;
    if (arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST &&
        arena <= ANCHOR_BOSS_ARENA_IMPACT_LAST)
        return anchor_boss_invite_world_warp_stage((unsigned int)room, 0, 0);
    if (D_800C7AB2 == (unsigned short)room)
        return 0;
    /* Invitations retain their existing default-entrance semantics. */
    start = room_default_start((unsigned int)room);
    if (!start)
        return 0;
    return request_live_world_transfer((unsigned short)room, start,
                                       start[0], start[1], start[2]);
}
