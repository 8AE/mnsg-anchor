#include "anchor_impact_boss.h"
#include "anchor_remote_model_pool.h"

const AnchorImpactBossProfile *anchor_impact_boss_profile(unsigned int encounter)
{
    switch (encounter) {
    case 1: return anchor_impact_kashiwagi_profile();
    case 2: return anchor_impact_taisamba_profile();
    default: return 0;
    }
}

int anchor_impact_boss_pointer_valid(const void *p)
{
#ifdef ANCHOR_IMPACT_BOSS_HOST_TEST
    return p != 0;
#else
    unsigned int a = (unsigned int)(unsigned long)p;
    return !(a & 3u) && ((a >= 0x80001000u && a < 0x80800000u) ||
                        anchor_remote_model_pool_contains(p));
#endif
}

void anchor_impact_boss_capture(unsigned int e, void *s, void *t, unsigned int *r)
{
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
    if (p && p->capture) p->capture(s,t,r);
}

int anchor_impact_boss_validate(unsigned int e, const unsigned int *r)
{
    unsigned int i;
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
    if (p && p->validate) return p->validate(r);
    if (r[IMP_AUX_KIND] == 1) return 0;
    for (i = 0; i < ANCHOR_IMPACT_BOSS_WORDS; ++i)
        if (r[IMP_BOSS_DATA+i]) return 0;
    return 1;
}

void anchor_impact_boss_apply(unsigned int e, void *s, void *t, const unsigned int *r)
{
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
    if (p && p->apply) p->apply(s,t,r);
}

void anchor_impact_boss_apply_lifecycle(unsigned int e, void *s, const unsigned int *r)
{
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
    if (p && p->apply_lifecycle) p->apply_lifecycle(s,r);
}

int anchor_impact_boss_is_reel_callback(unsigned int callback)
{
    unsigned int e;
    for (e = 1; e <= 2; ++e) {
        const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
        if (callback == ((unsigned int)(unsigned long)p->reel & ~0x00800000u)) return 1;
    }
    return callback == 0x801FED3Cu; /* existing Balberra integration */
}
int anchor_impact_boss_sound_valid(unsigned int e, unsigned int cue)
{
    unsigned int i;
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
    if (p) for (i = 0; i < p->sound_cue_count; ++i)
        if (cue == p->sound_cues[i]) return 1;
    return 0;
}
