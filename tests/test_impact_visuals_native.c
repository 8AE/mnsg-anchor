#include <stdio.h>
#include "impact_test_pointers.h"
#define ANCHOR_IMPACT_VISUAL_HOST_TEST
#define IV_PTR(p,o) TP(p,o)
char *anchor_impact_visuals_update(int, unsigned int, unsigned int, unsigned int, const char *);
void recomp_free(void *);
#include "../src/anchor_impact_visuals.c"

static unsigned int state[0x300/4], system_data[0xC2DA0/4];
static unsigned int head[64], manager[64], world[64], boss[64], model[64];
static unsigned int tasks[64][64], models[64][64], allocated;
static unsigned int overflow_models[64][64];
static unsigned int owner, shots_hidden;
static unsigned int cache_releases;
static char received_json[ANCHOR_IMPACT_VISUAL_JSON];
static AnchorImpactVisualFrame sent, frame;
unsigned char D_80167FC0_168BC0[48*8];
void *D_8020EED0_63A2B0 = state;
unsigned char *D_8015C5C8_15D1C8 = (unsigned char *)system_data;
static const AnchorImpactVisualRecipe recipe = {0x48009AC0u,0x4A8};
void anchor_impact_visual_catalog_init(void) {}
unsigned int anchor_impact_visual_recipe_id(unsigned int m, unsigned int f) {
    return m == recipe.model && f == recipe.file ? 1 : 0;
}
const AnchorImpactVisualRecipe *anchor_impact_visual_recipe(unsigned int id) { return id == 1 ? &recipe : 0; }
unsigned int anchor_impact_visual_material_id(unsigned int p) {
    return !p ? 0 : (p&~0x60000000u) == 0x80000004u ? 1 : 0xFFFFFFFFu;
}
void *anchor_impact_visual_material(unsigned int id) { return id == 1 ? (void *)(uintptr_t)0x80000004u : 0; }
unsigned int anchor_impact_native_stage(void) { return 0x260; }
unsigned int anchor_impact_native_encounter(void) { return 1; }
unsigned int anchor_impact_native_visit(void) { return 1; }
int anchor_impact_native_is_owner(void) { return owner; }
int anchor_impact_native_root_live(void) { return 1; }
void anchor_impact_players_hide_shots(void) { ++shots_hidden; }
char *anchor_impact_visuals_update(int active, unsigned int stage, unsigned int encounter,
                                  unsigned int visit, const char *sample) {
    assert(stage == 0x260 && encounter == 1 && visit == 1);
    if (owner && active) assert(anchor_impact_visual_decode(sample,&sent));
    return active && !owner ? received_json : "null";
}
void recomp_free(void *p) { (void)p; }
int func_8003674C_3734C(void *p) { ++cache_releases; TP(p,0x78) = 0; return 1; }
int func_80036798_37398(void *p) { ++cache_releases; TP(p,0x74) = 0; return 1; }
void *func_80034E08_35A08(void *parent, void (*callback)(void *, void *), unsigned short flags) {
    void *task, *next;
    assert(parent == manager && callback == display_update && flags == 0 && allocated < 64);
    task = tasks[allocated++];
    /* Insertion as a sibling before the world subtree. */
    next = TP(manager,0); TP(manager,0) = task; TP(task,4) = manager;
    TP(task,0) = next; TP(next,4) = task; TP(task,0x0C) = (void *)callback;
    TU16(task,0x20) = 1;
    return task;
}
void *func_8000DBF0_E7F0(void *task, unsigned int m, unsigned int material,
    float x, float y, float z, short rx, short ry, short rz, float sx, float sy, float sz, short f, short f9) {
    unsigned int i = (unsigned int)((unsigned int *)task-tasks[0])/64;
    (void)material; (void)x; (void)y; (void)z; (void)rx; (void)ry; (void)rz; (void)sx; (void)sy; (void)sz;
    assert(m == recipe.model && f == recipe.file && f9 == 0 && i < 64);
    TP(task,0x18) = models[i]; ((unsigned char *)models[i])[4] = 2;
    TU32(models[i],0x2C) = m; TU16(models[i],0x34) = f; return models[i];
}
void *func_8000DDF0_E9F0(void *task, unsigned int m, int r, unsigned int g, unsigned int b, unsigned int a) {
    TU32(task,0xD0) = 0x06000000; TU32(task,0xD4) = m; TU32(task,0xD8) = 0xFA000000;
    TU32(task,0xDC) = (unsigned int)r<<24 | g<<16 | b<<8 | a;
    TU32(task,0xE0) = 0xB8000000; return (unsigned char *)task+0xD0;
}
void *func_8000DF10_EB10(void *task, unsigned int m, int r, unsigned int g, unsigned int b, unsigned int a) {
    void *p = func_8000DDF0_E9F0(task,m,r,g,b,a); TU32(task,0xD8) = 0xFB000000; return p;
}
static void setup(void) {
    unsigned int i;
    TP(head,0) = manager; TP(manager,4) = head; TP(manager,0) = world;
    TP(world,4) = manager; TP(world,0) = boss; TU16(world,0x20) = 1;
    TP(boss,4) = world; TP(boss,0x18) = model; TU16(boss,0x20) = 2;
    TP(state,0x1BC) = manager; TP(state,0x1C4) = world;
    TP(state,0x1E0) = model;
    ((unsigned char *)model)[4] = 2; ((unsigned char *)model)[5] = 9;
    TU32(model,0x2C) = recipe.model; TU32(model,0x30) = 0x80000004;
    TU16(model,0x34) = recipe.file; TU32(model,0x38) = 0x80200000;
    for (i = 0; i < 3; ++i) TF32(model,0x1C+i*4) = .2f;
    TU16(D_80167FC0_168BC0,0) = recipe.file;
    TP(D_80167FC0_168BC0,4) = (void *)(uintptr_t)0x80200000;
    TP(D_80167FC0_168BC0+8,4) = (void *)(uintptr_t)0x80210000;
}
int main(void) {
    unsigned int i, bad[29], bases[6], original_id, inserted_id;
    setup(); owner = 1; anchor_impact_visuals_tick(1);
    assert(sent.count == 1 && sent.rows[0][1] == 1 && sent.rows[0][17] == recipe.file);
    original_id = sent.rows[0][0];
    /* Inserting a model ahead of the boss must not relabel the boss. */
    memcpy(overflow_models[0],model,sizeof(model));
    TP(overflow_models[0],0) = model; TP(boss,0x18) = overflow_models[0];
    anchor_impact_visuals_tick(1);
    assert(sent.count == 2 && sent.rows[1][0] == original_id);
    inserted_id = sent.rows[0][0];
    TP(boss,0x18) = model; anchor_impact_visuals_tick(1);
    assert(sent.count == 1 && sent.rows[0][0] == original_id);
    TP(boss,0x18) = overflow_models[0]; anchor_impact_visuals_tick(1);
    assert(sent.rows[0][0] != inserted_id && sent.rows[1][0] == original_id);
    TP(boss,0x18) = model; anchor_impact_visuals_tick(1);
    TU32(model,0x2C) = 0x48009999; s_capture.count = 0;
    assert(!walk(0)); /* never replace a visible unsupported boss with an empty frame */
    TU32(model,0x2C) = recipe.model;
    frame = sent; frame.rows[0][7] = 0x42C80000; /* host x=100 */
    frame.rows[0][3] = 2; frame.rows[0][4] = 0x20A0FF28; /* translucent native effect */
    frame.rows[0][6] = 0x30000; /* material command tags survive replication */
    assert(anchor_impact_visual_encode(&frame,received_json,sizeof(received_json)));
    owner = 0; anchor_impact_visuals_tick(1); anchor_impact_visuals_render();
    assert(anchor_impact_visuals_active() && allocated == 1 && shots_hidden == 1);
    assert(TF32(models[0],8) == 100 && TU32(tasks[0],0xDC) == 0x20A0FF28);
    assert(TU32(tasks[0],0xD8) == 0xFB000000);
    assert((TU32(models[0],0x30)&0x60000000u) == 0x60000000u);
    assert(IV_U8(model,0x64)&1); assert(TF32(model,8) == 0); /* native simulation untouched */
    anchor_impact_visuals_begin_frame(); assert(!(IV_U8(model,0x64)&1));
    for (i = 0; i < 50; ++i) {
        anchor_impact_visuals_tick(0); assert(IV_U8(models[0],0x64)&1);
        anchor_impact_visuals_tick(1); anchor_impact_visuals_render();
    }
    assert(allocated == 1); /* retained slots across pauses */
    assert(cache_releases == 0);
    TU16(models[0],0x34) = 0x4B5; TP(models[0],0x78) = model;
    anchor_impact_visuals_begin_frame(); anchor_impact_visuals_tick(1); anchor_impact_visuals_render();
    assert(cache_releases == 2 && !TP(models[0],0x78) && allocated == 1);
    anchor_impact_visuals_begin_frame();
    for (i = 0; i < 64; ++i) {
        memcpy(overflow_models[i],model,sizeof(model));
        TP(overflow_models[i],0) = i+1 < 64 ? overflow_models[i+1] : 0;
    }
    TP(model,0) = overflow_models[0]; /* 65 native objects exceed one frame */
    anchor_impact_visuals_render();
    assert(!anchor_impact_visuals_active() && s_hidden_count == 0);
    assert(!(IV_U8(model,0x64)&1) && (IV_U8(models[0],0x64)&1));
    for (i = 0; i < 64; ++i) assert(!(IV_U8(overflow_models[i],0x64)&1));
    TP(model,0) = 0;
    memcpy(bad,frame.rows[0],sizeof(bad)); bad[18] = 0xFFFF;
    assert(!resolve(bad,bases)); bad[18] = 0; bad[19] = 0x4B0;
    assert(!resolve(bad,bases)); bad[19] = recipe.file; bad[20] = 0x10000;
    assert(!resolve(bad,bases));
    anchor_impact_visuals_begin_frame(); strcpy(received_json,"null");
    anchor_impact_visuals_tick(1); anchor_impact_visuals_render();
    assert(!anchor_impact_visuals_active() && !(IV_U8(model,0x64)&1) && (IV_U8(models[0],0x64)&1));
    frame.count = 0; assert(anchor_impact_visual_encode(&frame,received_json,sizeof(received_json)));
    anchor_impact_visuals_tick(1); anchor_impact_visuals_render();
    assert(IV_U8(model,0x64)&1); /* completed empty frame removes expired attacks */
    anchor_impact_visuals_begin_frame(); owner = 1; anchor_impact_visuals_tick(1);
    assert(!(IV_U8(model,0x64)&1));
    frame = sent; frame.count = 2; memcpy(frame.rows[1],frame.rows[0],sizeof(frame.rows[0]));
    assert(anchor_impact_visual_encode(&frame,received_json,sizeof(received_json)));
    assert(!anchor_impact_visual_decode(received_json,&frame)); /* duplicate slot */
    frame.count = 2; frame.rows[1][0] += 64; /* new generation cannot alias an occupied slot */
    assert(anchor_impact_visual_encode(&frame,received_json,sizeof(received_json)));
    assert(!anchor_impact_visual_decode(received_json,&frame));
    puts("Impact native render, texture bounds, alpha and retained lifetime tests passed");
    return 0;
}
