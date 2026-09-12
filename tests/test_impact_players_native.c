#include <stdio.h>
#include <stdlib.h>
#include "impact_test_pointers.h"
char *anchor_impact_players_update(int, unsigned int, unsigned int, unsigned int, const char *);
void recomp_free(void *);
#define ANCHOR_IMPACT_PLAYERS_HOST_TEST
#define IP_READ_PTR(p, o) TP(p, o)
#include "../src/anchor_impact_players.c"

static unsigned int state[0xC0], manager[0x80], local_task[0x80], local_object[0x80], local_backlink[4];
static unsigned char system_data[0x40000], resource[16];
static unsigned int tasks[64][0x80], objects[64][0x80], backlinks[64][4];
static unsigned int allocations, constructions, last_model, last_material;
static float last_scale;
static int owner = 1, ready = 1, resource_ready = 1;
unsigned char D_8020A728_635B08[16], D_8020A7D0_635BB0[16];
void *D_8020EED0_63A2B0 = state;
unsigned char *D_8015C5C8_15D1C8 = system_data;
int anchor_impact_native_ready(void) { return ready; }
int anchor_impact_native_is_owner(void) { return owner; }
unsigned int anchor_impact_native_stage(void) { return 0x260; }
unsigned int anchor_impact_native_encounter(void) { return 1; }
unsigned int anchor_impact_native_visit(void) { return 1; }
void func_801DB200_6065E0(void) {}
void *func_800141C4_14DC4(unsigned int f) { assert(f == 0x4A8); return resource_ready ? resource : 0; }
void *func_80034E08_35A08(void *parent, void (*update)(void *, void *), unsigned short flags) {
    unsigned int i = allocations++;
    assert(parent == manager && !flags && i < 64);
    TP(tasks[i], 4) = backlinks[i]; TP(backlinks[i], 0) = tasks[i];
    TP(tasks[i], 0x0C) = (void *)(uintptr_t)update;
    return tasks[i];
}
void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                         float x, float y, float z, short rx, short ry, short rz,
                         float sx, float sy, float sz, short f8, short f9) {
    unsigned int i;
    for (i = 0; i < allocations && task != tasks[i]; ++i) {}
    assert(i < allocations && f8 == 0x4A8 && f9 == 0);
    ++constructions; last_model = model; last_material = material; last_scale = sx;
    assert(sx == sy && sy == sz);
    TP(task, 0x18) = objects[i]; TU32(objects[i], 0x2C) = model;
    TF32(objects[i], 8) = x; TF32(objects[i], 0xC) = y; TF32(objects[i], 0x10) = z;
    TU16(objects[i], 0x14) = rx; TU16(objects[i], 0x16) = ry; TU16(objects[i], 0x18) = rz;
    return objects[i];
}
char *anchor_impact_players_update(int active, unsigned int stage, unsigned int boss,
                                    unsigned int visit, const char *sample) {
    const char *value = "{\"accepted\":0,\"c\":[],\"a\":[]}";
    char *out = malloc(strlen(value) + 1);
    assert(active && stage == 0x260 && boss == 1 && visit == 1 && sample);
    strcpy(out, value); return out;
}
void recomp_free(void *p) { free(p); }
static void setup(void) {
    TP(state, 0x1BC) = manager; TP(state, 0) = TP(state, 0xC) = local_task;
    TP(local_task, 4) = local_backlink; TP(local_backlink, 0) = local_task;
    TP(local_task, 0x18) = local_object;
    TU32(local_object, 0x2C) = IP_CURSOR_MODEL; TU16(local_object, 0x16) = 512;
    s_manager = manager;
}
int main(void) {
    AnchorImpactPlayerStatus status = {0};
    unsigned int row[10] = {2, 200, 1, 2, 0x2000, 0x2000, 1000, 600, 1, 1};
    void *retained;
    unsigned int i;
    setup();
    anchor_impact_players_set_authority(1, 1);
    status.cursor_count = 1;
    status.cursors[0][0] = 2; status.cursors[0][1] = 200; status.cursors[0][2] = 1;
    status.cursors[0][3] = 1; status.cursors[0][4] = float_bits(2);
    status.cursors[0][7] = 1000; status.cursors[0][8] = 600;
    resource_ready = 0; apply_cursors(&status); assert(allocations == 0);
    resource_ready = 1; apply_cursors(&status);
    assert(allocations == 1 && constructions == 1);
    assert(last_model == 0x4800A1E0 && last_scale == 0.2f);
    assert(last_material == (unsigned int)(uintptr_t)D_8020A728_635B08);
    retained = s_cursors[0].object;
    assert(IP_READ_U8(retained, 5) == 9 && TU16(retained, 0x16) == 600);
    assert(TF32(retained, 8) == 2 && !(IP_READ_U8(retained, 0x64) & 1));
    /* Tint uses the real mesh and preserves TEXEL0 alpha in both cycles. */
    assert(TU32(s_cursors[0].task, 0xDC) == 0xFFCC40FF);
    assert(((TU32(s_cursors[0].task, 0xE4) >> 15) & 7) == 3);
    assert(((TU32(s_cursors[0].task, 0xE4) >> 6) & 7) == 3);
    assert(((TU32(s_cursors[0].task, 0xE4) >> 9) & 7) == 1);
    assert((TU32(s_cursors[0].task, 0xE4) & 7) == 1);
    assert(TU32(s_cursors[0].task, 0xE8) == 0xB8000000);
    assert(TU32(local_object, 0x30) == 0); /* local material untouched */
    /* New cursor samples move on native ticks; repeated samples do not restart
     * interpolation. Crossing 1024 follows the short arc. */
    status.cursors[0][4] = float_bits(14);
    status.cursors[0][7] = 24;
    apply_cursors(&status); assert(TF32(retained, 8) == 2);
    impact_cursor_update(s_cursors[0].task, retained);
    assert(TF32(retained, 8) == 6 && TU16(retained, 0x14) == 1016);
    apply_cursors(&status);
    impact_cursor_update(s_cursors[0].task, retained);
    assert(TF32(retained, 8) == 10 && TU16(retained, 0x14) == 8);
    impact_cursor_update(s_cursors[0].task, retained);
    assert(TF32(retained, 8) == 14 && TU16(retained, 0x14) == 24);
    /* Pause and returning peer reuse one retained object, with no hidden leak. */
    for (i = 0; i < 50; ++i) {
        anchor_impact_players_reset(); assert(IP_READ_U8(retained, 0x64) & 1);
        apply_cursors(&status); assert(s_cursors[0].object == retained);
    }
    assert(allocations == 1 && constructions == 1);
    status.cursors[0][3] = 0; apply_cursors(&status);
    assert(IP_READ_U8(retained, 0x64) & 1);
    /* A remote press uses that cursor only inside the native interpreter. */
    anchor_impact_players_set_authority(1, 1);
    receive_controls(row); TU16(system_data, 0x3B07A) = 0; TU16(system_data, 0x3B07C) = 0;
    TF32(state, 4) = 13; TF32(state, 8) = 17;
    anchor_impact_controls_begin(0, 0);
    assert(TU16(system_data, 0x3B07C) == 0x2000);
    assert(TU16(local_object, 0x14) == 1000 && TU16(local_object, 0x16) == 600);
    anchor_impact_controls_end();
    assert(TU16(system_data, 0x3B07C) == 0 && TU16(local_object, 0x16) == 512);
    assert(TF32(state, 4) == 13 && TF32(state, 8) == 17);
    anchor_impact_controls_begin(0, 0);
    assert(TU16(system_data, 0x3B07C) == 0); anchor_impact_controls_end();
    /* Follower input is exported but cannot execute a second native shot. */
    owner = 0; TU16(system_data, 0x3B07A) = TU16(system_data, 0x3B07C) = 0x2000;
    TU16(system_data, 0x3B07E) = 31;
    anchor_impact_controls_begin(0, 0);
    assert(TU16(system_data, 0x3B07A) == 0 && TU16(system_data, 0x3B07C) == 0);
    assert(TU16(system_data, 0x3B07E) == 31);
    anchor_impact_controls_end(); assert(TU16(system_data, 0x3B07C) == 0x2000);
    /* Expiry runs even on followers; promotion drops old button edges. */
    receive_controls(row);
    for (i = 0; i < 12; ++i) anchor_impact_players_tick(1);
    for (i = 0; i < IP_CURSOR_MAX; ++i) assert(!s_controls[i].cid);
    owner = 1; receive_controls(row);
    anchor_impact_players_set_authority(1, 2); anchor_impact_players_tick(1);
    for (i = 0; i < IP_CURSOR_MAX; ++i) assert(!s_controls[i].cid);
    receive_controls(row); /* old owner term */
    for (i = 0; i < IP_CURSOR_MAX; ++i) assert(!s_controls[i].cid);
    row[9] = 2; receive_controls(row);
    assert(s_controls[IP_CURSOR_MAX - 1].cid == 2);
    /* Capture the constructor's actual native velocity without replaying AI. */
    TP(local_task, 0x0C) = (void *)(uintptr_t)func_801DB200_6065E0;
    TF32(local_task, 0x70) = 1; TF32(local_task, 0x74) = 2; TF32(local_task, 0x78) = -12;
    anchor_impact_shot_constructor_begin();
    anchor_impact_shot_velocity_begin(12, 0, 0, 0, local_task);
    anchor_impact_shot_velocity_end();
    assert(s_ctor_captured && s_ctor_velocity[2] == -12);
    anchor_impact_shot_constructor_end(); assert(!s_ctor_bracket);
    puts("Impact cursor, retained lifecycle, controls and shot capture tests passed");
    return 0;
}
