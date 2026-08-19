/**
 * @file boss_sync.c
 * @brief Synchronise live boss defeat transitions separately from save flags.
 *
 * Boss completion flags are late progression/cutscene state.  Applying one to
 * a client whose local boss is still alive skips the native damage, reaction,
 * and teardown callbacks and can lock that client in the victory camera.
 *
 * Each supported fight therefore converts MNSG_BOSS_DEFEAT into the boss's
 * real final-hit input and holds its durable SET_FLAG until a native completion
 * callback is observed.  Congo and Tsurami share the common actor damage
 * wrapper, Dharmanyo calls the inner damage routine directly and consumes a
 * separate lives byte, and Benkei uses its own scripted lives byte and hit
 * bit.  Keeping those paths boss-specific avoids treating a reward flag or a
 * visual child actor as the actual kill.
 */

#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#include "boss_sync.h"

extern unsigned short D_800C7AB2;
extern void func_80024088_24C88(int flag_id);
extern int func_800240DC_24CDC(int flag_id);
extern int item_sync_save_is_loaded(void);
extern void item_sync_mark_boss_defeat_announced(const char *flag_name);
extern void item_sync_commit_boss_completion(const char *flag_name);

#define ENTITY_DARUMANYO 0x00CCu
#define ENTITY_BENKEI 0x01C0u
#define ENTITY_CONGO 0x0323u

#define ACTOR_ENTITY_ID(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0x5E))
#define ACTOR_STATUS(actor) \
    (*(volatile unsigned int *)((char *)(actor) + 0x68))
#define ACTOR_HEALTH(actor) \
    (*(volatile unsigned char *)((char *)(actor) + 0x8D))
#define DARUMANYO_LIVES(actor) \
    (*(volatile unsigned char *)((char *)(actor) + 0xD1))
#define DARUMANYO_HIT_TIMER(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0xD6))
#define BENKEI_STATE(actor) \
    (*(volatile unsigned char *)((char *)(actor) + 0xD0))
#define BENKEI_COMMAND(actor) \
    (*(volatile unsigned char *)((char *)(actor) + 0xD2))
#define BENKEI_HEALTH(actor) \
    (*(volatile signed char *)((char *)(actor) + 0xD3))
#define ACTOR_STATUS_ACTIVE 0x00000001u
/* Actor initializers set status bit 1 when a persistent completion flag says
 * the actor should be removed instead of entering its normal update. */
#define ACTOR_STATUS_REMOVE_PENDING 0x00000002u
/* Consumed and cleared by the common actor update after func_80218350.  Its
 * fast damage path subtracts one HP without requiring an attacker pointer. */
#define ACTOR_STATUS_DAMAGE_PENDING 0x00040000u
#define BENKEI_STATUS_HIT_PENDING 0x00000080u
#define DARUMANYO_KILL_FLAG 0x018u
#define TSURAMI_KILL_FLAG 0x040u
#define BENKEI_KILL_FLAG 0x033u
#define CONGO_KILL_FLAG 0x1A1u
#define CONGO_REWARD_FLAG 0x12Du
#define DARUMANYO_ROOM 0x049u
#define DARUMANYO_NATIVE_DEATH_FLAG 0x16Bu
#define DARUMANYO_NATIVE_COMPLETE_FLAG 0x16Du

typedef struct
{
    void *actor;
    unsigned short room;
    unsigned short entity_id;
} TrackedBoss;

static TrackedBoss s_congo;
static TrackedBoss s_tsurami;
static TrackedBoss s_darumanyo;
static TrackedBoss s_benkei;

typedef struct
{
    unsigned int lethal_hit_pending;
    unsigned int lethal_hit_armed;
    unsigned short lethal_hit_room;
    unsigned int remote_defeat_in_progress;
    unsigned short remote_defeat_room;
    unsigned int remote_defeat_needs_rearm;
    unsigned int victory_complete;
    unsigned int local_defeat_started;
} NativeBossState;

static NativeBossState s_tsurami_state;
static NativeBossState s_darumanyo_state;
static NativeBossState s_benkei_state;
static unsigned int s_tsurami_setup_actor_changed;
static unsigned int s_darumanyo_setup_actor_changed;
static void *s_darumanyo_damage_actor;
static void *s_darumanyo_terminal_controller;
static unsigned int s_benkei_setup_actor_changed;
static void *s_benkei_update_actor;
static unsigned int s_congo_lethal_hit_pending;
static unsigned int s_congo_lethal_hit_armed;
static unsigned short s_congo_lethal_hit_room;
/* Keep Congo's later 0x12D reward/cutscene flag blocked throughout the native
 * death transition.  Only the native 0x07D24 victory callback may release it. */
static unsigned int s_congo_remote_defeat_in_progress;
static unsigned short s_congo_remote_defeat_room;
static unsigned int s_congo_remote_defeat_needs_rearm;
static unsigned int s_congo_victory_complete;
static unsigned int s_congo_local_defeat_started;

static void reset_native_boss_state(NativeBossState *state)
{
    state->lethal_hit_pending = 0;
    state->lethal_hit_armed = 0;
    state->lethal_hit_room = 0;
    state->remote_defeat_in_progress = 0;
    state->remote_defeat_room = 0;
    state->remote_defeat_needs_rearm = 0;
    state->victory_complete = 0;
    state->local_defeat_started = 0;
}

void boss_sync_reset(void)
{
    /* A connection reset can occur between clearing a contaminated flag and
     * consuming the native final hit.  Preserve an accepted transition while
     * its save is still live; restoring progression here would put the flag
     * back on a living boss and recreate the original softlock.  Save unload
     * still force-clears every pointer/state below, preventing cross-save use. */
    if (item_sync_save_is_loaded() &&
        (s_congo_remote_defeat_in_progress ||
         s_tsurami_state.remote_defeat_in_progress ||
         s_darumanyo_state.remote_defeat_in_progress ||
         s_benkei_state.remote_defeat_in_progress))
    {
        /* Item sync is not polling encounter state while disconnected.  Make
         * Congo retry on its next authoritative health update so leaving and
         * re-entering during the outage cannot strand a same-address actor. */
        if (s_congo_remote_defeat_in_progress)
            s_congo_remote_defeat_needs_rearm = 1;
        recomp_printf("[BossSync] Preserving an in-flight native boss defeat across connection reset.\n");
        return;
    }

    s_congo.actor = 0;
    s_congo.room = 0;
    s_congo.entity_id = 0;
    s_tsurami.actor = 0;
    s_tsurami.room = 0;
    s_tsurami.entity_id = 0;
    s_darumanyo.actor = 0;
    s_darumanyo.room = 0;
    s_darumanyo.entity_id = 0;
    s_benkei.actor = 0;
    s_benkei.room = 0;
    s_benkei.entity_id = 0;
    reset_native_boss_state(&s_tsurami_state);
    reset_native_boss_state(&s_darumanyo_state);
    reset_native_boss_state(&s_benkei_state);
    s_tsurami_setup_actor_changed = 0;
    s_darumanyo_setup_actor_changed = 0;
    s_darumanyo_damage_actor = 0;
    s_darumanyo_terminal_controller = 0;
    s_benkei_setup_actor_changed = 0;
    s_benkei_update_actor = 0;
    s_congo_lethal_hit_pending = 0;
    s_congo_lethal_hit_armed = 0;
    s_congo_lethal_hit_room = 0;
    s_congo_remote_defeat_in_progress = 0;
    s_congo_remote_defeat_room = 0;
    s_congo_remote_defeat_needs_rearm = 0;
    s_congo_victory_complete = 0;
    s_congo_local_defeat_started = 0;
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        ++a;
        ++b;
    }
    return *a == *b;
}

static void track_boss(TrackedBoss *boss, void *actor, unsigned short entity_id)
{
    if (!actor)
        return;
    boss->actor = actor;
    boss->room = D_800C7AB2;
    boss->entity_id = entity_id;
}

static int tracked_boss_can_take_damage(const TrackedBoss *boss)
{
    if (!boss->actor || boss->room != D_800C7AB2 ||
        (boss->entity_id && ACTOR_ENTITY_ID(boss->actor) != boss->entity_id))
        return 0;
    return ACTOR_HEALTH(boss->actor) != 0 &&
           (ACTOR_STATUS(boss->actor) & ACTOR_STATUS_REMOVE_PENDING) == 0;
}

static int tracked_boss_is_active(const TrackedBoss *boss)
{
    return tracked_boss_can_take_damage(boss) &&
           (ACTOR_STATUS(boss->actor) & ACTOR_STATUS_ACTIVE) != 0;
}

static int tracked_boss_is_local(const TrackedBoss *boss)
{
    return boss->actor && boss->room == D_800C7AB2 &&
           (!boss->entity_id || ACTOR_ENTITY_ID(boss->actor) == boss->entity_id);
}

static int congo_remote_defeat_is_current(void)
{
    /* Keep the hold across room changes.  If the player leaves before Congo's
     * callback, A228 will capture the replacement actor on re-entry and re-arm
     * the native hit.  Releasing 0x12D in an unrelated room would recreate the
     * same bad persistent state on the next visit. */
    if (!s_congo_remote_defeat_in_progress)
        return 0;
    if (s_congo_remote_defeat_room != D_800C7AB2)
        s_congo_remote_defeat_needs_rearm = 1;
    return 1;
}

static int native_remote_defeat_is_current(NativeBossState *state)
{
    /* Once a live defeat has been accepted, keep its progression flag held
     * even if the player leaves the room.  The authoritative tracker will
     * bind the replacement actor and re-arm the native hit on re-entry. */
    if (!state->remote_defeat_in_progress)
        return 0;
    if (state->remote_defeat_room != D_800C7AB2)
        state->remote_defeat_needs_rearm = 1;
    return 1;
}

static int benkei_is_local(void)
{
    return tracked_boss_is_local(&s_benkei);
}

static int darumanyo_is_local(void)
{
    return tracked_boss_is_local(&s_darumanyo);
}

static int darumanyo_can_take_damage(void)
{
    return darumanyo_is_local() &&
           DARUMANYO_LIVES(s_darumanyo.actor) > 0 &&
           ACTOR_HEALTH(s_darumanyo.actor) > 0;
}

static int darumanyo_can_accept_hit(void)
{
    return darumanyo_can_take_damage() &&
           DARUMANYO_HIT_TIMER(s_darumanyo.actor) == 0;
}

static int benkei_can_take_damage(void)
{
    return benkei_is_local() && BENKEI_HEALTH(s_benkei.actor) > 0 &&
           (ACTOR_STATUS(s_benkei.actor) & ACTOR_STATUS_REMOVE_PENDING) == 0;
}

static TrackedBoss *boss_for_flag(const char *flag_name)
{
    if (streq(flag_name, "fl_congo_killed") || streq(flag_name, "fl_congo"))
        return &s_congo;
    if (streq(flag_name, "fl_tsurami"))
        return &s_tsurami;
    if (streq(flag_name, "fl_dharmanyo"))
        return &s_darumanyo;
    if (streq(flag_name, "fl_benkei"))
        return &s_benkei;
    return 0;
}

int boss_sync_is_completion_flag(const char *flag_name)
{
    return streq(flag_name, "fl_dharmanyo") ||
           streq(flag_name, "fl_thaisamba") ||
           streq(flag_name, "fl_tsurami") ||
           streq(flag_name, "fl_benkei") ||
           streq(flag_name, "fl_congo_killed");
}

int boss_sync_should_defer_flag(const char *flag_name)
{
    /* 0x12D/fl_congo is Congo's later reward/cutscene bit.  It is not the
     * live defeat trigger, but it must still be held while Congo is active. */
    return boss_sync_is_completion_flag(flag_name) || streq(flag_name, "fl_congo");
}

int boss_sync_has_active_encounter(const char *flag_name)
{
    TrackedBoss *boss = boss_for_flag(flag_name);

    if ((streq(flag_name, "fl_congo_killed") || streq(flag_name, "fl_congo")) &&
        congo_remote_defeat_is_current())
        return 1;
    if ((streq(flag_name, "fl_congo_killed") || streq(flag_name, "fl_congo")) &&
        boss == &s_congo && !s_congo_victory_complete)
        return tracked_boss_can_take_damage(&s_congo);
    if (streq(flag_name, "fl_tsurami"))
    {
        if (native_remote_defeat_is_current(&s_tsurami_state))
            return 1;
        return !s_tsurami_state.victory_complete &&
               tracked_boss_can_take_damage(&s_tsurami);
    }
    if (streq(flag_name, "fl_dharmanyo"))
    {
        if (native_remote_defeat_is_current(&s_darumanyo_state))
            return 1;
        return !s_darumanyo_state.victory_complete &&
               darumanyo_can_take_damage();
    }
    if (streq(flag_name, "fl_benkei"))
    {
        if (native_remote_defeat_is_current(&s_benkei_state))
            return 1;
        return !s_benkei_state.victory_complete && benkei_can_take_damage();
    }
    return boss && tracked_boss_is_active(boss);
}

int boss_sync_has_local_encounter(const char *flag_name)
{
    TrackedBoss *boss = boss_for_flag(flag_name);

    if (streq(flag_name, "fl_tsurami"))
        return !s_tsurami_state.victory_complete &&
               tracked_boss_is_local(&s_tsurami);
    if (streq(flag_name, "fl_dharmanyo"))
    {
        if (native_remote_defeat_is_current(&s_darumanyo_state))
            return 1;
        return !s_darumanyo_state.victory_complete &&
               darumanyo_is_local();
    }
    if (streq(flag_name, "fl_benkei"))
        return !s_benkei_state.victory_complete && benkei_is_local();
    return boss && tracked_boss_is_local(boss);
}

int boss_sync_send_defeat(const char *flag_name)
{
    char payload[64];
    char *wp = payload;
    char *team_id;
    int sent;
    const char *name = flag_name;

    if (!boss_sync_is_completion_flag(flag_name))
        return 0;

    *wp++ = '{';
    *wp++ = '"';
    *wp++ = 'f';
    *wp++ = 'l';
    *wp++ = 'a';
    *wp++ = 'g';
    *wp++ = '"';
    *wp++ = ':';
    *wp++ = '"';
    while (*name && wp < payload + sizeof(payload) - 3)
        *wp++ = *name++;
    *wp++ = '"';
    *wp++ = '}';
    *wp = '\0';

    team_id = anchor_get_team_id();
    if (!team_id || !team_id[0])
    {
        if (team_id)
            recomp_free(team_id);
        return 0;
    }

    /* This event is meaningful only to clients currently in the fight.  It
     * must not be queued; SET_FLAG, sent afterward, is the offline update. */
    sent = anchor_send_custom_packet("MNSG_BOSS_DEFEAT", payload,
                                     team_id, 0, 0);
    recomp_free(team_id);
    if (sent)
        item_sync_mark_boss_defeat_announced(flag_name);
    return sent;
}

int boss_sync_apply_remote_defeat(const char *flag_name)
{
    NativeBossState *state;
    TrackedBoss *boss;
    unsigned int flag_id;

    if (streq(flag_name, "fl_dharmanyo"))
    {
        state = &s_darumanyo_state;
        flag_id = DARUMANYO_KILL_FLAG;
        if (native_remote_defeat_is_current(state))
            return 1;
        if (state->victory_complete)
            return 0;
        if (state->local_defeat_started)
            return 1;

        /* A live event can be drained on Dharmanyo's first room frame before
         * 03E64 has installed entity 0xCC.  Admit only the known fight room
         * and only while the durable completion bit is still clear. */
        if (!darumanyo_is_local())
        {
            if (D_800C7AB2 != DARUMANYO_ROOM ||
                func_800240DC_24CDC(DARUMANYO_KILL_FLAG) ||
                func_800240DC_24CDC(DARUMANYO_NATIVE_DEATH_FLAG) ||
                func_800240DC_24CDC(DARUMANYO_NATIVE_COMPLETE_FLAG))
            {
                recomp_printf("[BossSync] Dharmanyo defeat received before the combat actor was tracked.\n");
                return 0;
            }

            state->lethal_hit_pending = 1;
            state->lethal_hit_armed = 0;
            state->lethal_hit_room = D_800C7AB2;
            state->remote_defeat_in_progress = 1;
            state->remote_defeat_room = D_800C7AB2;
            state->remote_defeat_needs_rearm = 1;
            recomp_printf("[BossSync] Dharmanyo native final hit queued before actor setup.\n");
            return 1;
        }
        if (!darumanyo_can_take_damage())
        {
            recomp_printf("[BossSync] Dharmanyo defeat rejected: lives=%u carrier=%u status=0x%08X.\n",
                          (unsigned int)DARUMANYO_LIVES(s_darumanyo.actor),
                          (unsigned int)ACTOR_HEALTH(s_darumanyo.actor),
                          ACTOR_STATUS(s_darumanyo.actor));
            return 0;
        }

        func_80024088_24C88((int)flag_id);
        state->lethal_hit_pending = 1;
        state->lethal_hit_armed = 0;
        state->lethal_hit_room = D_800C7AB2;
        state->remote_defeat_in_progress = 1;
        state->remote_defeat_room = D_800C7AB2;
        state->remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Dharmanyo native final hit queued with %u lives.\n",
                      (unsigned int)DARUMANYO_LIVES(s_darumanyo.actor));
        return 1;
    }

    if (streq(flag_name, "fl_tsurami"))
    {
        state = &s_tsurami_state;
        boss = &s_tsurami;
        flag_id = TSURAMI_KILL_FLAG;
        if (native_remote_defeat_is_current(state))
            return 1;
        if (state->victory_complete)
            return 0;
        if (state->local_defeat_started)
            return 1;
        if (!tracked_boss_is_local(boss))
        {
            recomp_printf("[BossSync] Tsurami defeat received before the root actor was tracked.\n");
            return 0;
        }
        if (!tracked_boss_can_take_damage(boss))
        {
            recomp_printf("[BossSync] Tsurami defeat rejected: health=%u status=0x%08X.\n",
                          (unsigned int)ACTOR_HEALTH(boss->actor),
                          ACTOR_STATUS(boss->actor));
            return 0;
        }

        /* fl_tsurami is script progression, not the root's death trigger.
         * Remove a stale copy until the native teardown callback is reached. */
        func_80024088_24C88((int)flag_id);
        state->lethal_hit_pending = 1;
        state->lethal_hit_armed = 0;
        state->lethal_hit_room = D_800C7AB2;
        state->remote_defeat_in_progress = 1;
        state->remote_defeat_room = D_800C7AB2;
        state->remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Tsurami native final hit queued at health %u.\n",
                      (unsigned int)ACTOR_HEALTH(boss->actor));
        return 1;
    }

    if (streq(flag_name, "fl_benkei"))
    {
        state = &s_benkei_state;
        flag_id = BENKEI_KILL_FLAG;
        if (native_remote_defeat_is_current(state))
            return 1;
        if (state->victory_complete)
            return 0;
        if (state->local_defeat_started)
            return 1;
        if (!benkei_is_local())
        {
            recomp_printf("[BossSync] Benkei defeat received before the combat actor was tracked.\n");
            return 0;
        }
        if (!benkei_can_take_damage())
        {
            recomp_printf("[BossSync] Benkei defeat rejected: lives=%d state=%u command=%u status=0x%08X.\n",
                          (int)BENKEI_HEALTH(s_benkei.actor),
                          (unsigned int)BENKEI_STATE(s_benkei.actor),
                          (unsigned int)BENKEI_COMMAND(s_benkei.actor),
                          ACTOR_STATUS(s_benkei.actor));
            return 0;
        }

        func_80024088_24C88((int)flag_id);
        state->lethal_hit_pending = 1;
        state->lethal_hit_armed = 0;
        state->lethal_hit_room = D_800C7AB2;
        state->remote_defeat_in_progress = 1;
        state->remote_defeat_room = D_800C7AB2;
        state->remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Benkei native final hit queued with %d lives.\n",
                      (int)BENKEI_HEALTH(s_benkei.actor));
        return 1;
    }

    if (!streq(flag_name, "fl_congo_killed"))
        return 0;
    if (congo_remote_defeat_is_current())
        return 1;
    if (s_congo_victory_complete)
        return 0;
    /* Ignore a packet echoed from this client's own already-started native
     * kill.  A228/07D24, not another synthetic hit, owns that transition. */
    if (s_congo_local_defeat_started)
        return 1;
    if (!tracked_boss_is_local(&s_congo))
    {
        recomp_printf("[BossSync] Congo defeat received before a local actor was tracked.\n");
        return 0;
    }
    if (!tracked_boss_can_take_damage(&s_congo))
    {
        recomp_printf("[BossSync] Congo defeat rejected: health=%u status=0x%08X entity=0x%04X.\n",
                      (unsigned int)ACTOR_HEALTH(s_congo.actor),
                      ACTOR_STATUS(s_congo.actor),
                      (unsigned int)ACTOR_ENTITY_ID(s_congo.actor));
        return 0;
    }

    /* Repair saves contaminated by an earlier flag-only sync.  The official
     * callback will set 0x12D again after this actor reaches native death. */
    func_80024088_24C88(CONGO_REWARD_FLAG);
    s_congo_lethal_hit_pending = 1;
    s_congo_lethal_hit_room = D_800C7AB2;
    s_congo_remote_defeat_in_progress = 1;
    s_congo_remote_defeat_room = D_800C7AB2;
    s_congo_remote_defeat_needs_rearm = 0;
    recomp_printf("[BossSync] Congo native lethal hit queued at health %u.\n",
                  (unsigned int)ACTOR_HEALTH(s_congo.actor));
    return 1;
}

/* Congo can be defeated by another client during this client's first room
 * frame.  In that ordering func_0800A228_6BD4C8 runs during actor setup, but
 * the scheduled func_0800000C_6B32AC main update has not run yet.  Capture the
 * exact health-bearing actor here so an incoming defeat packet at frame end
 * can still queue native damage for the following update. */
RECOMP_HOOK("func_0800A228_6BD4C8")
void boss_sync_track_congo_health_actor(void *actor)
{
    /* Tsurami's root also calls A228, but it has no Congo entity ID. */
    if (!actor || ACTOR_ENTITY_ID(actor) != ENTITY_CONGO)
        return;

    unsigned int actor_changed =
        actor != s_congo.actor || s_congo.room != D_800C7AB2;
    unsigned int health = ACTOR_HEALTH(actor);

    if (actor_changed || (health > 1 && s_congo_victory_complete))
    {
        track_boss(&s_congo, actor, ENTITY_CONGO);
        s_congo_victory_complete = 0;
        s_congo_local_defeat_started = 0;
        recomp_printf("[BossSync] Congo health actor tracked: health=%u active=%u removed=%u.\n",
                      health,
                      (ACTOR_STATUS(actor) & ACTOR_STATUS_ACTIVE) != 0,
                      (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING) != 0);
    }

    /* A remote defeat that survived leaving the room belongs to the newly
     * tracked Congo instance.  Restore the pending native hit, not 0x12D. */
    if ((actor_changed || s_congo_remote_defeat_needs_rearm) &&
        congo_remote_defeat_is_current())
    {
        func_80024088_24C88(CONGO_REWARD_FLAG);
        s_congo_lethal_hit_pending = 1;
        s_congo_lethal_hit_armed = 0;
        s_congo_lethal_hit_room = D_800C7AB2;
        s_congo_remote_defeat_room = D_800C7AB2;
        s_congo_remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Re-armed Congo native defeat after room re-entry.\n");
    }

    /* A queued/offline flag can arrive before this actor exists.  Convert
     * that persisted flag-only state into the same native lethal transition
     * as soon as a real Congo health actor appears. */
    if (!congo_remote_defeat_is_current() &&
        !s_congo_local_defeat_started && !s_congo_victory_complete &&
        tracked_boss_can_take_damage(&s_congo) &&
        (func_800240DC_24CDC(CONGO_KILL_FLAG) ||
         func_800240DC_24CDC(CONGO_REWARD_FLAG)))
    {
        func_80024088_24C88(CONGO_REWARD_FLAG);
        s_congo_lethal_hit_pending = 1;
        s_congo_lethal_hit_armed = 0;
        s_congo_lethal_hit_room = D_800C7AB2;
        s_congo_remote_defeat_in_progress = 1;
        s_congo_remote_defeat_room = D_800C7AB2;
        s_congo_remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Recovered persisted Congo flags through native defeat.\n");
    }

    /* Announce native health-zero directly as a once-per-encounter fallback.
     * This still works when a prior broken save already contains 0x1A1, so
     * item_sync cannot observe another zero-to-one flag transition. */
    if (health == 0 && !congo_remote_defeat_is_current() &&
        !s_congo_local_defeat_started)
    {
        s_congo_local_defeat_started = 1;
        if (boss_sync_send_defeat("fl_congo_killed"))
            recomp_printf("[BossSync] Sent Congo defeat from native health-zero.\n");
    }
}

/* Congo and the health-bearing Tsurami root both call this complete common
 * damage pipeline with the real actor in a0.  The exact tracked pointer keeps
 * this global hook away from Tsurami's 0xCB visual child actors and unrelated
 * enemies. */
RECOMP_HOOK("func_80218E7C_5D434C")
void boss_sync_apply_common_lethal_hit(void *actor)
{
    unsigned int old_health;

    if (s_congo_lethal_hit_pending)
    {
        if (s_congo_lethal_hit_room != D_800C7AB2)
        {
            s_congo_lethal_hit_pending = 0;
        }
        else if (actor == s_congo.actor && !s_congo_victory_complete &&
                 tracked_boss_can_take_damage(&s_congo))
        {
            old_health = ACTOR_HEALTH(actor);
            ACTOR_HEALTH(actor) = 1;
            ACTOR_STATUS(actor) |= ACTOR_STATUS_DAMAGE_PENDING;
            s_congo_lethal_hit_pending = 0;
            s_congo_lethal_hit_armed = 1;
            recomp_printf("[BossSync] Armed Congo native damage: health %u -> 1.\n",
                          old_health);
            return;
        }
    }

    if (!s_tsurami_state.lethal_hit_pending)
        return;
    if (s_tsurami_state.lethal_hit_room != D_800C7AB2)
    {
        s_tsurami_state.lethal_hit_pending = 0;
        return;
    }
    if (actor != s_tsurami.actor || s_tsurami_state.victory_complete ||
        !tracked_boss_can_take_damage(&s_tsurami))
        return;

    /* Tsurami's callback treats HP==1 as lethal.  Enter at two HP so the
     * common one-point damage path leaves one and invokes actor+0x90.  Setting
     * one here would hit zero and incorrectly bypass that custom callback. */
    old_health = ACTOR_HEALTH(actor);
    ACTOR_HEALTH(actor) = 2;
    ACTOR_STATUS(actor) |= ACTOR_STATUS_DAMAGE_PENDING;
    s_tsurami_state.lethal_hit_pending = 0;
    s_tsurami_state.lethal_hit_armed = 1;
    recomp_printf("[BossSync] Armed Tsurami native damage: health %u -> 2.\n",
                  old_health);
}

RECOMP_HOOK_RETURN("func_80218E7C_5D434C")
void boss_sync_check_common_lethal_hit(void)
{
    unsigned int health;
    unsigned int active;

    if (s_congo_lethal_hit_armed)
    {
        s_congo_lethal_hit_armed = 0;

        if (tracked_boss_is_local(&s_congo))
        {
            health = ACTOR_HEALTH(s_congo.actor);
            active = (ACTOR_STATUS(s_congo.actor) & ACTOR_STATUS_ACTIVE) != 0;
            recomp_printf("[BossSync] Congo common damage returned: health=%u active=%u.\n",
                          health, active);

            /* HP zero acknowledges that func_80218350 consumed the final hit.
             * The caller now enters A228 and the official victory callback. */
            if (health == 0)
            {
                recomp_printf("[BossSync] Congo native damage accepted; awaiting victory callback.\n");
            }
            else if (!s_congo_victory_complete &&
                     (ACTOR_STATUS(s_congo.actor) & ACTOR_STATUS_REMOVE_PENDING) == 0)
            {
                ACTOR_STATUS(s_congo.actor) &= ~ACTOR_STATUS_DAMAGE_PENDING;
                s_congo_lethal_hit_pending = 1;
                recomp_printf("[BossSync] Congo lethal hit was not consumed; retrying.\n");
            }
        }
    }

    if (!s_tsurami_state.lethal_hit_armed)
        return;
    s_tsurami_state.lethal_hit_armed = 0;

    if (!tracked_boss_is_local(&s_tsurami))
        return;

    health = ACTOR_HEALTH(s_tsurami.actor);
    active = (ACTOR_STATUS(s_tsurami.actor) & ACTOR_STATUS_ACTIVE) != 0;
    recomp_printf("[BossSync] Tsurami common damage returned: health=%u active=%u.\n",
                  health, active);

    /* HP one proves the common routine consumed exactly one point and handed
     * control to Tsurami's actor+0x90 callback.  That callback schedules the
     * native func_08002FFC death entry. */
    if (health == 1)
    {
        recomp_printf("[BossSync] Tsurami native damage accepted; awaiting death entry.\n");
        return;
    }

    if (!s_tsurami_state.victory_complete &&
        (ACTOR_STATUS(s_tsurami.actor) & ACTOR_STATUS_REMOVE_PENDING) == 0)
    {
        ACTOR_STATUS(s_tsurami.actor) &= ~ACTOR_STATUS_DAMAGE_PENDING;
        s_tsurami_state.lethal_hit_pending = 1;
        recomp_printf("[BossSync] Tsurami final hit was not consumed; retrying.\n");
    }
}

/* func_0800A228 schedules this callback only from Congo's native zero-health
 * branch.  Releasing the hold here prevents a received 0x12D from starting
 * the reward cutscene before the official death transition reaches it. */
RECOMP_HOOK("func_08007D24_6BAFC4")
void boss_sync_finish_congo_remote_defeat(void *actor)
{
    if (actor != s_congo.actor)
        return;

    s_congo_victory_complete = 1;
    if (congo_remote_defeat_is_current())
    {
        s_congo_remote_defeat_in_progress = 0;
        s_congo_remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Congo native victory callback reached; reward sync released.\n");
    }
}

/* Capture Tsurami's root while its setup callback is still installing 00388.
 * This closes the frame-end packet gap without reading its not-yet-initialized
 * health or running recovery/damage logic before setup has completed. */
RECOMP_HOOK("func_080017F4_6B4A94")
void boss_sync_capture_tsurami_root_setup(void *actor)
{
    if (!actor)
        return;

    /* This unique initializer proves a fresh encounter even if the actor pool
     * reuses the same pointer after returning to the same room. */
    s_tsurami_setup_actor_changed = 1;
    track_boss(&s_tsurami, actor, 0);
}

/* func_08000388 is registered only on Tsurami's health-bearing root.  The
 * nearby 0xCB callbacks belong to visual components and must never become the
 * authoritative pointer.  The root does not initialize actor+0x5E, so entity
 * validation is intentionally disabled for this tracker. */
RECOMP_HOOK("func_08000388_6B3628")
void boss_sync_track_tsurami_root(void *actor)
{
    unsigned int actor_changed;
    unsigned int health;

    if (!actor)
        return;

    actor_changed = s_tsurami_setup_actor_changed ||
                    actor != s_tsurami.actor ||
                    s_tsurami.room != D_800C7AB2;
    health = ACTOR_HEALTH(actor);

    if (actor_changed || (health > 1 && s_tsurami_state.victory_complete))
    {
        track_boss(&s_tsurami, actor, 0);
        s_tsurami_state.victory_complete = 0;
        s_tsurami_state.local_defeat_started = 0;
        recomp_printf("[BossSync] Tsurami root tracked: health=%u status=0x%08X.\n",
                      health, ACTOR_STATUS(actor));
    }
    s_tsurami_setup_actor_changed = 0;

    if ((actor_changed || s_tsurami_state.remote_defeat_needs_rearm) &&
        native_remote_defeat_is_current(&s_tsurami_state) &&
        tracked_boss_can_take_damage(&s_tsurami))
    {
        func_80024088_24C88(TSURAMI_KILL_FLAG);
        s_tsurami_state.lethal_hit_pending = 1;
        s_tsurami_state.lethal_hit_armed = 0;
        s_tsurami_state.lethal_hit_room = D_800C7AB2;
        s_tsurami_state.remote_defeat_room = D_800C7AB2;
        s_tsurami_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Re-armed Tsurami native defeat after room re-entry.\n");
    }

    /* Repair a durable flag received before this root existed. */
    if (!native_remote_defeat_is_current(&s_tsurami_state) &&
        !s_tsurami_state.local_defeat_started &&
        !s_tsurami_state.victory_complete &&
        tracked_boss_can_take_damage(&s_tsurami) &&
        func_800240DC_24CDC(TSURAMI_KILL_FLAG))
    {
        func_80024088_24C88(TSURAMI_KILL_FLAG);
        s_tsurami_state.lethal_hit_pending = 1;
        s_tsurami_state.lethal_hit_armed = 0;
        s_tsurami_state.lethal_hit_room = D_800C7AB2;
        s_tsurami_state.remote_defeat_in_progress = 1;
        s_tsurami_state.remote_defeat_room = D_800C7AB2;
        s_tsurami_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Recovered persisted Tsurami flag through native defeat.\n");
    }
}

/* Tsurami reaches its official death state at HP one through actor+0x90. */
RECOMP_HOOK("func_08002FFC_6B629C")
void boss_sync_start_tsurami_native_death(void *actor)
{
    int remote;

    if (actor != s_tsurami.actor ||
        s_tsurami.room != D_800C7AB2)
        return;

    remote = native_remote_defeat_is_current(&s_tsurami_state);
    s_tsurami_state.lethal_hit_pending = 0;
    s_tsurami_state.lethal_hit_armed = 0;
    if (!s_tsurami_state.local_defeat_started)
    {
        s_tsurami_state.local_defeat_started = 1;
        if (!remote && boss_sync_send_defeat("fl_tsurami"))
            recomp_printf("[BossSync] Sent Tsurami defeat from native death entry.\n");
    }
    recomp_printf("[BossSync] Tsurami native death entry reached.\n");
}

/* 3A00 is scheduled only after the native destruction flag 0x1C1 and its
 * post-destruction wait; its original routine tears down the root task. */
RECOMP_HOOK("func_08003A00_6B6CA0")
void boss_sync_finish_tsurami_native_death(void *actor)
{
    if (actor != s_tsurami.actor ||
        s_tsurami.room != D_800C7AB2)
        return;

    s_tsurami_state.victory_complete = 1;
    s_tsurami_state.lethal_hit_pending = 0;
    s_tsurami_state.lethal_hit_armed = 0;
    if (native_remote_defeat_is_current(&s_tsurami_state))
    {
        /* Commit progression at the proven native terminal itself.  This is
         * stronger than relying on a deferred SET_FLAG to be redelivered if
         * the connection reset while the death sequence was running. */
        item_sync_commit_boss_completion("fl_tsurami");
        s_tsurami_state.remote_defeat_in_progress = 0;
        s_tsurami_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Tsurami native teardown reached; progression sync released.\n");
    }
}

/* ENTITY_DARUMANYO used to point at ordinary entity 0x132.  The actual boss
 * controller is entity 0xCC, created by 03E64 with twelve lives at +0xD1.
 * Capture it during setup so a frame-end live event cannot expose flag 0x18
 * before the first scheduled 03F84 combat update. */
RECOMP_HOOK("func_08003E64_6CC074")
void boss_sync_capture_darumanyo_setup(void *actor)
{
    if (!actor)
        return;

    /* Treat every initializer invocation as a fresh actor; pointer and room
     * values can both be reused on a later visit. */
    s_darumanyo_setup_actor_changed = 1;
    track_boss(&s_darumanyo, actor, ENTITY_DARUMANYO);
}

/* 03F84 is the authoritative live controller.  Its generic +0x8D byte is
 * reset to ten every frame; +0xD1 is the real boss lives counter. */
RECOMP_HOOK("func_08003F84_6CC194")
void boss_sync_track_darumanyo_controller(void *actor)
{
    unsigned int actor_changed;

    if (!actor || ACTOR_ENTITY_ID(actor) != ENTITY_DARUMANYO)
        return;

    actor_changed = s_darumanyo_setup_actor_changed ||
                    actor != s_darumanyo.actor ||
                    s_darumanyo.room != D_800C7AB2;
    if (actor_changed ||
        (DARUMANYO_LIVES(actor) > 0 && s_darumanyo_state.victory_complete))
    {
        track_boss(&s_darumanyo, actor, ENTITY_DARUMANYO);
        s_darumanyo_state.victory_complete = 0;
        s_darumanyo_state.local_defeat_started = 0;
        s_darumanyo_terminal_controller = 0;
        recomp_printf("[BossSync] Dharmanyo controller tracked: lives=%u carrier=%u status=0x%08X.\n",
                      (unsigned int)DARUMANYO_LIVES(actor),
                      (unsigned int)ACTOR_HEALTH(actor),
                      ACTOR_STATUS(actor));
    }
    s_darumanyo_setup_actor_changed = 0;

    if ((actor_changed || s_darumanyo_state.remote_defeat_needs_rearm) &&
        native_remote_defeat_is_current(&s_darumanyo_state) &&
        darumanyo_can_take_damage())
    {
        func_80024088_24C88(DARUMANYO_KILL_FLAG);
        s_darumanyo_state.lethal_hit_pending = 1;
        s_darumanyo_state.lethal_hit_armed = 0;
        s_darumanyo_state.lethal_hit_room = D_800C7AB2;
        s_darumanyo_state.remote_defeat_room = D_800C7AB2;
        s_darumanyo_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Re-armed Dharmanyo native defeat after actor setup/re-entry.\n");
    }

    /* Repair a flag-only save by replaying the real last hit and restoring
     * durable progression only after 066E0 confirms teardown completion. */
    if (!native_remote_defeat_is_current(&s_darumanyo_state) &&
        !s_darumanyo_state.local_defeat_started &&
        !s_darumanyo_state.victory_complete &&
        darumanyo_can_take_damage() &&
        func_800240DC_24CDC(DARUMANYO_KILL_FLAG))
    {
        func_80024088_24C88(DARUMANYO_KILL_FLAG);
        s_darumanyo_state.lethal_hit_pending = 1;
        s_darumanyo_state.lethal_hit_armed = 0;
        s_darumanyo_state.lethal_hit_room = D_800C7AB2;
        s_darumanyo_state.remote_defeat_in_progress = 1;
        s_darumanyo_state.remote_defeat_room = D_800C7AB2;
        s_darumanyo_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Recovered persisted Dharmanyo flag through native defeat.\n");
    }
}

/* Unlike Congo/Tsurami, 03F84 calls the inner damage routine directly.  Two
 * carrier HP leaves one after its fast one-point path, causing status bit 0
 * and the official 0432C hit callback without triggering generic HP-zero. */
RECOMP_HOOK("func_80218350_5D3820")
void boss_sync_apply_darumanyo_native_hit(void *actor)
{
    if (!s_darumanyo_state.lethal_hit_pending ||
        s_darumanyo_state.lethal_hit_room != D_800C7AB2 ||
        actor != s_darumanyo.actor ||
        s_darumanyo_state.victory_complete ||
        !darumanyo_can_accept_hit())
        return;

    DARUMANYO_LIVES(actor) = 1;
    ACTOR_HEALTH(actor) = 2;
    ACTOR_STATUS(actor) |= ACTOR_STATUS_DAMAGE_PENDING;
    s_darumanyo_state.lethal_hit_pending = 0;
    s_darumanyo_state.lethal_hit_armed = 1;
    s_darumanyo_damage_actor = actor;
    recomp_printf("[BossSync] Armed Dharmanyo native damage: lives -> 1, carrier -> 2.\n");
}

RECOMP_HOOK_RETURN("func_80218350_5D3820")
void boss_sync_check_darumanyo_native_hit(void)
{
    void *actor;
    unsigned int health;
    unsigned int status;

    if (!s_darumanyo_state.lethal_hit_armed)
        return;

    actor = s_darumanyo_damage_actor;
    s_darumanyo_damage_actor = 0;
    s_darumanyo_state.lethal_hit_armed = 0;
    if (!actor || actor != s_darumanyo.actor || !darumanyo_is_local())
        return;

    health = ACTOR_HEALTH(actor);
    status = ACTOR_STATUS(actor);
    /* 03F84 calls 80218350 directly, so no outer 80218E7C wrapper exists to
     * clear this synthetic input bit before the next frame. */
    ACTOR_STATUS(actor) = status & ~ACTOR_STATUS_DAMAGE_PENDING;

    if (health == 1 && (status & ACTOR_STATUS_ACTIVE) != 0)
    {
        recomp_printf("[BossSync] Dharmanyo native damage accepted; awaiting lives callback.\n");
        return;
    }

    if (!s_darumanyo_state.victory_complete &&
        DARUMANYO_LIVES(actor) > 0)
    {
        s_darumanyo_state.lethal_hit_pending = 1;
        recomp_printf("[BossSync] Dharmanyo final hit was not consumed; retrying.\n");
    }
}

/* 0432C consumes one real Dharmanyo life on entry.  Seeing the last remaining
 * life here is the native final-hit onset; its original routine decrements to
 * zero and starts the 030A8 -> 0314C -> 036FC destruction chain. */
RECOMP_HOOK("func_0800432C_6CC53C")
void boss_sync_start_darumanyo_native_death(void *actor)
{
    int remote;

    if (actor != s_darumanyo.actor || !darumanyo_is_local() ||
        DARUMANYO_HIT_TIMER(actor) != 0 ||
        DARUMANYO_LIVES(actor) != 1 ||
        s_darumanyo_state.local_defeat_started)
        return;

    remote = native_remote_defeat_is_current(&s_darumanyo_state);
    s_darumanyo_state.local_defeat_started = 1;
    s_darumanyo_state.lethal_hit_pending = 0;
    s_darumanyo_state.lethal_hit_armed = 0;
    s_darumanyo_terminal_controller = 0;
    if (!remote && boss_sync_send_defeat("fl_dharmanyo"))
        recomp_printf("[BossSync] Sent Dharmanyo defeat from native last-life callback.\n");
    recomp_printf("[BossSync] Dharmanyo native last-life callback reached.\n");
}

/* 0636C owns the post-fight gauge/fade controller.  It polls the 0x16B
 * destruction flag and eventually schedules 065E8 then terminal 066E0. */
RECOMP_HOOK("func_0800636C_6CE57C")
void boss_sync_track_darumanyo_terminal_controller(void *actor)
{
    if (!actor || s_darumanyo.room != D_800C7AB2 ||
        !s_darumanyo_state.local_defeat_started)
        return;
    s_darumanyo_terminal_controller = actor;
}

/* 066E0 is installed only after 065E8 reaches zero, sets native internal flag
 * 0x16D, and tears down the fight.  Only now may fl_dharmanyo reach scripts. */
RECOMP_HOOK("func_080066E0_6CE8F0")
void boss_sync_finish_darumanyo_native_death(void *actor)
{
    if (!actor || actor != s_darumanyo_terminal_controller ||
        s_darumanyo.room != D_800C7AB2 ||
        !s_darumanyo_state.local_defeat_started)
        return;

    s_darumanyo_state.victory_complete = 1;
    s_darumanyo_state.lethal_hit_pending = 0;
    s_darumanyo_state.lethal_hit_armed = 0;
    if (native_remote_defeat_is_current(&s_darumanyo_state))
    {
        item_sync_commit_boss_completion("fl_dharmanyo");
        s_darumanyo_state.remote_defeat_in_progress = 0;
        s_darumanyo_state.remote_defeat_needs_rearm = 0;
    }
    s_darumanyo_terminal_controller = 0;
    recomp_printf("[BossSync] Dharmanyo terminal callback reached; progression sync released.\n");
}

/* Benkei is a scripted fight rather than a common +0x8D health actor.  Its
 * own update consumes status bit 0x80 in AI states 2/5/6 and decrements the
 * signed lives byte at +0xD3. */
RECOMP_HOOK("func_08000B98_70ABD8")
void boss_sync_drive_benkei_native_hit(void *actor)
{
    unsigned int actor_changed;
    unsigned int state;

    s_benkei_update_actor = actor;
    if (!actor)
        return;

    /* The first state-zero callback performs the 0x1C0 initialization.  Keep
     * that fresh-actor evidence for the next validated update; otherwise the
     * pre-initialization hook would consume the only re-entry opportunity. */
    if (ACTOR_ENTITY_ID(actor) != ENTITY_BENKEI || BENKEI_STATE(actor) == 0)
        s_benkei_setup_actor_changed = 1;

    actor_changed = s_benkei_setup_actor_changed ||
                    actor != s_benkei.actor ||
                    s_benkei.room != D_800C7AB2;
    if (actor_changed ||
        (ACTOR_ENTITY_ID(actor) == ENTITY_BENKEI &&
         BENKEI_HEALTH(actor) > 0 && s_benkei_state.victory_complete))
    {
        track_boss(&s_benkei, actor, ENTITY_BENKEI);
        s_benkei_state.victory_complete = 0;
        s_benkei_state.local_defeat_started = 0;
        if (ACTOR_ENTITY_ID(actor) == ENTITY_BENKEI)
        {
            recomp_printf("[BossSync] Benkei combat actor tracked: lives=%d state=%u.\n",
                          (int)BENKEI_HEALTH(actor),
                          (unsigned int)BENKEI_STATE(actor));
        }
    }

    /* The first callback initializes entity 0x1C0.  Validation below delays
     * all injection/recovery work until a subsequent real combat update. */
    if (!benkei_is_local())
        return;

    /* A recycled actor slot can still contain Benkei's old entity id while
     * the state-zero initializer is rebuilding the rest of the actor.  Do
     * not consume the fresh-setup latch until the initialized update that
     * follows, when its lives and command fields are authoritative again. */
    if (BENKEI_STATE(actor) == 0)
        return;
    s_benkei_setup_actor_changed = 0;

    if ((actor_changed || s_benkei_state.remote_defeat_needs_rearm) &&
        native_remote_defeat_is_current(&s_benkei_state) &&
        benkei_can_take_damage())
    {
        func_80024088_24C88(BENKEI_KILL_FLAG);
        s_benkei_state.lethal_hit_pending = 1;
        s_benkei_state.lethal_hit_armed = 0;
        s_benkei_state.lethal_hit_room = D_800C7AB2;
        s_benkei_state.remote_defeat_room = D_800C7AB2;
        s_benkei_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Re-armed Benkei native defeat after room re-entry.\n");
    }

    if (!native_remote_defeat_is_current(&s_benkei_state) &&
        !s_benkei_state.local_defeat_started &&
        !s_benkei_state.victory_complete &&
        benkei_can_take_damage() &&
        func_800240DC_24CDC(BENKEI_KILL_FLAG))
    {
        func_80024088_24C88(BENKEI_KILL_FLAG);
        s_benkei_state.lethal_hit_pending = 1;
        s_benkei_state.lethal_hit_armed = 0;
        s_benkei_state.lethal_hit_room = D_800C7AB2;
        s_benkei_state.remote_defeat_in_progress = 1;
        s_benkei_state.remote_defeat_room = D_800C7AB2;
        s_benkei_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Recovered persisted Benkei flag through native defeat.\n");
    }

    if (!s_benkei_state.lethal_hit_pending ||
        s_benkei_state.lethal_hit_room != D_800C7AB2 ||
        s_benkei_state.victory_complete || !benkei_can_take_damage() ||
        BENKEI_COMMAND(actor) != 0)
        return;

    state = BENKEI_STATE(actor);
    if (state != 2 && state != 5 && state != 6)
        return;

    BENKEI_HEALTH(actor) = 1;
    ACTOR_STATUS(actor) |= BENKEI_STATUS_HIT_PENDING;
    s_benkei_state.lethal_hit_pending = 0;
    s_benkei_state.lethal_hit_armed = 1;
    recomp_printf("[BossSync] Armed Benkei native hit in AI state %u.\n", state);
}

RECOMP_HOOK_RETURN("func_08000B98_70ABD8")
void boss_sync_check_benkei_native_hit(void)
{
    void *actor = s_benkei_update_actor;
    int health;
    int remote;

    s_benkei_update_actor = 0;
    if (!actor || actor != s_benkei.actor || !benkei_is_local())
        return;

    health = (int)BENKEI_HEALTH(actor);
    if (s_benkei_state.lethal_hit_armed)
    {
        s_benkei_state.lethal_hit_armed = 0;
        if (health <= 0)
        {
            recomp_printf("[BossSync] Benkei native final hit accepted; awaiting fall animation.\n");
        }
        else if (!s_benkei_state.victory_complete &&
                 (ACTOR_STATUS(actor) & ACTOR_STATUS_REMOVE_PENDING) == 0)
        {
            ACTOR_STATUS(actor) &= ~BENKEI_STATUS_HIT_PENDING;
            s_benkei_state.lethal_hit_pending = 1;
            recomp_printf("[BossSync] Benkei final hit was not consumed; retrying.\n");
        }
    }

    if (health <= 0 && !s_benkei_state.local_defeat_started)
    {
        remote = native_remote_defeat_is_current(&s_benkei_state);
        s_benkei_state.local_defeat_started = 1;
        if (!remote && boss_sync_send_defeat("fl_benkei"))
            recomp_printf("[BossSync] Sent Benkei defeat from native lives-zero.\n");
    }
}

/* The fall-animation acknowledgement is not the end of Benkei's quest.  His
 * scenario still has to present Sasuke's body, increment the recovery profile,
 * and finally set 0x033.  Committing 0x033 at D2==5 makes that scenario see an
 * already-completed fight and skip the reward event.
 *
 * Hook the scenario's own flag write instead.  item_sync_commit_boss_completion
 * writes the same bit directly (so this pre-hook cannot recurse), aligns the
 * network caches, and releases the durable hold at the exact native commit. */
RECOMP_HOOK("func_80024038_24C38")
void boss_sync_finish_benkei_reward_script(int flag_id)
{
    if (flag_id != (int)BENKEI_KILL_FLAG ||
        !s_benkei_state.local_defeat_started ||
        !native_remote_defeat_is_current(&s_benkei_state))
        return;

    s_benkei_state.victory_complete = 1;
    s_benkei_state.lethal_hit_pending = 0;
    s_benkei_state.lethal_hit_armed = 0;
    if (native_remote_defeat_is_current(&s_benkei_state))
    {
        item_sync_commit_boss_completion("fl_benkei");
        s_benkei_state.remote_defeat_in_progress = 0;
        s_benkei_state.remote_defeat_needs_rearm = 0;
        recomp_printf("[BossSync] Benkei reward script committed Sasuke body progression.\n");
    }
}
