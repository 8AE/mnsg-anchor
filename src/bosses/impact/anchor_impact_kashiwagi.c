/* USA file_13 boss-specific AI checkpoint profile. IDs retain the existing
 * wire mapping; native callbacks and assets are resolved only after binding. */
#include "bosses/impact/anchor_impact_boss.h"
#include "bosses/impact/anchor_impact_players.h"
#ifndef ANCHOR_IMPACT_BOSS_HOST_TEST
#include "platform/modding.h"
#endif
extern void func_801E4978_60FD58(void *, void *);
extern void func_801E4A40_60FE20(void *, void *);
extern void func_801E4BC0_60FFA0(void *, void *);
extern void func_801E4C98_610078(void *, void *);
extern void func_801E818C_61356C(void *, void *);
extern void func_801E8368_613748(void *, void *);
extern void func_801E83E8_6137C8(void *, void *);
extern void func_801E8DF8_6141D8(void *, void *);
extern void func_801E9E10_6151F0(void *, void *);
extern void func_801E493C_60FD1C(void *, void *);
extern void func_801E4A9C_60FE7C(void *, void *);
extern void func_801E4B38_60FF18(void *, void *);
extern void func_801E4BF4_60FFD4(void *, void *);
extern void func_801E4C5C_61003C(void *, void *);
extern void func_801E4CE8_6100C8(void *, void *);
extern void func_801E4DF8_6101D8(void *, void *);
extern void func_801E4F2C_61030C(void *, void *);
extern void func_801E4FB0_610390(void *, void *);
extern void func_801E4FD4_6103B4(void *, void *);
extern void func_801E5064_610444(void *, void *);
extern void func_801E5144_610524(void *, void *);
extern void func_801E51EC_6105CC(void *, void *);
extern void func_801E5274_610654(void *, void *);
extern void func_801E52CC_6106AC(void *, void *);
extern void func_801E52D8_6106B8(void *, void *);
extern void func_801E59F8_610DD8(void *, void *);
extern void func_801E5A88_610E68(void *, void *);
extern void func_801E5BDC_610FBC(void *, void *);
extern void func_801E5D38_611118(void *, void *);
extern void func_801E5DAC_61118C(void *, void *);
extern void func_801E5E14_6111F4(void *, void *);
extern void func_801E5EE0_6112C0(void *, void *);
extern void func_801E5F64_611344(void *, void *);
extern void func_801E6024_611404(void *, void *);
extern void func_801E607C_61145C(void *, void *);
extern void func_801E60CC_6114AC(void *, void *);
extern void func_801E6150_611530(void *, void *);
extern void func_801E61A8_611588(void *, void *);
extern void func_801E628C_61166C(void *, void *);
extern void func_801E6370_611750(void *, void *);
extern void func_801E641C_6117FC(void *, void *);
extern void func_801E6474_611854(void *, void *);
extern void func_801E651C_6118FC(void *, void *);
extern void func_801E6600_6119E0(void *, void *);
extern void func_801E66C4_611AA4(void *, void *);
extern void func_801E6778_611B58(void *, void *);
extern void func_801E67D4_611BB4(void *, void *);
extern void func_801E6854_611C34(void *, void *);
extern void func_801E69F4_611DD4(void *, void *);
extern void func_801E6D00_6120E0(void *, void *);
extern void func_801E6DE0_6121C0(void *, void *);
extern void func_801E6ECC_6122AC(void *, void *);
extern void func_801E6FC4_6123A4(void *, void *);
extern void func_801E700C_6123EC(void *, void *);
extern void func_801E7064_612444(void *, void *);
extern void func_801E7134_612514(void *, void *);
extern void func_801E71E8_6125C8(void *, void *);
extern void func_801E72F4_6126D4(void *, void *);
extern void func_801E752C_61290C(void *, void *);
extern void func_801E75E4_6129C4(void *, void *);
extern void func_801E7714_612AF4(void *, void *);
extern void func_801E7838_612C18(void *, void *);
extern void func_801E79E4_612DC4(void *, void *);
extern void func_801E7C50_613030(void *, void *);
extern void func_801E7CA8_613088(void *, void *);
extern void func_801E7FB0_613390(void *, void *);
extern void func_801E8108_6134E8(void *, void *);
extern void func_801E8220_613600(void *, void *);
extern void func_801E8488_613868(void *, void *);
extern void func_801E88C8_613CA8(void *, void *);
extern void func_801E8C04_613FE4(void *, void *);
extern void func_801E8CDC_6140BC(void *, void *);
extern void func_801E8E58_614238(void *, void *);
extern void func_801E8FA4_614384(void *, void *);
extern void func_801E91BC_61459C(void *, void *);
extern void func_801E9274_614654(void *, void *);
extern void func_801E9624_614A04(void *, void *);
extern void func_801E9C90_615070(void *, void *);
extern void func_801E9E68_615248(void *, void *);
extern unsigned int D_8020B170_636550[];
extern unsigned int D_8020B178_636558[];
extern unsigned int D_8020B180_636560[];
extern unsigned int D_8020B188_636568[];
extern unsigned int D_8020B190_636570[];
extern unsigned int D_8020B198_636578[];
extern unsigned int D_8020B1A0_636580[];
extern unsigned int D_8020B1A8_636588[];
extern unsigned int D_8020B1B0_636590[];
extern unsigned int D_8020B1B8_636598[];
extern unsigned int D_8020B1C0_6365A0[];
extern unsigned int D_8020B1C8_6365A8[];
extern unsigned int D_8020B1D0_6365B0[];

static AnchorImpactBossProfile kashiwagi_profile;
const AnchorImpactBossProfile *anchor_impact_kashiwagi_profile(void)
{
    static int initialized;
    if (initialized) return &kashiwagi_profile;
    initialized = 1;
#define profile kashiwagi_profile
    profile.reel = func_801E9624_614A04;
    profile.task_id = 0x50;
    profile.private_mask = 0xffbfdfu;
    profile.phases[71] = func_801E4978_60FD58;
    profile.phases[72] = func_801E4A40_60FE20;
    profile.phases[73] = func_801E4BC0_60FFA0;
    profile.phases[74] = func_801E4C98_610078;
    profile.phases[75] = func_801E818C_61356C;
    profile.phases[76] = func_801E8368_613748;
    profile.phases[77] = func_801E83E8_6137C8;
    profile.phases[78] = func_801E8DF8_6141D8;
    profile.phases[79] = func_801E9E10_6151F0;
    profile.phases[1] = func_801E493C_60FD1C;
    profile.phases[2] = func_801E4A9C_60FE7C;
    profile.phases[3] = func_801E4B38_60FF18;
    profile.phases[4] = func_801E4BF4_60FFD4;
    profile.phases[5] = func_801E4C5C_61003C;
    profile.phases[6] = func_801E4CE8_6100C8;
    profile.phases[7] = func_801E4DF8_6101D8;
    profile.phases[8] = func_801E4F2C_61030C;
    profile.phases[9] = func_801E4FB0_610390;
    profile.phases[10] = func_801E4FD4_6103B4;
    profile.phases[11] = func_801E5064_610444;
    profile.phases[12] = func_801E5144_610524;
    profile.phases[13] = func_801E51EC_6105CC;
    profile.phases[14] = func_801E5274_610654;
    profile.phases[15] = func_801E52CC_6106AC;
    profile.phases[16] = func_801E52D8_6106B8;
    profile.phases[17] = func_801E59F8_610DD8;
    profile.phases[18] = func_801E5A88_610E68;
    profile.phases[19] = func_801E5BDC_610FBC;
    profile.phases[20] = func_801E5D38_611118;
    profile.phases[21] = func_801E5DAC_61118C;
    profile.phases[22] = func_801E5E14_6111F4;
    profile.phases[23] = func_801E5EE0_6112C0;
    profile.phases[24] = func_801E5F64_611344;
    profile.phases[25] = func_801E6024_611404;
    profile.phases[26] = func_801E607C_61145C;
    profile.phases[27] = func_801E60CC_6114AC;
    profile.phases[28] = func_801E6150_611530;
    profile.phases[29] = func_801E61A8_611588;
    profile.phases[30] = func_801E628C_61166C;
    profile.phases[31] = func_801E6370_611750;
    profile.phases[32] = func_801E641C_6117FC;
    profile.phases[33] = func_801E6474_611854;
    profile.phases[34] = func_801E651C_6118FC;
    profile.phases[35] = func_801E6600_6119E0;
    profile.phases[36] = func_801E66C4_611AA4;
    profile.phases[37] = func_801E6778_611B58;
    profile.phases[38] = func_801E67D4_611BB4;
    profile.phases[39] = func_801E6854_611C34;
    profile.phases[40] = func_801E69F4_611DD4;
    profile.phases[41] = func_801E6D00_6120E0;
    profile.phases[42] = func_801E6DE0_6121C0;
    profile.phases[43] = func_801E6ECC_6122AC;
    profile.phases[44] = func_801E6FC4_6123A4;
    profile.phases[45] = func_801E700C_6123EC;
    profile.phases[46] = func_801E7064_612444;
    profile.phases[47] = func_801E7134_612514;
    profile.phases[48] = func_801E71E8_6125C8;
    profile.phases[49] = func_801E72F4_6126D4;
    profile.phases[50] = func_801E752C_61290C;
    profile.phases[51] = func_801E75E4_6129C4;
    profile.phases[52] = func_801E7714_612AF4;
    profile.phases[53] = func_801E7838_612C18;
    profile.phases[54] = func_801E79E4_612DC4;
    profile.phases[55] = func_801E7C50_613030;
    profile.phases[56] = func_801E7CA8_613088;
    profile.phases[57] = func_801E7FB0_613390;
    profile.phases[58] = func_801E8108_6134E8;
    profile.phases[59] = func_801E8220_613600;
    profile.phases[60] = func_801E8488_613868;
    profile.phases[61] = func_801E88C8_613CA8;
    profile.phases[62] = func_801E8C04_613FE4;
    profile.phases[63] = func_801E8CDC_6140BC;
    profile.phases[64] = func_801E8E58_614238;
    profile.phases[65] = func_801E8FA4_614384;
    profile.phases[66] = func_801E91BC_61459C;
    profile.phases[67] = func_801E9274_614654;
    profile.phases[68] = func_801E9624_614A04;
    profile.phases[69] = func_801E9C90_615070;
    profile.phases[70] = func_801E9E68_615248;
    profile.clips[1] = D_8020B170_636550;
    profile.clips[2] = D_8020B178_636558;
    profile.clips[3] = D_8020B180_636560;
    profile.clips[4] = D_8020B188_636568;
    profile.clips[5] = D_8020B190_636570;
    profile.clips[6] = D_8020B198_636578;
    profile.clips[7] = D_8020B1A0_636580;
    profile.clips[8] = D_8020B1A8_636588;
    profile.clips[9] = D_8020B1B0_636590;
    profile.clips[10] = D_8020B1B8_636598;
    profile.clips[11] = D_8020B1C0_6365A0;
    profile.clips[12] = D_8020B1C8_6365A8;
    profile.clips[13] = D_8020B1D0_6365B0;
#undef profile
    return &kashiwagi_profile;
}

RECOMP_HOOK("func_801E4800_60FBE0")
void anchor_impact_bind_1(void *task)
{
    (void)anchor_impact_native_bind(task, 1);
}

RECOMP_HOOK("func_801E9624_614A04")
void anchor_impact_kashiwagi_reel_begin(void *task)
{ anchor_impact_players_reel_begin(task); }
RECOMP_HOOK_RETURN("func_801E9624_614A04")
void anchor_impact_kashiwagi_reel_end(void)
{ anchor_impact_players_reel_end(); }
