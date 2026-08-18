/**
 * @file boss_sync.c
 * @brief Synchronise live boss defeat transitions separately from save flags.
 *
 * Congo's main update runs the common actor damage routine before it processes
 * actor byte 0x8D as health.  The damage routine does more than write zero: it
 * records the lethal hit and invokes the common zero-health callback.  Applying
 * only the later reward flag, writing zero directly, or jumping to the later
 * flag task skips part of that path and can leave the victory sequence waiting
 * forever.
 *
 * We therefore remember the live Congo actor and turn MNSG_BOSS_DEFEAT into a
 * one-point lethal hit.  It is armed in the shared common actor pipeline
 * (func_80218E7C_5D434C), which both of Congo's update states call immediately
 * before the boss-specific health handler.  The ordinary queued SET_FLAG
 * remains the durable/offline progression update.
 * Other known boss actors are tracked as encounter guards: their progression
 * flags are deferred while their local fight is still active instead of being
 * injected into the middle of an encounter.
 */

#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#include "boss_sync.h"

extern unsigned short D_800C7AB2;
extern void func_80024088_24C88(int flag_id);
extern int func_800240DC_24CDC(int flag_id);

#define ENTITY_TSURAMI 0x00CBu
#define ENTITY_DARUMANYO 0x0132u
#define ENTITY_BENKEI 0x01C0u
#define ENTITY_CONGO 0x0323u

#define ACTOR_ENTITY_ID(actor) \
    (*(volatile unsigned short *)((char *)(actor) + 0x5E))
#define ACTOR_STATUS(actor) \
    (*(volatile unsigned int *)((char *)(actor) + 0x68))
#define ACTOR_HEALTH(actor) \
    (*(volatile unsigned char *)((char *)(actor) + 0x8D))
#define ACTOR_STATUS_ACTIVE 0x00000001u
/* Actor initializers set status bit 1 when a persistent completion flag says
 * the actor should be removed instead of entering its normal update. */
#define ACTOR_STATUS_REMOVE_PENDING 0x00000002u
/* Consumed and cleared by the common actor update after func_80218350.  Its
 * fast damage path subtracts one HP without requiring an attacker pointer. */
#define ACTOR_STATUS_DAMAGE_PENDING 0x00040000u
#define CONGO_KILL_FLAG 0x1A1u
#define CONGO_REWARD_FLAG 0x12Du

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

void boss_sync_reset(void)
{
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
    return boss && tracked_boss_is_active(boss);
}

int boss_sync_has_local_encounter(const char *flag_name)
{
    TrackedBoss *boss = boss_for_flag(flag_name);
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
    return sent;
}

int boss_sync_apply_remote_defeat(const char *flag_name)
{
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

/* Both Congo update states call this complete common actor pipeline and then
 * immediately call func_0800A228_6BD4C8.  Unlike one overlay-state entry,
 * this function always receives the actual actor in a0.  Filtering against
 * the pointer captured by A228 keeps this global hook Congo-only. */
RECOMP_HOOK("func_80218E7C_5D434C")
void boss_sync_apply_congo_lethal_hit(void *actor)
{
    unsigned int old_health;

    if (!s_congo_lethal_hit_pending)
        return;
    if (s_congo_lethal_hit_room != D_800C7AB2)
    {
        s_congo_lethal_hit_pending = 0;
        return;
    }
    if (actor != s_congo.actor || s_congo_victory_complete ||
        !tracked_boss_can_take_damage(&s_congo))
        return;

    old_health = ACTOR_HEALTH(actor);
    ACTOR_HEALTH(actor) = 1;
    ACTOR_STATUS(actor) |= ACTOR_STATUS_DAMAGE_PENDING;
    s_congo_lethal_hit_pending = 0;
    s_congo_lethal_hit_armed = 1;
    recomp_printf("[BossSync] Armed Congo native damage: health %u -> 1.\n",
                  old_health);
}

RECOMP_HOOK_RETURN("func_80218E7C_5D434C")
void boss_sync_check_congo_lethal_hit(void)
{
    unsigned int health;
    unsigned int active;

    if (!s_congo_lethal_hit_armed)
        return;
    s_congo_lethal_hit_armed = 0;

    if (!tracked_boss_is_local(&s_congo))
        return;

    health = ACTOR_HEALTH(s_congo.actor);
    active = (ACTOR_STATUS(s_congo.actor) & ACTOR_STATUS_ACTIVE) != 0;
    recomp_printf("[BossSync] Congo common damage returned: health=%u active=%u.\n",
                  health, active);

    /* HP zero is the acknowledgement from func_80218350 that the synthetic
     * final hit was consumed.  The caller now enters A228, which performs the
     * official 0x1A1 write and schedules 0x07D24. */
    if (health == 0)
    {
        recomp_printf("[BossSync] Congo native damage accepted; awaiting victory callback.\n");
        return;
    }

    if (!s_congo_victory_complete &&
        (ACTOR_STATUS(s_congo.actor) & ACTOR_STATUS_REMOVE_PENDING) == 0)
    {
        ACTOR_STATUS(s_congo.actor) &= ~ACTOR_STATUS_DAMAGE_PENDING;
        s_congo_lethal_hit_pending = 1;
        recomp_printf("[BossSync] Congo lethal hit was not consumed; retrying.\n");
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

/* Actor hooks run before their original routine.  Only the pointer and room
 * are needed here; the entity ID is validated later, after initialization. */

RECOMP_HOOK("func_08003DB0_6B7050")
void boss_sync_track_tsurami_a(void *actor)
{
    track_boss(&s_tsurami, actor, ENTITY_TSURAMI);
}

RECOMP_HOOK("func_080045F8_6B7898")
void boss_sync_track_tsurami_b(void *actor)
{
    track_boss(&s_tsurami, actor, ENTITY_TSURAMI);
}

RECOMP_HOOK("func_080049A4_6B7C44")
void boss_sync_track_tsurami_c(void *actor)
{
    track_boss(&s_tsurami, actor, ENTITY_TSURAMI);
}

RECOMP_HOOK("func_08004808_6B7AA8")
void boss_sync_track_tsurami_d(void *actor)
{
    track_boss(&s_tsurami, actor, ENTITY_TSURAMI);
}

/* The Darumanyo factory allocates the actor internally, so its scheduled
 * callback (rather than the factory's a0) is the first reliable actor pointer. */
RECOMP_HOOK("func_08000A88_721358")
void boss_sync_track_darumanyo(void *actor)
{
    track_boss(&s_darumanyo, actor, ENTITY_DARUMANYO);
}

RECOMP_HOOK("func_08000B98_70ABD8")
void boss_sync_track_benkei(void *actor)
{
    track_boss(&s_benkei, actor, ENTITY_BENKEI);
}
