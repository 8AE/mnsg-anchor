/* USA file_13 native boss AI. Shared transport and player behavior live in
 * anchor_impact_native, anchor_impact_boss and anchor_impact_players. */
#include "bosses/impact/anchor_impact_boss.h"
#include "bosses/impact/anchor_impact_players.h"
#ifndef ANCHOR_IMPACT_BOSS_HOST_TEST
#include "platform/modding.h"
#endif
extern void func_802003E0_62B7C0(void *, void *);
extern void func_80200434_62B814(void *, void *);
extern void func_802004D0_62B8B0(void *, void *);
extern void func_80200508_62B8E8(void *, void *);
extern void func_802006BC_62BA9C(void *, void *);
extern void func_802006F8_62BAD8(void *, void *);
extern void func_80200818_62BBF8(void *, void *);
extern void func_80200BA4_62BF84(void *, void *);
extern void func_80200BE8_62BFC8(void *, void *);
extern void func_80201018_62C3F8(void *, void *);
extern void func_80201088_62C468(void *, void *);
extern void func_80201218_62C5F8(void *, void *);
extern void func_802012C8_62C6A8(void *, void *);
extern void func_80201374_62C754(void *, void *);
extern void func_802013D0_62C7B0(void *, void *);
extern void func_8020143C_62C81C(void *, void *);
extern void func_80201598_62C978(void *, void *);
extern void func_80201618_62C9F8(void *, void *);
extern void func_80201684_62CA64(void *, void *);
extern void func_80201770_62CB50(void *, void *);
extern void func_802017C8_62CBA8(void *, void *);
extern void func_802018B4_62CC94(void *, void *);
extern void func_8020190C_62CCEC(void *, void *);
extern void func_80201B38_62CF18(void *, void *);
extern void func_80201C7C_62D05C(void *, void *);
extern void func_80201ECC_62D2AC(void *, void *);
extern void func_80200760_62BB40(void *, void *);
extern void func_802007D4_62BBB4(void *, void *);
extern void func_80201AD4_62CEB4(void *, void *);
extern void func_80201ED8_62D2B8(void *, void *);
extern void func_80201F0C_62D2EC(void *, void *);
extern void func_80200D4C_62C12C(void *, void *);
extern void func_802008D0_62BCB0(void *, void *);

extern void func_80202264_62D644(void *, void *);
extern void func_802023DC_62D7BC(void *, void *);
extern void func_80202598_62D978(void *, void *);
extern void func_8020269C_62DA7C(void *, void *);
extern void func_80202904_62DCE4(void *, void *);
extern void func_80202BE4_62DFC4(void *, void *);
extern void func_80202EA4_62E284(void *, void *);
extern void func_80202F90_62E370(void *, void *);
extern void func_80203154_62E534(void *, void *);
extern void func_8020324C_62E62C(void *, void *);
extern void func_80203424_62E804(void *, void *);
extern void func_802034C4_62E8A4(void *, void *);

/* Gun, beam, two sliding pods, two deck cannons, six indexed rocket tubes.
 * Dead parts remain native tasks with explosion callbacks. A stable semantic
 * slot preserves their HP/AI without sharing parent or model pointers. */
#define BAL_PARTS 12u
#define BAL_PART_WORDS 8u
#define BAL_PART_BASE IB_COMMON_END
#define BAL_END (BAL_PART_BASE + BAL_PARTS*BAL_PART_WORDS)
static const unsigned int bal_models[BAL_PARTS] = {
    0x48017330,0x48017870,0x480137C0,0x48014540,0x180000A4,0x18000154,
    0x480183E0,0x48018960,0x480186A0,0x48017B40,0x48017E20,0x48018100
};
static const unsigned short bal_ids[BAL_PARTS] = {
    0x98,0x98,0x9A,0x9B,0x9C,0x9D,0x6E,0x6F,0x70,0x71,0x71,0x71
};
static AnchorImpactCallback bal_callback(unsigned int slot, unsigned int dead)
{
    switch (slot) {
    case 0: return dead ? func_802023DC_62D7BC : func_80202264_62D644;
    case 1: return dead ? func_8020269C_62DA7C : func_80202598_62D978;
    case 2: return func_80202904_62DCE4;
    case 3: return func_80202BE4_62DFC4;
    case 4: return dead ? func_80202F90_62E370 : func_80202EA4_62E284;
    case 5: return dead ? func_8020324C_62E62C : func_80203154_62E534;
    default: return dead ? func_802034C4_62E8A4 : func_80203424_62E804;
    }
}
static unsigned int bal_phase(void *task, unsigned int slot)
{
    unsigned int cb = IB_U32(task,0x0C)&~0x00800000u;
    if (cb == ((unsigned int)(unsigned long)bal_callback(slot,0)&~0x00800000u)) return 1;
    if (slot != 2 && slot != 3 &&
        cb == ((unsigned int)(unsigned long)bal_callback(slot,1)&~0x00800000u)) return 2;
    return 0;
}
static void bal_parts(void *root, void **parts)
{
    void *children[512];
    unsigned int i, slot, count = anchor_impact_boss_children(root,children,512);
    for (slot = 0; slot < BAL_PARTS; ++slot) parts[slot] = 0;
    for (i = 0; i < count; ++i) {
        void *t = children[i];
        if (!anchor_impact_boss_pointer_valid(IB_PTR(t,0x18)) ||
            !anchor_impact_boss_pointer_valid(IB_PTR(t,0xB4)) ||
            !anchor_impact_boss_pointer_valid(IB_PTR(t,0xB8))) continue;
        for (slot = 0; slot < BAL_PARTS; ++slot) {
            if (IB_U16(t,0x5C) != bal_ids[slot] || !bal_phase(t,slot) ||
                (slot >= 6 && IB_U32(t,0x90) != slot-6)) continue;
            parts[slot] = t; break;
        }
    }
}
static void balberra_capture(void *s, void *t, unsigned int *r)
{
    void *parts[BAL_PARTS];
    unsigned int i;
    anchor_impact_boss_common_capture(s,t,r);
    r[IMP_BOSS_HP] = IB_U32(t,0xAC);
    bal_parts(t,parts);
    for (i = 0; i < BAL_PARTS; ++i) if (parts[i]) {
        void *p = parts[i], *o = IB_PTR(p,0x18);
        unsigned int *w = r+BAL_PART_BASE+i*BAL_PART_WORDS;
        w[0] = bal_phase(p,i) | ((unsigned int)IB_U8(p,0x30)<<8) |
               (IB_U32(o,0x2C) ? 0x10000u : 0);
        w[1] = IB_U32(p,0xAC); w[2] = IB_U32(p,0x7C); w[3] = IB_U32(p,0x80);
        w[4] = IB_U32(p,0x90); w[5] = IB_U32(p,0xB0);
        w[6] = IB_U32(p,0xBC); w[7] = IB_U32(o,0x28);
    }
}
static int balberra_validate(const unsigned int *r)
{
    unsigned int i,j;
    if (!anchor_impact_boss_common_validate(r)) return 0;
    for (i = 0; i < ANCHOR_IMPACT_PRIVATE_WORDS; ++i)
        if (!(0x1710DFu & (1u<<i)) && r[IMP_PRIVATE+i]) return 0;
    for (i = 0; i < BAL_PARTS; ++i) {
        const unsigned int *w = r+BAL_PART_BASE+i*BAL_PART_WORDS;
        unsigned int phase = w[0]&255u;
        if (w[0]&~0x1FF03u || phase > 2 || w[1] > 1000000u ||
            ((i == 2 || i == 3) && phase == 2) ||
            (phase && i >= 6 && w[4] != i-6) ||
            !anchor_impact_boss_finite(w[6]) || !anchor_impact_boss_finite(w[7])) return 0;
        if (!phase) for (j = 0; j < BAL_PART_WORDS; ++j) if (w[j]) return 0;
    }
    for (i = BAL_END; i < ANCHOR_IMPACT_ROOT_WORDS; ++i) if (r[i]) return 0;
    return 1;
}
static void balberra_lifecycle(void *s, const unsigned int *r)
{
    anchor_impact_boss_common_lifecycle(s,r);
    /* 8020407C republishes root+AC to the HUD every frame, including outro. */
    IB_U32(IB_PTR(s,0x1D8),0xAC) = r[IMP_BOSS_HP];
}
static void balberra_apply(void *s, void *t, const unsigned int *r)
{
    void *parts[BAL_PARTS];
    unsigned int i;
    anchor_impact_boss_common_apply(s,t,r);
    bal_parts(t,parts);
    for (i = 0; i < BAL_PARTS; ++i) {
        const unsigned int *w = r+BAL_PART_BASE+i*BAL_PART_WORDS;
        void *p = parts[i], *o;
        if (!p || !(w[0]&255u)) continue;
        o = IB_PTR(p,0x18);
        IB_U32(p,0x0C) = (unsigned int)(unsigned long)bal_callback(i,(w[0]&255u)==2) |
                         (IB_U32(p,0x0C)&0x00800000u);
        IB_U8(p,0x30) = w[0]>>8;
        IB_U32(o,0x2C) = w[0]&0x10000u ? bal_models[i] : 0;
        IB_U32(p,0xAC) = w[1]; IB_U32(p,0x7C) = w[2]; IB_U32(p,0x80) = w[3];
        IB_U32(p,0x90) = w[4]; IB_U32(p,0xB0) = w[5];
        IB_U32(p,0xBC) = w[6]; IB_U32(o,0x28) = w[7];
    }
}
static const unsigned short balberra_cues[] = {1,7,9,0x4C};


static AnchorImpactBossProfile balberra_profile;
const AnchorImpactBossProfile *anchor_impact_balberra_profile(void)
{
    static int initialized;
    if (initialized) return &balberra_profile;
    initialized = 1;
#define profile balberra_profile
    profile.task_id = 0x78;
    profile.private_mask = 0x1710DFu;
    profile.capture = balberra_capture;
    profile.validate = balberra_validate;
    profile.apply_lifecycle = balberra_lifecycle;
    profile.apply = balberra_apply;
    profile.sound_cues = balberra_cues;
    profile.sound_cue_count = sizeof(balberra_cues)/sizeof(balberra_cues[0]);
    profile.phases[27] = func_80200760_62BB40;
    profile.phases[28] = func_802007D4_62BBB4;
    profile.phases[29] = func_80201AD4_62CEB4;
    profile.phases[30] = func_80201ED8_62D2B8;
    profile.phases[31] = func_80201F0C_62D2EC;
    profile.phases[1] = func_802003E0_62B7C0;
    profile.phases[2] = func_80200434_62B814;
    profile.phases[3] = func_802004D0_62B8B0;
    profile.phases[4] = func_80200508_62B8E8;
    profile.phases[5] = func_802006BC_62BA9C;
    profile.phases[6] = func_802006F8_62BAD8;
    profile.phases[7] = func_80200818_62BBF8;
    profile.phases[8] = func_80200BA4_62BF84;
    profile.phases[9] = func_80200BE8_62BFC8;
    profile.phases[10] = func_80201018_62C3F8;
    profile.phases[11] = func_80201088_62C468;
    profile.phases[12] = func_80201218_62C5F8;
    profile.phases[13] = func_802012C8_62C6A8;
    profile.phases[14] = func_80201374_62C754;
    profile.phases[15] = func_802013D0_62C7B0;
    profile.phases[16] = func_8020143C_62C81C;
    profile.phases[17] = func_80201598_62C978;
    profile.phases[18] = func_80201618_62C9F8;
    profile.phases[19] = func_80201684_62CA64;
    profile.phases[20] = func_80201770_62CB50;
    profile.phases[21] = func_802017C8_62CBA8;
    profile.phases[22] = func_802018B4_62CC94;
    profile.phases[23] = func_8020190C_62CCEC;
    profile.phases[24] = func_80201B38_62CF18;
    profile.phases[25] = func_80201C7C_62D05C;
    profile.phases[26] = func_80201ECC_62D2AC;
    profile.phases[32] = func_80200D4C_62C12C;
    profile.phases[33] = func_802008D0_62BCB0;
#undef profile
    return &balberra_profile;
}

RECOMP_HOOK("func_80200200_62B5E0")
void anchor_impact_bind_3(void *task)
{ (void)anchor_impact_native_bind(task, 3); }
