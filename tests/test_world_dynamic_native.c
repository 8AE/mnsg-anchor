/* Real child hooks and restoration code with host native-call substitutes. */
#include "impact_test_pointers.h"
#include "item_sync.h"
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
#define WORLD_NPC_PTR(p, o) TP(p, o)
#define WORLD_NPC_DISABLED 0ul
#include "../src/anchor_world_dynamic.c"
#include "../src/anchor_world_npc.c"

unsigned short D_800C7AB2 = 302;
unsigned char D_800C7AE2, D_8015CD00[16];
void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
void *D_80236984_5F1E54[1026];
static unsigned int actors[8][64], objects[8][64], model[2];
static unsigned int clips[20];
static unsigned short resources[2] = {100, 101};
static int used, loaded = 1, calls, awards, binds, talks, npc_loaded = 1;
static unsigned int health, ryo, excluded_health, excluded_ryo;
static unsigned int shadow_object[64];
static void *bridge_owned_actor;
static float shine_x;
static unsigned int doll_parent;
static int doll_collected, doll_temp, doll_inits, doll_pickups, doll_cleanups;
static int doll_dialogue, doll_paused, doll_helper_removals;

unsigned int item_sync_local_player_health(void) { return health; }
unsigned int item_sync_local_player_ryo(void) { return ryo; }
void item_sync_exclude_loot_reward(unsigned int hp, unsigned int money) {
  excluded_health += hp;
  excluded_ryo += money;
}

int anchor_world_actor_placed(void *a) {
  (void)a;
  return placed_actor;
}
int anchor_world_bridge_owns(void *a) { return a && a == bridge_owned_actor; }
int anchor_world_is_paused(void) { return doll_paused; }
unsigned int anchor_world_doll_parent(void) { return doll_parent; }
int anchor_world_doll_parent_valid(unsigned int i) {
  return i && i == doll_parent && (D_800C7AB2==0x16a || D_800C7AB2==0x182);
}
int func_800240DC_24CDC(int flag) { assert(flag==0xee);return doll_collected; }
void func_80023DF0_249F0(int flag) { assert(flag==2);doll_temp=1; }
void func_80035020_35C20(void) { ++doll_helper_removals; }
void func_8021925C_5D472C(void *a,void *o) {(void)a;(void)o;}
void func_80218F30_5D4400(void *a,void *o) {(void)a;(void)o;}
void func_080027AC_723DCC(void *a,void *o) {(void)a;(void)o;}
void func_08000514_6AECF4(void *a,void *o) {
  assert(D_8016DAB4_16E6B4==a && H(a,0x28)==26 && H(a,0xd0)==0xee);
  assert(DPTR(a,0x8)==func_8021925C_5D472C && DPTR(a,0x10)==func_80218F30_5D4400);
  assert(H(a,0x5c)==0 && H(a,0x5e)==0 && !DPTR(a,0x84));
  assert(!H(a,0x3c) && !H(a,0x3e) && !H(a,0x40) && !H(a,0x96));
  assert(!W(a,0x98) && !F(o,0x68) && !F(o,0x6c));
  ++doll_inits;W(a,0x60)=0x004006e0;B(a,0x4c)=8;
  W(a,0x48)=0xffffffffu;H(a,0x4e)=H(a,0x50)=50;S(a,0x52)=0;
  H(a,0x5e)=1;world_dynamic_static(a,o,2);H(a,0x5e)=0;
  func_80219E70_5D5340(a,8,0);
  for(int j=0;j<3;++j)H(o,0x14+j*2)=0x8000;
  DPTR(a,0xc)=(void *)func_080005F8_6AEDD8;
}
void func_080005F8_6AEDD8(void *a,void *o) {
  (void)o;assert(D_8016DAB4_16E6B4==a && (W(a,0x68)&0x200u));
  assert(!D_800C7AE2);++doll_pickups;doll_collected=D_800C7AE2=1;
  W(a,0x60)=0;DPTR(a,0xc)=(void *)func_080006E0_6AEEC0;
}
void func_080006E0_6AEEC0(void *a,void *o) {
  (void)o;assert(D_8016DAB4_16E6B4==a && D_800C7AE2);
  if(!doll_dialogue) { ++doll_cleanups;D_800C7AE2=0;world_dynamic_delete(); }
}
static int stable_loot_ordinal = -1;
static unsigned int shutter_ordinal;
int anchor_world_shutter_birth(void *parent, unsigned int *index, unsigned int *ordinal) {
  (void)parent;
  if (!shutter_ordinal) return 0;
  *index = 3; *ordinal = shutter_ordinal;
  return 1;
}
int anchor_world_loot_ordinal(void *parent, unsigned int *ordinal) {
  (void)parent;
  if (stable_loot_ordinal < 0) return 0;
  *ordinal = (unsigned int)stable_loot_ordinal;
  return 1;
}
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
WORLD_NPC_PHASES(DSTUB)
DSTUB(func_802130C8_5CE598)
DSTUB(func_8021332C_5CE7FC)
static int robot_inits, robot_ai, robot_deaths, robot_drops;
void func_08001EA4_6D0F84(void *a, void *o) {
  assert(D_8016DAB4_16E6B4 == a && W(a, 0xd0) == 57 && W(a, 0xd4) == 1);
  ++robot_inits;
  H(a, 0x5e) = 0xfb; W(a, 0x60) = 0x082e8261u;
  H(a, 0x96) = 0x21; B(a, 0x8e) = 3; B(a, 0x8f) = 4;
  B(a, 0x4c) = 1; B(a, 0x6d) = 1; H(a, 0xc4) = 57;
  S(a, 0xc6) = 0; B(a, 0xce) = B(a, 0xcf) = 0;
  DPTR(a, 0x9c) = a;
  for (int i = 0; i < 3; ++i) S(a, 0xc8 + i * 2) = (short)F(o, 8 + i * 4);
  DPTR(a, 0xc) = func_08001FF4_6D10D4;
}
void func_08001FF4_6D10D4(void *a, void *o) {
  (void)o;
  ++robot_ai;
  ++S(a, 0xc6);
}
/* Verified ordering: native 18350 executes death/drop inside the call;
 * a return hook would already be too late to defer its side effects. */
static void robot_damage_step(void *a) {
  world_dynamic_enemy_commit(a);
  world_dynamic_enemy_damage(a);
  unsigned int status = W(a, 0x68);
  if ((status & 0x40000u) || ((status & 0x80u) && !(status & 1u) && DPTR(a, 0x38))) {
    if (--B(a, 0x8d) == 0) {
      ++robot_deaths;
      if (!(W(a, 0x64) & 0x8000u)) ++robot_drops;
      W(a, 0x68) |= 2u;
    }
  }
  W(a, 0x68) &= ~0x40000u;
}
short D_08001C50_71D6D0[20], D_08001C78_71D6F8[28];
static int face_calls;
void func_8021A764_5D5C34(void *a, unsigned int s, int i, int b) {
  (void)a;
  (void)s;
  (void)i;
  (void)b;
  ++face_calls;
}
void func_80224D50_5E0220(void *a, int b) {
  (void)a;
  (void)b;
  ++face_calls;
}
int func_800141C4_14DC4(unsigned int file) {
  if (file == 59 && !npc_loaded) return -1;
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
  world_dynamic_static(a, DPTR(a, 0x18), c);
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
  ryo = ryo + 5 > 9999 ? 9999 : ryo + 5;
}
void func_80213FF0_5CF4C0(void *a) {
  (void)a;
  ++awards;
  health = health + 2 > 20 ? 20 : health + 2;
}
int func_801DCD48_598C58(signed char amount) {
  health = health + amount > 20 ? 20 : health + amount;
  ++awards;
  return 0;
}
void func_80038B98_39798(unsigned int s) { (void)s; }
void *func_8021804C_5D351C(void *a, unsigned int f) {
  if (f)
    shine_x = F(DPTR(a, 0x18), 8);
  return NULL;
}
void func_80219E70_5D5340(void *a, unsigned char s, unsigned char o) {
  unsigned short model = H(a, 0x5e);
  (void)s;
  (void)o;
  W(a, 0x60) |= 0x08000000u;
  /* Native 80219E70 -> 80216E54 -> 80216CE0 reuses the parent task
   * with a linked object and temporarily chooses model 1, slot 0. */
  H(a, 0x5e) = 1;
  world_dynamic_static(a, shadow_object, 0);
  H(a, 0x5e) = model;
}
void func_80224ABC_5DFF8C(void *a, int i, float s, int f) {
  (void)i;
  (void)s;
  B(a, 0xab) = 5;
  B(a, 0xac) = 1;
  B(a, 0xad) = 0;
  B(a, 0xae) = 1;
  if (f)
    W(a, 0x60) |= 4;
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
  H(a,0x3c)=H(parent,0x3c);H(a,0x3e)=H(parent,0x3e);H(a,0x40)=H(parent,0x40);
  H(a,0x96)=H(parent,0x96);W(a,0x98)=W(parent,0x98);
  F(o,0x68)=(float)B(parent,0x99)/2;F(o,0x6c)=(float)B(parent,0x98);
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
  npc_loaded = 1;
  calls = awards = binds = talks = face_calls = 0;
  health = 10;
  ryo = 100;
  excluded_health = excluded_ryo = 0;
  parent_authority = 1;
  placed_actor = 0;
  bridge_owned_actor = 0;
  stable_loot_ordinal = -1;
  shutter_ordinal = 0;
  doll_parent=0;doll_collected=doll_temp=doll_inits=doll_pickups=doll_cleanups=0;
  doll_dialogue=doll_paused=doll_helper_removals=D_800C7AE2=0;
  D_800C7AB2=302;
  robot_inits = robot_ai = robot_deaths = robot_drops = 0;
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
  D_8016DAB4_16E6B4 = d->actor;
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
static void native_npc_reconstruction_test(void) {
  int r[WORLD_DYNAMIC_WORDS];
  DynamicActor *d;
  fixture();
  row(r, WD_NPC, 0);
  r[WD_ENTITY] = r[WD_MODEL] = 0x2be;
  r[WD_ROUTE] = 0;
  r[WD_NPC_CHECKPOINT] = 2;
  npc_loaded = 0;
  assert(!reconstruct(r) && !used);
  npc_loaded = 1;
  d = reconstruct(r);
  assert(d);
  d->have = 1;
  d->owner = 1;
  assert(apply(d));
  assert(d->native == func_08000230_71BCB0 && face_calls == 1);
  assert(W(d->actor, 0x90) == 0x080025b0 && W(d->actor, 0x9c) == 0x080045b0);
  assert(W(d->actor, 0xa0) == 0x080035b0);
  d->dirty = 1;
  assert(apply(d));
  assert(face_calls == 1); /* no blink reset on snapshots */
  tick(d);
  assert(!calls && face_calls == 2);
  d->owner = 2;
  tick(d);
  assert(calls == 1);
  assert(capture(d) && d->row[WD_NPC_CHECKPOINT] == 2);

  fixture();
  row(r, WD_NPC, 0);
  r[WD_ENTITY] = r[WD_MODEL] = 0x2c4;
  int *n = r + WD_NPC_CHECKPOINT;
  n[0] = 13;
  n[1] = -200;
  n[2] = 100;
  n[3] = 70;
  n[4] = 24;
  n[5] = -2;
  n[6] = 1;
  n[7] = 500;
  n[8] = 2;
  d = reconstruct(r);
  assert(d);
  d->have = 1;
  d->owner = 1;
  assert(apply(d));
  assert(d->native == func_08000FD4_71CA54);
  assert(DPTR(d->actor, 0xe0) == D_08001C78_71D6F8);
  assert(S(d->actor, 0xe4) == 24 && S(d->actor, 0xe6) == -2 &&
         S(d->actor, 0xea) == 500);
  tick(d);
  assert(!calls && !awards);
  d->owner = 2;
  tick(d);
  assert(calls == 1 && !awards);
  assert(capture(d) &&
         !memcmp(d->row + WD_NPC_CHECKPOINT, n, WORLD_NPC_WORDS * sizeof(int)));
  n[4] = 26;
  assert(!resource_valid(r)); /* terminator outside either live route */
  n[4] = 24;
  n[0] = 2;
  assert(!resource_valid(r)); /* cross-model callback */
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
  assert(ryo == 105 && excluded_ryo == 5 && excluded_health == 0);
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
  assert(health == 10 && excluded_health == 0 && excluded_ryo == 0);
}
static void capped_private_rewards_test(void) {
  int r[WORLD_DYNAMIC_WORDS], kind;
  for (kind = WD_COIN; kind <= WD_FOOD; ++kind) {
    DynamicActor *d;
    fixture();
    row(r, kind, kind == WD_COIN ? 1 : kind == WD_HEALTH ? 3 : 4);
    d = reconstruct(r);
    assert(d);
    d->have = 1;
    d->owner = 1;
    assert(apply(d));
    assert(H(DPTR(d->actor, 0x18), 0x14) == 0x8000);
    assert(H(DPTR(d->actor, 0x18), 0x16) == 0x8000);
    assert(H(DPTR(d->actor, 0x18), 0x18) == 0x8000);
    ryo = 9997;
    health = 19;
    d->claimed = 1;
    d->row[WD_LIFE] = WD_REMOVED;
    d->row[WD_OWNER] = 2;
    tick(d);
    tick(d);
    assert(awards == 1);
    if (kind == WD_COIN)
      assert(ryo == 9999 && excluded_ryo == 2 && excluded_health == 0);
    else
      assert(health == 20 && excluded_health == 1 && excluded_ryo == 0);
    d->granted = 0;
    tick(d);
    assert(awards == 2); /* another pickup at the cap excludes no extra gain */
    assert(excluded_ryo == (kind == WD_COIN ? 2u : 0u));
    assert(excluded_health == (kind == WD_COIN ? 0u : 1u));
  }
}
static void native_drop_shadow_registration_test(void) {
  unsigned int parent, kind;
  /* Both container and enemy drops keep their parent's entity number. */
  for (parent = 0; parent < 2; ++parent)
    for (kind = WD_COIN; kind <= WD_HEALTH; ++kind) {
      void *a;
      DynamicActor *d;
      fixture();
      addresses();
      a = func_802171A8_5D2678(actors[7], d_phases[kind == WD_COIN ? 0 : 2], 9);
      H(a, 0x5c) = parent ? 0x1e : 0x192;
      world_dynamic_child(actors[7], a);
      func_80216DF8_5D22C8(a, kind == WD_COIN ? 4 : 3);
      func_80219E70_5D5340(a, 5, 0);
      world_dynamic_post(a);
      d = lookup(a);
      assert(d && d->ready && d->kind == kind && capture(d));
      assert(d->row[WD_CLIP] == (kind == WD_COIN ? 4 : 3));
      W(a, 0x68) = 0x200;
      tick(d);
      assert(d->claimed && awards == 0 && capture(d));
      assert(d->row[WD_LIFE] == WD_CLAIM);
    }
}
static void local_coin_spin_and_shared_lifetime_test(void) {
  int r[WORLD_DYNAMIC_WORDS], owner, frame;
  for (owner = 1; owner <= 2; ++owner) {
    DynamicActor *d;
    unsigned int loops = 0;
    fixture();
    row(r, WD_COIN, 1);
    r[WD_TIMER] = 90;
    r[WD_FLAGS_LO] = 2; /* Native texture stepping; loop choice stays local. */
    d = reconstruct(r);
    assert(d);
    d->have = 1;
    d->owner = owner;
    for (frame = 0; frame < 80; ++frame) {
      unsigned char cursor = B(d->actor, 0xad);
      d->net[WD_TIMER] = 90 - frame;
      d->dirty = 1;
      tick(d);
      assert(B(d->actor, 0xad) == cursor); /* snapshots do not reset spin */
      assert(S(d->actor, 0x8a) == 90 - frame);
      /* Native 80224B88/80224834 advance locally and test bit 4 at the
       * end of the texture sequence. Use a short substitute sequence. */
      assert((W(d->actor, 0x60) & 6) == 6 && B(d->actor, 0xac) == 1);
      if (++B(d->actor, 0xad) == 8) {
        if (W(d->actor, 0x60) & 4)
          B(d->actor, 0xad) = 0;
        else
          B(d->actor, 0xac) = 0;
        ++loops;
      }
    }
    assert(loops == 10 && capture(d) && d->row[WD_TIMER] == 11);
    d->row[WD_LIFE] = WD_REMOVED;
    d->row[WD_OWNER] = 0; /* shared expiry, not a pickup */
    tick(d);
    assert((W(d->actor, 0x68) & 2) && awards == 0);
  }
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
  fixture();addresses();stable_loot_ordinal = 0xff08;
  for (int j = 0; j < 2; ++j) {
    a = func_802171A8_5D2678(actors[7], d_phases[0], 9);
    H(a, 0x5c) = 0x3d0;
    world_dynamic_child(actors[7], a);
    world_dynamic_static(a, DPTR(a, 0x18), 4);
    world_dynamic_post(a);
    d = lookup(a);
    if (!j) {
      assert(d && capture(d) && d->row[WD_ORDINAL] == 0xff08);
      assert(d_loot_ordinals[3][0] == 0);
    } else
      assert(!d && (W(a, 0x68) & 2u));
  }
  stable_loot_ordinal = -1;
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
static void shutter_enemy_test(void) {
  int r[WORLD_DYNAMIC_WORDS];
  fixture();
  row(r, WD_SHUTTER_ENEMY, 15);
  r[WD_CID] = 0x7ffffffc; r[WD_PARENT] = 3; r[WD_ORDINAL] = 7;
  r[WD_ENTITY] = 0xfc; r[WD_MODEL] = 0xfb; r[WD_ANIMATED] = 1;
  r[WD_ROUTE] = 57; r[WD_PATH_PC] = 12; r[WD_PATH_STATE] = 2;
  r[WD_PATH_TIMER] = 21; r[WD_ORIGIN_X] = -120; r[WD_ORIGIN_Y] = 50;
  r[WD_BASE_Y] = 1;
  assert(anchor_world_dynamic_row_valid(r));
  DynamicActor *d = reconstruct(r);
  assert(d && !robot_inits && !robot_ai);
  d->parent = 3; d->have = 1; d->owner = 1;
  void *a = d->actor;
  tick(d);
  assert(robot_inits == 1 && !robot_ai && d->enemy_initialized);
  assert(W(a, 0x60) == 0x082e8261u && DPTR(a, 0x9c) == a);
  assert(B(a, 0xcf) == 12 && B(a, 0xce) == 2 && S(a, 0xc8) == -120);
  assert(S(a, 0xc6) == 21 && (W(a, 0x68) & 0x400u));
  tick(d); assert(robot_inits == 1 && !robot_ai);
  d->owner = d_self; tick(d);
  assert(robot_ai == 1 && S(a, 0xc6) == 22); /* native handoff resumes once */
  d->owner = 1;
  DPTR(a, 0x38) = actors[7]; W(a, 0x68) |= 0x80u;
  robot_damage_step(a);
  assert(d->claimed && B(a, 0x8d) == 1 && !robot_deaths && !robot_drops);
  assert((W(a, 0x68) & 1u) && B(a, 0x8c) == 60 && capture(d));
  assert(d->row[WD_LIFE] == WD_CLAIM && d->row[WD_LANDED]);
  tick(d); assert(robot_ai == 1); /* pending kill cannot continue the route */
  d->row[WD_LIFE] = WD_REMOVED; d->row[WD_COMMITTER] = 1;
  robot_damage_step(a);
  assert(robot_deaths == 1 && !robot_drops && (W(a, 0x68) & 2u));
  assert(used == 3 && B(actors[1], 0x8e) == 3 && B(actors[2], 0x8f) == 4);
  assert(!lookup(actors[1]) && !lookup(actors[2])); /* Local visual children. */
  d->claimed = 0; d->have = 0; /* A non-hitting observer follows this path. */
  world_dynamic_post(a);
  assert(!d->actor && capture(d) && d->row[WD_COMMITTER] == 1);

  /* The arbiter uses the same native death path with exactly one drop roll. */
  fixture(); d = reconstruct(r); assert(d);
  d->parent = 3; d->have = 1; d->owner = d_self; tick(d); a = d->actor;
  d->row[WD_LIFE] = WD_REMOVED; d->row[WD_LANDED] = 1;
  d->row[WD_COMMITTER] = (int)d_self;
  robot_damage_step(a);
  assert(robot_deaths == 1 && robot_drops == 1);
  void *drop = func_802171A8_5D2678(a, d_phases[0], 9);
  world_dynamic_child(a, drop);
  DynamicActor *loot = lookup(drop);
  assert(loot && loot->eligible && loot->parent == 3 && loot->stable_ordinal);
  assert(loot->row[WD_ORDINAL] == 7);

  /* Ending the finite route is removal without damage, effects or a drop. */
  fixture(); d = reconstruct(r); assert(d);
  d->parent = 3; d->have = 1; d->owner = d_self; tick(d); a = d->actor;
  assert(capture(d)); W(a, 0x68) |= 2u; world_dynamic_post(a);
  assert(!d->actor && d->row[WD_LIFE] == WD_REMOVED && !d->row[WD_LANDED]);
  assert(d->row[WD_COMMITTER] == (int)d_self && !robot_deaths && !robot_drops);
  fixture(); r[WD_PATH_PC] = 1; assert(!reconstruct(r));
  fixture(); row(r, WD_COIN, 1);
  r[WD_ENTITY] = 0xfc; r[WD_CID] = 0x7ffffffb;
  r[WD_ORDINAL] = r[WD_SERIAL] = 0x7fffffff; r[WD_PARENT] = 3;
  assert(reconstruct(r)); /* Extended drop identity is not a placed sentinel. */

  /* The native child hook identifies actual births without replaying the
   * shutter. Each initialized robot retains the parent's emission ordinal. */
  fixture();
  for (unsigned int ordinal = 9; ordinal <= 10; ++ordinal) {
    shutter_ordinal = ordinal;
    a = func_802171A8_5D2678(actors[7], func_08001EA4_6D0F84, 6);
    world_dynamic_child(actors[7], a);
    H(a, 0x5c) = 0xfc; W(a, 0xd0) = 57; W(a, 0xd4) = 1; B(a, 0x8d) = 1;
    D_8016DAB4_16E6B4 = a;
    func_08001EA4_6D0F84(a, DPTR(a, 0x18));
    world_dynamic_animation(a, 0); world_dynamic_post(a);
    d = lookup(a);
    assert(d && d->kind == WD_SHUTTER_ENEMY && d->enemy_initialized && !d->death_started);
    assert(d->parent == 3 && d->row[WD_ORDINAL] == (int)ordinal && capture(d));
    /* Local invulnerability and unrelated routes cannot generate a claim. */
    W(a, 0x68) = 0x81; DPTR(a, 0x38) = actors[7];
    world_dynamic_enemy_damage(a); assert(!d->claimed);
    W(a, 0x68) = 0x80; H(a, 0xc4) = 56;
    world_dynamic_enemy_damage(a); assert(!d->claimed);
    H(a, 0xc4) = 57; W(a, 0x68) = 0;
  }
}
static void bridge_cohort_exclusion_test(void) {
  fixture();void *a=actors[0];DPTR(a,0x18)=objects[0];H(a,0x5c)=0x2d0;
  bridge_owned_actor=a;
  world_dynamic_child(actors[7],a);world_dynamic_npc(a);world_dynamic_path(a,0x44);
  assert(!lookup(a));
  /* The same native NPC outside the coupled controller still registers. */
  bridge_owned_actor=0;world_dynamic_child(actors[7],a);world_dynamic_npc(a);
  world_dynamic_path(a,0x44);DynamicActor *d=lookup(a);
  assert(d && d->kind==WD_NPC && d->path && d->row[WD_ROUTE]==0x44);
}
static DynamicActor *doll_fixture(void) {
  fixture();D_800C7AB2=d_room=0x16a;doll_parent=8;
  H(actors[7],0x3c)=23;H(actors[7],0x3e)=48;H(actors[7],0x40)=6;
  H(actors[7],0x96)=0x123;W(actors[7],0x98)=0x10182030;
  assert(anchor_world_dynamic_doll_spawn(actors[7],8));
  DynamicActor *d=lookup(actors[0]);assert(d && d->kind==WD_DOLL && doll_temp);
  assert(!doll_inits && !d->doll_initialized && used==1);
  assert(H(d->actor,0xd0)==0xee && H(d->actor,0xd2)==0);
  assert(DPTR(d->actor,0x84)==0 && d->parent==8);
  assert(anchor_world_dynamic_doll_spawn(actors[7],8) && used==1);
  tick(d);assert(doll_inits==1 && d->doll_initialized && capture(d));
  assert(d->row[WD_PHASE]==16 && d->row[WD_Y]==12500);
  assert(d->row[WD_ENTITY]==0 && d->row[WD_MODEL]==0 && d->row[WD_CLIP]==2);
  assert(H(DPTR(d->actor,0x18),0x14)==0x8000);
  return d;
}
static void nested_doll_test(void) {
  DynamicActor *d=doll_fixture();void *a=d->actor,*o=DPTR(a,0x18);
  tick(d);assert(F(o,12)==125); /* unconfirmed birth waits */
  d->have=1;d->owner=d_self;d->net[WD_PAUSED]=0;
  for(int i=0;i<91;++i) tick(d);
  assert(F(o,12)==34 && capture(d) && d->row[WD_PHASE]==17);
  tick(d);assert(F(o,12)==34);
  d->dirty=1;d->net[WD_Y]=10000;assert(apply(d) && F(o,12)==34);
  /* One native pickup after a committed claim. Cleanup keeps the actor alive
   * through dialogue/disconnect, then releases controls before deletion. */
  W(a,0x68)|=0x200;tick(d);assert(d->claimed && !doll_pickups);
  d->row[WD_LIFE]=WD_REMOVED;d->row[WD_OWNER]=(int)d_self;
  d->row[WD_COMMITTER]=1;D_800C7AE2=1;tick(d);assert(!doll_pickups);
  D_800C7AE2=0;tick(d);assert(doll_pickups==1 && D_800C7AE2 && d->doll_scene);
  doll_dialogue=1;d_active=0;tick(d);assert(doll_pickups==1 && !doll_cleanups && d->actor==a);
  doll_dialogue=0;tick(d);assert(doll_cleanups==1 && !D_800C7AE2 && !d->actor);

  d=doll_fixture();a=d->actor;o=DPTR(a,0x18);
  d->have=1;d->owner=1;d->dirty=1;d->net[WD_Y]=6300;
  tick(d);assert(F(o,12)==63 && !doll_pickups);
  doll_paused=1;d->owner=d_self;tick(d);assert(F(o,12)==63);
  doll_paused=0;tick(d);assert(F(o,12)==62 && capture(d));
  /* A recycled task isn't a collection; reconstruct its last descent without
   * touching the new object that occupies the old task address. */
  world_dynamic_reuse(a);assert(d->row[WD_LIFE]==WD_LIVE && !d->actor);
  H(a,0x5c)=0x555;doll_recover(d);d=lookup(actors[1]);
  assert(d && H(a,0x5c)==0x555 && !doll_pickups);
  tick(d);assert(F(DPTR(d->actor,0x18),12)==62 && doll_inits==2);
  d->claimed=1;d->row[WD_LIFE]=WD_REMOVED;d->row[WD_OWNER]=1;
  tick(d);assert(W(d->actor,0x68)&2u);assert(!doll_pickups && !doll_cleanups && !D_800C7AE2);

  d=doll_fixture();doll_collected=1;tick(d);
  assert((W(d->actor,0x68)&2u) && d->row[WD_LIFE]==WD_REMOVED && !doll_pickups);
  d=doll_fixture();d_active=0;W(d->actor,0x68)|=0x200u;tick(d);
  assert(doll_pickups==1 && d->doll_scene && d->row[WD_LIFE]==WD_REMOVED);
  tick(d);assert(!d->actor && doll_cleanups==1 && !D_800C7AE2);
  d_active=1;assert(capture(d) && d->row[WD_COMMITTER]==(int)d_self);
  fixture();D_800C7AB2=d_room=0x16a;doll_parent=8;
  loaded=0;assert(!anchor_world_dynamic_doll_spawn(actors[7],8) && !used);
  loaded=1;D_80236984_5F1E54[1]=0;
  assert(!anchor_world_dynamic_doll_spawn(actors[7],8) && !used);
  D_80236984_5F1E54[1]=model;clips[2]=0;
  assert(!anchor_world_dynamic_doll_spawn(actors[7],8) && !used);
  clips[2]=3;assert(!anchor_world_dynamic_doll_spawn(actors[7],3) && !used);
  doll_collected=1;assert(anchor_world_dynamic_doll_spawn(actors[7],8) && !used);
  /* Legacy/offline helper births are explicitly attached to the container.
   * Retiring the mover cannot dereference a deleted/reused Doll. */
  fixture();D_800C7AB2=d_room=0x16a;doll_parent=8;
  void *helper=actors[6];world_dynamic_doll_helper(helper);
  a=func_802171A8_5D2678(helper,func_08000514_6AECF4,9);
  world_dynamic_child(helper,a);d=lookup(a);
  assert(d && d->doll_birth && d->eligible && d->parent==8);
  DPTR(helper,0xec)=a;world_dynamic_doll_helper_return();
  assert(DPTR(helper,0xc)==doll_helper_retire);
  DPTR(helper,0xec)=0;D_8016DAB4_16E6B4=helper;
  doll_helper_retire(helper,0);assert(doll_helper_removals==1);
  H(a,0x5c)=H(a,0x5e)=0;H(a,0xd0)=0xee;
  F(DPTR(a,0x18),8)=-6;F(DPTR(a,0x18),12)=125;F(DPTR(a,0x18),16)=-102;
  d->clip=2;DPTR(a,0xc)=(void *)func_080005F8_6AEDD8;
  world_dynamic_post(a);assert(d->ready && d->kind==WD_DOLL && d->doll_initialized);
  assert(DPTR(a,0xc)==doll_callback && capture(d));
}
int main(void) {
  nested_doll_test();
  bridge_cohort_exclusion_test();
  shutter_enemy_test();
  native_npc_reconstruction_test();
  local_coin_spin_and_shared_lifetime_test();
  native_drop_shadow_registration_test();
  capped_private_rewards_test();
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
