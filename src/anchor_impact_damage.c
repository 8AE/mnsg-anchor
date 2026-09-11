#include "anchor_impact_damage.h"
#include "anchor_boss_arenas.h"

#ifndef ANCHOR_IMPACT_DAMAGE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

extern unsigned short D_800C7AB2;
extern void *D_8020EED0_63A2B0;
extern int func_801D3954_5FED34(int amount);
extern int func_801D38A4_5FEC84(int amount);

#define IMPACT_STATE_HP 0x60
#define IMPACT_STATE_MECH_HP 0x68
#define IMPACT_HIT_QUEUE 8u
#define IMPACT_HIT_MIN 1
#define IMPACT_HIT_MAX 255

static int s_active;
static int s_owner;
static int s_paused;
static unsigned int s_encounter;
static int s_applying;
static int s_next_sequence;
static AnchorImpactHit s_hits[IMPACT_HIT_QUEUE];
static unsigned int s_hit_read;
static unsigned int s_hit_count;
static int s_boss_before_valid;
static int s_boss_before;
static int s_mech_before_valid;
static int s_mech_before;

static int state_pointer_valid(const void *pointer)
{
#ifdef ANCHOR_IMPACT_DAMAGE_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           address >= 0x80001000u && address < 0x80800000u;
#endif
}

static void *impact_state(void)
{
    void *state = D_8020EED0_63A2B0;
    if (!state_pointer_valid(state) ||
        !(ANCHOR_BOSS_IMPACT_STAGE_VALID(D_800C7AB2) || D_800C7AB2 == 0x0260u))
        return 0;
    return state;
}

void anchor_impact_damage_reset(void)
{
    s_active = 0;
    s_owner = 0;
    s_paused = 0;
    s_encounter = 0;
    s_applying = 0;
    s_boss_before_valid = 0;
    s_boss_before = 0;
    s_mech_before_valid = 0;
    s_mech_before = 0;
    s_hit_read = 0;
    s_hit_count = 0;
    /* Hit sequence numbers remain monotonic across encounter reuse. */
}

void anchor_impact_damage_set_context(int active, int owner, int paused,
                                      unsigned int encounter)
{
    if (!!active != s_active || encounter != s_encounter)
    {
        s_hit_read = 0;
        s_hit_count = 0;
        s_boss_before_valid = 0;
        s_mech_before_valid = 0;
    }
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
    s_encounter = encounter;
}

int anchor_impact_damage_is_shared(void)
{
    return s_active && ANCHOR_BOSS_IMPACT_STAGE_VALID(D_800C7AB2) &&
           D_8020EED0_63A2B0 != 0;
}

int anchor_impact_damage_is_owner(void)
{
    return anchor_impact_damage_is_shared() && s_owner;
}

static void queue_hit(int amount)
{
    unsigned int slot;
    if (s_hit_count >= IMPACT_HIT_QUEUE)
        return;
    s_next_sequence = s_next_sequence == 0x7fffffff ? 1 : s_next_sequence + 1;
    slot = (s_hit_read + s_hit_count) % IMPACT_HIT_QUEUE;
    s_hits[slot].sequence = s_next_sequence;
    s_hits[slot].amount = amount;
    ++s_hit_count;
}

int anchor_impact_damage_take_local_hit(AnchorImpactHit *out)
{
    if (!out || !s_hit_count)
        return 0;
    *out = s_hits[s_hit_read];
    s_hit_read = (s_hit_read + 1u) % IMPACT_HIT_QUEUE;
    --s_hit_count;
    return 1;
}

RECOMP_HOOK("func_801D3954_5FED34")
void anchor_impact_damage_boss_before(int amount)
{
    void *state = impact_state();
    (void)amount;
    s_boss_before_valid = 0;
    if (!state)
        return;
    s_boss_before = *(volatile signed int *)((unsigned char *)state +
                                             IMPACT_STATE_HP);
    s_boss_before_valid = 1;
}

RECOMP_HOOK_RETURN("func_801D3954_5FED34")
void anchor_impact_damage_boss_after(void)
{
    void *state = impact_state();
    int after;
    int delta;
    if (!s_boss_before_valid)
        return;
    s_boss_before_valid = 0;
    if (!state)
        return;
    after = *(volatile signed int *)((unsigned char *)state + IMPACT_STATE_HP);
    if (s_applying || s_paused || s_owner || !s_encounter)
        return;
    delta = s_boss_before - after;
    if (delta <= 0)
        return;
    /* Followers never mutate the shared boss health directly. Capture the
     * native delta, restore the checkpoint value, and forward the hit. */
    *(volatile signed int *)((unsigned char *)state + IMPACT_STATE_HP) =
        s_boss_before;
    if (delta < IMPACT_HIT_MIN)
        delta = IMPACT_HIT_MIN;
    if (delta > IMPACT_HIT_MAX)
        delta = IMPACT_HIT_MAX;
    queue_hit(delta);
}

RECOMP_HOOK("func_801D38A4_5FEC84")
void anchor_impact_damage_mech_before(int amount)
{
    void *state = impact_state();
    (void)amount;
    s_mech_before_valid = 0;
    if (!state)
        return;
    s_mech_before = *(volatile signed int *)((unsigned char *)state +
                                             IMPACT_STATE_MECH_HP);
    s_mech_before_valid = 1;
}

RECOMP_HOOK_RETURN("func_801D38A4_5FEC84")
void anchor_impact_damage_mech_after(void)
{
    void *state = impact_state();
    if (!s_mech_before_valid)
        return;
    s_mech_before_valid = 0;
    if (!state || s_applying || s_paused || s_owner || !s_encounter)
        return;
    /* The shared mech's health is owned by the elected authority. Undo any
     * local reduction so a follower's own hazards cannot desync the pool. */
    *(volatile signed int *)((unsigned char *)state + IMPACT_STATE_MECH_HP) =
        s_mech_before;
}

int anchor_impact_damage_apply(int amount)
{
    void *state;
    if (!anchor_impact_damage_is_owner() || s_applying || s_paused ||
        !s_encounter || amount < IMPACT_HIT_MIN || amount > IMPACT_HIT_MAX)
        return 0;
    state = impact_state();
    if (!state)
        return 0;
    s_applying = 1;
    (void)func_801D3954_5FED34(amount);
    s_applying = 0;
    return 1;
}
