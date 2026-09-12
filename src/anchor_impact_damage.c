#include "anchor_impact_damage.h"
#include "anchor_boss_arenas.h"

#ifndef ANCHOR_IMPACT_DAMAGE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_PATCH
#endif

extern unsigned short D_800C7AB2;
extern void *D_8020EED0_63A2B0;
extern int func_801D3954_5FED34(int amount);
extern int func_801D38A4_5FEC84(int amount);
extern int func_801D36CC_5FEAAC(void);
extern void func_801D3894_5FEC74(void);
#ifndef IMPACT_DAMAGE_PTR
#define IMPACT_DAMAGE_PTR(p,o) (*(void **)((unsigned char *)(p)+(o)))
#endif

#define IMPACT_STATE_HP 0x60
#define IMPACT_STATE_MECH_HP 0x68
#define IMPACT_HIT_MIN 1
#define IMPACT_HIT_MAX 255

static int s_active;
static int s_owner;
static int s_paused;
static unsigned int s_encounter;
static int s_applying;
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

}

void anchor_impact_damage_set_context(int active, int owner, int paused,
                                      unsigned int encounter)
{
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
    s_encounter = encounter;
}

int anchor_impact_damage_is_shared(void)
{
    return s_active &&
           (ANCHOR_BOSS_IMPACT_STAGE_VALID(D_800C7AB2) ||
            D_800C7AB2 == 0x0260u) &&
           D_8020EED0_63A2B0 != 0;
}

int anchor_impact_damage_is_owner(void)
{
    return anchor_impact_damage_is_shared() && s_owner;
}

/* All shared controls execute on the owner. A follower's speculative native
 * collision must not damage the pool or become an acknowledged second hit. */
int anchor_impact_damage_take_local_hit(AnchorImpactHit *out)
{
    (void)out;
    return 0;
}

/* Verified USA native leaf routines: subtract, clamp, return the stored HP.
 * A return hook cannot correct the native return register: the caller could
 * enter defeat with zero even after HP was restored. Gate before subtraction
 * and preserve both native state and return semantics outside shared mode. */
static int apply_health(int offset, int amount, int maximum)
{
    void *state = D_8020EED0_63A2B0;
    volatile signed int *hp;
    int result;
    if (!state_pointer_valid(state)) return 0;
    hp = (volatile signed int *)((unsigned char *)state + offset);
    if (anchor_impact_damage_is_shared() &&
        (s_paused || !s_owner) && !s_applying)
        return *hp;
    /* Match the native 32-bit subtraction without signed-overflow UB. */
    result = (int)((unsigned int)*hp - (unsigned int)amount);
    if (maximum && result > maximum) result = maximum;
    if (result < 1) result = 0;
    *hp = result;
    return result;
}

RECOMP_PATCH int func_801D3954_5FED34(int amount)
{
    return apply_health(IMPACT_STATE_HP, amount, 0);
}

RECOMP_PATCH int func_801D38A4_5FEC84(int amount)
{
    return apply_health(IMPACT_STATE_MECH_HP, amount, 999);
}

/* Balberra stores HP in its task, then republishes it to the shared block.
 * Preserve the native hit-kind return and flash timing for the authority. */
RECOMP_PATCH unsigned char func_8020451C_62F8FC(void *task)
{
    unsigned char *p = task, kind;
    void *hit = IMPACT_DAMAGE_PTR(task,0x38);
    int hp;
    if ((anchor_impact_damage_is_shared() && (s_paused || !s_owner)) ||
        (*(unsigned int *)(p+0x64) & 0x1000u) || !hit) {
        func_801D3894_5FEC74(); return 0;
    }
    kind = *((unsigned char *)hit+0x4C);
    switch (kind) {
    case 0x32: case 0x33: case 0x3C: case 0x3D: case 0x3E:
    case 0x46: case 0x50: case 0x51: case 0x5B:
        *(int *)(p+0xB0) = 40; break;
    case 0x5A: *(int *)(p+0xB0) = 200; break;
    case 0x64: func_801D3894_5FEC74(); return 0;
    }
    hp = (int)(*(unsigned int *)(p+0xAC) - (unsigned int)func_801D36CC_5FEAAC());
    *(int *)(p+0xAC) = hp < 1 ? 0 : hp;
    func_801D3894_5FEC74(); return kind;
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
