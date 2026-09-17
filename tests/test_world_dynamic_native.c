/* Real child hooks and restoration code with host native-call substitutes. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define WORLD_DYNAMIC_HOST_TEST 1
#define DPTR(p, o) TP(p, o)
#define DDISABLED 0ul
#define RECOMP_HOOK(n)
#define RECOMP_HOOK_RETURN(n)
#define recomp_free free
static unsigned int anchor_get_client_id(void) { return 2; }
static const char *bridge_reply;
static char *anchor_update_world_actors(const char *s) {
  char *result;
  (void)s;
  if (!bridge_reply)
    return NULL;
  result = malloc(strlen(bridge_reply) + 1);
  strcpy(result, bridge_reply);
  return result;
}
static int parent_authority = 1, placed_actor;
static int enemy_sync_actor_authority(void *a) {
  (void)a;
  return -1;
}
#include "../src/anchor_world_dynamic.c"

unsigned short D_800C7AB2 = 302;
void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
void *D_80236984_5F1E54[1026];
static unsigned int actors[8][64], objects[8][64], model[2];
static unsigned int clips[20];
static unsigned short resources[2] = {100, 101};
static int used, loaded = 1, calls, awards, binds, talks;
static float shine_x;

int anchor_world_actor_placed(void *a) {
  (void)a;
  return placed_actor;
}
int anchor_world_is_paused(void) { return 0; }
int anchor_world_actor_authority(void *a, unsigned int *index) {
  (void)a;
  *index = 3;
  return parent_authority;
}
#define DSTUB(f)                                                               \
  void f(void *a, void *o) {                                                   \
    (void)a;                                                                   \
    (void)o;                                                                   \
    ++calls;                                                                   \
  }
DYNAMIC_PHASES(DSTUB)
int func_800141C4_14DC4(unsigned int file) {
  (void)file;
  return loaded ? 1 : -1;
}
void func_8021664C_5D1B1C(void *a, unsigned int c, float s, unsigned int f) {
  (void)a;
  (void)c;
  (void)s;
  (void)f;
  ++binds;
}
void func_80216DF8_5D22C8(void *a, unsigned int c) {
  (void)a;
  (void)c;
  ++binds;
}
void func_80218DCC_5D429C(void *a, unsigned int s) {
  (void)a;
  (void)s;
  ++binds;
}
float func_8001B5AC_1C1AC(void *o) {
  (void)o;
  return 10;
}
void func_802145F0_5CFAC0(void *a) {
  (void)a;
  ++awards;
}
void func_80213FF0_5CF4C0(void *a) {
  (void)a;
  ++awards;
}
void func_801DCD48_598C58(unsigned int amount) {
  (void)amount;
  ++awards;
}
void func_80038B98_39798(unsigned int s) { (void)s; }
void *func_8021804C_5D351C(void *a, unsigned int f) {
  if (f)
    shine_x = F(DPTR(a, 0x18), 8);
  return NULL;
}
void func_80219E70_5D5340(void *a, unsigned char s, unsigned char o) {
  (void)s;
  (void)o;
  W(a, 0x60) |= 0x08000000u;
}
void func_80224ABC_5DFF8C(void *a, int i, float s, int f) {
  (void)a;
  (void)i;
  (void)s;
  (void)f;
}
void func_802268A8_5E1D78(void *a) {
  (void)a;
  ++calls;
}
static void dialogue(void *a, void *o) {
  (void)o;
  DPTR(a, 0xc) = DPTR(a, 0xb4);
  W(a, 0x68) &= ~0x100u;
}
int func_80220F70_5DC440(void *a) {
  ++talks;
  if (W(a, 0x68) & 0x100u) {
    DPTR(a, 0xb4) = DPTR(a, 0xc);
    DPTR(a, 0xc) = dialogue;
    return 1;
  }
  return 0;
}
void *func_802171A8_5D2678(void *parent, DynamicCallback callback,
                           unsigned char category) {
  void *a, *o;
  (void)parent;
  (void)category;
  assert(used < 8);
  a = actors[used];
  o = objects[used++];
  DPTR(a, 0x18) = o;
  DPTR(a, 0xc) = (void *)callback;
  B(a, 0x74) = 3;
  H(a, 0x5c) = 0x82;
  H(a, 0x5e) = 1;
  F(o, 0x1c) = F(o, 0x20) = F(o, 0x24) = 1;
  return a;
}
static void fixture(void) {
  unsigned int i;
  anchor_world_dynamic_room();
  test_ptr_count = 0;
  memset(test_ptrs, 0, sizeof(test_ptrs));
  memset(actors, 0, sizeof(actors));
  memset(objects, 0, sizeof(objects));
  used = 0;
  loaded = 1;
  calls = awards = binds = talks = 0;
  parent_authority = 1;
  placed_actor = 0;
  bridge_reply = NULL;
  d_active = 1;
  d_self = 2;
  d_leader = 1;
  d_room = 302;
  D_801FC604_5B8514 = actors[7];
  for (i = 0; i < 19; ++i)
    clips[i] = i + 1;
  clips[19] = 0;
  DPTR(model, 0) = resources;
  DPTR(model, 4) = clips;
  for (i = 0; i < 1026; ++i)
    D_80236984_5F1E54[i] = model;
}
static void row(int *r, int kind, int phase) {
  memset(r, 0, WORLD_DYNAMIC_WORDS * sizeof(int));
  r[0] = 1;
  r[1] = 10;
  r[2] = 1;
  r[3] = 99;
  r[4] = 1;
  r[WD_KIND] = kind;
  r[WD_ENTITY] = 0x82;
  r[WD_MODEL] = 1;
  r[WD_ROUTE] = 163;
  r[WD_PHASE] = phase;
  r[WD_X] = 12345;
  r[WD_Y] = 2000;
  r[WD_Z] = -321;
  r[WD_SX] = r[WD_SY] = r[WD_SZ] = 1000;
  r[WD_RATE] = 256;
  if (kind == WD_NPC) {
    r[WD_MODEL] = 0x2bd;
    r[WD_ANIMATED] = 1;
  }
  if (kind == WD_COIN)
    r[WD_CLIP] = 4;
  if (kind == WD_HEALTH)
    r[WD_CLIP] = 3;
  if (kind == WD_FOOD)
    r[WD_MODEL] = 0x85;
  if (kind == WD_HAZARD) {
    r[WD_MODEL] = 0x191;
    r[WD_ANIMATED] = 1;
  }
}
static void tick(DynamicActor *d) {
  world_dynamic_scheduler_begin();
  ((DynamicCallback)DPTR(d->actor, 0xc))(d->actor, DPTR(d->actor, 0x18));
  world_dynamic_scheduler_end();
}
static void reconstruction_test(void) {
  int r[WORLD_DYNAMIC_WORDS];
  DynamicActor *d;
  fixture();
  row(r, WD_HAZARD, 6);
  r[WD_MODEL] = 0x191;
  r[WD_TIMER] = 42;
  r[WD_GRAVITY] = 17;
  r[WD_SPHERE] = 17;
  r[WD_BOUNCE] = -2250;
  loaded = 0;
  assert(!reconstruct(r) && used == 0 && binds == 0);
  loaded = 1;
  d = reconstruct(r);
  assert(d && d->dirty);
  d->have = 1;
  d->owner = 1;
  assert(apply(d));
  assert(F(DPTR(d->actor, 0x18), 8) == 123.45f && S(d->actor, 0x8a) == 42);
  assert(F(d->actor, 0xd8) == -2.25f && d->native == (void *)d_phases[5]);
  assert(B(d->actor, 0x75) == 17 && W(d->actor, 0x48) == 0xffffffffu);
  tick(d);
  assert(calls == 0); /* a checkpoint is not a replayed constructor */
  d->owner = 2;
  tick(d);
  assert(calls == 1); /* restored continuation on handoff */
  assert(capture(d));
  r[WD_MODEL] = 0;
  assert(!resource_valid(r));
  r[WD_MODEL] = 0x191;
  r[WD_CLIP] = 255;
  assert(!resource_valid(r));
}
static void claim_test(void) {
  int r[WORLD_DYNAMIC_WORDS];
  DynamicActor *d;
  fixture();
  row(r, WD_COIN, 1);
  d = reconstruct(r);
  assert(d);
  d->have = 1;
  d->owner = 1;
  assert(apply(d));
  W(d->actor, 0x68) = 0x200;
  tick(d);
  assert(d->claimed && awards == 0 && calls == 0);
  assert(capture(d) && d->row[WD_LIFE] == WD_CLAIM);
  d->row[WD_LIFE] = WD_REMOVED;
  d->row[WD_OWNER] = 2;
  tick(d);
  tick(d);
  assert(awards == 1 && (W(d->actor, 0x68) & 2u));
  fixture();
  row(r, WD_HEALTH, 3);
  d = reconstruct(r);
  assert(d);
  d->have = 1;
  d->owner = 1;
  apply(d);
  d->claimed = 1;
  d->row[WD_LIFE] = WD_REMOVED;
  d->row[WD_OWNER] = 1;
  tick(d);
  assert(awards == 0 && (W(d->actor, 0x68) & 2u));
}
static void birth_and_reuse_test(void) {
  void *a;
  DynamicActor *d;
  fixture();
  addresses();
  a = func_802171A8_5D2678(actors[7], d_phases[0], 9);
  H(a, 0x5c) = 0x192; /* dropped coins inherit container entity id */
  world_dynamic_child(actors[7], a);
  world_dynamic_static(a, DPTR(a, 0x18), 4);
  world_dynamic_post(a);
  d = lookup(a);
  assert(d && d->kind == WD_COIN && capture(d));
  assert(d->row[WD_ENTITY] == 0x192 && d->row[WD_CLIP] == 4);
  world_dynamic_reuse(a);
  assert(!d->actor && d->row[WD_LIFE] == WD_REMOVED);
  assert(capture(d));
  fixture();
  addresses();
  parent_authority = 0;
  a = func_802171A8_5D2678(actors[7], d_phases[0], 9);
  world_dynamic_child(actors[7], a);
  world_dynamic_static(a, DPTR(a, 0x18), 4);
  world_dynamic_post(a);
  assert((W(a, 0x68) & 2u) && !lookup(a));
}
static void npc_and_invalid_state_test(void) {
  int r[WORLD_DYNAMIC_WORDS];
  DynamicActor *d;
  volatile union {
    unsigned int u;
    float f;
  } invalid;
  fixture();
  row(r, WD_NPC, 0);
  r[WD_ANIMATED] = 1;
  r[WD_FRAME] = 2000;
  r[WD_TALKABLE] = 1;
  r[WD_DIALOG] = 12;
  r[WD_ROUTE] = 0;
  r[WD_PATH_PC] = 0;
  d = reconstruct(r);
  assert(d);
  d->owner = 1;
  d->have = 1;
  assert(apply(d));
  assert(F(DPTR(d->actor, 0x18), 0x28) == 9);
  assert(DPTR(d->actor, 0x9c) == d->actor);
  tick(d);
  assert(talks == 1 && calls == 0);
  d->owner = 2;
  W(d->actor, 0x68) = 0x100;
  tick(d);
  assert(DPTR(d->actor, 0xb4) == (void *)npc_native);
  tick(d);
  assert(!(W(d->actor, 0x68) & 0x100u));
  r[WD_DIALOG] = 794;
  assert(!resource_valid(r));
  invalid.u = 0x7fc00000;
  assert(!quantize(invalid.f, 100, -100, 100, &r[0]));
  F(DPTR(d->actor, 0x18), 8) = invalid.f;
  assert(!capture(d));
  B(d->actor, 0x74)++;
  assert(!alive(d));
}
static void placed_claim_test(void) {
  void *a;
  DynamicActor *d;
  fixture();
  addresses();
  placed_actor = 1;
  a = func_802171A8_5D2678(actors[7], d_phases[12], 9);
  world_dynamic_post(a);
  d = lookup(a);
  assert(d && d->placed && d->row[WD_ORDINAL] == 0x7fffffff && capture(d));
  W(a, 0x68) = 0x200;
  tick(d);
  assert(d->claimed && awards == 0);
  W(a, 0x68) = 2;
  world_dynamic_post(a);
  assert(!lookup(a));
}
static void candidate_capacity_and_effect_pose_test(void) {
  static unsigned int candidates[WORLD_DYNAMIC_MAX + 1][64];
  unsigned int i;
  int r[WORLD_DYNAMIC_WORDS];
  DynamicActor *d;
  fixture();
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
    assert(allocate(candidates[i]));
  d = allocate(candidates[WORLD_DYNAMIC_MAX]);
  assert(d && d->actor == candidates[WORLD_DYNAMIC_MAX]);
  fixture();
  addresses();
  parent_authority = 0;
  void *a = func_802171A8_5D2678(actors[7], d_phases[5], 12);
  world_dynamic_child(actors[7], a);
  world_dynamic_post(a);
  assert(!(W(a, 0x68) & 2u) && !lookup(a)->kind);
  fixture();
  row(r, WD_FOOD, 4);
  shine_x = 0;
  assert(reconstruct(r));
  assert(shine_x == 123.45f);
}
static void frame_reconstruction_and_speculative_retirement_test(void) {
  int r[WORLD_DYNAMIC_WORDS];
  char reply[4096];
  DynamicActor *d;
  void *old_actor;
  fixture();
  row(r, WD_HAZARD, 6);
  d = reconstruct(r);
  assert(d);
  old_actor = d->actor;
  d->actor = 0; /* local disappearance must not hide a live remote checkpoint */
  assert(anchor_world_dynamic_encode(&r, 1, reply, sizeof(reply)));
  strcpy(reply + strlen(reply) - 1, ",\"l\":1}");
  bridge_reply = reply;
  anchor_world_dynamic_frame(302, 42, 1, 1);
  d = match(r);
  assert(d && alive(d) && d->actor != old_actor && used == 2);
  r[WD_LIFE] = WD_REMOVED;
  r[WD_OWNER] = r[WD_COMMITTER] = 0;
  assert(anchor_world_dynamic_encode(&r, 1, reply, sizeof(reply)));
  strcpy(reply + strlen(reply) - 1, ",\"l\":1}");
  old_actor = d->actor;
  anchor_world_dynamic_frame(302, 42, 1, 1);
  assert(!lookup(old_actor) && (W(old_actor, 0x68) & 2u));
  bridge_reply = NULL;
}
int main(void) {
  frame_reconstruction_and_speculative_retirement_test();
  candidate_capacity_and_effect_pose_test();
  reconstruction_test();
  claim_test();
  birth_and_reuse_test();
  npc_and_invalid_state_test();
  placed_claim_test();
  puts("world children: reconstruction, resource guards, handoff, claims, NPC "
       "continuation and pool reuse passed");
  return 0;
}
