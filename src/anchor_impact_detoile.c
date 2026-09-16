/* USA file_13 native boss AI. Shared transport and player behavior live in
 * anchor_impact_native, anchor_impact_boss and anchor_impact_players. */
#include "anchor_impact_boss.h"
#include "anchor_impact_players.h"
#ifndef ANCHOR_IMPACT_BOSS_HOST_TEST
#include "modding.h"
#endif
extern void func_801FB0CC_6264AC(void *, void *);
extern void func_801FB120_626500(void *, void *);
extern void func_801FB274_626654(void *, void *);
extern void func_801FB3E8_6267C8(void *, void *);
extern void func_801FB50C_6268EC(void *, void *);
extern void func_801FB658_626A38(void *, void *);
extern void func_801FB6A0_626A80(void *, void *);
extern void func_801FB73C_626B1C(void *, void *);
extern void func_801FB814_626BF4(void *, void *);
extern void func_801FB934_626D14(void *, void *);
extern void func_801FB9D4_626DB4(void *, void *);
extern void func_801FBA34_626E14(void *, void *);
extern void func_801FBAD8_626EB8(void *, void *);
extern void func_801FBB74_626F54(void *, void *);
extern void func_801FBBBC_626F9C(void *, void *);
extern void func_801FBC2C_62700C(void *, void *);
extern void func_801FBCA8_627088(void *, void *);
extern void func_801FBD44_627124(void *, void *);
extern void func_801FBD6C_62714C(void *, void *);
extern void func_801FC460_627840(void *, void *);
extern void func_801FC510_6278F0(void *, void *);
extern void func_801FC638_627A18(void *, void *);
extern void func_801FC75C_627B3C(void *, void *);
extern void func_801FC8A4_627C84(void *, void *);
extern void func_801FC9E4_627DC4(void *, void *);
extern void func_801FCA6C_627E4C(void *, void *);
extern void func_801FCAB4_627E94(void *, void *);
extern void func_801FCB88_627F68(void *, void *);
extern void func_801FCC64_628044(void *, void *);
extern void func_801FCD1C_6280FC(void *, void *);
extern void func_801FCDAC_62818C(void *, void *);
extern void func_801FCE74_628254(void *, void *);
extern void func_801FCF68_628348(void *, void *);
extern void func_801FD028_628408(void *, void *);
extern void func_801FD088_628468(void *, void *);
extern void func_801FD114_6284F4(void *, void *);
extern void func_801FD1D0_6285B0(void *, void *);
extern void func_801FD30C_6286EC(void *, void *);
extern void func_801FD46C_62884C(void *, void *);
extern void func_801FD500_6288E0(void *, void *);
extern void func_801FD634_628A14(void *, void *);
extern void func_801FD724_628B04(void *, void *);
extern void func_801FD840_628C20(void *, void *);
extern void func_801FD888_628C68(void *, void *);
extern void func_801FD8F0_628CD0(void *, void *);
extern void func_801FDA20_628E00(void *, void *);
extern void func_801FDB70_628F50(void *, void *);
extern void func_801FDC84_629064(void *, void *);
extern void func_801FDD84_629164(void *, void *);
extern void func_801FDE94_629274(void *, void *);
extern void func_801FE008_6293E8(void *, void *);
extern void func_801FE050_629430(void *, void *);
extern void func_801FE0C4_6294A4(void *, void *);
extern void func_801FE1E8_6295C8(void *, void *);
extern void func_801FE288_629668(void *, void *);
extern void func_801FE308_6296E8(void *, void *);
extern void func_801FE3C0_6297A0(void *, void *);
extern void func_801FE47C_62985C(void *, void *);
extern void func_801FE4D0_6298B0(void *, void *);
extern void func_801FE66C_629A4C(void *, void *);
extern void func_801FE964_629D44(void *, void *);
extern void func_801FED3C_62A11C(void *, void *);
extern void func_801FF294_62A674(void *, void *);
extern void func_801FF380_62A760(void *, void *);
extern void func_801FF460_62A840(void *, void *);
extern void func_801FF4CC_62A8AC(void *, void *);
extern void func_801FF528_62A908(void *, void *);
extern void func_801FF730_62AB10(void *, void *);
extern void func_801FF798_62AB78(void *, void *);
extern void func_801FF8B4_62AC94(void *, void *);
extern void func_801FF944_62AD24(void *, void *);
extern void func_801FF9EC_62ADCC(void *, void *);
extern void func_801FFA40_62AE20(void *, void *);
extern unsigned int D_8020B3F0_6367D0[];
extern unsigned int D_8020B3F8_6367D8[];
extern unsigned int D_8020B400_6367E0[];
extern unsigned int D_8020B408_6367E8[];
extern unsigned int D_8020B410_6367F0[];
extern unsigned int D_8020B418_6367F8[];
extern unsigned int D_8020B420_636800[];
extern unsigned int D_8020B428_636808[];
extern unsigned int D_8020B430_636810[];
extern unsigned int D_8020B438_636818[];
extern unsigned int D_8020B440_636820[];
extern unsigned int D_8020B448_636828[];
extern unsigned int D_8020B450_636830[];
extern unsigned int D_8020B458_636838[];
extern void func_801FB67C_626A5C(void *, void *);
extern void func_801FB6E8_626AC8(void *, void *);
extern void func_801FE8B4_629C94(void *, void *);

extern void *D_8020EED0_63A2B0;
extern void func_801FC3FC_6277DC(void *, void *);
extern void func_802065B8_631998(void *, void *);
extern void func_8020679C_631B7C(void *, void *);
enum { DT_CARRY = IB_COMMON_END, DT_SHIELD = DT_CARRY+8, DT_END = DT_SHIELD+11 };
static void *detoile_state, *detoile_root, *detoile_saved_carrier, *detoile_carrier_root;
static unsigned int detoile_visit, detoile_reference[8];
static AnchorImpactCarryView detoile_carrier;
static int detoile_context(void *task)
{
    return anchor_impact_native_ready() && anchor_impact_native_encounter() == 4 &&
        anchor_impact_boss_pointer_valid(D_8020EED0_63A2B0) &&
        IB_PTR(D_8020EED0_63A2B0,0x1D8) == task;
}
static int detoile_reference_live(void *task)
{
    return detoile_context(task) && task == detoile_root &&
        detoile_state == D_8020EED0_63A2B0 && detoile_visit == anchor_impact_native_visit();
}
static unsigned int detoile_shield_phase(void *t)
{
    unsigned int cb = IB_U32(t,0x0C)&~0x00800000u;
    if (cb == ((unsigned int)(unsigned long)func_802065B8_631998&~0x00800000u)) return 1;
    if (cb == ((unsigned int)(unsigned long)func_8020679C_631B7C&~0x00800000u)) return 2;
    return 0;
}
static void *detoile_shield(void *root)
{
    void *children[512];
    unsigned int i, n = anchor_impact_boss_children(root,children,512);
    for (i = 0; i < n; ++i) {
        void *t = children[i];
        if (detoile_shield_phase(t) && IB_PTR(t,0xB4) == root &&
            IB_PTR(t,0xB8) == IB_PTR(root,0x18) &&
            anchor_impact_boss_pointer_valid(IB_PTR(t,0x18))) return t;
    }
    return 0;
}
static void detoile_capture(void *s, void *t, unsigned int *r)
{
    unsigned int i, *w = r+DT_SHIELD;
    void *shield, *o;
    anchor_impact_boss_common_capture(s,t,r);
    /* +DC is a local collision-task pointer, excluded from the private mask. */
    if (r[IMP_PHASE] == 66 || r[IMP_PHASE] == 67)
        anchor_impact_boss_carry_capture(IB_PTR(t,0xDC),r+DT_CARRY);
    if (!anchor_impact_native_is_owner() && detoile_reference_live(t))
        for (i = 0; i < 8; ++i) r[DT_CARRY+i] = detoile_reference[i];
    shield = detoile_shield(t);
    if (!shield) return;
    o = IB_PTR(shield,0x18);
    /* The shield consumes +816's pulse inside its callback. Snapshot the
     * resulting shield state as well, so a skipped producer is harmless. */
    w[0] = detoile_shield_phase(shield) | ((unsigned int)IB_U8(shield,0x30)<<8) |
           (IB_U32(o,0x2C) ? 0x10000u : 0) | ((IB_U8(o,0x64)&1u)<<17);
    w[1] = IB_U32(shield,0x7C); w[2] = IB_U32(shield,0x90);
    for (i = 0; i < 3; ++i) { w[3+i] = IB_U32(shield,0xBC+4*i); w[6+i] = IB_U16(o,0x14+2*i); }
    w[9] = IB_U32(o,0x28); w[10] = IB_U32(shield,0x48);
}
static int detoile_validate(const unsigned int *r)
{
    unsigned int i, phase;
    const unsigned int *w = r+DT_SHIELD;
    if (!anchor_impact_boss_common_validate(r) || !anchor_impact_boss_carry_validate(r+DT_CARRY)) return 0;
    for (i = 0; i < ANCHOR_IMPACT_PRIVATE_WORDS; ++i)
        if (!(0xF787FFu & (1u<<i)) && r[IMP_PRIVATE+i]) return 0;
    if ((r[IMP_PHASE] == 66 || r[IMP_PHASE] == 67) && !r[DT_CARRY]) return 0;
    /* +98/+A0 are native grapple direction/meter floats. +94/+9C are angles. */
    if (!anchor_impact_boss_finite(r[IMP_PRIVATE+8]) ||
        !anchor_impact_boss_finite(r[IMP_PRIVATE+10])) return 0;
    phase = w[0]&255u;
    if (w[0]&~0x3FF03u || phase > 2 || !anchor_impact_boss_finite(w[9])) return 0;
    for (i = 0; i < 3; ++i)
        if (!anchor_impact_boss_finite(w[3+i]) || w[6+i] > 65535) return 0;
    if (!phase) for (i = 0; i < 11; ++i) if (w[i]) return 0;
    for (i = DT_END; i < ANCHOR_IMPACT_ROOT_WORDS; ++i) if (r[i]) return 0;
    return 1;
}
static void detoile_apply(void *s, void *t, const unsigned int *r)
{
    unsigned int i;
    const unsigned int *w = r+DT_SHIELD;
    void *shield, *o;
    anchor_impact_boss_common_apply(s,t,r);
    detoile_state = s; detoile_root = t; detoile_visit = anchor_impact_native_visit();
    for (i = 0; i < 8; ++i) detoile_reference[i] = r[DT_CARRY+i];
    shield = detoile_shield(t);
    if (!shield || !(w[0]&255u)) return;
    o = IB_PTR(shield,0x18);
    IB_U32(shield,0x0C) = (unsigned int)(unsigned long)((w[0]&255u)==2 ?
        func_8020679C_631B7C : func_802065B8_631998) | (IB_U32(shield,0x0C)&0x00800000u);
    IB_U8(shield,0x30) = w[0]>>8; IB_U32(shield,0x7C) = w[1]; IB_U32(shield,0x90) = w[2];
    for (i = 0; i < 3; ++i) { IB_U32(shield,0xBC+4*i) = w[3+i]; IB_U16(o,0x14+2*i) = w[6+i]; }
    IB_U32(o,0x2C) = w[0]&0x10000u ? 0x480199E0u : 0;
    IB_U8(o,0x64) = (IB_U8(o,0x64)&~1u) | ((w[0]>>17)&1u);
    IB_U32(o,0x28) = w[9]; IB_U32(shield,0x48) = w[10];
}
static void detoile_recover(void *task)
{
    IB_U32(task,0x0C) = (unsigned int)(unsigned long)func_801FC3FC_6277DC |
        (IB_U32(task,0x0C)&0x00800000u);
    IB_PTR(D_8020EED0_63A2B0,0x174) = 0; IB_U32(D_8020EED0_63A2B0,0x70) = 0;
}
RECOMP_HOOK("func_8001481C_1541C")
void anchor_impact_detoile_task_begin(void *task)
{
    unsigned int cb;
    void *carrier;
    if (!detoile_context(task) || !anchor_impact_native_is_owner()) return;
    cb = IB_U32(task,0x0C)&~0x00800000u;
    if (cb != ((unsigned int)(unsigned long)func_801FF4CC_62A8AC&~0x00800000u) &&
        cb != ((unsigned int)(unsigned long)func_801FF528_62A908&~0x00800000u)) return;
    carrier = IB_PTR(task,0xDC);
    if (!anchor_impact_boss_pointer_valid(carrier) ||
        !anchor_impact_boss_pointer_valid(IB_PTR(carrier,0x18))) detoile_recover(task);
}
RECOMP_HOOK("func_801FF528_62A908")
void anchor_impact_detoile_carrier_begin(void *task)
{
    static const unsigned int empty[8] = {0};
    detoile_carrier_root = 0;
    if (!detoile_context(task) || anchor_impact_native_is_owner()) return;
    detoile_saved_carrier = IB_PTR(task,0xDC); detoile_carrier_root = task;
    IB_PTR(task,0xDC) = anchor_impact_boss_carry_view(&detoile_carrier,
        detoile_reference_live(task) ? detoile_reference : empty);
}
RECOMP_HOOK_RETURN("func_801FF528_62A908")
void anchor_impact_detoile_carrier_end(void)
{
    if (detoile_carrier_root && detoile_context(detoile_carrier_root))
        IB_PTR(detoile_carrier_root,0xDC) = detoile_saved_carrier;
    detoile_carrier_root = 0;
}
RECOMP_HOOK("func_801FE66C_629A4C")
void anchor_impact_detoile_release_begin(void *task)
{
    if (detoile_context(task) && anchor_impact_native_is_owner())
        anchor_impact_players_source_aim_begin(IB_PTR(task,0xDC));
}
RECOMP_HOOK_RETURN("func_801FE66C_629A4C")
void anchor_impact_detoile_release_end(void) { anchor_impact_players_source_aim_end(); }
RECOMP_HOOK("func_801FE964_629D44")
void anchor_impact_detoile_grapple_begin(void *task)
{
    if (detoile_context(task) && anchor_impact_native_is_owner())
        anchor_impact_players_source_axes_begin(IB_PTR(task,0xDC));
}
RECOMP_HOOK_RETURN("func_801FE964_629D44")
void anchor_impact_detoile_grapple_end(void) { anchor_impact_players_source_axes_end(); }
RECOMP_HOOK("func_801FED3C_62A11C")
void anchor_impact_detoile_reel_begin(void *task) { anchor_impact_players_reel_begin(task); }
RECOMP_HOOK_RETURN("func_801FED3C_62A11C")
void anchor_impact_detoile_reel_end(void) { anchor_impact_players_reel_end(); }
static const unsigned short detoile_cues[] = {1,7,8,0x37,0x38,0x4A,0x4C,0x4D};


static AnchorImpactBossProfile detoile_profile;
const AnchorImpactBossProfile *anchor_impact_detoile_profile(void)
{
    static int initialized;
    if (initialized) return &detoile_profile;
    initialized = 1;
#define profile detoile_profile
    profile.task_id = 0x64;
    profile.private_mask = 0xF787FFu;
    profile.capture = detoile_capture;
    profile.validate = detoile_validate;
    profile.apply_lifecycle = anchor_impact_boss_common_lifecycle;
    profile.apply = detoile_apply;
    profile.sound_cues = detoile_cues;
    profile.sound_cue_count = sizeof(detoile_cues)/sizeof(detoile_cues[0]);
    profile.reel = func_801FED3C_62A11C;
    profile.phases[74] = func_801FB67C_626A5C;
    profile.phases[75] = func_801FB6E8_626AC8;
    profile.phases[1] = func_801FB0CC_6264AC;
    profile.phases[2] = func_801FB120_626500;
    profile.phases[3] = func_801FB274_626654;
    profile.phases[4] = func_801FB3E8_6267C8;
    profile.phases[5] = func_801FB50C_6268EC;
    profile.phases[6] = func_801FB658_626A38;
    profile.phases[7] = func_801FB6A0_626A80;
    profile.phases[8] = func_801FB73C_626B1C;
    profile.phases[9] = func_801FB814_626BF4;
    profile.phases[10] = func_801FB934_626D14;
    profile.phases[11] = func_801FB9D4_626DB4;
    profile.phases[12] = func_801FBA34_626E14;
    profile.phases[13] = func_801FBAD8_626EB8;
    profile.phases[14] = func_801FBB74_626F54;
    profile.phases[15] = func_801FBBBC_626F9C;
    profile.phases[16] = func_801FBC2C_62700C;
    profile.phases[17] = func_801FBCA8_627088;
    profile.phases[18] = func_801FBD44_627124;
    profile.phases[19] = func_801FBD6C_62714C;
    profile.phases[20] = func_801FC460_627840;
    profile.phases[21] = func_801FC510_6278F0;
    profile.phases[22] = func_801FC638_627A18;
    profile.phases[23] = func_801FC75C_627B3C;
    profile.phases[24] = func_801FC8A4_627C84;
    profile.phases[25] = func_801FC9E4_627DC4;
    profile.phases[26] = func_801FCA6C_627E4C;
    profile.phases[27] = func_801FCAB4_627E94;
    profile.phases[28] = func_801FCB88_627F68;
    profile.phases[29] = func_801FCC64_628044;
    profile.phases[30] = func_801FCD1C_6280FC;
    profile.phases[31] = func_801FCDAC_62818C;
    profile.phases[32] = func_801FCE74_628254;
    profile.phases[33] = func_801FCF68_628348;
    profile.phases[34] = func_801FD028_628408;
    profile.phases[35] = func_801FD088_628468;
    profile.phases[36] = func_801FD114_6284F4;
    profile.phases[37] = func_801FD1D0_6285B0;
    profile.phases[38] = func_801FD30C_6286EC;
    profile.phases[39] = func_801FD46C_62884C;
    profile.phases[40] = func_801FD500_6288E0;
    profile.phases[41] = func_801FD634_628A14;
    profile.phases[42] = func_801FD724_628B04;
    profile.phases[43] = func_801FD840_628C20;
    profile.phases[44] = func_801FD888_628C68;
    profile.phases[45] = func_801FD8F0_628CD0;
    profile.phases[46] = func_801FDA20_628E00;
    profile.phases[47] = func_801FDB70_628F50;
    profile.phases[48] = func_801FDC84_629064;
    profile.phases[49] = func_801FDD84_629164;
    profile.phases[50] = func_801FDE94_629274;
    profile.phases[51] = func_801FE008_6293E8;
    profile.phases[52] = func_801FE050_629430;
    profile.phases[53] = func_801FE0C4_6294A4;
    profile.phases[54] = func_801FE1E8_6295C8;
    profile.phases[55] = func_801FE288_629668;
    profile.phases[56] = func_801FE308_6296E8;
    profile.phases[57] = func_801FE3C0_6297A0;
    profile.phases[58] = func_801FE47C_62985C;
    profile.phases[59] = func_801FE4D0_6298B0;
    profile.phases[60] = func_801FE66C_629A4C;
    profile.phases[61] = func_801FE964_629D44;
    profile.phases[62] = func_801FED3C_62A11C;
    profile.phases[63] = func_801FF294_62A674;
    profile.phases[64] = func_801FF380_62A760;
    profile.phases[65] = func_801FF460_62A840;
    profile.phases[66] = func_801FF4CC_62A8AC;
    profile.phases[67] = func_801FF528_62A908;
    profile.phases[68] = func_801FF730_62AB10;
    profile.phases[69] = func_801FF798_62AB78;
    profile.phases[70] = func_801FF8B4_62AC94;
    profile.phases[71] = func_801FF944_62AD24;
    profile.phases[72] = func_801FF9EC_62ADCC;
    profile.phases[73] = func_801FFA40_62AE20;
    profile.clips[1] = D_8020B3F0_6367D0;
    profile.clips[2] = D_8020B3F8_6367D8;
    profile.clips[3] = D_8020B400_6367E0;
    profile.clips[4] = D_8020B408_6367E8;
    profile.clips[5] = D_8020B410_6367F0;
    profile.clips[6] = D_8020B418_6367F8;
    profile.clips[7] = D_8020B420_636800;
    profile.clips[8] = D_8020B428_636808;
    profile.clips[9] = D_8020B430_636810;
    profile.clips[10] = D_8020B438_636818;
    profile.clips[11] = D_8020B440_636820;
    profile.clips[12] = D_8020B448_636828;
    profile.clips[13] = D_8020B450_636830;
    profile.clips[14] = D_8020B458_636838;
    profile.phases[76] = func_801FE8B4_629C94;
#undef profile
    return &detoile_profile;
}

RECOMP_HOOK("func_801FAEB0_626290")
void anchor_impact_bind_4(void *task)
{ (void)anchor_impact_native_bind(task, 4); }
