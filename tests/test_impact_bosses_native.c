#include <stdio.h>
#include "impact_test_pointers.h"
#define ANCHOR_IMPACT_BOSS_HOST_TEST
#define ANCHOR_IMPACT_NATIVE_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define IB_PTR(p,o) TP(p,o)
#define ANCHOR_IMPACT_READ_PTR(p,o) TP(p,o)
#define ANCHOR_IMPACT_AI(p) TU32(p,0x0C)
#define ANCHOR_IMPACT_SET_AI(p,v) (TU32(p,0x0C) = (v))
#include "../src/anchor_impact_kashiwagi.c"
#include "../src/anchor_impact_taisamba.c"
#include "../src/anchor_impact_balberra.c"
#include "../src/anchor_impact_detoile.c"
#include "../src/anchor_impact_boss.c"
#include "../src/utils/anchor_impact_catalog.c"
#include "../src/utils/anchor_impact_visual_catalog.c"
#include "../src/anchor_impact_native.c"

static unsigned int battle[0x300/4], root[64], model[64], aux[8];
static unsigned int weapon[64], carrier[64], carrier_model[64];
static unsigned char system_data[0x40000];
unsigned short D_800C7AB2 = 0x260;
unsigned char *D_8015C5C8_15D1C8 = system_data;
void *D_8020EED0_63A2B0 = battle, *D_8020EF30_63A310 = aux;
void *D_8020EF40_63A320, *D_8016DAB4_16E6B4;
static unsigned int reel_begin_count, reel_end_count;
static void *reaction_source;
void anchor_impact_players_source_axes_begin(void *task) { reaction_source = task; }
void anchor_impact_players_source_axes_end(void) { reaction_source = 0; }
void anchor_impact_players_source_aim_begin(void *task) { reaction_source = task; }
void anchor_impact_players_source_aim_end(void) { reaction_source = 0; }
/* Title-menu boss rush never establishes an ordinary loaded-room visit. */
unsigned int anchor_boss_invite_world_visit(void) { return 0; }
void anchor_impact_visuals_begin_frame(void) {}
void anchor_impact_visuals_render(void) {}
void anchor_impact_players_reel_begin(void *task) { assert(task == root); ++reel_begin_count; }
void anchor_impact_players_reel_end(void) { ++reel_end_count; }
void func_801D2EE4_5FE2C4(void *object, const void *data) {
    const unsigned int *clip = data;
    TU32(object,0x2C) = clip[0]; TU16(object,0x34) = (unsigned short)clip[1];
    TF32(object,0x28) = 0;
}
static unsigned int address(AnchorImpactCallback c) { return (unsigned int)(uintptr_t)c; }
static void setup(unsigned int encounter) {
    unsigned int i;
    D_800C7AB2 = 0x25F + encounter;
    memset(battle,0,sizeof(battle)); memset(root,0,sizeof(root));
    memset(model,0,sizeof(model)); memset(aux,0,sizeof(aux));
    memset(test_ptrs,0,sizeof(test_ptrs)); test_ptr_count = 0;
    TP(battle,0x1D8) = root; TP(root,0x18) = model; TP(battle,0x1E0) = model;
    TU16(system_data,0x3ADF4) = encounter;
    TU16(root,0x5C) = anchor_impact_boss_profile(encounter)->task_id;
    TU32(battle,0x60) = 2000; TU32(battle,0x68) = 999; TU32(battle,0x64) = 100;
    for (i = 0; i < 3; ++i) TF32(model,0x1C+4*i) = 1;
    TU32(root,0x0C) = anchor_impact_phase_callback(encounter,21);
    if (encounter != 3) func_801D2EE4_5FE2C4(model,anchor_impact_clip_data(encounter,1));
    else { TU32(model,0x2C) = 0x48016730; TU16(model,0x34) = 0x4B5; TU32(root,0xAC) = 1000; }
    assert(anchor_impact_native_bind(root,encounter));
    assert(anchor_impact_native_snapshot_ready());
    anchor_impact_native_set_role(1,1,0);
}
static void test_final_bosses(void);
int main(void) {
    AnchorImpactNativeSnapshot shot, bad;
    unsigned int i, e, first_visit;
    const AnchorImpactBossProfile *p;
    anchor_impact_catalog_init();
    /* Every real profile phase round trips, including IDs beyond the old 127 limit. */
    for (e = 1; e <= 4; ++e) {
        p = anchor_impact_boss_profile(e);
        for (i = 1; i < ANCHOR_IMPACT_PHASES; ++i) if (p->phases[i]) {
            assert(anchor_impact_phase_id(e,address(p->phases[i])) == i);
            assert(anchor_impact_phase_id(e,address(p->phases[i]) | 0x00800000u) == i);
        }
        for (i = 1; i <= (e == 1 ? 13u : e == 2 ? 16u : e == 3 ? 0u : 14u); ++i) {
            const unsigned int *clip = anchor_impact_clip_data(e,i);
            assert(clip && (clip[1]&0xFFFF) == (e == 1 ? 0x4B0u : e == 2 ? 0x4B2u : 0x4B5u));
            assert(anchor_impact_clip_id(e,clip[0],clip[1]&0xFFFF) == i);
            assert(anchor_impact_visual_recipe_id(clip[0],clip[1]&0xFFFF));
        }
    }
    assert(anchor_impact_phase_callback(1,68) == address(func_801E9624_614A04));
    assert(anchor_impact_phase_callback(2,129) == address(func_801F38DC_61ECBC));
    assert(anchor_impact_phase_callback(2,128) == address(func_801F16E0_61CAC0));
    assert(!anchor_impact_phase_callback(1,129));
    assert(anchor_impact_visual_recipe_id(0x4800F5A0,0x4B2));
    assert(anchor_impact_visual_recipe_id(0x4800FE90,0x4B2));
    assert(anchor_impact_boss_is_reel_callback(address(func_801F5F0C_6212EC)&~0x00800000u));
    setup(1);
    anchor_impact_kashiwagi_reel_begin(root); anchor_impact_kashiwagi_reel_end();
    assert(anchor_impact_native_capture(&shot));
    first_visit = anchor_impact_native_visit();
    assert(first_visit > 0 && shot.stage == 0x260);
    for (i = IMP_BOSS_DATA; i < ANCHOR_IMPACT_ROOT_WORDS; ++i) assert(!shot.root[i]);
    bad = shot; bad.root[TS_ASCEND] = 1; assert(!anchor_impact_native_apply(&bad));
    setup(2);
    assert(anchor_impact_native_stage() == 0x261);
    assert(anchor_impact_native_visit() > first_visit);
    assert(!anchor_impact_native_apply(&shot)); /* Old Kashiwagi checkpoint. */
    anchor_impact_native_reset();
    anchor_impact_native_tick(); /* Recover even if an initializer hook was missed. */
    assert(anchor_impact_native_snapshot_ready());
    anchor_impact_native_set_role(1,1,0);
    anchor_impact_taisamba_reel_begin(root); anchor_impact_taisamba_reel_end();
    assert(reel_begin_count == 2 && reel_end_count == 2);
    TF32(battle,0x17C) = 1; TF32(battle,0x188) = 200;
    ((unsigned char *)battle)[0x2C5] = 1; TU32(battle,0x70) = 54;
    ((unsigned char *)root)[0x30] = 2; TU32(root,0x48) = 0xFFFFFFFF;
    ((unsigned char *)root)[0x4C] = 0x5A;
    TP(battle,0x174) = root;
    TU32(root,0xD8) = 22; TF32(root,0x74) = 8;
    TU32(root,0x0C) = address(func_801F3950_61ED30);
    TP(root,0x9C) = weapon; TU16(weapon,0x5C) = 0x5B;
    TP(root,0xDC) = carrier; TP(carrier,0x18) = carrier_model;
    TU32(aux,8) = 0xFFFFFFFE; TU16(aux,4) = 3;
    assert(anchor_impact_native_capture(&shot));
    assert(shot.root[TS_WEAPON_ACTIVE] == 1);
    assert(!shot.root[IMP_PRIVATE+9] && !shot.root[IMP_PRIVATE+19]);
    assert(shot.root[IMP_PRIVATE+18] == 22);
    /* New receiver: arena, graph links and camera differ from the owner. */
    TP(root,0x9C) = 0; TP(root,0xDC) = 0;
    TF32(battle,0x188) = 0; TF32(battle,0x17C) = 0;
    TF32(battle,4) = 71; TF32(battle,8) = -12; TF32(battle,0xA0) = 345;
    TP(aux,0) = carrier; /* native task list is never overwritten */
    anchor_impact_native_set_role(1,0,0);
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    assert(TF32(battle,0x188) == 200 && TF32(battle,0x17C) == 1);
    assert(TU32(battle,0x70) == 54 && TP(battle,0x174) == root);
    assert(TP(aux,0) == carrier && TU32(aux,8) == 0xFFFFFFFE);
    assert(TF32(battle,4) == 71 && TF32(battle,8) == -12 && TF32(battle,0xA0) == 345);
    assert(!TP(root,0x9C) && !TP(root,0xDC));
    anchor_impact_taisamba_weapon_begin(root);
    assert(TU16(TP(root,0x9C),0x5C) == 0x5B);
    anchor_impact_taisamba_weapon_end(); assert(!TP(root,0x9C));
    shot.root[TS_WEAPON_ACTIVE] = 0;
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    anchor_impact_taisamba_weapon_begin(root);
    assert(TU16(TP(root,0x9C),0x5C) == 0);
    anchor_impact_taisamba_weapon_end(); assert(!TP(root,0x9C));
    /* A carry checkpoint references another process's limb by scalar data. */
    shot.root[IMP_PHASE] = 117; shot.root[TS_CARRY_VALID] = 1;
    shot.root[TS_CARRY_POS] = impact_pose_bits(123);
    shot.root[TS_CARRY_VELOCITY+1] = impact_pose_bits(4);
    shot.root[TS_CARRY_YAW] = 700;
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    anchor_impact_taisamba_carrier_begin(root);
    assert(TF32(TP(TP(root,0xDC),0x18),8) == 123);
    assert(TF32(TP(root,0xDC),0x74) == 4);
    assert(TU16(TP(TP(root,0xDC),0x18),0x16) == 700);
    anchor_impact_taisamba_carrier_end(); assert(!TP(root,0xDC));
    /* Invalid scalar/pointer words cannot partially write HP or arena state. */
    bad = shot; bad.root[TS_WORLD+4] = 0x7FC00000; bad.root[IMP_BOSS_HP] = 1;
    assert(!anchor_impact_native_apply(&bad)); assert(TU32(battle,0x60) == 2000);
    bad = shot; bad.root[IMP_PRIVATE+19] = 0x80400000; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[TS_END] = 1; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[TS_REEL] = 101; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[TS_CARRY_VALID] = 0; assert(!anchor_impact_native_apply(&bad));
    /* Promotion with a missing local limb recovers without dereferencing NULL. */
    anchor_impact_native_set_role(1,1,0);
    anchor_impact_taisamba_task_begin(root);
    assert(!TP(root,0xDC) && !TP(battle,0x174));
    assert(TU32(root,0x0C) == address(func_801F0540_61B920));
    assert(anchor_impact_boss_sound_valid(2,0x4A));
    assert(!anchor_impact_boss_sound_valid(1,0x4A));
    /* Boss-only aim readers use the collision limb's shared source binding. */
    TP(root,0xDC) = carrier;
    anchor_impact_taisamba_grab_begin(root); assert(reaction_source == carrier);
    anchor_impact_taisamba_grab_end(); assert(!reaction_source);
    anchor_impact_taisamba_release_begin(root); assert(reaction_source == carrier);
    anchor_impact_taisamba_release_end(); assert(!reaction_source);
    /* Defeat survives the combat/outro pause without changing local aim. */
    shot.root[IMP_BOSS_HP] = 0; shot.root[TS_DEFEATED] = 1;
    ((unsigned char *)battle)[0x2C0] = 1;
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    assert(TU32(battle,0x60) == 0 && ((unsigned char *)battle)[0x2C4] == 1);
    assert(TF32(battle,4) == 71 && TF32(battle,8) == -12);
    /* The rush advances before the next initializer replaces the old root.
     * A pending Taisamba defeat must not survive that boundary. */
    assert(anchor_impact_native_apply(&shot));
    D_800C7AB2 = 0x262;
    TU32(battle,0x60) = 1000;
    anchor_impact_native_scheduler_begin();
    assert(TU32(battle,0x60) == 1000);
    assert(!anchor_impact_native_ready());
    /* Reused root binds to the next fully supported boss, but rejects the
     * previous boss's pending defeat. */
    TU16(system_data,0x3ADF4) = 3; TU16(root,0x5C) = 0x78;
    TU32(root,0xAC) = 1000; TU32(root,0x0C) = anchor_impact_phase_callback(3,1);
    anchor_impact_bind_3(root); anchor_impact_native_tick();
    assert(anchor_impact_native_snapshot_ready());
    assert(anchor_impact_native_capture(&bad));
    assert(!anchor_impact_native_apply(&shot));
    assert(TU32(battle,0x60) == 1000);
    D_800C7AB2 = 0x263; TU16(system_data,0x3ADF4) = 4;
    TU16(root,0x5C) = 0x64; anchor_impact_bind_4(root); anchor_impact_native_tick();
    assert(anchor_impact_native_snapshot_ready());
    assert(!anchor_impact_native_apply(&bad));
    test_final_bosses();
    /* A different boss cannot use the previous Taisamba reference view. */
    setup(1); anchor_impact_taisamba_weapon_begin(root); assert(!TP(root,0x9C));
    puts("Impact Kashiwagi/Taisamba real profile, arena, pointer dependency and promotion tests passed");
    return 0;
}

static void test_final_bosses(void)
{
    static unsigned int last_aux[0x820/4], parts[12][64], objects[12][64];
    AnchorImpactNativeSnapshot shot, bad, previous;
    unsigned int i,j, before, visit;
    for (i = 0; i < BAL_PARTS; ++i) assert(anchor_impact_visual_recipe_id(bal_models[i],0x4B5));
    assert(anchor_impact_visual_recipe_id(0x480199E0,0x4B5));
    assert(anchor_impact_visual_recipe_id(0x480008B0,0x4B6));
    assert(anchor_impact_visual_recipe_id(0x48000170,0x4AE));
    setup(3); D_8020EF40_63A320 = last_aux;
    memset(last_aux,0,sizeof(last_aux)); memset(parts,0,sizeof(parts)); memset(objects,0,sizeof(objects));
    TU32(last_aux,0) = 0x81234560; TU32(last_aux,0x14) = 0xDEADBEEF;
    IB_U8(last_aux,0x814) = 37; IB_U8(last_aux,6) = 3; IB_U8(last_aux,8) = 2;
    TP(root,0) = parts[0]; TU16(root,0x20) = 2;
    for (i = 0; i < 12; ++i) {
        void *p = parts[i], *o = objects[i];
        TP(p,4) = i ? parts[i-1] : root; TP(p,0) = i == 11 ? 0 : parts[i+1];
        TU16(p,0x20) = i >= 4 ? 4 : 3; TU16(p,0x5C) = bal_ids[i];
        TP(p,0x18) = o; TP(p,0xB4) = i >= 4 ? parts[2] : root;
        TP(p,0xB8) = i >= 4 ? objects[2] : model;
        TU32(p,0x0C) = address(bal_callback(i,0)); TU32(o,0x2C) = bal_models[i];
        TU32(p,0xAC) = 50; TU32(p,0x7C) = 8+i; TU32(p,0x80) = 16+i;
        TU32(p,0x90) = i >= 6 ? i-6 : 0; TF32(p,0xBC) = 174; TF32(o,0x28) = 3;
    }
    TU32(root,0x0C) = address(func_80200D4C_62C12C); TU32(root,0x7C) = 740;
    TU32(root,0xAC) = 712; TU32(root,0xD8) = 8;
    TF32(battle,4) = 9; TF32(battle,8) = -4; TF32(battle,0xA0) = 999;
    TF32(battle,0x188) = 200;
    assert(anchor_impact_native_capture(&shot));
    assert(shot.root[IMP_PHASE] == 32 && shot.root[IMP_BOSS_HP] == 712);
    assert(shot.root[IMP_AUX_DATA] == 0x00000300 && shot.root[IMP_AUX_DATA+1] == 0x02000000);
    for (i = 0; i < 12; ++i) assert(shot.root[BAL_PART_BASE+i*8] == 0x10001);
    /* Every native child survives independent HP/phase reconciliation, including
     * the three rocket tasks sharing ID 0x71. No pointer comes from a peer. */
    for (i = 0; i < 12; ++i) {
        unsigned int *w = shot.root+BAL_PART_BASE+i*8;
        if (i != 2 && i != 3) { w[0] = 2; w[1] = 0; }
        else w[6] = impact_pose_bits(i == 2 ? 250 : -250);
    }
    anchor_impact_native_set_role(1,0,0);
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    for (i = 0; i < 12; ++i) {
        assert(TP(parts[i],0xB4) == (i >= 4 ? parts[2] : root));
        assert(TP(parts[i],0xB8) == (i >= 4 ? objects[2] : model));
        assert(TU32(parts[i],0x7C) == 8+i && TU32(parts[i],0x80) == 16+i);
        if (i != 2 && i != 3) {
            assert(TU32(parts[i],0xAC) == 0 && TU32(objects[i],0x2C) == 0);
            assert(TU32(parts[i],0x0C) == address(bal_callback(i,1)));
        }
    }
    assert(TF32(parts[2],0xBC) == 250 && TF32(parts[3],0xBC) == -250);
    assert(TU32(root,0xAC) == 712 && TF32(battle,0x188) == 200);
    assert(TF32(battle,4) == 9 && TF32(battle,8) == -4 && TF32(battle,0xA0) == 999);
    assert(TU32(last_aux,0) == 0x81234560 && TU32(last_aux,0x14) == 0xDEADBEEF && IB_U8(last_aux,0x814) == 37);
    bad = shot; bad.root[BAL_PART_BASE+11*8+4] = 99; bad.root[IMP_BOSS_HP] = 1;
    assert(!anchor_impact_native_apply(&bad)); assert(TU32(root,0xAC) == 712);
    bad = shot; bad.root[BAL_PART_BASE+6] = 0x7F800000; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[IMP_PRIVATE+19] = 0x80400000; assert(!anchor_impact_native_apply(&bad));
    /* Reapply a live part checkpoint to a locally dead part (owner promotion
     * or a corrected speculative reaction) using its local asset recipe. */
    shot.root[BAL_PART_BASE] = 0x10101; shot.root[BAL_PART_BASE+1] = 42;
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    assert(TU32(objects[0],0x2C) == bal_models[0] && TU32(parts[0],0xAC) == 42);
    /* Native story handoff: same scene and same root address, selector becomes
     * four before D'Etoile's initializer assigns ID 0x64. */
    D_800C7AB2 = 0x222; assert(anchor_impact_native_capture(&previous));
    previous.root[IMP_BOSS_HP] = 0; previous.root[IB_DEFEATED] = 1;
    assert(anchor_impact_native_apply(&previous)); visit = anchor_impact_native_visit();
    TU16(system_data,0x3ADF4) = 4; IB_U8(battle,0x2C4) = 0; TU32(battle,0x60) = 2000;
    anchor_impact_native_scheduler_begin(); assert(TU32(battle,0x60) == 2000 && !IB_U8(battle,0x2C4));
    assert(!anchor_impact_native_ready());
    anchor_impact_bind_4(root); assert(!anchor_impact_native_root_live());
    TU16(root,0x5C) = 0x64; TP(root,0) = 0;
    TU32(root,0x0C) = address(func_801FBD6C_62714C);
    func_801D2EE4_5FE2C4(model,anchor_impact_clip_data(4,1));
    assert(anchor_impact_native_root_live()); assert(anchor_impact_native_visit() > visit);
    assert(!anchor_impact_native_apply(&previous));
    /* Shield's command pulse has already been consumed; sync the resulting
     * spinning/broken child with all local graph links retained. */
    memset(parts,0,sizeof(parts)); memset(objects,0,sizeof(objects));
    TP(root,0) = parts[0]; TP(parts[0],4) = root; TP(parts[0],0) = 0;
    TU16(parts[0],0x20) = 3; TP(parts[0],0x18) = objects[0];
    TP(parts[0],0xB4) = root; TP(parts[0],0xB8) = model;
    TU32(parts[0],0x0C) = address(func_8020679C_631B7C);
    TU32(parts[0],0x7C) = 21; TU32(parts[0],0x90) = 16; TF32(parts[0],0xBC) = -156;
    TU32(objects[0],0x2C) = 0x480199E0; TU16(objects[0],0x16) = 256;
    IB_U8(last_aux,0x816) = 0; IB_U8(last_aux,0x815) = 1; IB_U8(last_aux,0x817) = 180;
    TP(root,0xDC) = carrier; TP(carrier,0x18) = carrier_model;
    TF32(carrier_model,8) = 123; TF32(carrier,0x74) = 4; TU16(carrier_model,0x16) = 700;
    TU32(root,0x0C) = address(func_801FF528_62A908);
    anchor_impact_native_set_role(1,1,0); assert(anchor_impact_native_capture(&shot));
    assert(shot.root[IMP_PHASE] == 67 && shot.root[DT_CARRY] == 1 && !shot.root[IMP_PRIVATE+19]);
    assert(shot.root[DT_SHIELD] == 0x10002 && shot.root[DT_SHIELD+1] == 21);
    TP(root,0xDC) = 0; TU32(parts[0],0x0C) = address(func_802065B8_631998);
    TU32(objects[0],0x2C) = 0; TU32(parts[0],0x7C) = 0;
    anchor_impact_native_set_role(1,0,0);
    assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
    assert(TU32(parts[0],0x0C) == address(func_8020679C_631B7C));
    assert(TU32(parts[0],0x7C) == 21 && TU32(objects[0],0x2C) == 0x480199E0);
    assert(TU16(objects[0],0x16) == 256 && TP(parts[0],0xB4) == root);
    assert(IB_U8(last_aux,0x815) == 1 && IB_U8(last_aux,0x817) == 180);
    assert(TU32(last_aux,0x14) == 0xDEADBEEF && IB_U8(last_aux,0x814) == 37);
    anchor_impact_detoile_carrier_begin(root);
    assert(TF32(TP(TP(root,0xDC),0x18),8) == 123 && TF32(TP(root,0xDC),0x74) == 4);
    anchor_impact_detoile_carrier_end(); assert(!TP(root,0xDC));
    bad = shot; bad.root[DT_CARRY] = 0; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[DT_CARRY+1] = 0x7FC00000; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[DT_SHIELD+6] = 65536; assert(!anchor_impact_native_apply(&bad));
    bad = shot; bad.root[IMP_PRIVATE+19] = 0x80400000; assert(!anchor_impact_native_apply(&bad));
    anchor_impact_native_set_role(1,1,0); anchor_impact_detoile_task_begin(root);
    assert(TU32(root,0x0C) == address(func_801FC3FC_6277DC));
    TP(root,0xDC) = carrier; anchor_impact_detoile_release_begin(root); assert(reaction_source == carrier);
    anchor_impact_detoile_release_end(); assert(!reaction_source);
    anchor_impact_detoile_grapple_begin(root); assert(reaction_source == carrier);
    anchor_impact_detoile_grapple_end(); assert(!reaction_source);
    before = reel_begin_count; anchor_impact_detoile_reel_begin(root); anchor_impact_detoile_reel_end();
    assert(reel_begin_count == before+1 && reel_end_count == reel_begin_count);
    /* Last defeat exits both story and rush without a stale cached HP write. */
    for (j = 0; j < 2; ++j) {
        D_800C7AB2 = j ? 0x263 : 0x222;
        assert(anchor_impact_native_capture(&shot)); shot.root[IB_DEFEATED] = 1; shot.root[IMP_BOSS_HP] = 0;
        IB_U8(battle,0x2C0) = 1;
        assert(anchor_impact_native_apply(&shot)); anchor_impact_native_scheduler_begin();
        assert(IB_U8(battle,0x2C4) == 1 && TU32(battle,0x60) == 0);
        assert(anchor_impact_native_apply(&shot));
        D_800C7AB2 = j ? 0x25F : 0x100; TU32(battle,0x60) = 1000;
        anchor_impact_native_scheduler_begin(); assert(TU32(battle,0x60) == 1000);
    }
    assert(anchor_impact_boss_sound_valid(3,9) && anchor_impact_boss_sound_valid(4,0x38));
    puts("Balberra parts, D'Etoile shield/carry, story handoff and final defeat tests passed");
}
