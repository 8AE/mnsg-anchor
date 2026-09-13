/* USA file_13 boss-specific AI checkpoint profile. IDs retain the existing
 * wire mapping; native callbacks and assets are resolved only after binding. */
#include "anchor_impact_boss.h"
#include "anchor_impact_players.h"
#ifndef ANCHOR_IMPACT_BOSS_HOST_TEST
#include "modding.h"
#endif
extern void func_801EF84C_61AC2C(void *, void *);
extern void func_801EF958_61AD38(void *, void *);
extern void func_801F00C4_61B4A4(void *, void *);
extern void func_801EF42C_61A80C(void *, void *);
extern void func_801EF4D4_61A8B4(void *, void *);
extern void func_801EF580_61A960(void *, void *);
extern void func_801EF630_61AA10(void *, void *);
extern void func_801EF704_61AAE4(void *, void *);
extern void func_801EF7A8_61AB88(void *, void *);
extern void func_801EF818_61ABF8(void *, void *);
extern void func_801EF888_61AC68(void *, void *);
extern void func_801EF8C8_61ACA8(void *, void *);
extern void func_801EF924_61AD04(void *, void *);
extern void func_801EF998_61AD78(void *, void *);
extern void func_801EFA88_61AE68(void *, void *);
extern void func_801EFBC4_61AFA4(void *, void *);
extern void func_801EFC34_61B014(void *, void *);
extern void func_801EFD04_61B0E4(void *, void *);
extern void func_801F00B8_61B498(void *, void *);
extern void func_801F0138_61B518(void *, void *);
extern void func_801F02A0_61B680(void *, void *);
extern void func_801F0330_61B710(void *, void *);
extern void func_801F0398_61B778(void *, void *);
extern void func_801F03C4_61B7A4(void *, void *);
extern void func_801F0414_61B7F4(void *, void *);
extern void func_801F047C_61B85C(void *, void *);
extern void func_801F04E0_61B8C0(void *, void *);
extern void func_801F060C_61B9EC(void *, void *);
extern void func_801F07E4_61BBC4(void *, void *);
extern void func_801F0A50_61BE30(void *, void *);
extern void func_801F0AD4_61BEB4(void *, void *);
extern void func_801F0BF4_61BFD4(void *, void *);
extern void func_801F0CF8_61C0D8(void *, void *);
extern void func_801F0E00_61C1E0(void *, void *);
extern void func_801F0F3C_61C31C(void *, void *);
extern void func_801F0FCC_61C3AC(void *, void *);
extern void func_801F1074_61C454(void *, void *);
extern void func_801F1110_61C4F0(void *, void *);
extern void func_801F1218_61C5F8(void *, void *);
extern void func_801F12F8_61C6D8(void *, void *);
extern void func_801F1340_61C720(void *, void *);
extern void func_801F14FC_61C8DC(void *, void *);
extern void func_801F1738_61CB18(void *, void *);
extern void func_801F1788_61CB68(void *, void *);
extern void func_801F20B4_61D494(void *, void *);
extern void func_801F214C_61D52C(void *, void *);
extern void func_801F223C_61D61C(void *, void *);
extern void func_801F234C_61D72C(void *, void *);
extern void func_801F2418_61D7F8(void *, void *);
extern void func_801F2438_61D818(void *, void *);
extern void func_801F24F4_61D8D4(void *, void *);
extern void func_801F25A0_61D980(void *, void *);
extern void func_801F2644_61DA24(void *, void *);
extern void func_801F26E0_61DAC0(void *, void *);
extern void func_801F2788_61DB68(void *, void *);
extern void func_801F2854_61DC34(void *, void *);
extern void func_801F28A4_61DC84(void *, void *);
extern void func_801F2914_61DCF4(void *, void *);
extern void func_801F2A04_61DDE4(void *, void *);
extern void func_801F2A98_61DE78(void *, void *);
extern void func_801F2B1C_61DEFC(void *, void *);
extern void func_801F2B7C_61DF5C(void *, void *);
extern void func_801F2C24_61E004(void *, void *);
extern void func_801F2C70_61E050(void *, void *);
extern void func_801F2CB4_61E094(void *, void *);
extern void func_801F2DDC_61E1BC(void *, void *);
extern void func_801F2ED8_61E2B8(void *, void *);
extern void func_801F2FA0_61E380(void *, void *);
extern void func_801F300C_61E3EC(void *, void *);
extern void func_801F30A0_61E480(void *, void *);
extern void func_801F31A0_61E580(void *, void *);
extern void func_801F3254_61E634(void *, void *);
extern void func_801F331C_61E6FC(void *, void *);
extern void func_801F3498_61E878(void *, void *);
extern void func_801F3634_61EA14(void *, void *);
extern void func_801F3714_61EAF4(void *, void *);
extern void func_801F37F4_61EBD4(void *, void *);
extern void func_801F3878_61EC58(void *, void *);
extern void func_801F3950_61ED30(void *, void *);
extern void func_801F39A8_61ED88(void *, void *);
extern void func_801F3A34_61EE14(void *, void *);
extern void func_801F3A68_61EE48(void *, void *);
extern void func_801F3AC8_61EEA8(void *, void *);
extern void func_801F3B20_61EF00(void *, void *);
extern void func_801F3BAC_61EF8C(void *, void *);
extern void func_801F3C9C_61F07C(void *, void *);
extern void func_801F3DCC_61F1AC(void *, void *);
extern void func_801F3E24_61F204(void *, void *);
extern void func_801F3EB8_61F298(void *, void *);
extern void func_801F3FC0_61F3A0(void *, void *);
extern void func_801F40DC_61F4BC(void *, void *);
extern void func_801F4220_61F600(void *, void *);
extern void func_801F436C_61F74C(void *, void *);
extern void func_801F43D0_61F7B0(void *, void *);
extern void func_801F443C_61F81C(void *, void *);
extern void func_801F453C_61F91C(void *, void *);
extern void func_801F45A0_61F980(void *, void *);
extern void func_801F45E8_61F9C8(void *, void *);
extern void func_801F4654_61FA34(void *, void *);
extern void func_801F4780_61FB60(void *, void *);
extern void func_801F47E0_61FBC0(void *, void *);
extern void func_801F4860_61FC40(void *, void *);
extern void func_801F4A38_61FE18(void *, void *);
extern void func_801F4B4C_61FF2C(void *, void *);
extern void func_801F4C0C_61FFEC(void *, void *);
extern void func_801F4C74_620054(void *, void *);
extern void func_801F4CF4_6200D4(void *, void *);
extern void func_801F5190_620570(void *, void *);
extern void func_801F5550_620930(void *, void *);
extern void func_801F5638_620A18(void *, void *);
extern void func_801F56C8_620AA8(void *, void *);
extern void func_801F5710_620AF0(void *, void *);
extern void func_801F58AC_620C8C(void *, void *);
extern void func_801F5B78_620F58(void *, void *);
extern void func_801F5F0C_6212EC(void *, void *);
extern void func_801F6450_621830(void *, void *);
extern void func_801F6500_6218E0(void *, void *);
extern void func_801F65B8_621998(void *, void *);
extern void func_801F6624_621A04(void *, void *);
extern void func_801F6660_621A40(void *, void *);
extern void func_801F6988_621D68(void *, void *);
extern void func_801F69E0_621DC0(void *, void *);
extern void func_801F6AC8_621EA8(void *, void *);
extern void func_801F6BCC_621FAC(void *, void *);
extern unsigned int D_8020B260_636640[];
extern unsigned int D_8020B268_636648[];
extern unsigned int D_8020B270_636650[];
extern unsigned int D_8020B278_636658[];
extern unsigned int D_8020B280_636660[];
extern unsigned int D_8020B288_636668[];
extern unsigned int D_8020B290_636670[];
extern unsigned int D_8020B298_636678[];
extern unsigned int D_8020B2A8_636688[];
extern unsigned int D_8020B2B0_636690[];
extern unsigned int D_8020B2B8_636698[];
extern unsigned int D_8020B2C0_6366A0[];
extern unsigned int D_8020B2C8_6366A8[];
extern unsigned int D_8020B2D0_6366B0[];
extern unsigned int D_8020B2D8_6366B8[];
extern unsigned int D_8020B2A0_636680[];
extern void func_801F05BC_61B99C(void *, void *);
extern void func_801F0A0C_61BDEC(void *, void *);
extern void func_801F1690_61CA70(void *, void *);
extern void func_801F16E0_61CAC0(void *, void *);
extern void func_801F38DC_61ECBC(void *, void *);

extern void *D_8020EED0_63A2B0, *D_8020EF30_63A310;
extern void func_801F0540_61B920(void *, void *);

/* +9C is a union: returning-weapon task during F3950, angle elsewhere.
 * +DC is the last collision task, dereferenced by the carrying reaction.
 * Neither belongs in the scalar private mask. */
enum {
    TS_WORLD = IMP_BOSS_DATA, /* +178..+18C: pending displacement and arena origin */
    TS_ASCEND = TS_WORLD+6, TS_REEL, TS_COLLISION_KIND, TS_COLLISION_MASK,
    TS_ATTACK_KIND, TS_WEAPON_ACTIVE, TS_CARRY_VALID, TS_CARRY_POS,
    TS_CARRY_VELOCITY = TS_CARRY_POS+3, TS_CARRY_YAW = TS_CARRY_VELOCITY+3,
    TS_PLAYER_ACTION, TS_LATCH, TS_DEFEATED, TS_GRAB_TIMER, TS_END
};
static void *taisamba_state, *taisamba_root;
static unsigned int taisamba_visit;
static unsigned int taisamba_references[9];
/* Read-only native inputs, scoped to one callback. These never join a task
 * list, render, collide, or execute AI. Each peer keeps its real task pointers. */
static unsigned int taisamba_weapon[0x60/4];
static unsigned int taisamba_carrier[0x80/4], taisamba_carrier_model[0x30/4];
static void *taisamba_saved_weapon, *taisamba_saved_carrier;
static void *taisamba_weapon_root, *taisamba_carrier_root;
static int taisamba_recover_carrier;

static int taisamba_finite(unsigned int word)
{
    return (word & 0x7f800000u) != 0x7f800000u;
}
static int taisamba_context(void *task)
{
    return anchor_impact_native_ready() && anchor_impact_native_encounter() == 2 &&
        anchor_impact_boss_pointer_valid(D_8020EED0_63A2B0) &&
        IB_PTR(D_8020EED0_63A2B0,0x1D8) == task;
}
static int taisamba_reference_live(void *task)
{
    return taisamba_context(task) && task == taisamba_root &&
        taisamba_state == D_8020EED0_63A2B0 &&
        taisamba_visit == anchor_impact_native_visit();
}
static void taisamba_capture(void *state, void *task, unsigned int *r)
{
    unsigned int i;
    void *aux = D_8020EF30_63A310;
    void *reference;
    if (anchor_impact_boss_pointer_valid(aux)) {
        r[IMP_AUX_KIND] = 1;
        r[IMP_AUX_DATA] = IB_U16(aux,4);
        r[IMP_AUX_DATA+1] = IB_U32(aux,8);
        r[IMP_AUX_DATA+2] = IB_U8(aux,0xC);
    }
    for (i = 0; i < 6; ++i) r[TS_WORLD+i] = IB_U32(state,0x178+i*4);
    r[TS_ASCEND] = IB_U8(state,0x2C5);
    r[TS_REEL] = IB_U32(state,0x70);
    r[TS_COLLISION_KIND] = IB_U8(task,0x30);
    r[TS_COLLISION_MASK] = IB_U32(task,0x48);
    r[TS_ATTACK_KIND] = IB_U8(task,0x4C);
    r[TS_PLAYER_ACTION] = IB_U8(state,0x14E);
    r[TS_LATCH] = IB_PTR(state,0x174) == task;
    r[TS_DEFEATED] = IB_U8(state,0x2C4);
    r[TS_GRAB_TIMER] = IB_U8(state,0x14F);
    reference = IB_PTR(task,0x9C);
    /* Check phase before interpreting this union as a pointer. */
    if (r[IMP_PHASE] == 76 && anchor_impact_boss_pointer_valid(reference))
        r[TS_WEAPON_ACTIVE] = IB_U16(reference,0x5C) == 0x5B;
    reference = IB_PTR(task,0xDC);
    if ((r[IMP_PHASE] == 116 || r[IMP_PHASE] == 117) &&
        anchor_impact_boss_pointer_valid(reference) &&
        anchor_impact_boss_pointer_valid(IB_PTR(reference,0x18))) {
        void *object = IB_PTR(reference,0x18);
        r[TS_CARRY_VALID] = 1;
        for (i = 0; i < 3; ++i) {
            r[TS_CARRY_POS+i] = IB_U32(object,8+i*4);
            r[TS_CARRY_VELOCITY+i] = IB_U32(reference,0x70+i*4);
        }
        r[TS_CARRY_YAW] = IB_U16(object,0x16);
    }
    if (!anchor_impact_native_is_owner() && taisamba_reference_live(task)) {
        for (i = 0; i < 9; ++i) r[TS_WEAPON_ACTIVE+i] = taisamba_references[i];
    }
}
static int taisamba_validate(const unsigned int *r)
{
    unsigned int i;
    if (r[IMP_AUX_KIND] > 1 || r[IMP_AUX_DATA] > 65535 ||
        r[IMP_AUX_DATA+2] > 255 || r[IMP_AUX_DATA+3] || r[IMP_AUX_DATA+4]) return 0;
    for (i = 0; i < 6; ++i) if (!taisamba_finite(r[TS_WORLD+i])) return 0;
    for (i = 0; i < 6; ++i) if (!taisamba_finite(r[TS_CARRY_POS+i])) return 0;
    /* Pending displacement can briefly cross the clamp before D0C70 runs. */
    if (r[TS_ASCEND] > 1 || r[TS_REEL] > 100 || r[TS_COLLISION_KIND] > 255 ||
        r[TS_ATTACK_KIND] > 255 || r[TS_WEAPON_ACTIVE] > 1 ||
        r[TS_CARRY_VALID] > 1 || r[TS_CARRY_YAW] > 65535 ||
        r[TS_PLAYER_ACTION] > 255 || r[TS_LATCH] > 1 ||
        r[TS_DEFEATED] > 1 || r[TS_GRAB_TIMER] > 255) return 0;
    if (r[IMP_PRIVATE+9] || r[IMP_PRIVATE+19]) return 0;
    if ((r[IMP_PHASE] == 116 || r[IMP_PHASE] == 117) && !r[TS_CARRY_VALID]) return 0;
    for (i = 12; i <= 14; ++i) if (!taisamba_finite(r[IMP_PRIVATE+i])) return 0;
    if (!taisamba_finite(r[IMP_PRIVATE+10])) return 0;
    for (i = TS_END; i < ANCHOR_IMPACT_ROOT_WORDS; ++i) if (r[i]) return 0;
    return 1;
}
static void taisamba_apply_lifecycle(void *state, const unsigned int *r)
{
    /* F4B4C/F5550 signal defeat before switching into the outro. Applying
     * the phase without this gate skips its producer and can stall victory.
     * Unlike combat pose/input, this completion flag must cross a pause. */
    IB_U8(state,0x2C4) = r[TS_DEFEATED];
}
static void taisamba_apply(void *state, void *task, const unsigned int *r)
{
    unsigned int i;
    void *aux = D_8020EF30_63A310;
    if (r[IMP_AUX_KIND] == 1 && anchor_impact_boss_pointer_valid(aux)) {
        IB_U16(aux,4) = r[IMP_AUX_DATA];
        IB_U32(aux,8) = r[IMP_AUX_DATA+1];
        IB_U8(aux,0xC) = r[IMP_AUX_DATA+2];
    }
    for (i = 0; i < 6; ++i) IB_U32(state,0x178+i*4) = r[TS_WORLD+i];
    IB_U8(state,0x2C5) = r[TS_ASCEND];
    IB_U32(state,0x70) = r[TS_REEL];
    IB_U8(task,0x30) = r[TS_COLLISION_KIND];
    IB_U32(task,0x48) = r[TS_COLLISION_MASK];
    IB_U8(task,0x4C) = r[TS_ATTACK_KIND];
    IB_U8(state,0x14E) = r[TS_PLAYER_ACTION];
    IB_U8(state,0x14F) = r[TS_GRAB_TIMER];
    IB_PTR(state,0x174) = r[TS_LATCH] ? task : 0;
    taisamba_state = state; taisamba_root = task;
    taisamba_visit = anchor_impact_native_visit();
    for (i = 0; i < 9; ++i) taisamba_references[i] = r[TS_WEAPON_ACTIVE+i];
}

/* The combo grab and its release read global aim from the boss callback,
 * after the attacking limb's scheduler scope has ended. Reuse that limb's
 * tracked initiator only for these native readers; restore the local camera. */
#define TAISAMBA_SOURCE_AIM(symbol, name) \
    RECOMP_HOOK(symbol) void name##_begin(void *task) { \
        if (taisamba_context(task) && anchor_impact_native_is_owner()) \
            anchor_impact_players_source_aim_begin(IB_PTR(task,0xDC)); \
    } \
    RECOMP_HOOK_RETURN(symbol) void name##_end(void) { anchor_impact_players_source_aim_end(); }
TAISAMBA_SOURCE_AIM("func_801F5710_620AF0", anchor_impact_taisamba_grab)
TAISAMBA_SOURCE_AIM("func_801F58AC_620C8C", anchor_impact_taisamba_release)
#undef TAISAMBA_SOURCE_AIM

/* The scheduler calls 8001481C before reading this task's pre/update/post
 * callbacks. Recover before entering native carry math when promotion has
 * no local attack to carry the boss. No callback is replayed here. */
RECOMP_HOOK("func_8001481C_1541C")
void anchor_impact_taisamba_task_begin(void *task)
{
    unsigned int callback;
    void *carrier;
    if (!taisamba_context(task) || !anchor_impact_native_is_owner()) return;
    callback = IB_U32(task,0x0C) & ~0x00800000u;
    if (callback != ((unsigned int)(unsigned long)func_801F6660_621A40 & ~0x00800000u) &&
        callback != ((unsigned int)(unsigned long)func_801F6624_621A04 & ~0x00800000u)) return;
    carrier = IB_PTR(task,0xDC);
    if (anchor_impact_boss_pointer_valid(carrier) &&
        anchor_impact_boss_pointer_valid(IB_PTR(carrier,0x18))) return;
    IB_U32(task,0x0C) = (unsigned int)(unsigned long)func_801F0540_61B920 |
                       (IB_U32(task,0x0C) & 0x00800000u);
    IB_PTR(D_8020EED0_63A2B0,0x174) = 0;
    IB_U32(D_8020EED0_63A2B0,0x70) = 0;
}

/* F3950 reads only weapon+5C. A checkpoint can skip the local constructor;
 * reconstruct this dependency from a boolean for followers, never a host
 * address. A promoted owner with no weapon lets the native wait finish. */
RECOMP_HOOK("func_801F3950_61ED30")
void anchor_impact_taisamba_weapon_begin(void *task)
{
    void *weapon;
    taisamba_weapon_root = 0;
    if (!taisamba_context(task)) return;
    weapon = IB_PTR(task,0x9C);
    if (anchor_impact_native_is_owner() && anchor_impact_boss_pointer_valid(weapon)) return;
    taisamba_saved_weapon = weapon; taisamba_weapon_root = task;
    IB_U16(taisamba_weapon,0x5C) = !anchor_impact_native_is_owner() &&
        taisamba_reference_live(task) && taisamba_references[0] ? 0x5B : 0;
    IB_PTR(task,0x9C) = taisamba_weapon;
}
RECOMP_HOOK_RETURN("func_801F3950_61ED30")
void anchor_impact_taisamba_weapon_end(void)
{
    if (taisamba_weapon_root && taisamba_context(taisamba_weapon_root))
        IB_PTR(taisamba_weapon_root,0x9C) = taisamba_saved_weapon;
    taisamba_weapon_root = 0;
}

/* F6660 consumes the carrying attack's pose/velocity, not its callback.
 * A local scalar view supplies those inputs when the owner spawned the limb.
 * A promotion with no real local carrying attack resumes native selection. */
RECOMP_HOOK("func_801F6660_621A40")
void anchor_impact_taisamba_carrier_begin(void *task)
{
    unsigned int i;
    void *carrier;
    taisamba_carrier_root = 0;
    if (!taisamba_context(task)) return;
    carrier = IB_PTR(task,0xDC);
    if (anchor_impact_native_is_owner() && anchor_impact_boss_pointer_valid(carrier) &&
        anchor_impact_boss_pointer_valid(IB_PTR(carrier,0x18))) return;
    taisamba_saved_carrier = carrier; taisamba_carrier_root = task;
    taisamba_recover_carrier = anchor_impact_native_is_owner();
    IB_PTR(taisamba_carrier,0x18) = taisamba_carrier_model;
    for (i = 0; i < 3; ++i) {
        IB_U32(taisamba_carrier_model,8+i*4) = taisamba_reference_live(task) ? taisamba_references[2+i] : 0;
        IB_U32(taisamba_carrier,0x70+i*4) = taisamba_reference_live(task) ? taisamba_references[5+i] : 0;
    }
    IB_U16(taisamba_carrier_model,0x16) = taisamba_reference_live(task) ? taisamba_references[8] : 0;
    IB_PTR(task,0xDC) = taisamba_carrier;
}
RECOMP_HOOK_RETURN("func_801F6660_621A40")
void anchor_impact_taisamba_carrier_end(void)
{
    void *task = taisamba_carrier_root;
    if (task && taisamba_context(task)) {
        IB_PTR(task,0xDC) = taisamba_saved_carrier;
        if (taisamba_recover_carrier) {
            IB_U32(task,0x0C) = (unsigned int)(unsigned long)func_801F0540_61B920;
            IB_PTR(D_8020EED0_63A2B0,0x174) = 0;
            IB_U32(D_8020EED0_63A2B0,0x70) = 0;
        }
    }
    taisamba_carrier_root = 0;
}

/* Native intro/victory cues in F9568..FABF8, in addition to shared combat sounds. */
static const unsigned short taisamba_cues[] = {1,7,8,0x27,0x28,0x4A,0x4C,0x4D,0x60};
static AnchorImpactBossProfile taisamba_profile;
const AnchorImpactBossProfile *anchor_impact_taisamba_profile(void)
{
    static int initialized;
    if (initialized) return &taisamba_profile;
    initialized = 1;
#define profile taisamba_profile
    profile.reel = func_801F5F0C_6212EC;
    profile.task_id = 0x5A;
    profile.private_mask = 0xf775ffu;
    profile.sound_cues = taisamba_cues;
    profile.sound_cue_count = sizeof(taisamba_cues)/sizeof(taisamba_cues[0]);
    profile.capture = taisamba_capture;
    profile.validate = taisamba_validate;
    profile.apply_lifecycle = taisamba_apply_lifecycle;
    profile.apply = taisamba_apply;
    profile.phases[122] = func_801EF84C_61AC2C;
    profile.phases[123] = func_801EF958_61AD38;
    profile.phases[124] = func_801F00C4_61B4A4;
    profile.phases[1] = func_801EF42C_61A80C;
    profile.phases[2] = func_801EF4D4_61A8B4;
    profile.phases[3] = func_801EF580_61A960;
    profile.phases[4] = func_801EF630_61AA10;
    profile.phases[5] = func_801EF704_61AAE4;
    profile.phases[6] = func_801EF7A8_61AB88;
    profile.phases[7] = func_801EF818_61ABF8;
    profile.phases[8] = func_801EF888_61AC68;
    profile.phases[9] = func_801EF8C8_61ACA8;
    profile.phases[10] = func_801EF924_61AD04;
    profile.phases[11] = func_801EF998_61AD78;
    profile.phases[12] = func_801EFA88_61AE68;
    profile.phases[13] = func_801EFBC4_61AFA4;
    profile.phases[14] = func_801EFC34_61B014;
    profile.phases[15] = func_801EFD04_61B0E4;
    profile.phases[16] = func_801F00B8_61B498;
    profile.phases[17] = func_801F0138_61B518;
    profile.phases[18] = func_801F02A0_61B680;
    profile.phases[19] = func_801F0330_61B710;
    profile.phases[20] = func_801F0398_61B778;
    profile.phases[21] = func_801F03C4_61B7A4;
    profile.phases[22] = func_801F0414_61B7F4;
    profile.phases[23] = func_801F047C_61B85C;
    profile.phases[24] = func_801F04E0_61B8C0;
    profile.phases[25] = func_801F060C_61B9EC;
    profile.phases[26] = func_801F07E4_61BBC4;
    profile.phases[27] = func_801F0A50_61BE30;
    profile.phases[28] = func_801F0AD4_61BEB4;
    profile.phases[29] = func_801F0BF4_61BFD4;
    profile.phases[30] = func_801F0CF8_61C0D8;
    profile.phases[31] = func_801F0E00_61C1E0;
    profile.phases[32] = func_801F0F3C_61C31C;
    profile.phases[33] = func_801F0FCC_61C3AC;
    profile.phases[34] = func_801F1074_61C454;
    profile.phases[35] = func_801F1110_61C4F0;
    profile.phases[36] = func_801F1218_61C5F8;
    profile.phases[37] = func_801F12F8_61C6D8;
    profile.phases[38] = func_801F1340_61C720;
    profile.phases[39] = func_801F14FC_61C8DC;
    profile.phases[40] = func_801F1738_61CB18;
    profile.phases[41] = func_801F1788_61CB68;
    profile.phases[42] = func_801F20B4_61D494;
    profile.phases[43] = func_801F214C_61D52C;
    profile.phases[44] = func_801F223C_61D61C;
    profile.phases[45] = func_801F234C_61D72C;
    profile.phases[46] = func_801F2418_61D7F8;
    profile.phases[47] = func_801F2438_61D818;
    profile.phases[48] = func_801F24F4_61D8D4;
    profile.phases[49] = func_801F25A0_61D980;
    profile.phases[50] = func_801F2644_61DA24;
    profile.phases[51] = func_801F26E0_61DAC0;
    profile.phases[52] = func_801F2788_61DB68;
    profile.phases[53] = func_801F2854_61DC34;
    profile.phases[54] = func_801F28A4_61DC84;
    profile.phases[55] = func_801F2914_61DCF4;
    profile.phases[56] = func_801F2A04_61DDE4;
    profile.phases[57] = func_801F2A98_61DE78;
    profile.phases[58] = func_801F2B1C_61DEFC;
    profile.phases[59] = func_801F2B7C_61DF5C;
    profile.phases[60] = func_801F2C24_61E004;
    profile.phases[61] = func_801F2C70_61E050;
    profile.phases[62] = func_801F2CB4_61E094;
    profile.phases[63] = func_801F2DDC_61E1BC;
    profile.phases[64] = func_801F2ED8_61E2B8;
    profile.phases[65] = func_801F2FA0_61E380;
    profile.phases[66] = func_801F300C_61E3EC;
    profile.phases[67] = func_801F30A0_61E480;
    profile.phases[68] = func_801F31A0_61E580;
    profile.phases[69] = func_801F3254_61E634;
    profile.phases[70] = func_801F331C_61E6FC;
    profile.phases[71] = func_801F3498_61E878;
    profile.phases[72] = func_801F3634_61EA14;
    profile.phases[73] = func_801F3714_61EAF4;
    profile.phases[74] = func_801F37F4_61EBD4;
    profile.phases[75] = func_801F3878_61EC58;
    profile.phases[76] = func_801F3950_61ED30;
    profile.phases[77] = func_801F39A8_61ED88;
    profile.phases[78] = func_801F3A34_61EE14;
    profile.phases[79] = func_801F3A68_61EE48;
    profile.phases[80] = func_801F3AC8_61EEA8;
    profile.phases[81] = func_801F3B20_61EF00;
    profile.phases[82] = func_801F3BAC_61EF8C;
    profile.phases[83] = func_801F3C9C_61F07C;
    profile.phases[84] = func_801F3DCC_61F1AC;
    profile.phases[85] = func_801F3E24_61F204;
    profile.phases[86] = func_801F3EB8_61F298;
    profile.phases[87] = func_801F3FC0_61F3A0;
    profile.phases[88] = func_801F40DC_61F4BC;
    profile.phases[89] = func_801F4220_61F600;
    profile.phases[90] = func_801F436C_61F74C;
    profile.phases[91] = func_801F43D0_61F7B0;
    profile.phases[92] = func_801F443C_61F81C;
    profile.phases[93] = func_801F453C_61F91C;
    profile.phases[94] = func_801F45A0_61F980;
    profile.phases[95] = func_801F45E8_61F9C8;
    profile.phases[96] = func_801F4654_61FA34;
    profile.phases[97] = func_801F4780_61FB60;
    profile.phases[98] = func_801F47E0_61FBC0;
    profile.phases[99] = func_801F4860_61FC40;
    profile.phases[100] = func_801F4A38_61FE18;
    profile.phases[101] = func_801F4B4C_61FF2C;
    profile.phases[102] = func_801F4C0C_61FFEC;
    profile.phases[103] = func_801F4C74_620054;
    profile.phases[104] = func_801F4CF4_6200D4;
    profile.phases[105] = func_801F5190_620570;
    profile.phases[106] = func_801F5550_620930;
    profile.phases[107] = func_801F5638_620A18;
    profile.phases[108] = func_801F56C8_620AA8;
    profile.phases[109] = func_801F5710_620AF0;
    profile.phases[110] = func_801F58AC_620C8C;
    profile.phases[111] = func_801F5B78_620F58;
    profile.phases[112] = func_801F5F0C_6212EC;
    profile.phases[113] = func_801F6450_621830;
    profile.phases[114] = func_801F6500_6218E0;
    profile.phases[115] = func_801F65B8_621998;
    profile.phases[116] = func_801F6624_621A04;
    profile.phases[117] = func_801F6660_621A40;
    profile.phases[118] = func_801F6988_621D68;
    profile.phases[119] = func_801F69E0_621DC0;
    profile.phases[120] = func_801F6AC8_621EA8;
    profile.phases[121] = func_801F6BCC_621FAC;
    profile.clips[1] = D_8020B260_636640;
    profile.clips[2] = D_8020B268_636648;
    profile.clips[3] = D_8020B270_636650;
    profile.clips[4] = D_8020B278_636658;
    profile.clips[5] = D_8020B280_636660;
    profile.clips[6] = D_8020B288_636668;
    profile.clips[7] = D_8020B290_636670;
    profile.clips[8] = D_8020B298_636678;
    profile.clips[9] = D_8020B2A8_636688;
    profile.clips[10] = D_8020B2B0_636690;
    profile.clips[11] = D_8020B2B8_636698;
    profile.clips[12] = D_8020B2C0_6366A0;
    profile.clips[13] = D_8020B2C8_6366A8;
    profile.clips[14] = D_8020B2D0_6366B0;
    profile.clips[15] = D_8020B2D8_6366B8;
    profile.clips[16] = D_8020B2A0_636680;
    profile.phases[125] = func_801F05BC_61B99C;
    profile.phases[126] = func_801F0A0C_61BDEC;
    profile.phases[127] = func_801F1690_61CA70;
    profile.phases[128] = func_801F16E0_61CAC0;
    profile.phases[129] = func_801F38DC_61ECBC;
#undef profile
    return &taisamba_profile;
}

RECOMP_HOOK("func_801EF2E0_61A6C0")
void anchor_impact_bind_2(void *task)
{
    (void)anchor_impact_native_bind(task, 2);
}

RECOMP_HOOK("func_801F5F0C_6212EC")
void anchor_impact_taisamba_reel_begin(void *task)
{ anchor_impact_players_reel_begin(task); }
RECOMP_HOOK_RETURN("func_801F5F0C_6212EC")
void anchor_impact_taisamba_reel_end(void)
{ anchor_impact_players_reel_end(); }
