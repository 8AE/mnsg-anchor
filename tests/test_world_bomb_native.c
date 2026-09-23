/* Host harness for the File_40 0x1A9 bomb adapter.
 *
 * It includes the real dynamic lifecycle plus the guarded bomb include and
 * drives the adapter API directly, so the checks do not depend on the parent's
 * wiring order. The four bomb bookkeeping fields (bomb_birth, bomb_initialized,
 * bomb_frozen, bomb_particles) and the "#include anchor_world_bomb.inc" line
 * are the parent's dynamic.c integration points; until they land this file is
 * compiled against a patched copy of the tree (see the handoff run command). */
#include "impact_test_pointers.h"
#include "progression/item_sync.h"
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
  if (!bridge_reply) return NULL;
  result = malloc(strlen(bridge_reply) + 1);
  strcpy(result, bridge_reply);
  return result;
}
static int parent_authority = 1, placed_actor;
static unsigned int placed_index = 8;
static int enemy_sync_actor_authority(void *a) { (void)a; return -1; }
#define WORLD_NPC_PTR(p, o) TP(p, o)
#define WORLD_NPC_DISABLED 0ul
#include "../src/world/anchor_world_dynamic.c"
#include "../src/world/anchor_world_npc.c"
#include "../src/world/anchor_world_bomb.inc"

/* The bomb harness links the complete dynamic source. File_46 wave natives
 * are unreachable in this room, but their typed callback symbols must link. */
unsigned short D_8015CDB4, D_8015CDC0;
void *D_801FC60C_5B851C;
#define WAVE_STUB(name) void name(void *a, void *o) { (void)a; (void)o; }
WAVE_STUB(func_0800370C_70480C)
WAVE_STUB(func_0800376C_70486C)
WAVE_STUB(func_080037B0_7048B0)
WAVE_STUB(func_08003B4C_704C4C)
WAVE_STUB(func_08003C90_704D90)
WAVE_STUB(func_08003D64_704E64)
WAVE_STUB(func_08003E60_704F60)
WAVE_STUB(func_08003F68_705068)
WAVE_STUB(func_08004034_705134)
WAVE_STUB(func_80212088_5CD558)
#undef WAVE_STUB
/* File30 fragile callbacks are unreachable in this File40 bomb fixture. */
#define FRAGILE_STUB(name) void name(void *a, void *o) { (void)a; (void)o; }
FRAGILE_STUB(func_0800028C_6BF9DC)
FRAGILE_STUB(func_08004CEC_6C443C)
FRAGILE_STUB(func_08004D64_6C44B4)
FRAGILE_STUB(func_08004AA0_6C41F0)
FRAGILE_STUB(func_08004AB4_6C4204)
FRAGILE_STUB(func_0800664C_6C5D9C)
FRAGILE_STUB(func_08006778_6C5EC8)
FRAGILE_STUB(func_0800676C_6C5EBC)
FRAGILE_STUB(func_08004AE8_6C4238)
FRAGILE_STUB(func_08007984_6C70D4)
FRAGILE_STUB(func_80214314_5CF7E4)
FRAGILE_STUB(func_802141AC_5CF67C)
#undef FRAGILE_STUB
void func_080014B4_6D57F4(void *a,void *o) { (void)a;(void)o; }
void func_080002C0_72ACF0(void *a,void *o) { (void)a;(void)o; }
void func_08000668_72B098(void *a,void *o) { (void)a;(void)o; }
void func_08000928_72B358(void *a,void *o) { (void)a;(void)o; }
int anchor_race_boulder_duplicate(void *a,unsigned int *parent,unsigned int *ordinal) {
  (void)a;(void)parent;(void)ordinal;return 0;
}
void anchor_race_boulder_forget(void *a) { (void)a; }
int anchor_world_source_position(unsigned int parent,short out[3]) {
  (void)parent;(void)out;return 0;
}
int func_08003A30_704B30(void *a) { (void)a; return 0; }
void func_80219E08_5D52D8(void *a, float scale) { (void)a; (void)scale; }

unsigned short D_800C7AB2 = 0x65;
unsigned char D_800C7AE2, D_8015CD00[16];
void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
void *D_80236984_5F1E54[1026];
unsigned char D_8020CBF0_5C8B00[8];
short D_08001C50_71D6D0[20], D_08001C78_71D6F8[28];
static unsigned int actors[16][64], objects[16][64];
static unsigned int root_model[2], child_model[2];
static unsigned short root_files[2] = {0x1de, 0x160};
static unsigned short child_files[2] = {0x191, 0x0000};
static unsigned int root_clips[2] = {0x08000198u, 0};
static unsigned int child_clips[9] = {0x08000100u, 0x080002c0u, 0x08000500u,
                                      0x08000610u, 0x080006f0u, 0x080007d0u,
                                      0x080008b0u, 0x08000990u, 0x08000b60u};
static int used, loaded = 1, calls, awards, binds, talks, npc_loaded = 1;
static unsigned int health, ryo, excluded_health, excluded_ryo;
static void *bridge_owned_actor;
static unsigned int doll_parent;
static int doll_collected, doll_temp, doll_inits, doll_pickups, doll_cleanups;
static int doll_dialogue, doll_paused, doll_helper_removals;
static int bomb_sounds, bomb_fanouts, bomb_removals, bomb_dl_calls;

unsigned int item_sync_local_player_health(void) { return health; }
unsigned int item_sync_local_player_ryo(void) { return ryo; }
void item_sync_exclude_loot_reward(unsigned int hp, unsigned int money) {
  excluded_health += hp; excluded_ryo += money;
}
int anchor_world_actor_placed(void *a) {
  return a == (void *)actors[0] && placed_actor;
}
int anchor_world_actor_authority(void *a, unsigned int *index) {
  (void)a; *index = placed_index; return parent_authority;
}
int anchor_world_bridge_owns(void *a) { return a && a == bridge_owned_actor; }
int anchor_world_is_paused(void) { return doll_paused; }
unsigned int anchor_world_doll_parent(void) { return doll_parent; }
int anchor_world_doll_parent_valid(unsigned int i) {
  return i && i == doll_parent && (D_800C7AB2 == 0x16a || D_800C7AB2 == 0x182);
}
int func_800240DC_24CDC(int flag) { assert(flag == 0xee); return doll_collected; }
void func_80023DF0_249F0(int flag) { assert(flag == 2); doll_temp = 1; }
void func_80035020_35C20(void) { ++doll_helper_removals; }
void func_8021925C_5D472C(void *a, void *o) { (void)a; (void)o; }
void func_80218F30_5D4400(void *a, void *o) { (void)a; (void)o; }
void func_080027AC_723DCC(void *a, void *o) { (void)a; (void)o; }
void func_08000514_6AECF4(void *a, void *o) { (void)a; (void)o; ++doll_inits; }
void func_080005F8_6AEDD8(void *a, void *o) { (void)a; (void)o; }
void func_080006E0_6AEEC0(void *a, void *o) { (void)a; (void)o; }
void func_08001EA4_6D0F84(void *a, void *o) { (void)a; (void)o; }
void func_08001FF4_6D10D4(void *a, void *o) { (void)a; (void)o; }
void func_08004550_6C3CA0(void *a, void *o) { (void)a; (void)o; }
void func_08004594_6C3CE4(void *a, void *o) { (void)a; (void)o; }
void func_08004654_6C3DA4(void *a, void *o) { (void)a; (void)o; }
void func_08004694_6C3DE4(void *a, void *o) { (void)a; (void)o; }
void func_0800488C_6C3FDC(void *a, void *o) { (void)a; (void)o; }
int func_8021B988_5D6E58(void *a, float x, float y, float z, float s) {
  (void)a; (void)x; (void)y; (void)z; (void)s; return 1;
}
#define DSTUB(f) void f(void *a, void *o) { (void)a; (void)o; ++calls; }
DYNAMIC_PHASES(DSTUB)
WORLD_NPC_PHASES(DSTUB)
DSTUB(func_802130C8_5CE598)
DSTUB(func_8021332C_5CE7FC)
void func_8021A764_5D5C34(void *a, unsigned int s, int i, int b) {
  (void)a; (void)s; (void)i; (void)b;
}
void func_80224D50_5E0220(void *a, int b) { (void)a; (void)b; }
int func_800141C4_14DC4(unsigned int file) {
  if (!file) return 0;
  return loaded ? 1 : -1;
}
void func_8021664C_5D1B1C(void *a, unsigned int c, float s, unsigned int f) {
  (void)a; (void)c; (void)s; (void)f; ++binds;
}
void func_80216DF8_5D22C8(void *a, unsigned int c) {
  world_dynamic_static(a, DPTR(a, 0x18), c); ++binds;
}
void func_80218DCC_5D429C(void *a, unsigned int s) { (void)a; (void)s; ++binds; }
float func_8001B5AC_1C1AC(void *o) { (void)o; return 10; }
void func_802145F0_5CFAC0(void *a) { (void)a; ++awards; }
void func_80213FF0_5CF4C0(void *a) { (void)a; ++awards; }
int func_801DCD48_598C58(signed char amount) { (void)amount; ++awards; return 0; }
void func_80038B98_39798(unsigned int s) { (void)s; }
void *func_8021804C_5D351C(void *a, unsigned int f) {
  (void)a; (void)f; return NULL;
}
void func_80219E70_5D5340(void *a, unsigned char s, unsigned char o) {
  (void)s; (void)o; W(a, 0x60) |= 0x08000000u;
}
void func_80224ABC_5DFF8C(void *a, int i, float s, int f) {
  (void)i; (void)s; (void)f; B(a, 0xab) = 5;
}
void func_802268A8_5E1D78(void *a) { (void)a; ++calls; }
int func_80220F70_5DC440(void *a) { (void)a; ++talks; return 0; }
int anchor_world_loot_ordinal(void *parent, unsigned int *ordinal) {
  (void)parent; (void)ordinal; return 0;
}
int anchor_world_shutter_birth(void *parent, unsigned int *index,
                               unsigned int *ordinal) {
  (void)parent; (void)index; (void)ordinal; return 0;
}
void func_080079FC_6C714C(void *a, void *o) { (void)a; (void)o; }
void func_08007B28_6C7278(void *a, void *o) { (void)a; (void)o; }
void func_08007BDC_6C732C(void *a, void *o) { (void)a; (void)o; }
int func_802196FC_5D4BCC(void *a, float x, float y, float z, int s) {
  (void)a; (void)x; (void)y; (void)z; (void)s; return 1;
}
/* ---- bomb native substitutes ------------------------------------------ */
static int bomb_sound_calls, bomb_tint_calls;
void func_08000508_6E9658(void *a, void *o) { (void)a; (void)o; }
void func_08000560_6E96B0(void *a, void *o) { (void)a; (void)o; }
void func_080005F8_6E9748(void *a, void *o) { (void)a; (void)o; }
void func_08000950_6E9AA0(void *a, void *o) { (void)a; (void)o; }
void func_0800042C_6E957C(void *a, void *o) { (void)a; (void)o; }
/* Faithful ring initializer: the fields the native body writes, so the row
 * capture reads real values. */
void func_08000984_6E9AD4(void *a, void *o) {
  unsigned int e8 = W(a, 0xe8);
  (void)o;
  H(a, 0x5e) = 0x7e;
  W(a, 0x60) = 0x004006e0u;
  if ((e8 & 1u) || (e8 & 2u)) {
    B(a, 0xd0) = 0xff;
    H(o, 0x14) = (e8 & 2u) ? 0x200 : 0;
  } else {
    S(a, 0x8a) = 0x5a;
    B(a, 0xd0) = 0xc0;
    B(a, 0x4c) = 6;
    func_80218DA8_5D4278(a, 900, 0, 0);
    if (e8 & 8u) H(o, 0x14) = 0x200;
  }
  DPTR(a, 0xc) = (void *)func_08000AD0_6E9C20;
}
void func_08000AD0_6E9C20(void *a, void *o) { (void)a; (void)o; }
void func_08000C74_6E9DC4(void *a, void *o) {
  (void)o;
  B(a, 0xd0) = 0xff;
  H(a, 0x5e) = 0x7e;
  W(a, 0x60) = 0x02c007e0u;
  B(a, 0x75) = 0x0f;               /* particle gravity */
  DPTR(a, 0xc) = (void *)func_08000E24_6E9F74;
}
void func_08000E24_6E9F74(void *a, void *o) { (void)a; (void)o; }
void *func_8021DF60_5D9430(void *a, unsigned int al, unsigned int r,
                           unsigned int g, unsigned int b) {
  (void)al; (void)r; (void)g; (void)b;
  ++bomb_tint_calls;
  return (void *)((char *)DPTR(a, 0x18) + 0x80);
}
void *func_8021A26C_5D573C(void *a, unsigned int al, unsigned int r,
                           unsigned int g, unsigned int b) {
  (void)al; (void)r; (void)g; (void)b;
  ++bomb_dl_calls;
  return (void *)((char *)DPTR(a, 0x18) + 0x80);
}
void *func_8021DD4C_5D921C(void *a, unsigned int al, unsigned int r,
                           unsigned int g, unsigned int b) {
  (void)al; (void)r; (void)g; (void)b;
  ++bomb_dl_calls;
  return (void *)((char *)DPTR(a, 0x18) + 0x80);
}
void func_80218DA8_5D4278(void *a, int r, unsigned short h, short o) {
  W(a, 0x48) = 0xffffffffu; H(a, 0x4e) = (unsigned short)(r / 10);
  H(a, 0x50) = h; S(a, 0x52) = o;
}
void func_80216E1C_5D22EC(void *a, unsigned int slot) {
  world_dynamic_static(a, DPTR(a, 0x18), slot); ++binds;
}
void func_8000F420_10020(unsigned int id, unsigned char *bank, void *o, float v) {
  assert(id == 0x272 && bank == D_8020CBF0_5C8B00 && o && v == 400.0f);
  ++bomb_sounds;
}
int func_8021B7AC_5D6C7C(void *a, float radius) {
  (void)a; return radius >= 79.0f && radius <= 81.0f ? 1 : 0;
}
void func_80034ED4_35AD4(void) { ++bomb_removals; }
static int spawn_calls;
/* The single native fanout: twelve children in the verified order. */
void func_080006D4_6E9824(void *parent, void *o) {
  unsigned int j;
  (void)o;
  ++bomb_fanouts;
  for (j = 0; j < 4; ++j) {
    unsigned int bit = (j == 0) ? 1u : (j == 1) ? 4u : (j == 2) ? 2u : 8u;
    unsigned char kind = (j == 1 || j == 3) ? 1 : 8;
    void *c = func_802171A8_5D2678(parent, func_08000984_6E9AD4, kind);
    if (c) { H(c, 0x28) = 0x28; W(c, 0xe8) |= bit; }
  }
  for (j = 0; j < 8; ++j) {
    void *c = func_802171A8_5D2678(parent, func_08000C74_6E9DC4, 8);
    if (c) H(c, 0x28) = 0x28;
  }
  W(parent, 0x68) |= 2u;
}
void *func_802171A8_5D2678(void *parent, DynamicCallback callback,
                           unsigned char category) {
  void *a, *o;
  (void)category;
  assert(used < 16);
  a = actors[used]; o = objects[used++];
  DPTR(a, 0x18) = o; DPTR(a, 0xc) = (void *)callback;
  B(a, 0x74) = 3;
  H(a, 0x5c) = 0x1a9; H(a, 0x5e) = 0x7e;
  F(o, 0x1c) = F(o, 0x20) = F(o, 0x24) = 0.2f;
  ++spawn_calls;
  /* The common child hook runs inside func_802171A8 before the caller ORs
   * +0xE8, exactly as the native registry does. */
  world_dynamic_child(parent, a);
  return a;
}
static void fixture(void) {
  unsigned int i;
  anchor_world_dynamic_room();
  test_ptr_count = 0;
  memset(test_ptrs, 0, sizeof(test_ptrs));
  memset(actors, 0, sizeof(actors));
  memset(objects, 0, sizeof(objects));
  used = 0; loaded = 1; npc_loaded = 1;
  calls = awards = binds = talks = 0;
  health = 10; ryo = 100; excluded_health = excluded_ryo = 0;
  parent_authority = 1; placed_actor = 0; placed_index = 8;
  bridge_owned_actor = 0; doll_parent = 0;
  doll_collected = doll_temp = doll_inits = doll_pickups = doll_cleanups = 0;
  doll_dialogue = doll_paused = doll_helper_removals = D_800C7AE2 = 0;
  D_800C7AB2 = 0x65;
  bridge_reply = NULL;
  d_active = 1; d_self = 2; d_leader = 1; d_room = 0x65;
  D_801FC604_5B8514 = actors[15];
  bomb_sounds = bomb_fanouts = bomb_removals = bomb_dl_calls = 0;
  bomb_sound_calls = bomb_tint_calls = spawn_calls = 0;
  DPTR(root_model, 0) = root_files; DPTR(root_model, 4) = root_clips;
  DPTR(child_model, 0) = child_files; DPTR(child_model, 4) = child_clips;
  for (i = 0; i < 1026; ++i) D_80236984_5F1E54[i] = 0;
  D_80236984_5F1E54[WORLD_BOMB_ENTITY] = root_model;
  D_80236984_5F1E54[WORLD_BOMB_CHILD_MODEL] = child_model;
}
/* A placed room 0x65 root on the verified index-8 birth coordinates. */
static void *root_fixture(void) {
  void *a = actors[0], *o = objects[0];
  DPTR(a, 0x18) = o; DPTR(a, 0xc) = (void *)func_08000508_6E9658;
  B(a, 0x74) = 3; H(a, 0x5c) = H(a, 0x5e) = WORLD_BOMB_ENTITY;
  W(a, 0x60) = 0x002006e1u; W(a, 0x68) = 0; B(a, 0x8d) = WB_ROOT_HEALTH;
  W(a, 0x64) |= 0x8000u;         /* native no-loot bit, as func_0800042C ORs */
  H(a, 0x3c) = WB_ROOT_RADIUS; H(a, 0x3e) = WB_ROOT_HEIGHT; H(a, 0x40) = 0;
  DPTR(a, 0x84) = objects[1];   /* the local target the proximity helper reads */
  F(o, 8) = 40.0f; F(o, 0xc) = 120.0f; F(o, 0x10) = 100.0f;
  F(o, 0x1c) = F(o, 0x20) = F(o, 0x24) = 0.2f;
  used = 1;                      /* actors[0] is the placed root */
  placed_actor = 1;
  return a;
}
/* Offer the retained row back as a confirmed remote checkpoint. */
static void offer(DynamicActor *d, unsigned int owner) {
  unsigned int j;
  d->have = 1; d->owner = owner;
  for (j = 0; j < WORLD_DYNAMIC_WORDS; ++j) d->net[j] = d->row[j];
  d->net[WB_RECEIPT] = 1; d->net[WB_INSTANCE] = (int)d->serial;
  d->net[WD_OWNER] = (int)owner; d->dirty = 0;
  d->row[WB_RECEIPT] = 1;
}
static void test_register_and_recipe(void) {
  void *a = root_fixture();
  DynamicActor *d = bomb_register_root(a);
  assert(d && d->kind == WORLD_BOMB && d->row[WB_ROLE] == WB_ROLE_ROOT);
  assert(d->row[WD_PARENT] == 8 && d->row[WD_ORDINAL] == 0);
  assert(d->placed && d->bomb_birth);
  assert(bomb_capture(d));
  assert(anchor_world_bomb_valid(d->row));
  assert(anchor_world_bomb_scope(d->row, 0x65));
  assert(!anchor_world_bomb_scope(d->row, 0x66));
  assert(bomb_resource_valid(d->row));
  assert(d->row[WD_RADIUS] == WB_ROOT_RADIUS &&
         d->row[WD_HEIGHT] == WB_ROOT_HEIGHT && d->row[WD_OFFSET] == 0);
  assert(d->row[WD_X] == 4000 && d->row[WD_Y] == 12000 &&
         d->row[WD_Z] == 10000);
  assert(d->row[WD_BIRTH_X] == 4000 && d->row[WD_BIRTH_Z] == 10000);
  assert(d->row[WD_PHASE] == WB_IDLE && d->row[WD_BASE_Y] == 1);
  /* A wrong slot command is rejected even when the files are resident. */
  root_clips[0] = 0;
  assert(!bomb_resource_valid(d->row));
  root_clips[0] = 0x08000198u;
  assert(bomb_resource_valid(d->row));
  /* A non-placement birth coordinate is rejected. */
  d->row[WD_BIRTH_Z] = 0;
  assert(!bomb_resource_valid(d->row));
  d->row[WD_BIRTH_Z] = 10000;
  assert(bomb_resource_valid(d->row));
  /* Room 0x66 index 16 is a different verified placement. */
  D_800C7AB2 = 0x66; placed_index = 16;
  {
    void *b = root_fixture();
    F(DPTR(b, 0x18), 8) = 90.0f; F(DPTR(b, 0x18), 0x10) = 270.0f;
    DynamicActor *e = bomb_register_root(b);
    assert(e && bomb_capture(e));
    assert(e->row[WD_BIRTH_X] == 9000 && e->row[WD_BIRTH_Z] == 27000);
    assert(bomb_scope(e->row) && bomb_resource_valid(e->row));
  }
}
static void test_damage_defers_native_hit(void) {
  void *a = root_fixture();
  DynamicActor *d = bomb_register_root(a);
  assert(bomb_capture(d));
  W(a, 0x68) = 0x80u;              /* native pending-hit bit */
  DPTR(a, 0x38) = actors[1];       /* attacker */
  bomb_damage(a);
  assert(d->claimed && d->row[WD_LIFE] == WD_CLAIM);
  /* A claim records shared state only: the simulation owner and the sticky
   * committer arbiter stay transport decisions. */
  assert(d->row[WD_COMMITTER] == 0 && d->row[WD_OWNER] == 0);
  assert(d->row[WD_LANDED] == 1 && d->row[WB_CAUSE] == WB_CAUSE_HIT);
  assert(d->row[WD_PHASE] == WB_EXPLODE);
  assert(!(W(a, 0x68) & 0x40080u) && (W(a, 0x68) & 1u));
  assert(B(a, 0x8c) == 60);
  assert(bomb_fanouts == 0 && spawn_calls == 0);
  bomb_damage(a);
  /* The lease is preserved: a claim never rewrites the committer. */
  assert(d->row[WD_COMMITTER] == 0);
  bomb_damage(actors[1]);
  assert(bomb_fanouts == 0);
}
static void test_committer_fanout_once(void) {
  void *a = root_fixture();
  DynamicActor *d = bomb_register_root(a);
  assert(bomb_capture(d));
  offer(d, 2);
  d->row[WD_LIFE] = WD_REMOVED;
  d->row[WD_LANDED] = 1;
  d->row[WD_COMMITTER] = 2;
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(bomb_fanouts == 1 && spawn_calls == 12);
  assert(d->death_started && (W(a, 0x68) & 2u));
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(bomb_fanouts == 1 && spawn_calls == 12);
  /* A peer-named committer retires its own copy without a fanout. */
  {
    DynamicActor *e = &d_actors[1];
    void *b = actors[1];
    clear(e, sizeof(*e));
    e->used = 1; e->actor = b; e->kind = WORLD_BOMB; e->row[WB_ROLE] = 0;
    e->row[WD_LIFE] = WD_REMOVED; e->row[WD_LANDED] = 1;
    e->row[WD_COMMITTER] = 9; e->serial = 77;
    DPTR(b, 0x18) = objects[1];
    assert(bomb_callback(e, b, DPTR(b, 0x18)));
    assert(bomb_fanouts == 1 && (W(b, 0x68) & 2u));
  }
}
static void test_phase_machine_and_claim(void) {
  void *a = root_fixture();
  DynamicActor *d = bomb_register_root(a);
  int i;
  assert(bomb_capture(d));
  offer(d, 2);
  d->net[WD_BASE_Y] = 0; d->row[WD_BASE_Y] = 0;
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(d->row[WD_PHASE] == WB_IDLE);
  d->net[WD_BASE_Y] = 1;            /* aggregated activation */
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(d->row[WD_PHASE] == WB_FALL && d->row[WB_CAUSE] == WB_CAUSE_PROXIMITY);
  assert(W(a, 0x60) == 0x02a007e1u);
  W(a, 0x68) &= ~0x20u;             /* airborne */
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(d->row[WD_PHASE] == WB_FUSE && bomb_sounds == 1);
  for (i = 0; i < 43; ++i) assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(d->row[WB_ALPHA] == WB_FUSE_CAP - WB_FUSE_STEP);
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(d->row[WB_ALPHA] == WB_FUSE_CAP);
  assert(d->row[WD_PHASE] == WB_EXPLODE && d->claimed);
  assert(d->row[WB_CAUSE] == WB_CAUSE_FUSE);
  assert(d->row[WD_LIFE] == WD_CLAIM && d->row[WD_LANDED] == 1);
  /* The claim freezes the root; the native fanout waits for the tombstone. */
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  assert(bomb_fanouts == 0 && d->bomb_frozen);
  assert(W(a, 0x60) == 0 && F(a, 0x78) == 0 && F(a, 0x7c) == 0);
  assert(bomb_capture(d) && d->row[WD_X] == 4000 && d->row[WD_VX] == 0);
  assert(d->row[WD_LIFE] == WD_CLAIM);
}
static void test_children_roles_and_attackers(void) {
  void *a = root_fixture();
  DynamicActor *root = bomb_register_root(a);
  int ring = 0, particle = 0, i;
  assert(bomb_capture(root));
  offer(root, 2);
  root->row[WD_LIFE] = WD_REMOVED;
  root->row[WD_LANDED] = 1;
  root->row[WD_COMMITTER] = 2;
  assert(bomb_callback(root, a, DPTR(a, 0x18)));
  assert(spawn_calls == 12);
  for (i = 1; i < used; ++i) {
    DynamicActor *cd = lookup(actors[i]);
    assert(cd);
    cd->bomb_birth = 1;
    assert(bomb_child(root, cd, 0));
    assert(bomb_child(root, cd, DPTR(actors[i], 0xc)));
    cd->ready = cd->eligible = 1;
    cd->owner = 2;
    assert(bomb_capture(cd));
    assert(anchor_world_bomb_valid(cd->row));
    assert(bomb_scope(cd->row) && bomb_resource_valid(cd->row));
    if (cd->row[WB_ROLE] <= WB_ROLE_V8) {
      ++ring;
      assert(cd->row[WD_CLIP] == 8 && cd->row[WD_PHASE] == WB_RING);
      assert(cd->row[WD_RADIUS] == WB_ROOT_RADIUS &&
             cd->row[WD_HEIGHT] == WB_ROOT_HEIGHT);
      if (cd->row[WB_ROLE] == WB_ROLE_V4 || cd->row[WB_ROLE] == WB_ROLE_V8) {
        assert(cd->row[WD_ATTACK] == WB_ATTACK_TYPE);
        assert(cd->row[WD_ATTACK_RADIUS] == WB_ATTACK_RADIUS);
        assert(cd->row[WD_SPHERE] == 17);
        assert(cd->row[WD_ATTACK_HEIGHT] == 0 &&
               cd->row[WD_ATTACK_OFFSET] == 0);
      } else {
        assert(cd->row[WD_ATTACK] == 0 && cd->row[WD_SPHERE] == 0);
      }
    } else {
      ++particle;
      assert(cd->row[WD_CLIP] == 0 && cd->row[WD_PHASE] == WB_PARTICLE);
      assert(cd->row[WD_ATTACK] == 0 && cd->row[WD_SPHERE] == 0);
    }
    assert(cd->row[WD_ORDINAL] == cd->row[WB_ROLE]);
    assert(cd->row[WB_VARIANT] == (cd->row[WB_ROLE] <= WB_ROLE_V8
                                     ? bomb_role_variant[cd->row[WB_ROLE]] : 0));
    assert(cd->row[WD_BIRTH_X] == 4000 && cd->row[WD_BIRTH_Y] == 12000);
  }
  assert(ring == 4 && particle == 8);
  {
    DynamicActor *cd = lookup(actors[1]);
    cd->bomb_initialized = 0; cd->proxy = 0;
    assert(bomb_apply_extra(cd, cd->row));
    assert(W(cd->actor, 0x60) == 0x004006e0u);
    assert(cd->native == (void *)bomb_phase(WB_RING));
    cd->bomb_initialized = 0; cd->proxy = 1;
    assert(bomb_apply_extra(cd, cd->row));
    assert(cd->native == (void *)bomb_native);
  }
}
static void test_child_prediction_and_owner_expiry(void) {
  void *a = root_fixture();
  DynamicActor *root = bomb_register_root(a);
  DynamicActor *cd;
  void *ca, *co;
  unsigned char before;
  float s0;
  assert(bomb_capture(root));
  offer(root, 2);
  root->row[WD_LIFE] = WD_REMOVED;
  root->row[WD_LANDED] = 1;
  root->row[WD_COMMITTER] = 2;
  assert(bomb_callback(root, a, DPTR(a, 0x18)));
  cd = lookup(actors[1]);
  ca = actors[1]; co = DPTR(ca, 0x18);
  assert(cd);
  cd->bomb_birth = 1;
  assert(bomb_child(root, cd, 0));
  assert(bomb_child(root, cd, DPTR(ca, 0xc)));
  cd->ready = cd->eligible = 1; cd->owner = 2;
  assert(bomb_capture(cd));
  offer(cd, 2);
  before = (unsigned char)cd->row[WB_ALPHA];
  s0 = F(co, 0x1c);
  assert(bomb_callback(cd, ca, co));
  assert((unsigned char)cd->row[WB_ALPHA] == (unsigned char)(before - 0x30));
  assert(F(co, 0x1c) > s0);
  assert(H(co, 0x16) == 0x20);
  /* A replica predicts the same fade but never retires the task. */
  cd->owner = 9;
  cd->row[WB_ALPHA] = 0x3f;  /* 0x3f-0x30 = 0x0f lands below the 0x30 floor */
  assert(bomb_callback(cd, ca, co));
  assert(bomb_removals == 0);
  assert((unsigned char)cd->row[WB_ALPHA] == 0x0f);
  /* The owner commits the silent expiry as a shared removal. */
  cd->owner = 2;
  cd->row[WB_ALPHA] = 0x3f;
  assert(bomb_callback(cd, ca, co));
  assert(bomb_removals == 1);
  assert(cd->row[WD_COMMITTER] == 2 && cd->row[WD_LANDED] == 0);
}
static void test_tombstone_is_durable(void) {
  void *a = root_fixture();
  DynamicActor *d = bomb_register_root(a);
  assert(bomb_capture(d));
  d->row[WD_LIFE] = WD_REMOVED;
  d->row[WD_COMMITTER] = 2;
  d->claimed = 1;
  d->bomb_frozen = 1;
  assert(bomb_capture(d));
  assert(d->row[WD_LIFE] == WD_REMOVED && d->row[WD_COMMITTER] == 2);
  assert(anchor_world_bomb_valid(d->row));
}
static void test_detach_restores_native_phase(void) {
  void *a = root_fixture();
  DynamicActor *d = bomb_register_root(a);
  assert(bomb_capture(d));
  /* A root handed back on disconnect keeps a real native phase callback rather
   * than the bounded wrapper that would delete an idle block. */
  W(a, 0x60) = 0;                 /* frozen root flags, as the wrapper leaves them */
  F(a, 0x78) = F(a, 0x7c) = F(a, 0x80) = 0;
  d->row[WD_PHASE] = WB_FUSE;
  bomb_detach(d);
  assert(DPTR(a, 0xc) == (void *)bomb_phase(WB_FUSE));
  /* The retained phase flags are restored before the native continuation. */
  assert(W(a, 0x60) == 0x02a007e1u);
  assert(DPTR(a, 0x90) == (void *)func_08000950_6E9AA0);
  /* A child is handed back to the native ring/particle continuation. */
  offer(d, 2);
  d->row[WD_LIFE] = WD_REMOVED;
  d->row[WD_LANDED] = 1;
  d->row[WD_COMMITTER] = 2;
  assert(bomb_callback(d, a, DPTR(a, 0x18)));
  {
    DynamicActor *cd = lookup(actors[1]);
    assert(cd);
    cd->bomb_birth = 1;
    assert(bomb_child(d, cd, 0));
    assert(bomb_child(d, cd, DPTR(actors[1], 0xc)));
    cd->ready = cd->eligible = 1; cd->owner = 2;
    assert(bomb_capture(cd));
    cd->bomb_initialized = 1;   /* bindings already established */
    bomb_detach(cd);
    assert(DPTR(cd->actor, 0xc) == (void *)bomb_phase(cd->row[WD_PHASE]));
  }
}
/* The real lifecycle: the common child hook marks the birth, the entry-traits
 * hook classifies the child from its written +0xE8, and the post pass marks the
 * bindings established and captures the typed row. */
static void test_shared_child_lifecycle(void) {
  void *a = root_fixture();
  DynamicActor *root = bomb_register_root(a);
  int i, ring = 0, particle = 0;
  assert(bomb_capture(root));
  offer(root, 2);
  root->row[WD_LIFE] = WD_REMOVED;
  root->row[WD_LANDED] = 1;
  root->row[WD_COMMITTER] = 2;
  assert(bomb_callback(root, a, DPTR(a, 0x18)));
  assert(spawn_calls == 12);
  for (i = 1; i < used; ++i) {
    DynamicActor *cd = lookup(actors[i]);
    DynamicCallback initializer = (DynamicCallback)DPTR(actors[i], 0xc);
    assert(cd && cd->bomb_birth);   /* the kind lands with the traits hook */
    assert(cd->parent == 8 && cd->row[WD_PARENT] == 8);
    /* The common hook ran inside func_802171A8, before the caller wrote E8. */
    assert(!cd->ready);
    /* The entry-traits hook classifies from the variant the caller wrote. */
    if (initializer == func_08000984_6E9AD4) world_dynamic_bomb_ring_traits(actors[i]);
    else world_dynamic_bomb_particle_traits(actors[i]);
    assert(cd->kind == WORLD_BOMB);
    /* The native initializer then runs in its own scheduled slot. */
    initializer(actors[i], DPTR(actors[i], 0x18));
    assert(cd->row[WB_ROLE] >= 1 && cd->row[WB_ROLE] <= 12);
    if (cd->row[WB_ROLE] <= WB_ROLE_V8) ++ring; else ++particle;
    /* The post pass establishes the bindings and captures the typed row. */
    world_dynamic_post(actors[i]);
    assert(cd->bomb_initialized && cd->ready);
    assert(bomb_capture(cd));   /* the frame capture publishes the typed row */
    assert(cd->row[WD_ORDINAL] == cd->row[WB_ROLE]);
    assert(cd->row[WB_VARIANT] == (cd->row[WB_ROLE] <= WB_ROLE_V8
                                     ? bomb_role_variant[cd->row[WB_ROLE]] : 0));
    assert(cd->row[WD_BIRTH_X] == 4000 && cd->row[WD_BIRTH_Y] == 12000);
    /* Gravity is read live: the particles carry 15, the rings 0. */
    assert(cd->row[WD_GRAVITY] == (cd->row[WB_ROLE] <= WB_ROLE_V8 ? 0 : 0x0f));
  }
  assert(ring == 4 && particle == 8);
  /* The root keeps the native no-loot bit in its aux word. */
  assert(root->row[WD_AUX_LO] & 0x8000);
}
int main(void) {
  fixture(); test_register_and_recipe();
  fixture(); test_damage_defers_native_hit();
  fixture(); test_committer_fanout_once();
  fixture(); test_phase_machine_and_claim();
  fixture(); test_children_roles_and_attackers();
  fixture(); test_child_prediction_and_owner_expiry();
  fixture(); test_tombstone_is_durable();
  fixture(); test_detach_restores_native_phase();
  fixture(); test_shared_child_lifecycle();
  puts("bomb adapter: OK");
  return 0;
}
