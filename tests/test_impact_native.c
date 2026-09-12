#include <stdio.h>
#include "impact_test_pointers.h"
#define ANCHOR_IMPACT_NATIVE_HOST_TEST
#define ANCHOR_IMPACT_READ_PTR(p, o) TP(p, o)
#define ANCHOR_IMPACT_AI(p) TU32(p, 0x0C)
#define ANCHOR_IMPACT_SET_AI(p, v) (TU32(p, 0x0C) = (v))
#include "../src/anchor_impact_native.c"

static unsigned char state[0x300], system_data[0x40000];
static unsigned int task[0x100], object[0x80], hand[0x80];
static unsigned int auxiliary[0x820/4];
unsigned short D_800C7AB2 = 0x260;
unsigned char *D_8015C5C8_15D1C8 = system_data;
void *D_8020EED0_63A2B0 = state, *D_8016DAB4_16E6B4;
void *D_8020EF30_63A310 = auxiliary, *D_8020EF40_63A320 = auxiliary;
static unsigned int visit, clip_calls;
static unsigned int clip_words[] = {0x18001234, 0x140004B0};
unsigned int anchor_boss_invite_world_visit(void) { return visit; }
void anchor_impact_catalog_init(void) {}
unsigned int anchor_impact_phase_id(unsigned int e, unsigned int c) {
    return e == 1 && (c & ~0x00800000u) == 0x801E493Cu ? 1 : 0;
}
unsigned int anchor_impact_phase_callback(unsigned int e, unsigned int p) {
    return e == 1 && p == 1 ? 0x801E493Cu : 0;
}
unsigned int anchor_impact_clip_id(unsigned int e, unsigned int m, unsigned int f) {
    return e == 1 && m == clip_words[0] && f == 0x4B0 ? 1 : 0;
}
const void *anchor_impact_clip_data(unsigned int e, unsigned int c) {
    return e == 1 && c == 1 ? clip_words : 0;
}
unsigned int anchor_impact_private_offset(unsigned int i) { return 0x70 + i * 4; }
int anchor_impact_private_used(unsigned int e, unsigned int i) { return e == 1 && i < 3; }
void func_801D2EE4_5FE2C4(void *o, const void *c) {
    const unsigned int *r = c;
    ++clip_calls; TU32(o, 0x2C) = r[0]; TU16(o, 0x34) = (unsigned short)r[1];
    TF32(o, 0x28) = 0;
}
static void setup(void) {
    unsigned int i;
    memset(state, 0, sizeof(state)); memset(task, 0, sizeof(task));
    memset(object, 0, sizeof(object)); memset(hand, 0, sizeof(hand));
    memset(test_ptrs, 0, sizeof(test_ptrs)); test_ptr_count = 0;
    TU16(system_data, 0x3ADF4) = 1; D_800C7AB2 = 0x260;
    TP(state, 0x1D8) = task; TP(task, 0x18) = object; TP(state, 0x1C) = hand;
    TU16(task, 0x5C) = 0x50; TU32(task, 0x0C) = 0x801E493C;
    TU32(state, 0x60) = 2000; TU32(state, 0x64) = 100; TU32(state, 0x68) = 999;
    TU32(object, 0x2C) = clip_words[0]; TU16(object, 0x34) = 0x4B0;
    ((unsigned char *)object)[5] = 2;
    for (i = 0; i < 3; ++i) TF32(object, 0x1C + 4*i) = TF32(hand, 0x1C + 4*i) = 1;
    TF32(object, 0x08) = 50; TF32(object, 0x28) = 12;
    TF32(task, 0x70) = 3; TF32(hand, 0x08) = 24;
    TF32(state, 0xA0) = 12; TU16(state, 0xBA) = 40;
    clip_calls = 0; visit = 0;
    assert(anchor_impact_native_bind(task, 1));
    anchor_impact_native_set_role(1, 0, 0);
}
int main(void) {
    AnchorImpactNativeSnapshot snapshot, bad;
    unsigned int before, fallback;
    setup();
    assert(ANCHOR_IMPACT_ROOT_WORDS == 172);
    assert(anchor_impact_native_capture(&snapshot));
    assert(snapshot.root[IMP_PHASE] == 1 && snapshot.root[IMP_CLIP] == 1);
    assert(snapshot.root[IMP_MECH_MASK] == 1);
    fallback = anchor_impact_native_visit(); assert(fallback > 0);
    visit = 17; assert(anchor_impact_native_visit() == 17);
    /* Validate before queueing, including encounter, callback and float bits. */
    bad = snapshot; bad.root[IMP_PHASE] = 99;
    assert(!anchor_impact_native_apply(&bad));
    bad = snapshot; bad.root[IMP_MECH_POSES] = 0x7FC00000;
    assert(!anchor_impact_native_apply(&bad));
    bad = snapshot; bad.root[IMP_CAMERA] = 0x7F800000;
    assert(!anchor_impact_native_apply(&bad));
    bad = snapshot; bad.encounter = 2;
    assert(!anchor_impact_native_apply(&bad));
    assert(!s_pending_valid);
    /* Phase, animation and private motion land together before native AI. */
    snapshot.root[IMP_BOSS_HP] = 1800;
    TU32(task, 0x0C) = 0x801E4B38; TF32(task, 0x70) = 123;
    TF32(object, 0x28) = 99; TF32(object, 0x08) = -99;
    assert(anchor_impact_native_apply(&snapshot));
    assert(TU32(state, 0x60) == 2000);
    anchor_impact_native_scheduler_begin();
    assert(TU32(state, 0x60) == 1800 && TU32(task, 0x0C) == 0x801E493C);
    assert(TF32(task, 0x70) == 3 && TF32(object, 0x28) == 12);
    assert(TF32(object, 0x08) == 50 && clip_calls == 1);
    /* The shared camera and cockpit survive the follower's frame update. */
    TF32(hand, 0x08) = 0; TF32(state, 0xA0) = 0;
    anchor_impact_native_scheduler_end();
    assert(TF32(hand, 0x08) == 24 && TF32(state, 0xA0) == 12);
    anchor_impact_native_set_role(1, 1, 0); TF32(hand, 0x08) = 5;
    anchor_impact_native_scheduler_end(); assert(TF32(hand, 0x08) == 5);
    /* Do not jump out of a native intro before its graph has been created. */
    state[0x2C0] = 1; before = TU32(task, 0x0C); snapshot.root[IMP_BOSS_HP] = 1700;
    assert(anchor_impact_native_apply(&snapshot)); anchor_impact_native_scheduler_begin();
    assert(TU32(state, 0x60) == 1700 && TU32(task, 0x0C) == before && clip_calls == 1);
    /* A replaced root cannot receive a queued checkpoint or cached view. */
    TP(state, 0x1D8) = hand; assert(!anchor_impact_native_root_live());
    assert(!anchor_impact_native_apply(&snapshot));
    anchor_impact_native_reset(); assert(!s_view_valid && !s_pending_valid);
    setup(); TU16(system_data,0x3ADF4) = 3; TU16(task,0x5C) = 0x78;
    assert(anchor_impact_native_bind(task,3));
    anchor_impact_native_set_role(1,0,0);
    assert(anchor_impact_native_capture(&snapshot));
    snapshot.root[IMP_BOSS_HP] = 800; TU32(task,0xAC) = 1200;
    snapshot.root[IMP_AUX_DATA] = 0x50010301;
    snapshot.root[IMP_AUX_DATA+4] = 0x01010100;
    TU32(auxiliary,0) = 0x81234560; TU32(auxiliary,0x14) = 0x81234570;
    assert(anchor_impact_native_apply(&snapshot)); anchor_impact_native_scheduler_begin();
    assert(TU32(task,0xAC) == 800 && TU32(state,0x60) == 800);
    assert(((unsigned char *)auxiliary)[4] == 0x50 && ((unsigned char *)auxiliary)[6] == 3);
    assert(((unsigned char *)auxiliary)[0x815] == 1);
    assert(TU32(auxiliary,0) == 0x81234560 && TU32(auxiliary,0x14) == 0x81234570);
    assert(anchor_impact_native_capture(&bad));
    assert(bad.root[IMP_AUX_DATA] == snapshot.root[IMP_AUX_DATA]);
    bad.root[IMP_AUX_KIND] = 1; assert(!anchor_impact_native_apply(&bad));
    puts("Impact native checkpoint tests passed");
    return 0;
}

void anchor_impact_visuals_begin_frame(void) {}
void anchor_impact_visuals_render(void) {}
