/* Production crane hooks with native 32-bit pointer slots and bounded native
 * substitutes. Movement substitutes below follow the verified File_30 bodies. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#define WORLD_CRANE_HOST_TEST 1
#define CP(p,o) TP(p,o)
/* Use an otherwise unused pointer bit on the host; native uses 0x00800000. */
#define CR_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#include "../src/anchor_world_crane.c"

unsigned short D_800C7AB2;
void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
unsigned char D_8015C562_15D162;
float D_8015CDB4[3];
void *D_80236984_5F1E54[1026];
static unsigned int tasks[24][64], objects[24][64], player[64];
static unsigned char saves[2048], temps[800];
static unsigned short files[] = {30, 0x152};
static unsigned int clips[] = {1,1,1,1,1,1};
static void *models[] = {files, clips};
static int missing_file, allocations, next_actor, action_resets, sounds, cleanup_calls;
static int scene_calls, static_binds;
static float animation_limit;
static void native_step(void *, void *, CraneCallback);
#define STUB(f) void f(void *a, void *o) { native_step(a,o,f); }
CRANE_PHASES(STUB)
PAD0_PHASES(STUB)
PAD1_PHASES(STUB)
REWARD_PHASES(STUB)
STUB(func_08000ACC_6C021C)
STUB(func_080027CC_6C1F1C)
STUB(func_08002ACC_6C221C)
STUB(func_08000AC0_6C0210)
STUB(func_08002624_6C1D74)
STUB(func_0800221C_6C196C)
STUB(func_08002474_6C1BC4)
STUB(func_08002488_6C1BD8)
STUB(func_0800178C_6C0EDC)
int func_800141C4_14DC4(unsigned int f) { return (int)f == missing_file ? -1 : 0x1000; }
int func_800240DC_24CDC(int f) { assert(f >= 0 && f < 2048); return saves[f]; }
void func_80024038_24C38(int f) { assert(f >= 0 && f < 2048); saves[f] = 1; }
int func_80023E94_24A94(int f) { assert(f >= 0 && f < 800); return temps[f]; }
void func_80023DF0_249F0(int f) { assert(f >= 0 && f < 800); temps[f] = 1; }
void func_80023E40_24A40(int f) { assert(f >= 0 && f < 800); temps[f] = 0; }
void func_80038B98_39798(unsigned int s) { (void)s; ++sounds; }
void func_801DACDC_596BEC(void *a, unsigned char s) {
  assert(a == D_801FC604_5B8514 && s == 0); ++action_resets;
}
void func_801FA3FC_5B630C(unsigned char r, unsigned char g, unsigned char b) {
  assert(r == 255 && g == 255 && b == 255);
}
void func_80216DF8_5D22C8(void *a, unsigned int c) {
  assert(a == tasks[2] && c == 1); ++static_binds;
}
float func_8001B5AC_1C1AC(void *o) { (void)o; return animation_limit; }

static void task(unsigned int i, CraneCallback cb) {
  memset(tasks[i],0,sizeof(tasks[i])); memset(objects[i],0,sizeof(objects[i]));
  CP(tasks[i],0x18) = objects[i]; CP(tasks[i],0xc) = cb;
  CB(tasks[i],0x74) = 1;
  for (int j = 0; j < 3; ++j) CF(objects[i],0x1c+j*4) = .2f;
}
void *func_802171A8_5D2678(void *parent, CraneCallback cb, unsigned char type) {
  assert(type == 3 || type == 9); assert(next_actor < 24);
  unsigned int i = (unsigned int)next_actor++; ++allocations;
  task(i,cb); CP(tasks[i],0xdc) = parent;
  anchor_world_crane_child(parent,tasks[i]);
  return tasks[i];
}
static void native_step(void *a, void *o, CraneCallback cb) {
  if (cb == func_08000ACC_6C021C) {
    CW(a,0x60) = 0x802006e0u; CF(o,8) = -210; CF(o,0xc) = -10; CF(o,0x10) = 22;
    CP(a,0xc) = cr_phases[0]; return;
  }
  if (cb == func_080027CC_6C1F1C || cb == func_08002ACC_6C221C) {
    CW(a,0x60) = 0x802006e1u;
    CP(a,0xc) = cr_pad[cb == func_08002ACC_6C221C][0]; return;
  }
  if (cb == func_0800178C_6C0EDC) {
    if (saves[0x1a3]) { CW(a,0x68) |= 2u; return; }
    CW(a,0x60) = 0x2006e1u; CF(o,8) = -99; CF(o,0xc) = -59; CF(o,0x10) = -38;
    CP(a,0xc) = cr_reward[0]; return;
  }
  for (unsigned int pad = 0; pad < 2; ++pad) {
    if (cb == cr_pad[pad][1] || cb == cr_pad[pad][3]) {
      int up = cb == cr_pad[pad][3];
      CF(o,0xc) = (float)((double)CF(o,0xc) + (up ? .3 : -.3));
      if (!up) temps[pad] = 1;
      short timer = CS(a,0x8a); CS(a,0x8a) = timer-1;
      if (!timer) {
        if (up) { temps[pad] = 0; temps[pad+2] = 1; }
        CP(a,0xc) = cr_pad[pad][up ? 4 : 2];
      }
      return;
    }
    if (cb == cr_pad[pad][4]) { CP(a,0xc) = cr_pad[pad][0]; return; }
  }
  if (cb == cr_reward[0]) CH(o,0x16) = (CH(o,0x16)+4)&1023;
  if (cb == cr_reward[10] && (CW(a,0x68)&0x200u)) {
    ++scene_calls; saves[0x1a3] = 1; CP(a,0xc) = cr_reward[11];
  }
  if (cb == cr_reward[11]) ++cleanup_calls;
}
static void post(void *a) {
  anchor_world_crane_post(a);
  /* Only the post integration disabled by the production freeze hook. */
  if (CW(a,0x60)&0x800000u)
    for (int j = 0; j < 3; ++j) CF(CP(a,0x18),8+j*4) += CF(a,0x78+j*4);
  anchor_world_crane_post_return();
}
static void tick(void) {
  anchor_world_crane_begin();
  for (int i = 0; i < next_actor; ++i) {
    void *a = tasks[i]; if (CW(a,0x68)&2u) continue;
    CraneCallback cb = CP(a,0xc);
    D_8016DAB4_16E6B4 = a;
    if (cb && !((unsigned long)cb&CR_DISABLED)) cb(a,CP(a,0x18));
    post(a);
  }
  anchor_world_crane_end();
}
static void setup(void) {
  anchor_world_crane_reset(1);
  memset(tasks,0,sizeof(tasks)); memset(objects,0,sizeof(objects));
  memset(saves,0,sizeof(saves)); memset(temps,0,sizeof(temps));
  memset(player,0,sizeof(player)); memset(test_ptrs,0,sizeof(test_ptrs)); test_ptr_count = 0;
  D_800C7AB2 = 0x31; D_801FC604_5B8514 = player; D_8015C562_15D162 = 0;
  missing_file = -1; animation_limit = 90; next_actor = 7;
  allocations = action_resets = sounds = cleanup_calls = scene_calls = static_binds = 0;
  unsigned int ids[] = {0x1b9,0x1bb,0x1ba,0x3d3};
  CraneCallback root_cb[] = {func_08000AC0_6C0210,func_08002624_6C1D74,
                           func_0800221C_6C196C,func_0800178C_6C0EDC};
  for (unsigned int i = 0; i < 4; ++i) {
    D_80236984_5F1E54[ids[i]] = models;
    task(i,root_cb[i]); anchor_world_crane_register(tasks[i],ids[i]);
    if (i == 3) native_step(tasks[i],objects[i],root_cb[i]);
    post(tasks[i]);
  }
  CraneCallback init[] = {func_08000ACC_6C021C,func_080027CC_6C1F1C,func_08002ACC_6C221C};
  for (unsigned int i = 0; i < 3; ++i) {
    task(i+4,init[i]); void *parent = tasks[i ? 1 : 0];
    CP(tasks[i+4],0xdc) = parent; anchor_world_crane_child(parent,tasks[i+4]);
    native_step(tasks[i+4],objects[i+4],init[i]); post(tasks[i+4]);
  }
  anchor_world_crane_control(tasks[0],0,0,0);
  assert(prepare());
}
static void row(int *r) {
  memset(r,0,sizeof(int)*ANCHOR_WORLD_WORDS); r[1] = WORLD_CRANE_ENTITY; r[2] = WORLD_CRANE;
  assert(anchor_world_crane_capture(tasks[0],r)); assert(anchor_world_row_valid(r));
}
static void pad_and_local_effect_test(void) {
  setup(); saves[0x15a] = 1;
  anchor_world_crane_control(tasks[0],0,0,3); tick();
  assert(phase(&cr_nodes[1],cr_pad[0],5) == 2 && phase(&cr_nodes[2],cr_pad[1],5) == 2);
  assert(!D_8015C562_15D162 && !action_resets); /* Remote pads cannot lock local controls. */
  for (int i = 0; i < 10; ++i) tick();
  assert(phase(&cr_nodes[1],cr_pad[0],5) == 2);
  tick(); assert(phase(&cr_nodes[1],cr_pad[0],5) == 3);
  assert(fabsf(CF(objects[5],0xc)+3.3f) < .001f);
  /* One client releases, another remains: the OR aggregate still holds. */
  anchor_world_crane_control(tasks[0],0,0,1); tick();
  assert(phase(&cr_nodes[1],cr_pad[0],5) == 3 && phase(&cr_nodes[2],cr_pad[1],5) == 4);
  CP(player,0xa0) = objects[5]; tick(); assert(D_8015C562_15D162 == 1);
  CP(player,0xa0) = 0; tick(); assert(D_8015C562_15D162 == 0);
  anchor_world_crane_control(tasks[0],0,0,0); tick();
  for (int i = 0; i < 11; ++i) tick();
  assert(fabsf(CF(objects[5],0xc)) < .001f);
  assert(temps[2] && temps[3]); tick(); tick();
  assert(phase(&cr_nodes[0],cr_phases,19) == 14 && action_resets == 1);
}
static void snapshot_pause_and_retry_test(void) {
  setup(); int r[ANCHOR_WORLD_WORDS]; row(r);
  r[18] = 4; r[17] = 60; r[4] = -9900; r[5] = -6000; r[6] = -3800;
  r[19] = 2; r[20] = 5; r[21] = -150; r[22] = 4; r[23] = 8; r[24] = -210;
  r[25] = 0x841; r[30] = 1; r[33] = 3; r[37] = 3; r[41] = 200;
  temps[12] = temps[15] = 1; saves[0x15b] = saves[0x15c] = 1;
  void *parent = CP(tasks[5],0xdc), *model = CP(tasks[4],0x18);
  animation_limit = 0; assert(!anchor_world_crane_apply(tasks[0],r));
  assert(CF(objects[4],8) == -210 && !temps[6] && !saves[0x15a]);
  animation_limit = 90; missing_file = 0x152;
  assert(!anchor_world_crane_apply(tasks[0],r)); missing_file = -1;
  assert(anchor_world_crane_apply(tasks[0],r));
  assert(CP(tasks[5],0xdc) == parent && CP(tasks[4],0x18) == model);
  assert(temps[12] && temps[15] && saves[0x15b] && saves[0x15c]);
  assert(saves[0x15e] && saves[0x15f] && static_binds == 1 && !scene_calls);
  assert(CF(objects[4],8) == -99 && CS(tasks[5],0x8a) == 5);
  assert(phase(&cr_roots[3],cr_reward,12) == 3);
  anchor_world_crane_control(tasks[0],1,1,0);
  CF(tasks[4],0x78) = 1; unsigned int flags = CW(tasks[4],0x60);
  tick(); assert(CF(objects[4],8) == -99 && CS(tasks[5],0x8a) == 5);
  assert(CF(tasks[4],0x78) == 1 && CW(tasks[4],0x60) == flags);
  anchor_world_crane_control(tasks[0],1,0,0); tick();
  assert(CS(tasks[5],0x8a) == 4);
  r[25] |= 1<<12; assert(!anchor_world_crane_apply(tasks[0],r));
}
static void reconstruction_and_generation_test(void) {
  setup(); int r[ANCHOR_WORLD_WORDS]; row(r);
  r[19] = 4; r[20] = 6; r[21] = -180;
  CW(tasks[5],0x68) |= 2; anchor_world_crane_reuse(tasks[5]);
  assert(!anchor_world_crane_apply(tasks[0],r)); assert(allocations == 1);
  assert(!anchor_world_crane_apply(tasks[0],r)); assert(allocations == 1);
  tick(); assert(anchor_world_crane_needs_restore());
  int ignored[ANCHOR_WORLD_WORDS] = {0}; assert(!anchor_world_crane_capture(tasks[0],ignored));
  assert(anchor_world_crane_apply(tasks[0],r)); assert(allocations == 1);
  assert(cr_nodes[1].actor == tasks[7] && CS(tasks[7],0x8a) == 6);
  assert(fabsf(CF(objects[7],0xc)+1.8f) < .001f);
  CB(tasks[1],0x74)++; assert(!alive(&cr_nodes[1]));
  assert(!anchor_world_crane_apply(tasks[0],r));
  anchor_world_crane_reset(0); assert(!cr_restore && !cr_enabled);
}
static void reward_culling_and_cleanup_test(void) {
  setup(); int r[ANCHOR_WORLD_WORDS]; row(r);
  CW(tasks[3],0x68) |= 2; anchor_world_crane_reuse(tasks[3]);
  assert(!anchor_world_crane_apply(tasks[0],r)); tick();
  assert(cr_roots[3].proxy && allocations == 1);
  assert(anchor_world_crane_apply(tasks[0],r));
  task(8,cr_reward[0]); CW(tasks[8],0x60) = 0x2006e1u; next_actor = 9;
  anchor_world_crane_register(tasks[8],0x3d3); post(tasks[8]);
  assert(CW(tasks[7],0x68)&2u); assert(anchor_world_crane_apply(tasks[0],r));
  CP(tasks[8],0xc) = cr_reward[10]; CW(tasks[8],0x68) |= 0x200;
  tick(); assert(scene_calls == 1 && saves[0x1a3]);
  row(r); assert(r[32] && r[37] == 13 && r[44] == 2);
  assert(anchor_world_crane_apply(tasks[0],r));
  anchor_world_crane_control(tasks[0],1,1,0); tick();
  assert(cleanup_calls == 1 && !(CW(tasks[8],0x68)&2u));
  setup(); row(r); r[32] = 1; r[37] = 13; r[44] = 2;
  assert(anchor_world_crane_apply(tasks[0],r));
  assert(CW(tasks[3],0x68)&2u); assert(!scene_calls && saves[0x1a3]);
  tick(); assert(!allocations); /* Completion cannot resurrect the reward. */
  /* A native placement can replace a proxy while callbacks are wrapped.
   * The detached proxy must finish its scene even after losing its slot. */
  setup(); row(r); CW(tasks[3],0x68) |= 2; anchor_world_crane_reuse(tasks[3]);
  assert(!anchor_world_crane_apply(tasks[0],r)); tick();
  assert(anchor_world_crane_apply(tasks[0],r));
  CP(tasks[7],0xc) = cr_reward[11]; saves[0x1a3] = 1;
  anchor_world_crane_begin(); assert(CP(tasks[7],0xc) == crane_callback);
  task(8,cr_reward[0]); anchor_world_crane_register(tasks[8],0x3d3);
  assert(CP(tasks[7],0xc) == cr_reward[11] && !(CW(tasks[7],0x68)&2u));
  ((CraneCallback)CP(tasks[7],0xc))(tasks[7],objects[7]); assert(cleanup_calls == 1);
  anchor_world_crane_end();
}
static void disabled_and_failed_birth_test(void) {
  setup(); int r[ANCHOR_WORLD_WORDS]; row(r);
  CP(tasks[4],0xc) = (void *)((unsigned long)cr_phases[0]|CR_DISABLED);
  anchor_world_crane_begin();
  assert(((unsigned long)CP(tasks[4],0xc)&CR_DISABLED));
  anchor_world_crane_end();
  assert(CP(tasks[4],0xc) == (void *)((unsigned long)cr_phases[0]|CR_DISABLED));
  assert(anchor_world_crane_apply(tasks[0],r));
  assert(((unsigned long)CP(tasks[4],0xc)&CR_DISABLED));
  setup(); anchor_world_crane_begin(); CB(tasks[1],0x74)++;
  anchor_world_crane_end();
  assert(CP(tasks[5],0xc) == cr_pad[0][0] && CP(tasks[6],0xc) == cr_pad[1][0]);
  setup(); row(r); anchor_world_crane_begin();
  anchor_world_crane_reuse(tasks[1]); /* Parent pool slot begins reuse mid-scheduler. */
  assert((CW(tasks[5],0x68)&2u) && (CW(tasks[6],0x68)&2u));
  assert(CP(tasks[5],0xc) != crane_callback && CP(tasks[6],0xc) != crane_callback);
  anchor_world_crane_end();
  setup(); row(r); task(7,func_080027CC_6C1F1C);
  CP(tasks[7],0xdc) = tasks[1]; anchor_world_crane_child(tasks[1],tasks[7]);
  CP(tasks[7],0x18) = 0; post(tasks[7]); /* Failed birth cannot pin the pending queue. */
  assert(anchor_world_crane_apply(tasks[0],r));
  task(8,func_08002ACC_6C221C); CP(tasks[8],0xdc) = tasks[1];
  anchor_world_crane_child(tasks[1],tasks[8]); CW(tasks[8],0x68) |= 2;
  assert(anchor_world_crane_apply(tasks[0],r)); /* Even before its deletion post. */
  /* The power switch's local presentation can be distance-culled while the
   * coupled crane and pads continue using their shared power flag. */
  CW(tasks[2],0x68) |= 2u; anchor_world_crane_reuse(tasks[2]);
  assert(anchor_world_crane_apply(tasks[0],r)); row(r);
}
int main(void) {
  pad_and_local_effect_test(); snapshot_pause_and_retry_test();
  reconstruction_and_generation_test(); reward_culling_and_cleanup_test();
  disabled_and_failed_birth_test();
  puts("world crane: shared pads, native timing, atomic restore, pause, resources, reward cleanup and pool reuse passed");
  return 0;
}
