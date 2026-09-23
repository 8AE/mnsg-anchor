#include "bosses/impact/anchor_impact_boss.h"
#include "player/anchor_remote_model_pool.h"

const AnchorImpactBossProfile *anchor_impact_boss_profile(unsigned int encounter)
{
    switch (encounter) {
    case 1: return anchor_impact_kashiwagi_profile();
    case 2: return anchor_impact_taisamba_profile();
    case 3: return anchor_impact_balberra_profile();
    case 4: return anchor_impact_detoile_profile();
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
    for (e = 1; e <= 4; ++e) {
        const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
        if (p->reel && callback == ((unsigned int)(unsigned long)p->reel & ~0x00800000u)) return 1;
    }
    return 0;
}
int anchor_impact_boss_sound_valid(unsigned int e, unsigned int cue)
{
    unsigned int i;
    const AnchorImpactBossProfile *p = anchor_impact_boss_profile(e);
    if (p) for (i = 0; i < p->sound_cue_count; ++i)
        if (cue == p->sound_cues[i]) return 1;
    return 0;
}

extern void *D_8020EF40_63A320;
int anchor_impact_boss_finite(unsigned int w)
{ return (w & 0x7F800000u) != 0x7F800000u; }

void anchor_impact_boss_aux_capture(unsigned int *r)
{
    unsigned int i;
    void *aux = D_8020EF40_63A320;
    if (!anchor_impact_boss_pointer_valid(aux)) return;
    r[IMP_AUX_KIND] = 2;
    for (i = 0; i < 5; ++i) r[IMP_AUX_DATA+i] = 0;
    for (i = 0; i < 15; ++i)
        r[IMP_AUX_DATA+i/4] |= (unsigned int)IB_U8(aux,4+i) << (24-(i%4)*8);
    for (i = 0; i < 3; ++i)
        r[IMP_AUX_DATA+4] |= (unsigned int)IB_U8(aux,0x815+i) << (24-i*8);
}
void anchor_impact_boss_aux_apply(const unsigned int *r)
{
    unsigned int i;
    void *aux = D_8020EF40_63A320;
    if (r[IMP_AUX_KIND] != 2 || !anchor_impact_boss_pointer_valid(aux)) return;
    /* +0 is a local pointer. +14..+813 is D'Etoile's 64-entry pose history,
     * with the local write index at +814. Neither is a wire field. */
    for (i = 0; i < 15; ++i) IB_U8(aux,4+i) = r[IMP_AUX_DATA+i/4] >> (24-(i%4)*8);
    for (i = 0; i < 3; ++i) IB_U8(aux,0x815+i) = r[IMP_AUX_DATA+4] >> (24-i*8);
}
void anchor_impact_boss_common_capture(void *s, void *t, unsigned int *r)
{
    unsigned int i;
    anchor_impact_boss_aux_capture(r);
    for (i = 0; i < 6; ++i) r[IB_WORLD+i] = IB_U32(s,0x178+4*i);
    r[IB_ASCEND] = IB_U8(s,0x2C5); r[IB_REEL] = IB_U32(s,0x70);
    r[IB_PLAYER_ACTION] = IB_U8(s,0x14E); r[IB_GRAB_TIMER] = IB_U8(s,0x14F);
    r[IB_LATCH] = IB_PTR(s,0x174) == t; r[IB_DEFEATED] = IB_U8(s,0x2C4);
    r[IB_COLLISION_KIND] = IB_U8(t,0x30); r[IB_COLLISION_MASK] = IB_U32(t,0x48);
    r[IB_ATTACK_KIND] = IB_U8(t,0x4C);
}
int anchor_impact_boss_common_validate(const unsigned int *r)
{
    unsigned int i;
    if (r[IMP_AUX_KIND] != 0 && r[IMP_AUX_KIND] != 2) return 0;
    if ((r[IMP_AUX_DATA+3]&255u) || (r[IMP_AUX_DATA+4]&255u) || r[IMP_AUX_DATA+5]) return 0;
    for (i = 0; i < 6; ++i) if (!anchor_impact_boss_finite(r[IB_WORLD+i])) return 0;
    return r[IB_ASCEND] <= 1 && r[IB_REEL] <= 100 && r[IB_PLAYER_ACTION] <= 255 &&
        r[IB_GRAB_TIMER] <= 255 && r[IB_LATCH] <= 1 && r[IB_DEFEATED] <= 1 &&
        r[IB_COLLISION_KIND] <= 255 && r[IB_ATTACK_KIND] <= 255;
}
void anchor_impact_boss_common_lifecycle(void *s, const unsigned int *r)
{ IB_U8(s,0x2C4) = r[IB_DEFEATED]; }
void anchor_impact_boss_common_apply(void *s, void *t, const unsigned int *r)
{
    unsigned int i;
    anchor_impact_boss_aux_apply(r);
    for (i = 0; i < 6; ++i) IB_U32(s,0x178+4*i) = r[IB_WORLD+i];
    IB_U8(s,0x2C5) = r[IB_ASCEND]; IB_U32(s,0x70) = r[IB_REEL];
    IB_U8(s,0x14E) = r[IB_PLAYER_ACTION]; IB_U8(s,0x14F) = r[IB_GRAB_TIMER];
    IB_PTR(s,0x174) = r[IB_LATCH] ? t : 0;
    IB_U8(t,0x30) = r[IB_COLLISION_KIND]; IB_U32(t,0x48) = r[IB_COLLISION_MASK];
    IB_U8(t,0x4C) = r[IB_ATTACK_KIND];
}

unsigned int anchor_impact_boss_children(void *root, void **out, unsigned int capacity)
{
    void *task = root, *next;
    unsigned int n = 0, depth = IB_U16(root,0x20), traversed = 0;
    while (anchor_impact_boss_pointer_valid(next = IB_PTR(task,0)) && next != root &&
           IB_PTR(next,4) == task && IB_U16(next,0x20) > depth && traversed++ < 512) {
        task = next;
        if (IB_U32(task,0x68)&2u) continue;
        if (n == capacity) break;
        out[n++] = task;
    }
    return n;
}
void anchor_impact_boss_carry_capture(void *t, unsigned int *r)
{
    unsigned int i;
    void *o;
    for (i = 0; i < 8; ++i) r[i] = 0;
    if (!anchor_impact_boss_pointer_valid(t) ||
        !anchor_impact_boss_pointer_valid(o = IB_PTR(t,0x18))) return;
    r[0] = 1;
    for (i = 0; i < 3; ++i) { r[1+i] = IB_U32(o,8+4*i); r[4+i] = IB_U32(t,0x70+4*i); }
    r[7] = IB_U16(o,0x16);
}
int anchor_impact_boss_carry_validate(const unsigned int *r)
{
    unsigned int i;
    for (i = 1; i < 7; ++i) if (!anchor_impact_boss_finite(r[i])) return 0;
    return r[0] <= 1 && r[7] <= 65535;
}
void *anchor_impact_boss_carry_view(AnchorImpactCarryView *v, const unsigned int *r)
{
    unsigned int i;
    IB_PTR(v->task,0x18) = v->object;
    for (i = 0; i < 3; ++i) { IB_U32(v->object,8+4*i) = r[1+i]; IB_U32(v->task,0x70+4*i) = r[4+i]; }
    IB_U16(v->object,0x16) = r[7];
    return v->task;
}
