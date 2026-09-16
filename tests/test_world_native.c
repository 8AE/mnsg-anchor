/* Exercise the actual native hook implementation with 32-bit pointer slots. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define ANCHOR_WORLD_HOST_TEST 1
#define PTR(p, o) TP(p, o)
#define DISABLED 0ul
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define recomp_printf(...) ((void)0)
#define recomp_free free
static int anchor_dialog_world_paused(void) { return 0; }
static int anchor_is_connected(void) { return 1; }
static int anchor_is_disabled(void) { return 0; }
static int item_sync_save_is_loaded(void) { return 1; }
static unsigned int anchor_get_client_id(void) { return 2; }
static char *anchor_update_world(unsigned int r, unsigned int s, unsigned int v,
                                 const char *j) {
  (void)r;
  (void)s;
  (void)v;
  (void)j;
  return NULL;
}
#include "../src/anchor_world.c"
unsigned short D_800C7AB2;
void *D_8016DAB4_16E6B4, *D_801FC604_5B8514;
static unsigned int system_words[0x40000 / 4];
unsigned char *D_8015C5C8_15D1C8 = (unsigned char *)system_words;
void *D_80236984_5F1E54[4];
static int native_calls, talk_calls, anim_calls;
int func_80220F70_5DC440(void *a) {
  (void)a;
  ++talk_calls;
  return 0;
}
float func_80003E10_4A10(unsigned short angle) {
  return sinf((angle & 1023) * 6.283185307f / 1024);
}
float func_8001B5AC_1C1AC(void *o) {
  (void)o;
  return 16;
}
void func_8021664C_5D1B1C(void *a, unsigned int c, float r, unsigned int f) {
  (void)a;
  (void)c;
  (void)r;
  (void)f;
  ++anim_calls;
}
#define STUB(f)                                                                \
  void f(void *a, void *o) {                                                   \
    (void)a;                                                                   \
    (void)o;                                                                   \
    ++native_calls;                                                            \
  }
PLATFORM_STATES(STUB)
static unsigned int actor[64], object[64], source[8], definition[8];
static unsigned int clips[] = {1, 2, 0};
static void *model[] = {0, clips};
static void idle(void *a, void *o) {
  (void)a;
  (void)o;
  ++native_calls;
}
static WorldActor *fixture(unsigned int id, unsigned int variant) {
  anchor_world_reset();
  test_ptr_count = 0;
  memset(actor, 0, sizeof(actor));
  memset(object, 0, sizeof(object));
  memset(source, 0, sizeof(source));
  memset(definition, 0, sizeof(definition));
  memset(system_words, 0, sizeof(system_words));
  s_room = 0xffff;
  s_signature = 0;
  s_self = 2;
  s_active = 1;
  native_calls = talk_calls = anim_calls = 0;
  D_800C7AB2 = 0x161;
  D_801FC604_5B8514 = 0;
  U16(definition, 0) = id;
  U8(definition, 4) = variant;
  anchor_world_roster_begin(D_800C7AB2);
  anchor_world_roster_add(0, source, definition);
  anchor_world_roster_end(1);
  U16(actor, 0x5c) = id;
  U8(actor, 0x74) = 3;
  PTR(actor, 0x18) = object;
  PTR(actor, 0x0c) = idle;
  F32(object, 0x1c) = F32(object, 0x20) = F32(object, 0x24) = 1;
  U16(object, 0x7e) = 256;
  anchor_world_register(actor, source);
  anchor_world_post(actor);
  D_80236984_5F1E54[0] = model;
  return &s_actors[0];
}
static void checkpoint(WorldActor *w) {
  assert(capture(0, w->net));
  w->have = 1;
  w->owner = 1;
}
static void tick(void) {
  anchor_world_scheduler_begin();
  ((Callback)PTR(actor, 0x0c))(actor, object);
  anchor_world_scheduler_end();
}
static void npc_test(void) {
  WorldActor *w = fixture(0x2bd, 0);
  anchor_world_npc_init(actor);
  checkpoint(w);
  w->net[4] = 12345;
  w->net[10] = 1;
  w->net[11] = 9000;
  w->net[14] = 1000;
  tick();
  assert(native_calls == 0 && talk_calls == 1 && anim_calls == 1);
  assert(F32(object, 8) == 123.45f && F32(object, 0x28) == 15 &&
         F32(actor, 0x78) == 1);
  assert(PTR(actor, 0x0c) == (void *)idle);
  /* Local dialogue retains native control even before ownership converges. */
  U32(actor, 0x68) |= 0x100;
  w->net[4] = 999;
  tick();
  assert(native_calls == 1 && F32(object, 8) == 123.45f);
  U32(actor, 0x68) = 0;
  w->net[10] = 255;
  tick();
  assert(!w->have && native_calls == 2 && F32(object, 8) == 123.45f);
  w = fixture(0x2c3, 0);
  anchor_world_path_init(actor, 0);
  assert(w->kind == WORLD_NPC && !w->talkable && w->has_path);
  checkpoint(w);
  w->net[30] = 1;
  tick();
  assert(!w->have && native_calls == 1);
  /* An enemy using a path helper is not reclassified as a town NPC. */
  w = fixture(0x2bc, 0);
  anchor_world_path_init(actor, 0);
  assert(!w->kind);
}
static void platform_test(void) {
  WorldActor *w = fixture(0x3e0, 2);
  platform_addresses();
  PTR(actor, 0x0c) = (void *)platform_states[2];
  checkpoint(w);
  w->net[7] = 512;
  tick();
  assert(U16(object, 0x14) == 504 && native_calls == 0 &&
         PTR(actor, 0x0c) == (void *)platform_states[2]);
  tick();
  assert(U16(object, 0x14) ==
         496); /* Prediction does not reset unchanged snapshots. */
  w->have = 0;
  tick();
  assert(native_calls == 1);
  w = fixture(0x3e0, 0);
  PTR(actor, 0x0c) = (void *)platform_states[0];
  checkpoint(w);
  w->net[17] = 0;
  tick();
  assert(PTR(actor, 0x0c) == (void *)platform_states[1]);
  tick();
  assert(U16(object, 0x14) == 1016 && native_calls == 0);
  w->net[18] = 27;
  tick();
  assert(!w->have && native_calls == 1); /* Cross-variant callback rejected. */
  w = fixture(0x3e0, 12);
  PTR(actor, 0x0c) = (void *)platform_states[23];
  D_801FC604_5B8514 = source;
  PTR(source, 0xa0) = object;
  checkpoint(w);
  assert(w->net[3] == 1);
}
static void emitter_test(void) {
  WorldActor *w = fixture(0x19a, 0);
  S16(actor, 0x8a) = 0;
  anchor_world_emitter_rock(actor);
  assert(w->cycle == 1);
  S16(actor, 0x8a) = 150;
  checkpoint(w);
  tick();
  assert(native_calls == 0);
  w->net[18] = 2;
  w->net[17] = 145;
  tick();
  assert(native_calls == 1 && w->cycle == 2);
  tick();
  assert(native_calls ==
         1); /* An unchanged checkpoint cannot replay a spawn. */
  w->net[18] = 3;
  w->net[38] = 1;
  tick();
  assert(native_calls == 1);
  w->net[38] = 0;
  tick();
  assert(native_calls == 2);
  w->net[17] = 200;
  tick();
  assert(!w->have && native_calls == 3);
}
static void safety_test(void) {
  int n = 0;
  volatile union {
    unsigned int bits;
    float f;
  } invalid;
  invalid.bits = 0x7fc00000;
  assert(!scaled(invalid.f, 100, -1000, 1000, &n));
  invalid.bits = 0x7f800000;
  assert(!scaled(invalid.f, 100, -1000, 1000, &n));
  WorldActor *w = fixture(0x3e0, 9);
  platform_addresses();
  PTR(actor, 0x0c) = (void *)platform_states[16];
  checkpoint(w);
  tick();
  assert(native_calls == 0);
  w->net[31] = 1;
  tick();
  assert(native_calls == 1);
  tick();
  assert(native_calls == 1);
  w = fixture(0x85, 0);
  PTR(actor, 0xe4) = object;
  s_dead[0] = 1;
  tick();
  assert(native_calls == 0 && (U32(actor, 0x68) & 2));
  anchor_world_post(actor);
  assert(U8(object, 0xd0) == 1 && !w->actor);
}
static void lifecycle_test(void) {
  WorldActor *w = fixture(0x82, 0);
  unsigned int sig = s_signature, visit = s_visit;
  U32(actor, 0x68) = 2;
  anchor_world_post(actor);
  assert(!s_dead[0] && !w->actor);
  w = fixture(0x82, 0);
  anchor_world_coin(actor);
  assert(s_dead[0] == 1);
  anchor_world_roster_begin(D_800C7AB2);
  anchor_world_roster_add(0, source, definition);
  anchor_world_roster_end(1);
  assert(s_signature == sig && s_dead[0] == 1 && s_visit >= visit);
  anchor_world_register(actor, source);
  anchor_world_post(actor);
  assert(U32(actor, 0x68) & 2);
  U16(source, 0) = 1;
  anchor_world_roster_begin(D_800C7AB2);
  anchor_world_roster_add(0, source, definition);
  anchor_world_roster_end(1);
  assert(s_signature != sig && !s_dead[0]);
  w = fixture(0x84, 0);
  anchor_world_health(actor);
  assert(!s_dead[0]);
  U32(actor, 0x68) = 0x200;
  anchor_world_health(actor);
  assert(s_dead[0] == 1);
  w = fixture(0x85, 0);
  U32(actor, 0x68) = 0x200;
  anchor_world_food(actor);
  assert(s_dead[0] == 1);
  w = fixture(0x192, 0);
  anchor_world_container(actor);
  assert(s_dead[0] == 1);
  w = fixture(0x2bd, 0);
  anchor_world_npc_init(actor);
  checkpoint(w);
  U8(actor, 0x74)++;
  assert(!valid(0));
  anchor_world_scheduler_begin();
  assert(PTR(actor, 0x0c) == (void *)idle);
  anchor_world_register(actor, object);
  assert(!w->actor);
}
int main(void) {
  npc_test();
  platform_test();
  emitter_test();
  safety_test();
  lifecycle_test();
  puts("world native: NPC/dialogue, platform prediction/handoff, "
       "pickup/culling and pool reuse passed");
  return 0;
}
