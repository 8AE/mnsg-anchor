#include "anchor_impact_native.h"
#include "anchor_boss_arenas.h"
#include "anchor_boss_invite_world.h"

#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
#include "modding.h"
#include "recomputils.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#endif

/* Dedicated file_13 Impact-battle overlay (USA). The shared battle-state
 * pointer is D_8020EED0_63A2B0; the encounter selector is system+0x3ADF4.
 * All four bosses (1 Kashiwagi, 2 Thaisamba, 3 Balberra, 4 D'Etoile) reuse it. */
extern unsigned short D_800C7AB2;
extern unsigned char *D_8015C5C8_15D1C8;
extern void *D_8020EED0_63A2B0;
extern void *D_8016DAB4_16E6B4;

extern void func_801E4800_60FBE0(void *task);
extern void func_801EF2E0_61A6C0(void *task);
extern void func_80200200_62B5E0(void *task);
extern void func_801FAEB0_626290(void *task);

#define IMPACT_STATE_HP 0x60
#define IMPACT_STATE_AMMO 0x64
#define IMPACT_STATE_MECH_HP 0x68
#define IMPACT_STATE_PAUSE 0x2C0
#define IMPACT_STATE_CLOCK 0x2C8
#define IMPACT_STATE_ROOT 0x1D8
#define IMPACT_STATE_MODEL 0x1E0

#define IMPACT_ENCOUNTER_OFFSET 0x3ADF4u
#define IMPACT_CALLBACK_DISABLED 0x00800000ul
/* file_13 is linked at 0x801CB460 (size 0x43A70). An overlay AI callback is
 * accepted only from this executable range, never a caller-supplied pointer. */
#define IMPACT_CODE_BASE 0x801CB460u
#define IMPACT_CODE_END 0x8020EED0u
#define IMPACT_REMOVE_PENDING 0x00000002u

#define IMPACT_ID_KASHIWAGI 0x50u
#define IMPACT_ID_THAISAMBA 0x5Au
#define IMPACT_ID_BALBERRA 0x78u
#define IMPACT_ID_DETOILE 0x64u

static void *s_state;
static void *s_task;
static void *s_object;
static unsigned int s_encounter;
static unsigned int s_stage;
static unsigned int s_id;
/* Monotonic fallback encounter visit for modes (title-menu boss rush) that
 * never pass through the ordinary room loader and so report visit 0. */
static unsigned int s_visit_counter;
static int s_bound;
static int s_active;
static int s_owner;
static int s_paused;
static int s_pending_valid;
static AnchorImpactNativeSnapshot s_pending;

#ifndef ANCHOR_IMPACT_READ_U8
#define ANCHOR_IMPACT_READ_U8(p, o) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_U8(p, o, v) \
    (*(volatile unsigned char *)((unsigned char *)(p) + (o)) = (unsigned char)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_U16
#define ANCHOR_IMPACT_READ_U16(p, o) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_U16(p, o, v) \
    (*(volatile unsigned short *)((unsigned char *)(p) + (o)) = (unsigned short)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_U32
#define ANCHOR_IMPACT_READ_U32(p, o) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_U32(p, o, v) \
    (*(volatile unsigned int *)((unsigned char *)(p) + (o)) = (unsigned int)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_I32
#define ANCHOR_IMPACT_READ_I32(p, o) \
    (*(volatile signed int *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_I32(p, o, v) \
    (*(volatile signed int *)((unsigned char *)(p) + (o)) = (signed int)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_F32
#define ANCHOR_IMPACT_READ_F32(p, o) \
    (*(volatile float *)((unsigned char *)(p) + (o)))
#define ANCHOR_IMPACT_WRITE_F32(p, o, v) \
    (*(volatile float *)((unsigned char *)(p) + (o)) = (float)(v))
#endif
#ifndef ANCHOR_IMPACT_READ_PTR
#define ANCHOR_IMPACT_READ_PTR(p, o) \
    (*(void *volatile *)((unsigned char *)(p) + (o)))
#endif
#ifndef ANCHOR_IMPACT_AI
#define ANCHOR_IMPACT_AI(p) \
    (*(unsigned long volatile *)((unsigned char *)(p) + 0x0C))
#define ANCHOR_IMPACT_SET_AI(p, v) \
    (*(unsigned long volatile *)((unsigned char *)(p) + 0x0C) = \
         (unsigned long)(v))
#endif
#ifndef ANCHOR_IMPACT_POST_DISABLED
#define ANCHOR_IMPACT_POST_DISABLED(p) \
    ((ANCHOR_IMPACT_READ_U32((p), 0x10) & (unsigned int)IMPACT_CALLBACK_DISABLED) != 0u)
#endif

static int pointer_valid(const void *pointer)
{
#ifdef ANCHOR_IMPACT_NATIVE_HOST_TEST
    return pointer != 0;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return (address & 3u) == 0 &&
           address >= 0x80001000u && address < 0x80800000u;
#endif
}

static unsigned int current_encounter(void)
{
    if (!D_8015C5C8_15D1C8)
        return 0;
    return ANCHOR_IMPACT_READ_U16(D_8015C5C8_15D1C8, IMPACT_ENCOUNTER_OFFSET);
}

/* Stages that run a live Impact battle: the story intro/minigame/boss stages
 * and the dedicated title-menu boss-rush stage 0x0260, which plays all four
 * bosses in sequence. */
#define IMPACT_BOSS_RUSH_STAGE 0x0260u

static int stage_is_impact(unsigned int stage)
{
    return ANCHOR_BOSS_IMPACT_STAGE_VALID(stage) ||
           stage == IMPACT_BOSS_RUSH_STAGE;
}

static int task_id_valid(unsigned int id)
{
    return id == IMPACT_ID_KASHIWAGI || id == IMPACT_ID_THAISAMBA ||
           id == IMPACT_ID_BALBERRA || id == IMPACT_ID_DETOILE;
}

static int bound_live(void)
{
    void *state;
    unsigned int id;
    if (!s_bound || !s_task || !pointer_valid(s_task))
        return 0;
    state = D_8020EED0_63A2B0;
    if (!pointer_valid(state))
        return 0;
    /* The battle stage can change within one Impact sequence (intro cutscene
     * -> minigame -> boss), so always follow the current stage rather than a
     * value captured at bind time. */
    if (!stage_is_impact(D_800C7AB2))
        return 0;
    if (current_encounter() != s_encounter)
        return 0;
    if (ANCHOR_IMPACT_READ_PTR(state, IMPACT_STATE_ROOT) != s_task)
        return 0;
    id = ANCHOR_IMPACT_READ_U16(s_task, 0x5C);
    if (!task_id_valid(id))
        return 0;
    if (ANCHOR_IMPACT_READ_U32(s_task, 0x68) & IMPACT_REMOVE_PENDING)
        return 0;
    if (!pointer_valid(s_object))
    {
        s_object = ANCHOR_IMPACT_READ_PTR(s_task, 0x18);
        if (!pointer_valid(s_object))
            s_object = ANCHOR_IMPACT_READ_PTR(state, IMPACT_STATE_MODEL);
    }
    s_state = state;
    s_id = id;
    s_stage = D_800C7AB2;
    return pointer_valid(s_object);
}

int anchor_impact_native_root_live(void)
{
    return bound_live();
}

int anchor_impact_native_ready(void)
{
    return s_active && bound_live();
}

int anchor_impact_native_snapshot_ready(void)
{
    return bound_live();
}

unsigned int anchor_impact_native_visit(void)
{
    /* Read live so a bind that races the Impact-sequence start cannot pin a
     * stale visit. The sequence visit is stable for the whole sequence. A
     * mode that never reports one (boss rush) falls back to a per-boss value,
     * which keeps the coordinator's required positive visit and re-elects on
     * each new boss. */
    unsigned int visit = anchor_boss_invite_world_visit();
    return visit ? visit : s_visit_counter;
}

unsigned int anchor_impact_native_encounter(void)
{
    return s_encounter;
}

unsigned int anchor_impact_native_stage(void)
{
    return s_stage;
}

void anchor_impact_native_reset(void)
{
    s_state = 0;
    s_task = 0;
    s_object = 0;
    s_encounter = 0;
    s_stage = 0;
    s_id = 0;
    s_bound = 0;
    s_active = 0;
    s_owner = 0;
    s_paused = 0;
    s_pending_valid = 0;
}

void anchor_impact_native_set_role(int active, int owner, int paused)
{
    if (!active)
        s_pending_valid = 0;
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
}

/* Bind the boss root from its own file_13 initializer entry. The initializer
 * body has not run yet, so the task ID, model object and battle-state root
 * pointer are all resolved lazily by bound_live() on later frames. The four
 * initializers are boss-specific, so the expected encounter is passed in. */
int anchor_impact_native_bind(void *task, unsigned int encounter)
{
    if (!pointer_valid(task) || encounter < 1u || encounter > 4u)
        return 0;
    anchor_impact_native_reset();
    s_visit_counter = s_visit_counter >= 0x7fffffffu ? 1u : s_visit_counter + 1u;
    s_task = task;
    s_encounter = encounter;
    s_stage = D_800C7AB2;
    s_bound = 1;
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
    recomp_printf("[Impact] bind encounter=%u stage=%u task=%u\n",
                  encounter, (unsigned int)D_800C7AB2,
                  (unsigned int)(unsigned long)task);
#endif
    return 1;
}

#define IMPACT_BIND_HOOK(address, encounter) \
    RECOMP_HOOK(address) \
    void anchor_impact_bind_##encounter(void *task) \
    { \
        (void)anchor_impact_native_bind(task, encounter); \
    }
IMPACT_BIND_HOOK("func_801E4800_60FBE0", 1)
IMPACT_BIND_HOOK("func_801EF2E0_61A6C0", 2)
IMPACT_BIND_HOOK("func_80200200_62B5E0", 3)
IMPACT_BIND_HOOK("func_801FAEB0_626290", 4)
#undef IMPACT_BIND_HOOK

static int snapshot_valid(const AnchorImpactNativeSnapshot *snapshot)
{
    const unsigned int *r;
    if (!snapshot)
        return 0;
    r = snapshot->root;
    if (snapshot->encounter != s_encounter || snapshot->encounter < 1u ||
        snapshot->encounter > 4u ||
        snapshot->stage != (unsigned int)D_800C7AB2 ||
        !stage_is_impact(snapshot->stage))
        return 0;
    /* Only the shared health/ammunition words are published. Keep the bounds
     * generous: the values are signed 32-bit on the wire. */
    if (r[IMP_BOSS_HP] > 1000000u || r[IMP_MECH_HP] > 1000000u ||
        r[IMP_AMMO] > 1000000u)
        return 0;
    return 1;
}

int anchor_impact_native_capture(AnchorImpactNativeSnapshot *snapshot)
{
    unsigned int *r;
    if (!snapshot || !bound_live())
        return 0;
    r = snapshot->root;
    r[IMP_BOSS_HP] = (unsigned int)ANCHOR_IMPACT_READ_I32(
        s_state, IMPACT_STATE_HP);
    r[IMP_AMMO] = (unsigned int)ANCHOR_IMPACT_READ_I32(
        s_state, IMPACT_STATE_AMMO);
    r[IMP_MECH_HP] = (unsigned int)ANCHOR_IMPACT_READ_I32(
        s_state, IMPACT_STATE_MECH_HP);
    r[IMP_PAUSE] = ANCHOR_IMPACT_READ_U8(s_state, IMPACT_STATE_PAUSE);
    r[IMP_CLOCK] = ANCHOR_IMPACT_READ_U32(s_state, IMPACT_STATE_CLOCK);
    snapshot->encounter = s_encounter;
    snapshot->stage = s_stage;
    return snapshot_valid(snapshot);
}

static void apply_root(const AnchorImpactNativeSnapshot *snapshot)
{
    const unsigned int *r = snapshot->root;
    ANCHOR_IMPACT_WRITE_I32(s_state, IMPACT_STATE_HP, (int)r[IMP_BOSS_HP]);
    ANCHOR_IMPACT_WRITE_I32(s_state, IMPACT_STATE_AMMO, (int)r[IMP_AMMO]);
    ANCHOR_IMPACT_WRITE_I32(s_state, IMPACT_STATE_MECH_HP,
                           (int)r[IMP_MECH_HP]);
    /* Deliberately do not replace the boss AI callback, transform or combat
     * pause here. Followers keep running their own spawn/introduction/attack
     * AI; the authority owns the shared health and ammunition. This avoids
     * freezing or suppressing the boss's multi-frame introduction. */
}

int anchor_impact_native_apply(const AnchorImpactNativeSnapshot *snapshot)
{
    if (!snapshot_valid(snapshot) || !bound_live())
        return 0;
    s_pending = *snapshot;
    s_pending_valid = 1;
    return 1;
}

/* The battle-state root pointer is authoritative. This late bind runs every
 * frame, so a missing/incorrect initializer-hook argument cannot leave the
 * encounter unbound, and a new boss re-binds when its root replaces the old. */
void anchor_impact_native_tick(void)
{
    void *state = D_8020EED0_63A2B0;
    void *root;
    unsigned int encounter;
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
    static unsigned int s_diag;
#endif
    if (!pointer_valid(state) || !stage_is_impact(D_800C7AB2))
        return;
    root = ANCHOR_IMPACT_READ_PTR(state, IMPACT_STATE_ROOT);
    encounter = current_encounter();
#ifndef ANCHOR_IMPACT_NATIVE_HOST_TEST
    if ((++s_diag % 240u) == 0u)
        recomp_printf("[Impact] diag stage=%u sel=%u root=%u id=%u hp=%d mhp=%d ammo=%d bound=%d live=%d\n",
                      (unsigned int)D_800C7AB2, encounter,
                      (unsigned int)(unsigned long)root,
                      pointer_valid(root)
                          ? (unsigned int)ANCHOR_IMPACT_READ_U16(root, 0x5C) : 0u,
                      ANCHOR_IMPACT_READ_I32(state, IMPACT_STATE_HP),
                      ANCHOR_IMPACT_READ_I32(state, IMPACT_STATE_MECH_HP),
                      ANCHOR_IMPACT_READ_I32(state, IMPACT_STATE_AMMO),
                      s_bound, s_bound ? bound_live() : 0);
#endif
    if (!pointer_valid(root) || encounter < 1u || encounter > 4u)
        return;
    if (!s_bound || s_task != root || s_encounter != encounter ||
        s_stage != D_800C7AB2)
    {
        (void)anchor_impact_native_bind(root, encounter);
    }
}

int anchor_impact_native_world_paused(void)
{
    if (!anchor_impact_native_ready())
        return 0;
    /* Deliberately exclude s_paused: it is a coordinator role flag, not a
     * native pause. Feeding it back here would latch the encounter paused
     * forever during election (s_paused includes !role), which silently
     * disables hit delivery and checkpoint adoption. */
    return (ANCHOR_IMPACT_AI(s_task) & IMPACT_CALLBACK_DISABLED) != 0;
}

RECOMP_HOOK("func_80034734_35334")
void anchor_impact_native_scheduler_begin(void)
{
    if (!s_active)
        return;
    if (s_pending_valid && bound_live())
    {
        apply_root(&s_pending);
        s_pending_valid = 0;
    }
    /* This implementation deliberately never freezes the boss AI. Holding it
     * races the boss's multi-frame spawn/introduction (and any transient
     * scheduler pause mask), and a failed restore would strand the AI as a
     * no-op. Followers keep running their own AI; the authority owns the
     * shared health, ammunition and clock. */
}
