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
    TU16(root,0x5C) = encounter == 1 ? 0x50 : 0x5A;
    TU32(battle,0x60) = 2000; TU32(battle,0x68) = 999; TU32(battle,0x64) = 100;
    for (i = 0; i < 3; ++i) TF32(model,0x1C+4*i) = 1;
    TU32(root,0x0C) = anchor_impact_phase_callback(encounter,21);
    func_801D2EE4_5FE2C4(model,anchor_impact_clip_data(encounter,1));
    if (encounter == 1) anchor_impact_bind_1(root);
    else anchor_impact_bind_2(root);
    assert(anchor_impact_native_snapshot_ready());
    anchor_impact_native_set_role(1,1,0);
}
int main(void) {
    AnchorImpactNativeSnapshot shot, bad;
    unsigned int i, e, first_visit;
    const AnchorImpactBossProfile *p;
    anchor_impact_catalog_init();
    /* Every real profile phase round trips, including IDs beyond the old 127 limit. */
    for (e = 1; e <= 2; ++e) {
        p = anchor_impact_boss_profile(e);
        for (i = 1; i < ANCHOR_IMPACT_PHASES; ++i) if (p->phases[i]) {
            assert(anchor_impact_phase_id(e,address(p->phases[i])) == i);
            assert(anchor_impact_phase_id(e,address(p->phases[i]) | 0x00800000u) == i);
        }
        for (i = 1; i <= (e == 1 ? 13u : 16u); ++i) {
            const unsigned int *clip = anchor_impact_clip_data(e,i);
            assert(clip && (clip[1]&0xFFFF) == (e == 1 ? 0x4B0u : 0x4B2u));
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
    /* Both logs reuse the root address for Balberra. The legacy catalog is
     * not a complete sync profile: leave its AI and native graph alone. */
    TU16(system_data,0x3ADF4) = 3;
    TU16(root,0x5C) = 0x78;
    TU32(root,0x0C) = anchor_impact_phase_callback(3,1);
    anchor_impact_bind_3(root);
    anchor_impact_native_tick();
    assert(!anchor_impact_native_snapshot_ready());
    assert(!anchor_impact_native_capture(&bad));
    assert(!anchor_impact_native_apply(&shot));
    anchor_impact_native_scheduler_begin(); anchor_impact_native_scheduler_end();
    assert(TU32(battle,0x60) == 1000);
    assert(TU32(root,0x0C) == anchor_impact_phase_callback(3,1));
    D_800C7AB2 = 0x263; TU16(system_data,0x3ADF4) = 4;
    TU16(root,0x5C) = 0x64;
    anchor_impact_bind_4(root); anchor_impact_native_tick();
    assert(!anchor_impact_native_snapshot_ready());
    /* A different boss cannot use the previous Taisamba reference view. */
    setup(1); anchor_impact_taisamba_weapon_begin(root); assert(!TP(root,0x9C));
    puts("Impact Kashiwagi/Taisamba real profile, arena, pointer dependency and promotion tests passed");
    return 0;
}
