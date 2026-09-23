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
void anchor_race_boulder_room_reset(void) {}
/* A bridge fixture may serve exactly one synthetic frame reply. Every other
 * fixture keeps the NULL this stub always returned. */
static char bridge_frame_reply[8192];
static int bridge_frame_reply_ready;
static char *anchor_update_world(unsigned int r, unsigned int s, unsigned int v,
                                 const char *j) {
  char *copy;
  size_t n;
  (void)r;
  (void)s;
  (void)v;
  (void)j;
  if (!bridge_frame_reply_ready)
    return NULL;
  n = strlen(bridge_frame_reply);
  copy = (char *)malloc(n + 1);
  assert(copy);
  memcpy(copy, bridge_frame_reply, n + 1);
  return copy;
}
static char *anchor_update_world_quest(const char *sources, const char *status) {
  (void)sources; (void)status; return NULL;
}
#define WORLD_NPC_PTR(p, o) TP(p, o)
#define WORLD_NPC_DISABLED 0ul
#include "../src/world/anchor_world.c"
#include "../src/world/anchor_world_npc.c"
void anchor_world_quest_set_self(unsigned int self) { (void)self; }
void anchor_world_quest_frame(unsigned int r,unsigned int s,unsigned int v,int a) {
  (void)r;(void)s;(void)v;(void)a;
}
unsigned int anchor_world_quest_row_count(void) { return 0; }
const int *anchor_world_quest_rows(void) { static int rows[WORLD_QUEST_WORDS]; return rows; }
unsigned int anchor_world_quest_status(int rows[][WORLD_QUEST_WORDS],unsigned int c) {
  (void)rows;(void)c;return 0;
}
unsigned int anchor_world_quest_receive(const int rows[][WORLD_QUEST_WORDS],
                                         unsigned int c,unsigned int self) {
  (void)rows;(void)c;(void)self;return 0;
}
int anchor_world_quest_encode(const int rows[][WORLD_QUEST_WORDS],
                              unsigned int c,char *out,unsigned int n) {
  (void)rows;(void)c;if(n<9)return 0;memcpy(out,"{\"a\":[]}",9);return 1;
}
int anchor_world_quest_decode(const char *json,int rows[][WORLD_QUEST_WORDS],
                              unsigned int *c) {
  (void)json;(void)rows;(void)c;return 0;
}
/* Counterweight lifecycle has a dedicated native harness. */
static int cw_capture_ready,cw_capture_row[ANCHOR_WORLD_WORDS];
static int cw_confirmed,cw_remote,cw_applies;
static unsigned int cw_inputs;
void anchor_world_counterweight_reset(int changed) { (void)changed; }
void anchor_world_counterweight_register(void *a) { (void)a; }
unsigned int anchor_world_counterweight_local_inputs(void *a) { (void)a;return 0; }
int anchor_world_counterweight_needs_restore(void *a) { (void)a;return 0; }
int anchor_world_counterweight_capture(void *a,int *r) {
  (void)a;if(!cw_capture_ready)return 0;
  for(unsigned int i=3;i<WORLD_INSTANCE;++i)r[i]=cw_capture_row[i];return 1;
}
int anchor_world_counterweight_apply(void *a,const int *r) { (void)a;(void)r;++cw_applies;return 1; }
void anchor_world_counterweight_control(void *a,int remote,int paused,int confirmed,unsigned int mask) {
  (void)a;(void)paused;cw_confirmed=confirmed;cw_remote=remote;cw_inputs=mask;
}
void anchor_world_counterweight_begin(void) {}
void anchor_world_counterweight_end(void) {}
/* The coupled crane implementation has its own native lifecycle harness. */
static int crane_apply_calls, crane_restore, crane_control_remote, crane_control_paused;
static unsigned int crane_inputs, crane_control_inputs;
static int crane_capture_ready, crane_capture_row[ANCHOR_WORLD_WORDS];
void anchor_world_crane_reset(int changed) { (void)changed; crane_restore = 0; }
void anchor_world_crane_register(void *a, unsigned int e) { (void)a; (void)e; }
unsigned int anchor_world_crane_local_inputs(void) { return crane_inputs; }
int anchor_world_crane_needs_restore(void) { return crane_restore; }
int anchor_world_crane_capture(void *a, int *r) {
  (void)a;
  if (!crane_capture_ready) return 0;
  for (unsigned int i = 4; i < WORLD_INSTANCE; ++i) r[i] = crane_capture_row[i];
  return 1;
}
int anchor_world_crane_apply(void *a, const int *r) {
  (void)a; (void)r; ++crane_apply_calls; crane_restore = 0; return 1;
}
void anchor_world_crane_control(void *a, int remote, int paused, unsigned int inputs) {
  (void)a; crane_control_remote = remote; crane_control_paused = paused;
  crane_control_inputs = inputs;
}
void anchor_world_crane_begin(void) {}
void anchor_world_crane_end(void) {}
/* File51 bridge core fixtures drive the same scheduling contract as the crane
 * through a small stub adapter, so the core stays testable without native
 * addresses. */
static int bridge_apply_calls, bridge_restore, bridge_control_remote, bridge_control_paused;
static unsigned int bridge_inputs, bridge_control_inputs;
static unsigned int bridge_apply_result = 1;
static int bridge_capture_ready, bridge_capture_row[ANCHOR_WORLD_WORDS];
static unsigned int bridge_registered[8], bridge_registered_count;
static void *bridge_registered_actor[8];
void anchor_world_bridge_reset(int changed) { (void)changed; bridge_restore = 0; }
void anchor_world_bridge_register(void *a, unsigned int e, unsigned int selector) {
  if (bridge_registered_count < 8) {
    bridge_registered_actor[bridge_registered_count] = a;
    bridge_registered[bridge_registered_count] =
        (e & 0xffffffu) | (selector << 24);
    ++bridge_registered_count;
  }
}
unsigned int anchor_world_bridge_local_inputs(void) { return bridge_inputs; }
int anchor_world_bridge_needs_restore(void) { return bridge_restore; }
int anchor_world_bridge_capture(void *a, int *r) {
  (void)a;
  if (!bridge_capture_ready) return 0;
  for (unsigned int i = 4; i < WORLD_INSTANCE; ++i) r[i] = bridge_capture_row[i];
  return 1;
}
int anchor_world_bridge_apply(void *a, const int *r) {
  (void)a; (void)r; ++bridge_apply_calls; bridge_restore = 0;
  return bridge_apply_result;
}
void anchor_world_bridge_control(void *a, int remote, int paused, unsigned int inputs) {
  (void)a; bridge_control_remote = remote; bridge_control_paused = paused;
  bridge_control_inputs = inputs;
}
void anchor_world_bridge_begin(void) {}
void anchor_world_bridge_end(void) {}
static int gate64_capture_ready,gate64_capture_row[ANCHOR_WORLD_WORDS],gate64_apply_calls,gate64_confirmed,gate64_remote;
void anchor_world_gate64_reset(int changed) {(void)changed;}
void anchor_world_gate64_register(void *a) {(void)a;}
int anchor_world_gate64_capture(void *a,int *r) {
  (void)a;if(!gate64_capture_ready)return 0;
  for(unsigned int i=3;i<WORLD_INSTANCE;++i)r[i]=gate64_capture_row[i];return 1;
}
int anchor_world_gate64_apply(void *a,const int *r) {(void)a;(void)r;++gate64_apply_calls;return 1;}
int anchor_world_gate64_needs_restore(void) {return 0;}
void anchor_world_gate64_control(void *a,int remote,int paused,int confirmed) {(void)a;(void)paused;gate64_confirmed=confirmed;gate64_remote=remote;}
void anchor_world_gate64_begin(void) {}

/* The File62 doll container has its own native lifecycle harness. */
static int doll_apply_calls,doll_restore,doll_control_remote,doll_control_paused,doll_confirmed;
static int doll_capture_ready,doll_capture_row[ANCHOR_WORLD_WORDS];
void anchor_world_doll_reset(int changed) {(void)changed;doll_restore=0;}
void anchor_world_doll_register(void *a,unsigned int i) {(void)a;(void)i;}
int anchor_world_doll_needs_restore(void) {return doll_restore;}
int anchor_world_doll_capture(void *a,int *r) {
  (void)a;if(!doll_capture_ready)return 0;
  for(unsigned int i=3;i<WORLD_INSTANCE;++i)r[i]=doll_capture_row[i];return 1;
}
int anchor_world_doll_apply(void *a,const int *r) {(void)a;(void)r;++doll_apply_calls;doll_restore=0;return 1;}
void anchor_world_doll_control(void *a,int remote,int paused,int confirmed) {(void)a;doll_control_remote=remote;doll_control_paused=paused;doll_confirmed=confirmed;}
void anchor_world_doll_begin(void) {}
int anchor_world_doll_parent_valid(unsigned int i) {(void)i;return 0;}
unsigned int anchor_world_doll_parent(void) {return 0;}

void anchor_world_dynamic_room(void) {}
void anchor_world_dynamic_frame(unsigned int r, unsigned int s, unsigned int v,
                                int a) {
  (void)r;
  (void)s;
  (void)v;
  (void)a;
}
unsigned short D_800C7AB2;
unsigned int D_8015C5E4 = 10;
void *D_8016DAB4_16E6B4, *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
static unsigned int system_words[0x40000 / 4];
unsigned char *D_8015C5C8_15D1C8 = (unsigned char *)system_words;
void *D_80236984_5F1E54[1026];
static int native_calls, talk_calls, anim_calls;
static int travel_calls;
static unsigned char temporary_flags[100], save_flags[256];
static int switch_bursts, flag_writes;
int func_80023E94_24A94(int flag) {
  assert(flag >= 0 && flag < 800);
  return temporary_flags[flag >> 3] & (1u << (flag & 7));
}
void func_80023DF0_249F0(int flag) {
  assert(flag >= 0 && flag < 800);
  temporary_flags[flag >> 3] |= 1u << (flag & 7);
  ++flag_writes;
}
int func_800240DC_24CDC(int flag) {
  assert(flag >= 0 && flag < 2048);
  return save_flags[flag >> 3] & (1u << (flag & 7));
}
void func_80024038_24C38(int flag) {
  assert(flag >= 0 && flag < 2048);
  save_flags[flag >> 3] |= 1u << (flag & 7);
  ++flag_writes;
}
/* Native File_50 one-shot continuation behavior, including its local child
 * burst. The transport/hooks under test are the production implementation. */
void func_08000204_70CA24(void *a, void *o) {
  (void)o;
  if (U32(a, 0x68) & 0x80)
    PTR(a, 0xc) = func_08000298_70CAB8;
}
void func_08000298_70CAB8(void *a, void *o) {
  U32(a, 0x60) = (U32(a, 0x60) | 0x01000000u) & ~1u;
  F32(o, 0x28) = 15;
  U8(a, 0x6c) = 1;
  PTR(a, 0xc) = func_0800032C_70CB4C;
}
void func_0800032C_70CB4C(void *a, void *o) {
  (void)o;
  if (U8(a, 0xd4))
    func_80024038_24C38(U16(a, 0xd0));
  else
    func_80023DF0_249F0(U16(a, 0xd0));
  PTR(a, 0xbc) = func_080003AC_70CBCC;
  PTR(a, 0xc) = func_08000A8C_70D2AC;
}
void func_08000A8C_70D2AC(void *a, void *o) {
  (void)o;
  anchor_world_switch_sparks(a);
  ++switch_bursts;
  PTR(a, 0xc) = PTR(a, 0xbc);
}
void func_080003AC_70CBCC(void *a, void *o) { (void)a; (void)o; }
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
  if (U16(a, 0x5c) == 0x3d0) {
    anchor_world_animation(a, c);
    F32(PTR(a, 0x18), 0x28) = 0;
    U16(PTR(a, 0x18), 0x7e) = (unsigned short)(r * 256);
    U8(PTR(a, 0x18), 0x7c) = (unsigned char)f;
  }
}
static void physics_native_stub(void *, void *, Callback);
static void linked_native_stub(void *, void *, Callback);
#define STUB(f)                                                                \
  void f(void *a, void *o) {                                                   \
    linked_native_stub(a, o, f);                                               \
    physics_native_stub(a, o, f);                                              \
    ++native_calls;                                                            \
  }
PLATFORM_STATES(STUB)
/* Native phase stub: exercises callback scheduling and imported scalar state.
 * Real animation integration and collision remain outside this host fixture. */
void func_08000980_6ACED0(void *a, void *o) {
  ++native_calls;
  assert(U8(a,0xd4) != 0); /* Catch a replayed native constructor. */
  if (U8(a,0xd4) == 1 && S16(a,0x8a)-- == 0) {
    U8(a,0xd4) = 3;U32(a,0x60) |= 1;U8(o,0x7c) = 0;F32(o,0x28) = 0;
  } else if (U8(a,0xd4) == 3 && (U8(o,0x7c)&2)) {
    U8(a,0xd4) = 4;U32(a,0x60) |= 0x01000000u;
    U8(o,0x7c) = 0;F32(o,0x28) = 2;
  } else if (U8(a,0xd4) == 4 && (U8(o,0x7c)&2)) {
    U8(a,0xd4) = 1;U32(a,0x60) &= ~0x01000001u;
    S16(a,0x8a) = S16(a,0xd6);
  }
}
void func_0800074C_6ACC9C(void *a, void *o) {
  ++native_calls;
  U16(o,0x14) = (unsigned short)(U16(o,0x14)+U32(a,0xd0));
}
void func_080018E4_6EAA34(void *a, void *o) {
  ++native_calls;
  U16(o,0x16)=(U16(o,0x16)+4)&1023;
  float offset=cosf((float)U16(a,0xe4)*6.283185307179586f/1024.0f)*(float)U32(a,0xd4);
  if (U32(a,0xd0)<2) F32(o,8)=F32(a,0xdc)+offset;
  else F32(o,16)=F32(a,0xe0)+offset;
  U16(a,0xe4)=(U16(a,0xe4)+(short)U32(a,0xd8))&1023;
}
void func_08001CC4_6EAE14(void *a, void *o) {
  ++native_calls;
  U16(o,0x16)=(U16(o,0x16)+(U32(a,0xd0) ? 2 : -2))&1023;
}
static int shutter_births;
void func_08005624_6C4D74(void *a, void *o) {
  (void)o;
  if (S16(a, 0x8a)-- == 0) {
    U32(a, 0x60) |= 1u;
    PTR(a, 0xc) = func_08005684_6C4DD4;
  }
}
void func_08005684_6C4DD4(void *a, void *o) {
  if (U8(o, 0x7c) & 2u) {
    S16(a, 0x8a) = 90;
    U32(a, 0x60) = (U32(a, 0x60) & ~1u) | 0x01000000u;
    PTR(a, 0xc) = func_080056FC_6C4E4C;
  }
}
void func_080056FC_6C4E4C(void *a, void *o) {
  (void)o;
  anchor_world_shutter_emission(a);
  if (S16(a, 0x8a) == 30 && U32(a, 0xd0) == 1) {
    unsigned int parent, ordinal;
    assert(anchor_world_shutter_birth(a, &parent, &ordinal));
    assert(parent == 1 && ordinal == s_actors[0].cycle);
    ++shutter_births;
  }
  if (S16(a, 0x8a)-- == 0) {
    U32(a, 0x60) |= 1u;
    PTR(a, 0xc) = func_080057D0_6C4F20;
  }
}
void func_080057D0_6C4F20(void *a, void *o) {
  if (U8(o, 0x7c) & 2u) {
    U32(a, 0x60) &= ~0x01000001u;
    S16(a, 0x8a) = 60;
    PTR(a, 0xc) = func_08005624_6C4D74;
  }
}
WORLD_NPC_PHASES(STUB)
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
static int missing_file = -1;
int func_800141C4_14DC4(unsigned int f) {
  return (int)f == missing_file ? -1 : 1;
}
static unsigned int linked_actor[64], linked_object[64];
static int child_builds, tint_builds, child_fail, linked_inits;
static unsigned short linked_files[] = {44, 0x152};
static unsigned int linked_clips[] = {1, 2, 3, 4, 0};
static void *linked_model[] = {linked_files, linked_clips};
void func_08002DBC_6FDFBC(void *a, void *o) {
  U32(a, 0x60) = 0x600080;
  for (int j = 0; j < 3; ++j)
    F32(o, 8 + j * 4) = F32(a, 0xd0 + j * 4);
  PTR(a, 0xdc) = PTR(a, 0x38);
}
void *func_802171A8_5D2678(void *parent, Callback cb, unsigned char category) {
  assert(category == 2);
  if (child_fail)
    return NULL;
  anchor_world_reuse(linked_actor);
  memset(linked_actor, 0, sizeof(linked_actor));
  memset(linked_object, 0, sizeof(linked_object));
  ++child_builds;
  U16(linked_actor, 0x5c) = U16(parent, 0x5c);
  U8(linked_actor, 0x74) = (unsigned char)child_builds;
  PTR(linked_actor, 0xc) = cb;
  PTR(linked_actor, 0x18) = linked_object;
  return linked_actor;
}
void *func_8021DF60_5D9430(void *a, unsigned int alpha, unsigned int red,
                          unsigned int green, unsigned int blue) {
  void *o = PTR(a, 0x18);
  U32(o, 0x8c) = (red << 24) | (green << 16) | (blue << 8) | alpha;
  ++tint_builds;
  return (char *)o + 0x80;
}
static void linked_native_stub(void *a, void *o, Callback cb) {
  if (cb != func_080026D0_6FD8D0 && cb != func_08002E28_6FE028)
    return;
  if (cb == func_08002E28_6FE028 && (U32(a, 0xd8) & 2) &&
      !func_800240DC_24CDC(0x1a4))
    return;
  ++linked_inits;
  assert(D_8016DAB4_16E6B4 == a);
  for (int j = 0; j < 3; ++j)
    S16(a, 0xc8 + 2 * j) = (short)F32(o, 8 + 4 * j);
  for (int j = 0; j < 4; ++j)
    S16(a, 0xd0 + 2 * j) = (short)(U16(a, 0xd0 + 2 * j) - 0x8000);
  if (cb == func_08002E28_6FE028) {
    assert(!(U32(a, 0xd8) & 2) || func_800240DC_24CDC(0x1a4));
    U16(a, 0x5e) = 0x1f9;
    U32(a, 0x60) = 0x80000260u;
    anchor_world_animation(a, 2);
    PTR(a, 0xc) = func_08003074_6FE274;
  } else {
    U16(a, 0x5e) = 0x1f4;
    U32(a, 0x60) = 0x220;
    PTR(a, 0xc) = func_08002788_6FD988;
  }
}
static unsigned int actor[64], object[64], source[8], definition[8];
static unsigned int physics_child[64], physics_player[64], physics_player_work[64];
static unsigned int reward_ordinal[64], reward_angle[64], reward_kind[64];
static int reward_calls, physics_effects, carry_releases;
void func_80214314_5CF7E4(void *a, void *o) {(void)a;(void)o;}
void func_802141AC_5CF67C(void *a, void *o) {(void)a;(void)o;}
void func_80038B98_39798(unsigned int sound) {(void)sound;}
void func_8021A4E4_5D59B4(void *a, float scale) {(void)a;(void)scale;++physics_effects;}
void func_80216DF8_5D22C8(void *a, unsigned int clip) {
  anchor_world_static_model(a, PTR(a, 0x18), clip);
  ++anim_calls;
}
static unsigned int file30_bound_slot;
void func_80216E1C_5D22EC(void *a, unsigned int slot) {
  file30_bound_slot = slot;
  F32(PTR(a, 0x18), 0x1c) = F32(PTR(a, 0x18), 0x20) =
      F32(PTR(a, 0x18), 0x24) = 1.0f;
  anchor_world_static_model(a, PTR(a, 0x18), slot);
  ++anim_calls;
}
void func_08000970_6C00C0(void *a, void *o) {(void)a;(void)o;}
void func_08004AE8_6C4238(void *a, void *o) {(void)a;(void)o;}
void func_801E55A0_5A14B0(void *p) {
  void *work = PTR(p, 0x5c), *held = PTR(work, 0x8c);
  if (held) U32(held, 0x68) &= ~0x8000u;
  PTR(work, 0x8c) = 0;
}
void func_801DACDC_596BEC(void *p, unsigned char state) {
  assert(p == physics_player && state == 0);
  ++carry_releases;
}
void *func_8021DDE8_5D92B8(void *a, Callback init, unsigned char category,
                          float x, float y, float z, unsigned short angle) {
  assert(category == 9 && x == 0 && y == 8 && z == 0);
  assert(reward_calls < 64);
  assert(anchor_world_loot_ordinal(a, &reward_ordinal[reward_calls]));
  reward_angle[reward_calls] = angle;
  reward_kind[reward_calls] = init == func_80214314_5CF7E4 ? 2 : 3;
  ++reward_calls;
  return physics_child;
}
/* Relevant File_30 native continuations. Collision remains a separate native
 * post pass; these substitutes test the real adapter's authority boundaries. */
static void physics_native_stub(void *a, void *o, Callback cb) {
  if (cb == func_08006C18_6C6368) {
    if (!D_8015C5E4) {U32(a, 0x60) = 0x02a007e1;return;}
    if (U32(a, 0x68) & 0x8000) {
      U32(a, 0x60) = 0x08000020;
      U16(a, 0xdc) = 5;
      F32(a, 0xd0) = F32(a, 0x78);F32(a, 0xd4) = F32(a, 0x80);
    } else {
      if (U16(a, 0xdc)) --U16(a, 0xdc);
      if (U16(a, 0xdc) == 3 && (U32(a, 0x68) & 0x3000)) {
        U32(a, 0x68) |= 2;
        func_8021A4E4_5D59B4(a, 1);return;
      }
      U32(a, 0x60) = 0x0e800320;
      F32(a, 0x78) = F32(a, 0xd0);F32(a, 0x80) = F32(a, 0xd4);
    }
    if (F32(o, 8) > -4 && F32(o, 8) < 18 && F32(o, 0x10) > 103 &&
        F32(o, 0x10) < 120 && F32(o, 0xc) == 8) {
      U32(a, 0x60) = 0x06a007e1;U32(a, 0x68) |= 0x80000;
      F32(a, 0x78) = F32(a, 0x7c) = F32(a, 0x80) = 0;
      PTR(a, 0xc) = func_08006F48_6C6698;
    }
  } else if (cb == func_08006F48_6C6698) {
    for (int j = 0; j < 3; ++j) F32(o, 0x1c + j * 4) *= 0.95;
    if (F32(o, 0x1c) <= 0.05) {
      for (int j = 0; j < 3; ++j) {F32(o, 0x1c+j*4)=0.2;U16(o,0x14+j*2)=0;}
      U32(a, 0x60) = 0x21;
      func_8021664C_5D1B1C(a, 1, 1.0f/30, 0);
      PTR(a, 0xc) = func_08007040_6C6790;
    }
  } else if (cb == func_08007040_6C6790 && (U8(o, 0x7c) & 2)) {
    func_8021664C_5D1B1C(a, 2, 1.0f/30, 0);
    PTR(a, 0xc) = func_08007094_6C67E4;
  } else if (cb == func_08007094_6C67E4 && (U8(o, 0x7c) & 2)) {
    U16(a, 0x8a) = 200;PTR(a, 0xc) = func_080070E4_6C6834;
  } else if (cb == func_080070E4_6C6834)
    assert(!"The unkeyed native reward producer must not be replayed");
}
static unsigned int player_object[64], elevator_table[16];
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
  memset(actor, 0, sizeof(actor));
  memset(object, 0, sizeof(object));
  memset(source, 0, sizeof(source));
  memset(definition, 0, sizeof(definition));
  memset(system_words, 0, sizeof(system_words));
  memset(temporary_flags, 0, sizeof(temporary_flags));
  memset(save_flags, 0, sizeof(save_flags));
  switch_bursts = flag_writes = 0;
  s_room = 0xffff;
  s_signature = 0;
  s_self = 2;
  s_active = 1;
  native_calls = talk_calls = anim_calls = 0;
  child_builds = tint_builds = child_fail = linked_inits = 0;
  missing_file = -1;
  D_8016DAB4_16E6B4 = actor;
  travel_calls = 0;
  D_800C7AB2 = 0x161;
  D_801FC604_5B8514 = 0;
  D_801FC60C_5B851C = 0;
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
static void tick(void);
static WorldActor *switch_fixture(unsigned int mode) {
  WorldActor *w = fixture(0x226, 0);
  /* The native actor loader copies definition+8 to task+D4. */
  w->variant = mode;
  U8(actor, 0xd4) = mode;
  U16(actor, 0xd0) = mode ? 0x194 : 0;
  PTR(actor, 0xc) = func_08000204_70CA24;
  anchor_world_animation(actor, 0);
  return w;
}
static void switch_checkpoint_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w;
  for (unsigned int mode = 0; mode < 2; ++mode) {
    w = switch_fixture(mode);
    assert(w->kind == WORLD_SWITCH);
    checkpoint(w);
    U32(actor, 0x68) = 0x80;
    for (int j = 0; j < 5; ++j)
      tick(); /* delayed idle snapshots cannot undo the local hit */
    assert(switch_phase(PTR(actor, 0xc)) == 5 && switch_bursts == 1);
    assert(capture(0, r) && r[3] && r[18] == 5 && r[21] == 1);
    assert((mode ? func_800240DC_24CDC(0x194) : func_80023E94_24A94(0)) != 0);

    w = switch_fixture(mode);
    checkpoint(w);
    tick(); /* observe the unpressed state first */
    memcpy(w->net, r, sizeof(r));
    w->net[WORLD_INSTANCE] = (int)w->instance;
    w->net[WORLD_RECEIPT] = 0;
    tick();
    assert(U8(actor, 0x6c) == 1 && F32(object, 0x28) == 15);
    assert((U32(actor, 0x60) & 0x01000001u) == 0x01000000u);
    assert(switch_bursts == 1 && switch_phase(PTR(actor, 0xc)) == 5);
    tick();
    assert(switch_bursts == 1); /* duplicate checkpoints do not replay effects */
    int writes = flag_writes;
    w->net[20]++;
    assert(!apply(w) && writes == flag_writes); /* local definition is authoritative */

    w = switch_fixture(mode);
    memcpy(w->net, r, sizeof(r));
    w->net[WORLD_INSTANCE] = (int)w->instance;
    w->net[WORLD_RECEIPT] = 0;
    w->net[18] = 4; /* observer arrives at the burst boundary */
    w->have = 1;
    w->owner = 1;
    tick();
    assert(switch_bursts == 0 && switch_phase(PTR(actor, 0xc)) == 5);
    assert(PTR(actor, 0xbc) == (void *)func_080003AC_70CBCC);
    w->have = 0;
    tick(); /* ownership handoff must not repeat the already-restored burst */
    assert(switch_bursts == 0);
  }
}
static void puzzle_mechanism_checkpoint_test(void) {
  for (unsigned int id = 0x324; id <= 0x326; id += 2) {
    WorldActor *w = fixture(id, 0);
    int sample[ANCHOR_WORLD_WORDS];
    unsigned int first = id == 0x324 ? 53 : 56;
    platform_addresses();
    U8(actor, 0xd0) = id == 0x324 ? 11 : 7;
    U16(actor, 0xd4) = id == 0x324 ? 0 : 0x193;
    PTR(actor, 0xc) = platform_states[first - 1];
    checkpoint(w);
    w->net[3] = 1;
    w->net[18] = first + 1;
    w->net[21] = 1;
    w->net[4] = 12000;
    w->net[14] = -1000;
    /* A rider cannot override a mechanism controlled only by a room flag. */
    D_801FC604_5B8514 = source;
    PTR(source, 0xa0) = object;
    tick();
    assert(F32(object, 8) == 120 && F32(actor, 0x78) == -1 && native_calls == 1);
    assert(PTR(actor, 0xc) == (void *)platform_states[first]);
    assert(capture(0, sample) && sample[3] && sample[21]);
    assert(U8(actor, 0xd0) == (id == 0x324 ? 11 : 7));
    w->net[18] = first;
    w->net[3] = 0;
    w->net[4] = 0;
    tick(); /* the old idle phase cannot rewind the active puzzle */
    assert(F32(object, 8) == 120 && phase(PTR(actor, 0xc)) == (int)first + 1);
    w->net[18] = first + 1;
    w->net[3] = 1;
    w->net[19]++;
    int writes = flag_writes;
    assert(!apply(w) && writes == flag_writes);
    w->have = 0;
    tick();
    assert(phase(PTR(actor, 0xc)) == (int)first + 1);
  }
}
static void controller_culling_checkpoint_test(void) {
  for (unsigned int kind = 0; kind < 2; ++kind) {
    WorldActor *w = kind ? fixture(0x324, 0) : switch_fixture(0);
    int retained[ANCHOR_WORLD_WORDS], sample[ANCHOR_WORLD_WORDS];
    platform_addresses();
    PTR(actor, 0xc) = kind ? (void *)func_08003A74_6C31C4 : (void *)func_080003AC_70CBCC;
    U8(actor, 0x6c) = !kind;
    F32(object, 8) = 95;
    F32(object, 0x28) = 15;
    func_80023DF0_249F0(0);
    assert(capture(0, retained));
    U32(actor, 0x68) = 2; /* native distance culling, not a room reset */
    anchor_world_post(actor);
    assert(!w->actor && capture(0, sample));
    assert(sample[18] == retained[18] && sample[4] == 9500 && sample[38]);
    assert(!s_dead[0]);

    memset(actor, 0, sizeof(actor));
    memset(object, 0, sizeof(object));
    U16(actor, 0x5c) = kind ? 0x324 : 0x226;
    PTR(actor, 0x18) = object;
    PTR(actor, 0xc) = kind ? (void *)func_08003418_6C2B68 : (void *)func_08000204_70CA24;
    F32(object, 0x1c) = F32(object, 0x20) = F32(object, 0x24) = 1;
    anchor_world_register(actor, source);
    if (!kind)
      anchor_world_animation(actor, 0);
    anchor_world_post(actor);
    assert(capture(0, sample) && w->restore_controller && !sample[38]);
    w->have = 0; /* the only peer still in this room can restore its own memory */
    tick();
    assert(!w->restore_controller && F32(object, 8) == 95);
    assert((kind ? phase(PTR(actor, 0xc)) : switch_phase(PTR(actor, 0xc))) == retained[18]);
    assert(switch_bursts == 0);
    anchor_world_reset();
    w->actor = NULL;
    assert(!capture(0, sample)); /* no retained state crosses a reset */
  }
}
static void save_aware_constructor_does_not_finish_remote_motion_test(void) {
  WorldActor *w = fixture(0x326, 0);
  U16(actor, 0xd4) = 0x194;
  func_80024038_24C38(0x194);
  anchor_world_puzzle_constructor(actor);
  PTR(actor, 0xc) = func_080040B8_6C3808;
  F32(object, 8) = 130;
  checkpoint(w);
  assert(w->net[18] == 59 && w->net[24] == 1);
  w->net[18] = 57;
  w->net[24] = 0;
  w->net[4] = 5000;
  w->net[14] = 1000;
  tick();
  assert(F32(object, 8) == 50 && F32(actor, 0x78) == 1);
  assert(PTR(actor, 0xc) == (void *)func_08003C7C_6C33CC && !w->initialized_complete);
  int current[ANCHOR_WORLD_WORDS];
  assert(capture(0, current) && current[18] == 57 && !current[24]);
}
static void npc_checkpoint_test(void) {
  WorldActor *w = fixture(0x2c4, 0);
  anchor_world_path_init(actor, 0);
  U16(actor, 0x5e) = 0x2c4;
  D_80236984_5F1E54[0x2c4] = model;
  PTR(actor, 0xc) = func_08000FD4_71CA54;
  PTR(actor, 0xe0) = D_08001C50_71D6D0;
  S16(actor, 0xd8) = -100;
  S16(actor, 0xda) = 200;
  S16(actor, 0xdc) = 60;
  S16(actor, 0xe4) = 8;
  S16(actor, 0xe6) = -2;
  S16(actor, 0xe8) = 1;
  S16(actor, 0xea) = 50;
  checkpoint(w);
  assert(w->net[WORLD_NPC_CHECKPOINT] == 13);
  S16(actor, 0xe4) = 2;
  S16(actor, 0xe6) = 2;
  PTR(actor, 0xc) = func_08000AB0_71C530;
  tick();
  assert(!native_calls && PTR(actor, 0xc) == func_08000FD4_71CA54);
  assert(S16(actor, 0xe4) == 8 && S16(actor, 0xe6) == -2);
  assert(PTR(actor, 0xe0) == D_08001C50_71D6D0);
  w->have = 0;
  tick();
  assert(native_calls == 1);
  w->have = 1;
  w->net[WORLD_NPC_CHECKPOINT + 4] = 18;
  tick();
  assert(!w->have); /* route sentinel cannot become a live waypoint */

  w = fixture(0x2be, 0);
  anchor_world_npc_init(actor);
  U16(actor, 0x5e) = 0x2be;
  D_80236984_5F1E54[0x2be] = model;
  PTR(actor, 0xc) = idle;
  PTR(actor, 0xb4) = func_08000230_71BCB0;
  U32(actor, 0x68) = 0x100;
  checkpoint(w);
  assert(w->net[WORLD_NPC_CHECKPOINT] == 2);
  tick();
  assert(PTR(actor, 0xc) == idle && PTR(actor, 0xb4) == func_08000230_71BCB0);
}
static void tick(void) {
  D_8016DAB4_16E6B4 = actor;
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
  assert(native_calls == 0 && w->cycle == 2);
  tick();
  assert(native_calls ==
         0); /* An unchanged checkpoint cannot replay a spawn. */
  w->net[18] = 3;
  w->net[38] = 1;
  tick();
  assert(native_calls == 0);
  w->net[38] = 0;
  tick();
  assert(native_calls == 0);
  w->net[17] = 200;
  tick();
  assert(!w->have && native_calls == 1);
}
static void mechanism_test(void) {
  static const unsigned short ids[] = {0x197, 0x1fc, 0x1fd,
                                       0x1f7, 0x245, 0x34a};
  static const unsigned char phases[] = {31, 36, 41, 42, 43, 44};
  unsigned int i;
  WorldActor *w;
  for (i = 0; i < 6; ++i) {
    w = fixture(ids[i], 0);
    platform_addresses();
    assert(w->kind == WORLD_PLATFORM && w->variant == 14 + i);
    PTR(actor, 0xc) = (void *)platform_states[phases[i] - 1];
    U32(actor, 0xd4) =
        0x12345678; /* local private pointers/scalars stay local */
    S16(actor, 0xa0) = 77;
    S16(actor, 0xe4) = 30;
    if (ids[i] == 0x1f7)
      anchor_world_path_init(actor, 0);
    checkpoint(w);
    w->net[4] = 9900;
    w->net[29] = 12;
    tick();
    assert(F32(object, 8) == 99 && U32(actor, 0xd4) == 0x12345678);
    assert((U32(actor, 0x60) & 0x80000100u) == 0x80000100u);
    assert(PTR(actor, 0xc) == (void *)platform_states[phases[i] - 1]);
    w->net[18] = i == 0 ? 36 : 31;
    tick();
    assert(!w->have); /* no cross-family continuation */
  }
  w = fixture(0x245, 0);
  PTR(actor, 0xc) = (void *)platform_states[42];
  checkpoint(w);
  w->net[4] = 12000;
  U32(actor, 0x68) |= 0x10000u;
  tick();
  assert(F32(object, 8) == 0 && native_calls == 1);
  assert(capture(0, w->net) && w->net[3]);
  w = fixture(0x1fc, 0);
  PTR(actor, 0xc) = (void *)platform_states[35];
  checkpoint(w);
  w->net[4] = 12000;
  D_801FC604_5B8514 = source;
  PTR(source, 0xa0) = object;
  tick();
  assert(F32(object, 8) == 0 && native_calls == 1);
  /* A player on the other floor can call the elevator without riding it. */
  w = fixture(0x1fc, 0);
  PTR(actor, 0xc) = (void *)platform_states[35];
  PTR(actor, 0xe0) = elevator_table;
  S16(elevator_table, 0) = 50;
  F32(elevator_table, 20) = 0;
  F32(elevator_table, 8) = 100;
  D_801FC604_5B8514 = source;
  PTR(source, 0x18) = player_object;
  F32(player_object, 0xc) = 100;
  checkpoint(w);
  assert(w->net[3]);
  tick();
  assert(native_calls == 1 && w->local_motion);
  w = fixture(0x356, 0);
  assert(w->kind == WORLD_PLATFORM && w->variant == 20);
  PTR(actor, 0xc) = func_080054A4_6C4BF4;
  F32(actor, 0xdc) = 7;
  S16(actor, 0x8a) = 22;
  checkpoint(w);
  assert(w->net[18] == 46 && w->net[19] == 7000);
  F32(actor, 0xdc) = 1;
  tick();
  assert(F32(actor, 0xdc) == 7 && native_calls == 1);
  w->net[19] = 1500;
  tick();
  assert(!w->have);
  w = fixture(0x3b4, 0);
  assert(w->kind == WORLD_PLATFORM && w->variant == 21);
  PTR(actor, 0xc) = func_08006968_6C60B8;
  checkpoint(w);
  tick();
  assert(!native_calls); /* remote rider trigger is not replayed */
  w->net[18] = 49;
  w->net[16] = -4000;
  tick();
  assert(native_calls == 1 && F32(actor, 0x80) == -4);
  assert(PTR(actor, 0xc) == func_080069C8_6C6118);
  w->have = 0;
  tick();
  assert(native_calls == 2);
}
static void doors_and_walls_test(void) {
  WorldActor *w = fixture(0x23a, 0);
  assert(w->kind == WORLD_DOOR);
  anchor_world_animation(actor, 0);
  checkpoint(w);
  w->net[10] = 1;
  w->net[11] = 300;
  w->net[29] = 20;
  tick();
  assert(native_calls == 1 && anim_calls == 1 && F32(object, 0x28) == 3);
  assert((U32(actor, 0x60) & 0x80000001u) == 0x80000001u);
  D_8016DAB4_16E6B4 = actor;
  anchor_world_door_interaction();
  w->net[11] = 900;
  tick();
  assert(F32(object, 0x28) == 3 && native_calls == 2);
  for (int i = 0; i < 120; ++i) {
    assert(capture(0, w->net) && w->net[3]);
    assert(w->door_local);
  }
  w = fixture(0x23d, 1);
  assert(w->kind == WORLD_PICKUP);
  anchor_world_wall_begin(actor);
  anchor_world_wall_end();
  assert(!s_dead[0]);
  anchor_world_wall_begin(actor);
  U32(actor, 0x68) |= 2;
  anchor_world_wall_end();
  assert(s_dead[0] == 1 && w->owner == s_self);
  w = fixture(0x23d, 0);
  assert(!w->kind); /* non-breakable native subtype */
}
static void travel_idle(void *a, void *o) {
  (void)o;
  ++native_calls;
  if (U16(a, 0xda)) {
    D_8016DAB4_16E6B4 = a;
    anchor_world_door_interaction();
    U32(a, 0x60) |= 1;
    ++travel_calls;
  }
}
static void door_post(void) {
  /* Substitute the native common animation step 80216AB8. */
  if ((U32(actor, 0x60) & 1) && !(U8(object, 0x7c) & 2)) {
    float delta = (float)U16(object, 0x7e) * 2 / 256;
    F32(object, 0x28) += (U32(actor, 0x60) & 0x01000000u) ? -delta : delta;
    if (F32(object, 0x28) <= 0) {
      F32(object, 0x28) = 0;
      U8(object, 0x7c) |= 2;
    }
  }
}
static void remote_travel_door_closes_and_reopens_test(void) {
  WorldActor *w = fixture(0x23c, 0);
  int status[ANCHOR_WORLD_WORDS];
  PTR(actor, 0xc) = travel_idle;
  anchor_world_animation(actor, 0);
  anchor_world_static_model(actor, player_object, 1);
  assert(w->animated && w->clip == 0); /* linked decoration is not the door */
  checkpoint(w);
  w->net[3] = 1;
  w->net[11] = 1500;
  w->net[13] = 2; /* opening reached its last frame */
  w->net[29] = 20;
  tick();
  assert(w->door_remote && !w->door_local && !travel_calls);
  assert(F32(object, 0x28) == 15);
  w->have = 0; /* traveller leaves; local authority has no incoming row */
  tick();
  assert(w->door_closing && (U32(actor, 0x60) & 0x01000001u) == 0x01000001u);
  assert(!(U8(object, 0x7c) & 7));
  assert(capture(0, status) && (status[29] & 18) == 18 && !status[3]);
  for (int j = 0; j < 10; ++j) {
    door_post();
    tick();
  }
  assert(F32(object, 0x28) == 0 && !w->door_closing);
  assert((U32(actor, 0x60) & 0x81000001u) == 0x80000000u);
  assert(PTR(actor, 0xc) == (void *)travel_idle && !travel_calls);
  assert(capture(0, status) && !(status[29] & 18) && !status[3]);

  /* A late observer follows the reverse checkpoint between packets. */
  w->have = 1;
  w->net[3] = 0;
  w->net[11] = 1200;
  w->net[13] = 0;
  w->net[29] = 22;
  tick();
  door_post();
  assert(w->door_closing && F32(object, 0x28) == 10);
  tick();
  door_post();
  assert(F32(object, 0x28) == 8 && !travel_calls);

  /* Actual local input reverses direction and retains native travel. */
  U16(actor, 0xda) = 1;
  tick();
  assert(travel_calls == 1 && w->door_local && !w->door_closing);
  assert(!(U32(actor, 0x60) & 0x01000000u));
  door_post();
  assert(F32(object, 0x28) == 10);
  U16(actor, 0xda) = 0;
  w->net[11] = 200;
  tick();
  assert(F32(object, 0x28) ==
         10); /* stale remote close cannot override input */

  w = fixture(0x23a, 0);
  anchor_world_animation(actor, 0);
  w->door_remote = 1;
  F32(object, 0x28) = 15;
  tick();
  assert(!w->door_closing && F32(object, 0x28) == 15);
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
  assert(native_calls == 0);
  tick();
  assert(native_calls == 0);
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
static WorldActor *linked_fixture(int fire) {
  WorldActor *w = fixture(fire ? 0x1fe : 0x228, 0);
  static const short target[4] = {120, -50, 80, 20};
  for (int j = 0; j < 4; ++j) {
    U16(definition, 4 + j * 2) = (unsigned short)(target[j] + 0x8000);
    U16(actor, 0xd0 + j * 2) = U16(definition, 4 + j * 2);
  }
  if (fire)
    U32(definition, 12) = U32(actor, 0xd8) = 2;
  else {
    U8(definition, 12) = U8(actor, 0xd8) = 7;
    U8(definition, 13) = U8(actor, 0xd9) = 0;
    U8(definition, 14) = U8(actor, 0xda) = 1;
  }
  anchor_world_roster_add(0, source, definition);
  assert(w->kind == WORLD_PLATFORM && w->variant == (fire ? 25 : 24));
  assert(w->target[1] == -50 && w->target[3] == 20);
  D_80236984_5F1E54[0x1f9] = linked_model;
  D_80236984_5F1E54[0x1f4] = linked_model;
  PTR(actor, 0xc) = fire ? func_08002E28_6FE028 : func_08002600_6FD800;
  checkpoint(w);
  assert(w->net[23] == 0);
  w->net[18] = fire ? 65 : 62;
  w->net[23] = 1;
  w->net[4] = 4500;
  w->net[34] = 12;
  if (fire) {
    w->net[10] = 2;
    w->net[29] = 5;
    w->net[32] = w->net[33] = 1;
    w->net[37] = 50;
    w->net[42] = 0x240;
    w->net[43] = (short)0xff50;
    w->net[44] = 158;
    w->net[45] = -300;
    w->net[46] = 125;
    w->net[47] = -50;
  }
  assert(anchor_world_row_valid(w->net));
  return w;
}
static void linked_platform_checkpoint_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w = linked_fixture(0);
  w->net[18] = 61;w->net[23] = 0;
  tick();
  assert(!linked_inits && func_80023E94_24A94(6));
  assert(PTR(actor, 0xc) == func_080026D0_6FD8D0);
  w->have = 0;
  tick();
  assert(linked_inits == 1 && S16(actor, 0xd0) == 120);
  w = linked_fixture(0);
  tick();
  assert(linked_inits == 1 && func_80023E94_24A94(6));
  assert(S16(actor, 0xd0) == 120 && S16(actor, 0xd2) == -50);
  assert(S16(actor, 0xc8) == 12 && F32(object, 8) == 45);
  w->net[18] = 63;
  tick();
  assert(linked_inits == 1 && PTR(actor, 0xc) == func_08002820_6FDA20);
  w->net[18] = 60;w->net[23] = 0;
  tick();
  assert(PTR(actor, 0xc) == func_08002820_6FDA20);
  w = linked_fixture(1);
  missing_file = 44;
  tick();
  assert(!linked_inits); /* Native gated callback is still allowed to wait. */
  assert(!child_builds && !tint_builds && !flag_writes);
  w = linked_fixture(1);
  child_fail = 1;
  tick();
  assert(linked_inits == 1 && !child_builds && S16(actor, 0xd0) == 120);
  child_fail = 0;w->have = 1;
  tick();
  assert(linked_inits == 1 && child_builds == 1 && S16(actor, 0xd0) == 120);
  w = linked_fixture(1);
  tick();
  assert(linked_inits == 1 && child_builds == 1 && func_800240DC_24CDC(0x1a4));
  assert(linked_child(w) == linked_actor && PTR(actor, 0x9c) == linked_actor);
  assert(F32(linked_object, 8) == 42 && F32(linked_object, 0xc) == 1.25f);
  assert(U32(object, 0x8c) == 0xff50009eu && PTR(object, 0x30) == (char *)object + 0x80);
  for (int state = 1; state <= 5; ++state) {
    w->net[18] = state == 5 ? 66 : 68;
    w->net[24] = state;
    w->net[17] = state == 5 ? 0 : state == 3 ? 256 : 40;
    w->net[40] = state == 5 ? 0 : 12750;
    w->net[41] = state == 5 ? 0 : 6000;
    w->net[31] = 1024 + 672;
    w->net[42] = state == 5 ? 0 : 0x200;
    w->net[43] = (short)0xa57f;w->net[44] = (short)0xfe88;
    w->net[32] = 0;
    tick();
    assert(capture(0, r));
    assert(r[24] == state && r[31] == 1696 && r[42] == w->net[42]);
    assert(r[40] == w->net[40] && r[41] == w->net[41]);
    assert(r[43] == (short)0xa57f && r[44] == (short)0xfe88 && r[47] == -50);
  }
  assert(child_builds == 1 && linked_inits == 1);
  w->net[18] = 67;w->net[24] = 1;w->net[25] = 10;w->net[17] = 40;
  tick();
  assert(S16(actor, 0xee) == 10 && PTR(actor, 0xc) == func_0800321C_6FE41C);
  /* Reject a mismatched local definition before importing state or flags. */
  w->net[19] = 121;w->net[4] = 9900;
  tick();
  assert(!w->have && F32(object, 8) == 45 && S16(actor, 0xd0) == 120);
  w = linked_fixture(1);
  tick();
  w->net[33] = w->net[32] = w->net[37] = 0;
  w->net[45] = w->net[46] = w->net[47] = 0;
  tick();
  assert(!PTR(actor, 0x9c) && (U32(linked_actor, 0x68) & 2));
}
static void linked_lifetime_and_local_fire_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  unsigned int hitter[64] = {0};
  WorldActor *w = linked_fixture(1);
  tick();
  PTR(linked_actor, 0xdc) = hitter;
  U8(hitter, 0x4c) = 0x1a;
  U32(linked_actor, 0x68) |= 1;
  w->net[4] = 12300;
  assert(interacting(w));
  tick();
  assert(F32(object, 8) == 45);
  anchor_world_platform_fire(actor);
  S16(actor, 0xe2) = 1;
  assert(interacting(w));
  S16(actor, 0xe2) = 0;U32(linked_actor, 0x68) = 0;PTR(linked_actor, 0xdc) = 0;
  assert(!interacting(w));
  anchor_world_reuse(linked_actor);
  assert(!PTR(actor, 0x9c));
  w->applied_valid = 0;
  tick();
  assert(child_builds == 2 && linked_child(w));
  U8(linked_actor, 0x74)++;
  assert(!linked_child(w) && !PTR(actor, 0x9c));
  w->applied_valid = 0;
  tick();
  assert(child_builds == 3);
  assert(capture(0, r));
  U32(actor, 0x68) |= 2;
  anchor_world_post(actor);
  assert(!w->actor && (U32(linked_actor, 0x68) & 2));
  assert(capture(0, r) && r[38] && !r[3] && r[23]);
  /* Reloading the native source restores the retained active checkpoint. */
  U32(actor, 0x68) = 0;
  PTR(actor, 0xc) = func_08002E28_6FE028;
  for (int j = 0; j < 4; ++j)
    U16(actor, 0xd0 + 2*j) = (unsigned short)(w->target[j] + 0x8000);
  anchor_world_register(actor, source);anchor_world_post(actor);
  w->have = 0;
  tick();
  assert(!w->restore_controller && child_builds == 4 && S16(actor, 0xd0) == 120);
  anchor_world_reset();
  assert(!(U32(linked_actor, 0x68) & 2) && PTR(actor, 0x9c) == linked_actor);
}
static void bootstrap_receipt_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w = fixture(0x3e0, 0);
  PTR(actor, 0xc) = func_08001368_6FC568;
  checkpoint(w);
  unsigned int instance = w->instance;
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 7;
  w->net[4] = 12300;
  D_801FC604_5B8514 = source;
  PTR(source, 0xa0) = object; /* A newly loaded rider is not already synchronized. */
  assert(interacting(w));
  tick();
  assert(F32(object, 8) == 123 && w->receipt == (WORLD_BOOTSTRAP | 7));
  assert(capture(0, r) && r[WORLD_INSTANCE] == (int)instance &&
         r[WORLD_RECEIPT] == (int)(WORLD_BOOTSTRAP | 7));
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 8;
  w->net[18] = 27; /* Invalid continuation must not acknowledge the offer. */
  tick();
  assert(!w->have && w->receipt == WORLD_APPLY_FAILED);
  anchor_world_register(actor, source);anchor_world_post(actor);
  assert(w->instance != instance && !w->receipt);
  w->have = 1;
  w->net[18] = 1;w->net[4] = 25000;
  tick(); /* Offer addressed to a former native instance is rejected. */
  assert(w->receipt == WORLD_APPLY_FAILED && F32(object, 8) == 123 && !w->have);

  w = fixture(0x2bd, 0);
  anchor_world_npc_init(actor);checkpoint(w);
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 9;
  w->net[4] = 4500;
  U32(actor, 0x68) |= 0x100;
  tick();
  assert(!w->receipt && F32(object, 8) == 0); /* Local dialogue stays in control. */
  U32(actor, 0x68) &= ~0x100u;
  tick();
  assert(w->receipt == (WORLD_BOOTSTRAP | 9) && F32(object, 8) == 45);
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 11;
  U32(actor, 0x68) |= 0x100;
  F32(object, 8) = 99;
  tick(); /* An identical old row is not proof that a new bootstrap ran. */
  assert(w->receipt == (WORLD_BOOTSTRAP | 9) && F32(object, 8) == 99);
  U32(actor, 0x68) &= ~0x100u;
  tick();
  assert(w->receipt == (WORLD_BOOTSTRAP | 11) && F32(object, 8) == 45);
  w = linked_fixture(1);
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 10;
  missing_file = 44;
  tick();
  assert(w->receipt == WORLD_APPLY_FAILED && !w->have);
  missing_file = -1;w->have = 1;
  tick();
  assert(w->receipt == (WORLD_BOOTSTRAP | 10) && child_builds == 1);
  anchor_world_reset();
  assert(!w->receipt);
}
static WorldActor *physics_fixture(void) {
  WorldActor *w = fixture(0x3d0, 0);
  D_8015C5E4 = 10;
  memset(physics_player, 0, sizeof(physics_player));
  memset(physics_player_work, 0, sizeof(physics_player_work));
  reward_calls = physics_effects = carry_releases = 0;
  U16(actor, 0x5e) = 0x1c1;
  D_80236984_5F1E54[0x1c1] = linked_model;
  U32(actor, 0x60) = 0x8ea007e1;
  PTR(actor, 0x70) = source;
  PTR(actor, 0xc) = func_08006C18_6C6368;
  F32(object, 8) = 170;F32(object, 0xc) = -100;F32(object, 0x10) = 135;
  return w;
}
static void physics_checkpoint_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w = physics_fixture();
  F32(actor, 0xd0) = 1.25f;F32(actor, 0xd4) = -2.5f;
  U16(actor, 0xdc) = 4;U32(actor, 0x68) = 0x3020;
  checkpoint(w);
  w->net[4] = 13000;
  tick();
  assert(F32(object, 8) == 130 && U16(actor, 0xdc) == 4);
  assert(F32(actor, 0xd0) == 1.25f && F32(actor, 0xd4) == -2.5f);
  assert(native_calls == 0 && physics_effects == 0); /* no foreign contact replay */
  assert(capture(0, r) && r[19] == 1250 && r[20] == -2500);
  w->net[38] = 1;w->net[14] = 2000;w->net[15] = -1500;tick();
  anchor_world_post(actor);
  assert(!(U32(actor, 0x60) & 0x800300) && F32(actor, 0x7c) == 0);
  anchor_world_post_return();
  assert(U32(actor, 0x60) == 0x8ea007e1 && F32(actor, 0x7c) == -1.5f);
  w->net[38] = 0;
  w->net[10] = 2;w->net[18] = 73;w->net[17] = 120;
  w->net[30] = 6;w->net[29] = 17;w->net[3] = 0;
  tick();
  assert(w->animated && w->clip == 2 && anim_calls == 1);
  assert(phase(PTR(actor, 0xc)) == 73 && U16(actor, 0x8a) == 120 && !reward_calls);
  assert(capture(0, r) && r[18] == 73 && r[17] == 120);
  for (int j = 0; j < 25; ++j) tick();
  assert(!reward_calls && U16(actor, 0x8a) == 120);
  /* A stale checkpoint cannot replay a reward opportunity already observed. */
  w->net[17] = 140;tick();
  assert(U16(actor, 0x8a) == 120);
  w->have = 0;tick();
  assert(reward_calls == 1 && U16(actor, 0x8a) == 119 && reward_ordinal[0] == 0xff08);
  /* Both model waves and overlay availability guard imported animation. */
  w = physics_fixture();checkpoint(w);
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 29;
  missing_file = 30;tick();
  assert(!w->have && w->receipt == WORLD_APPLY_FAILED);
  missing_file = -1;
  D_8015C5E4 = 0;assert(capture(0, r) && r[38]);D_8015C5E4 = 10;
}
static void physics_carry_and_retry_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w = physics_fixture();checkpoint(w);
  D_801FC604_5B8514 = physics_player;
  PTR(physics_player, 0x5c) = physics_player_work;
  PTR(physics_player_work, 0x8c) = actor;
  U32(actor, 0x68) |= 0x8000;F32(object, 8) = 200;
  tick();
  assert(F32(object, 8) == 200 && U16(actor, 0xdc) == 5);
  assert(capture(0, r) && r[3] && (r[22] & 16));
  /* A winning simultaneous remote carry ends this local carry action safely. */
  w->net[3] = 1;w->net[22] = 16;w->net[21] = 5;
  w->net[30] = 2;w->net[29] = 1;tick();
  assert(carry_releases == 1 && !physics_held(w));
  assert((U32(actor, 0x68) & 0x88000) == 0x88000);
  assert(capture(0, r) && !r[3]);
  w->have = 0;tick(); /* departure releases imported carry without a player pointer */
  assert(!(U32(actor, 0x68) & 0x88000) && U16(actor, 0xdc) == 4);
  U32(actor, 0x68) |= 0x1000;tick();anchor_world_post(actor);
  assert(!w->actor && physics_effects == 1 && w->physics_broken);
  assert(capture(0, r) && r[18] == 75 && r[38] && !r[3]);
  w->retained[23] = 3; /* newer remote attempt while this source is unloaded */
  unsigned int old_instance = w->instance;
  U32(actor, 0x68) = 0;U16(actor, 0xdc) = 0;
  PTR(actor, 0xc) = func_08006C18_6C6368;U32(actor, 0x60) = 0x8ea007e1;
  anchor_world_register(actor, source);anchor_world_post(actor);
  assert(w->instance != old_instance && w->physics_round == 4 && !w->physics_broken);
  assert(capture(0, r) && r[23] == 4 && r[18] == 69);
  /* Normal native distance culling clears +70 first and preserves this attempt. */
  F32(object, 8) = 321;PTR(actor, 0x70) = 0;U32(actor, 0x68) = 2;
  anchor_world_post(actor);
  assert(!w->physics_broken && capture(0, r) && r[4] == 32100 && r[18] == 69);
  U32(actor, 0x68) = 0;F32(object, 8) = 170;
  anchor_world_register(actor, source);anchor_world_post(actor);tick();
  assert(w->physics_round == 4 && F32(object, 8) == 321);
  /* A late observer of a break has no old visual effect to replay. */
  w = physics_fixture();checkpoint(w);w->net[18] = 75;tick();
  assert((U32(actor, 0x68) & 2) && !physics_effects);
  anchor_world_post(actor);assert(capture(0, r) && r[18] == 75);
}
static void physics_completion_and_reward_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w = physics_fixture();
  F32(object, 8) = 0;F32(object, 0xc) = 8;F32(object, 0x10) = 110;
  tick();assert(phase(PTR(actor, 0xc)) == 70);
  for (int j = 0; j < 80 && phase(PTR(actor, 0xc)) == 70; ++j) tick();
  assert(phase(PTR(actor, 0xc)) == 71 && w->clip == 1);
  U8(object, 0x7c) |= 2;tick();assert(w->clip == 2 && phase(PTR(actor, 0xc)) == 72);
  U8(object, 0x7c) |= 2;tick();assert(U16(actor, 0x8a) == 200);
  for (int j = 0; j < 201; ++j) tick();
  assert(reward_calls == 21 && phase(PTR(actor, 0xc)) == 74);
  for (unsigned int j = 0; j < 21; ++j) assert(reward_ordinal[j] == 0xff00 + j);
  assert(capture(0, r) && r[18] == 74 && !r[17]);
  assert(!anchor_world_loot_ordinal(actor, &s_physics_ordinal));
  unsigned int angle = reward_angle[8], kind = reward_kind[8];
  /* Proximity culling must not reset the shared solved state or dispense a
   * second reward sequence while another player retains this room visit. */
  PTR(actor, 0x70) = 0;U32(actor, 0x68) = 2;
  anchor_world_post(actor);
  assert(!w->actor && w->retained_valid && w->retained[18] == 74);
  U32(actor, 0x68) = 0;U32(actor, 0x60) = 0x8ea007e1;
  F32(object, 8) = 170;F32(object, 0xc) = -100;F32(object, 0x10) = 135;
  PTR(actor, 0xc) = func_08006C18_6C6368;
  anchor_world_register(actor, source);anchor_world_post(actor);tick();
  assert(phase(PTR(actor, 0xc)) == 74 && !w->restore_controller);
  assert(F32(object, 8) == 0 && F32(object, 0xc) == 8 && w->clip == 2);
  for (int j = 0; j < 25; ++j) tick();
  assert(reward_calls == 21 && capture(0, r) && r[18] == 74);
  /* A real scope reset releases retained completion for the next room visit. */
  anchor_world_reset();
  assert(!w->retained_valid && !w->restore_controller);
  w = physics_fixture();
  w->physics_round = 7; /* concurrent success after a delayed retry, same loot */
  w->animated = 1;w->clip = 2;PTR(actor, 0xc) = func_080070E4_6C6834;
  U32(actor, 0x60) = 0x21;U16(actor, 0x8a) = 120;
  tick();assert(reward_calls == 1 && reward_ordinal[0] == 0xff08);
  assert(reward_angle[0] == angle && reward_kind[0] == kind && F32(physics_child, 0x7c) == 5);
}
static void physics_reconnect_after_break_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  WorldActor *w = physics_fixture();checkpoint(w);
  w->net[18] = 75;w->net[23] = 7;tick();anchor_world_post(actor);
  assert(!w->actor && w->physics_broken && w->retained_valid);
  anchor_world_reset(); /* Disconnect while the broken native task is absent. */
  U32(actor, 0x68) = 0;U32(actor, 0x60) = 0x8ea007e1;
  PTR(actor, 0xc) = func_08006C18_6C6368;U16(actor, 0xdc) = 0;
  anchor_world_register(actor, source);anchor_world_post(actor);
  s_active = 1;
  assert(!w->physics_broken && w->physics_round == 0);
  int before = native_calls;
  tick();
  assert(native_calls == before + 1 && capture(0, r) && r[18] == 69);
}
static void shutter_checkpoint_test(void) {
  WorldActor *w = fixture(WORLD_SHUTTER_ENTITY, 0);
  assert(!w->kind);
  D_800C7AB2 = 0xb2;
  U32(definition, 4) = 1;
  anchor_world_roster_begin(0xb2); anchor_world_roster_add(0, source, definition);
  anchor_world_roster_end(1); anchor_world_register(actor, source);
  U32(actor, 0xd0) = 1; U16(actor, 0x5e) = 0x23f;
  static unsigned short files[] = {30, 0x152};
  static unsigned int clips[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0};
  static void *shutter_model[] = {files, clips};
  D_80236984_5F1E54[0x23f] = shutter_model;
  PTR(actor, 0xc) = func_080056FC_6C4E4C;
  S16(actor, 0x8a) = 30;
  U32(actor, 0x60) = 0x812006e0u;
  anchor_world_animation(actor, 14); anchor_world_post(actor);
  w = &s_actors[0]; assert(w->kind == WORLD_SHUTTER);
  int r[ANCHOR_WORLD_WORDS]; assert(capture(0, r));
  assert(r[18] == 3 && r[17] == 30 && r[19] == 1 && !r[20]);
  checkpoint(w); shutter_births = 0;
  for (int i = 0; i < 5; ++i) tick();
  assert(!shutter_births && S16(actor, 0x8a) == 30);
  w->have = 0; tick();
  assert(shutter_births == 1 && w->cycle == 1 && S16(actor, 0x8a) == 29);
  assert(capture(0, r) && r[20] == 1);
  /* An occupied room keeps the attempt and continuation through local cull. */
  U32(actor, 0x68) |= 2u; anchor_world_post(actor);
  assert(!w->actor && capture(0, r) && r[38]);
  U32(actor, 0x68) = 0; PTR(actor, 0xc) = func_08005624_6C4D74;
  S16(actor, 0x8a) = 60;
  anchor_world_register(actor, source); anchor_world_animation(actor, 14);
  anchor_world_post(actor); assert(capture(0, r) && w->restore_controller);
  tick(); assert(!w->restore_controller && shutter_births == 1 && S16(actor, 0x8a) == 28);
  /* A new cycle gets a distinct identity; a follower cannot emit it. */
  S16(actor, 0x8a) = 30; tick();
  assert(shutter_births == 2 && w->cycle == 2);
  checkpoint(w); w->net[17] = 30; w->net[20] = 3;
  tick(); assert(shutter_births == 2 && w->cycle == 3);
  w->net[19] = 0; assert(!apply(w));
  w->net[19] = 1; w->net[20] = 2; assert(!apply(w));
  w->net[20] = 3; w->applied_valid = 0; missing_file = 30; assert(!apply(w));
  missing_file = -1;
}
/* ---- File51 Super Pass bridge core fixtures --------------------------- */

static unsigned int bridge_actor[4][64], bridge_object[4][64],
                    bridge_source[4][8], bridge_definition[4][8];

static void bridge_define(unsigned int slot, unsigned int entity,
                          unsigned int d4, unsigned int d8, unsigned int d12) {
  memset(bridge_definition[slot], 0, sizeof(bridge_definition[slot]));
  U16(bridge_definition[slot], 0) = (unsigned short)entity;
  U32(bridge_definition[slot], 4) = d4;
  U32(bridge_definition[slot], 8) = d8;
  U32(bridge_definition[slot], 12) = d12;
}

/* Room 0x15E only classifies the exact cohort: root, both guard selectors and
 * the blocker. Anything else leaves every member unclassified. */
static WorldActor *bridge_cohort(int with_blocker, int register_root) {
  unsigned int i;
  anchor_world_reset();
  test_ptr_count = 0;
  memset(bridge_actor, 0, sizeof(bridge_actor));
  memset(bridge_object, 0, sizeof(bridge_object));
  memset(bridge_source, 0, sizeof(bridge_source));
  memset(bridge_definition, 0, sizeof(bridge_definition));
  memset(system_words, 0, sizeof(system_words));
  memset(temporary_flags, 0, sizeof(temporary_flags));
  memset(save_flags, 0, sizeof(save_flags));
  switch_bursts = flag_writes = 0;
  native_calls = talk_calls = anim_calls = 0;
  bridge_registered_count = 0;
  bridge_apply_calls = bridge_restore = 0;
  bridge_control_remote = bridge_control_paused = 0;
  bridge_inputs = bridge_control_inputs = 0;
  bridge_capture_ready = 0;
  bridge_apply_result = 1;
  bridge_frame_reply_ready = 0;
  s_room = 0xffff;
  s_signature = 0;
  s_self = 2;
  s_active = 1;
  D_800C7AB2 = WORLD_BRIDGE_ROOM;
  bridge_define(0, WORLD_BRIDGE_ENTITY, 0, 0, 0x00010000);
  bridge_define(1, 0x2d0, 0, 0x10e, 0);
  bridge_define(2, 0x2d0, 0, 0x110, 0x00010000);
  bridge_define(3, 0x311, 0, 0, 0);
  anchor_world_roster_begin(WORLD_BRIDGE_ROOM);
  for (i = 0; i < 4; ++i) {
    if (i == 3 && !with_blocker)
      break;
    anchor_world_roster_add(i, bridge_source[i], bridge_definition[i]);
  }
  anchor_world_roster_end(with_blocker ? 4 : 3);
  if (register_root) {
    /* The root drives the shared scheduler fixture, so it uses the harness
     * actor/object pair exactly like the other core fixtures. */
    U16(actor, 0x5c) = WORLD_BRIDGE_ENTITY;
    U8(actor, 0x74) = 3;
    PTR(actor, 0x18) = object;
    PTR(actor, 0x0c) = idle;
    F32(object, 0x1c) = F32(object, 0x20) = F32(object, 0x24) = 1;
    U16(object, 0x7e) = 256;
    anchor_world_register(actor, bridge_source[0]);
    anchor_world_post(actor);
    D_80236984_5F1E54[0] = model;
  }
  return &s_actors[0];
}

static void bridge_actor_bind(unsigned int slot, unsigned int entity) {
  U16(bridge_actor[slot], 0x5c) = (unsigned short)entity;
  U8(bridge_actor[slot], 0x74) = 3;
  PTR(bridge_actor[slot], 0x18) = bridge_object[slot];
  PTR(bridge_actor[slot], 0x0c) = idle;
  anchor_world_register(bridge_actor[slot], bridge_source[slot]);
  anchor_world_post(bridge_actor[slot]);
}

/* A valid kind 9 row for the frame and delivery fixtures. */
static void bridge_valid_row(int *r, unsigned int flags, unsigned int fresh,
                             unsigned int guard0, unsigned int guard1,
                             unsigned int receipt) {
  unsigned int j;
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    r[j] = 0;
  r[0] = 0;
  r[1] = WORLD_BRIDGE_ENTITY;
  r[2] = WORLD_BRIDGE;
  r[WB_FLAGS] = (int)flags;
  r[WB_FRESH] = (int)fresh;
  r[WB_GATE_PHASE] = 1;
  r[WB_GUARD_0] = (int)guard0;
  r[WB_GUARD_1] = (int)guard1;
  r[WB_GUARD_0 + 13] = 40;
  r[WB_GUARD_1 + 13] = 39;
  r[WORLD_RECEIPT] = (int)receipt;
}

static void bridge_reply_build(unsigned int cid, unsigned int latency,
                               const int *row) {
  int n = 0;
  unsigned int j;
  n += snprintf(bridge_frame_reply + n, sizeof(bridge_frame_reply) - (size_t)n,
                "{\"a\":[[%u,%u", cid, latency);
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    n += snprintf(bridge_frame_reply + n, sizeof(bridge_frame_reply) - (size_t)n,
                  ",%d", row[j]);
  n += snprintf(bridge_frame_reply + n, sizeof(bridge_frame_reply) - (size_t)n,
                "]],\"d\":\"");
  for (j = 0; j < 32; ++j)
    n += snprintf(bridge_frame_reply + n, sizeof(bridge_frame_reply) - (size_t)n,
                  "00");
  snprintf(bridge_frame_reply + n, sizeof(bridge_frame_reply) - (size_t)n, "\"}");
  bridge_frame_reply_ready = 1;
}

static void bridge_roster_test(void) {
  WorldActor *root = bridge_cohort(1, 1);
  assert(root->kind == WORLD_BRIDGE && root->bridge_member && !root->variant);
  /* Only the root carries the typed kind; guards and blocker stay members. */
  assert(s_actors[1].bridge_member && !s_actors[1].kind && !s_actors[1].variant);
  assert(s_actors[2].bridge_member && !s_actors[2].kind && s_actors[2].variant == 1);
  assert(s_actors[3].bridge_member && !s_actors[3].kind);
  /* Registration forwards the placed entity and the guard selector. */
  assert(bridge_registered_count == 1);
  assert(bridge_registered_actor[0] == actor);
  assert((bridge_registered[0] & 0xffffffu) == WORLD_BRIDGE_ENTITY);
  /* A guard actor never becomes an independent talkable NPC. */
  bridge_actor_bind(1, 0x2d0);
  assert(bridge_registered_count == 2);
  assert((bridge_registered[1] & 0xffffffu) == 0x2d0);
  assert((bridge_registered[1] >> 24) == (unsigned int)s_actors[1].variant);
  anchor_world_npc_init(bridge_actor[1]);
  assert(!s_actors[1].kind && !s_actors[1].talkable && !s_actors[1].has_path);
  /* Control: the same entity outside the cohort is promoted normally. */
  WorldActor *plain = fixture(0x2d0, 0);
  assert(!plain->bridge_member && !plain->kind);
  anchor_world_npc_init(actor);
  assert(plain->kind == WORLD_NPC && plain->talkable);
}

static void bridge_cohort_rejection_test(void) {
  WorldActor *root = bridge_cohort(0, 1);
  assert(!root->kind && !root->bridge_member && !bridge_registered_count);
  assert(!s_actors[1].bridge_member && !s_actors[2].bridge_member);
  /* The same definitions outside room 0x15E classify nothing either. */
  bridge_cohort(1, 0);
  D_800C7AB2 = 0x161;
  anchor_world_roster_begin(0x161);
  for (unsigned int i = 0; i < 4; ++i)
    anchor_world_roster_add(i, bridge_source[i], bridge_definition[i]);
  anchor_world_roster_end(4);
  assert(!s_actors[0].kind && !s_actors[0].bridge_member);
  assert(!s_actors[1].bridge_member && !s_actors[2].bridge_member &&
         !s_actors[3].bridge_member);
}

static void bridge_scheduler_test(void) {
  WorldActor *w = bridge_cohort(1, 1);
  int row[ANCHOR_WORLD_WORDS];
  memset(bridge_capture_row, 0, sizeof(bridge_capture_row));
  bridge_capture_row[WB_FLAGS] = 1;
  bridge_capture_row[WB_GATE_PHASE] = 2;
  bridge_capture_row[WB_GUARD_0] = bridge_capture_row[WB_GUARD_1] = 2;
  bridge_capture_row[WB_INPUT] = 1;
  bridge_capture_ready = 1;
  bridge_inputs = 1;
  bridge_apply_calls = 0;
  assert(capture(0, row));
  assert(anchor_world_row_valid(row) && row[WB_INPUT] == 1);
  assert(w->retained_valid && w->retained[WB_INPUT] == 1);
  memcpy(w->net, row, sizeof(row));
  w->have = 1;
  w->owner = s_self;
  w->net[WB_AGGREGATE] = 3;
  /* The owner's own echo delivers the aggregate without an application. */
  tick();
  assert(!bridge_apply_calls && !bridge_control_remote);
  assert(bridge_control_inputs == 3 && !bridge_control_paused);
  /* A remote bootstrap applies once and retains its receipt. */
  w->owner = 1;
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 7;
  tick();
  assert(bridge_apply_calls == 1 && w->receipt == (WORLD_BOOTSTRAP | 7));
  assert(bridge_control_remote == 1);
  /* The accepted aggregate keeps reaching native control afterwards. */
  w->net[WB_AGGREGATE] = 2;
  tick();
  assert(bridge_apply_calls == 1 && bridge_control_inputs == 2);
  /* A paused incoming row still delivers the accepted latch. */
  w->net[38] = 1;
  tick();
  assert(bridge_control_paused == 1 && bridge_control_inputs == 2);
  w->net[38] = 0;
  /* A failed application must not acknowledge the offer. */
  bridge_apply_result = 0;
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 8;
  w->have = 1;
  tick();
  assert(!w->have && w->receipt == WORLD_APPLY_FAILED && bridge_apply_calls == 3);
  /* The next identical offer is retried: the failed receipt is not an ack. */
  bridge_apply_result = 1;
  w->have = 1;
  tick();
  assert(w->have && w->receipt == (WORLD_BOOTSTRAP | 8) && bridge_apply_calls == 4);
  assert(bridge_control_inputs == 2 && !bridge_control_paused);
  /* A retained bridge keeps the latched accepted event, unlike the crane pads. */
  bridge_inputs = 2;
  bridge_capture_ready = 0;
  assert(capture(0, row) && row[38] && row[WB_INPUT] == 2 && !row[WB_AGGREGATE]);
  assert(w->restore_controller);
  w->have = 0;
  tick();
  assert(bridge_apply_calls == 5 && !w->restore_controller);
  /* The unloaded-controller path preserves the same latch. */
  bridge_inputs = 3;
  bridge_capture_ready = 1;
  w->actor = 0;
  assert(capture(0, row) && row[38] && row[WB_INPUT] == 3 && !row[WB_AGGREGATE]);
  assert(w->restore_controller);
}

static void bridge_frame_bootstrap_test(void) {
  WorldActor *w = bridge_cohort(1, 1);
  int row[ANCHOR_WORLD_WORDS];
  /* The retained local state is the fresh save-1 constructor, which scores far
   * above an untouched pre-open route. */
  memset(bridge_capture_row, 0, sizeof(bridge_capture_row));
  bridge_capture_row[WB_FLAGS] = 3;
  bridge_capture_row[WB_FRESH] = 1;
  bridge_capture_row[WB_GATE_PHASE] = 3;
  bridge_capture_row[WB_GUARD_0] = bridge_capture_row[WB_GUARD_1] = 3;
  bridge_capture_row[WB_GUARD_0 + 13] = 40;
  bridge_capture_row[WB_GUARD_1 + 13] = 39;
  bridge_capture_ready = 1;
  assert(capture(0, row));
  assert(row[WB_FRESH] == 1 && controller_progress(row) == 3 * 16 + 3 + 3);
  /* An incoming established bootstrap is accepted even with lower progress. */
  bridge_valid_row(row, 0, 0, 1, 1, WORLD_BOOTSTRAP | 5);
  assert(controller_progress(row) == 2);
  row[WORLD_INSTANCE] = (int)w->instance;
  bridge_reply_build(1, 0, row);
  anchor_world_frame();
  assert(w->have && w->owner == 1 && !w->net[WB_FLAGS] && !w->net[WB_FRESH]);
  assert(w->net[WORLD_RECEIPT] == (WORLD_BOOTSTRAP | 5));
  /* Ordinary lower progress is still rejected: restore the retained pose and
   * repeat the frame without the bootstrap receipt. */
  memset(bridge_capture_row, 0, sizeof(bridge_capture_row));
  bridge_capture_row[WB_FLAGS] = 3;
  bridge_capture_row[WB_FRESH] = 1;
  bridge_capture_row[WB_GATE_PHASE] = 3;
  bridge_capture_row[WB_GUARD_0] = bridge_capture_row[WB_GUARD_1] = 3;
  bridge_capture_row[WB_GUARD_0 + 13] = 40;
  bridge_capture_row[WB_GUARD_1 + 13] = 39;
  assert(capture(0, row));
  assert(controller_progress(w->retained) == 3 * 16 + 3 + 3);
  bridge_valid_row(row, 0, 0, 1, 1, 0);
  row[WORLD_INSTANCE] = (int)w->instance;
  bridge_reply_build(1, 0, row);
  anchor_world_frame();
  assert(!w->have);
  /* A non-fresh retained row also rejects the lower-progress bootstrap. */
  memset(bridge_capture_row, 0, sizeof(bridge_capture_row));
  bridge_capture_row[WB_FLAGS] = 3;
  bridge_capture_row[WB_GATE_PHASE] = 3;
  bridge_capture_row[WB_GUARD_0] = bridge_capture_row[WB_GUARD_1] = 3;
  bridge_capture_row[WB_GUARD_0 + 13] = 40;
  bridge_capture_row[WB_GUARD_1 + 13] = 39;
  assert(capture(0, row));
  assert(!w->retained[WB_FRESH]);
  bridge_valid_row(row, 0, 0, 1, 1, WORLD_BOOTSTRAP | 6);
  row[WORLD_INSTANCE] = (int)w->instance;
  bridge_reply_build(2, 0, row);
  anchor_world_frame();
  assert(!w->have);
}

static void counterweight_core_test(void) {
  fixture(WORLD_COUNTERWEIGHT_ENTITY,0);
  D_800C7AB2=WORLD_COUNTERWEIGHT_ROOM;
  anchor_world_roster_begin(WORLD_COUNTERWEIGHT_ROOM);
  anchor_world_roster_add(13,source,definition);anchor_world_roster_end(14);
  anchor_world_register(actor,source);anchor_world_post(actor);
  WorldActor *w=&s_actors[13];assert(w->kind==WORLD_COUNTERWEIGHT);
  memset(cw_capture_row,0,sizeof(cw_capture_row));cw_capture_ready=1;
  cw_capture_row[5]=-8000;cw_capture_row[6]=-2000;cw_capture_row[8]=256;
  for(int j=10;j<=15;++j)cw_capture_row[j]=-8000;cw_capture_row[16]=63;
  int row[ANCHOR_WORLD_WORDS];assert(capture(13,row));
  assert(anchor_world_row_valid(row)&&controller_progress(row)==0);
  tick();assert(!cw_confirmed);
  memcpy(w->net,row,sizeof(row));w->net[47]=33;w->have=1;w->owner=s_self;
  cw_applies=0;tick();assert(cw_confirmed&&!cw_remote&&cw_inputs==33&&!cw_applies);
  w->owner=s_self+1;w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|99;
  tick();assert(cw_remote&&cw_applies==1&&w->receipt==(WORLD_BOOTSTRAP|99));
  w->net[WORLD_INSTANCE]++;tick();assert(!w->have&&w->receipt==WORLD_APPLY_FAILED);
  /* Culled roots retain their atomic heights but release all local inputs. */
  cw_capture_ready=0;w->actor=0;w->ready=0;
  assert(capture(13,row)&&row[38]&&!row[3]&&!row[46]&&!row[47]);
  anchor_world_reset();
}

static void gate64_core_test(void) {
  WorldActor *w=fixture(WORLD_GATE64_ENTITY,0);assert(!w->kind);
  D_800C7AB2=WORLD_GATE64_ROOM;
  anchor_world_roster_begin(WORLD_GATE64_ROOM);anchor_world_roster_add(0,source,definition);
  anchor_world_roster_end(1);anchor_world_register(actor,source);anchor_world_post(actor);
  w=&s_actors[0];assert(w->kind==WORLD_GATE64);
  memset(gate64_capture_row,0,sizeof(gate64_capture_row));gate64_capture_ready=1;
  gate64_capture_row[10]=1;gate64_capture_row[12]=3;gate64_capture_row[23]=150;
  int row[ANCHOR_WORLD_WORDS];assert(capture(0,row));assert(anchor_world_row_valid(row));
  memcpy(w->net,row,sizeof(row));w->have=1;w->owner=s_self;
  gate64_apply_calls=0;tick();assert(gate64_confirmed&&!gate64_remote&&!gate64_apply_calls);
  w->owner=s_self+1;w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|77;
  tick();assert(gate64_remote&&gate64_apply_calls==1&&w->receipt==(WORLD_BOOTSTRAP|77));
  w->actor=0;w->ready=0;gate64_capture_row[12]=10;gate64_capture_row[25]=1;
  assert(capture(0,row)&&row[WG64_COMPLETE]);
  memcpy(w->net,row,sizeof(row));w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|78;
  w->have=1;tick();assert(gate64_apply_calls==2&&w->receipt==(WORLD_BOOTSTRAP|78));
  gate64_capture_ready=0;anchor_world_reset();
}

/* File62's placed 0x3D6 container: roster classification, the one-based
 * parent slot the dynamic Doll registration resolves, and the owner/replica
 * handshake. The row codec and the container's own native cycle belong to the
 * dedicated doll harness. */
static void doll_core_test(void) {
  WorldActor *w=fixture(WORLD_DOLL_ENTITY,0);assert(!w->kind);
  int row[ANCHOR_WORLD_WORDS];
  D_800C7AB2=WORLD_DOLL_ROOM_A;
  anchor_world_roster_begin(WORLD_DOLL_ROOM_A);
  anchor_world_roster_add(7,source,definition);
  anchor_world_roster_end(8);
  anchor_world_register(actor,source);anchor_world_post(actor);
  w=&s_actors[7];assert(w->kind==WORLD_DOLL_CONTAINER);
  /* A non-zero parameter word is not the known-zero container record. */
  fixture(WORLD_DOLL_ENTITY,0);
  D_800C7AB2=WORLD_DOLL_ROOM_A;
  anchor_world_roster_begin(WORLD_DOLL_ROOM_A);
  U32(definition,8)=1;
  anchor_world_roster_add(7,source,definition);
  anchor_world_roster_end(8);
  assert(!s_actors[7].kind);
  /* Room 0x182 places the same container at roster slot 2. */
  fixture(WORLD_DOLL_ENTITY,0);
  D_800C7AB2=WORLD_DOLL_ROOM_B;
  anchor_world_roster_begin(WORLD_DOLL_ROOM_B);
  anchor_world_roster_add(2,source,definition);
  anchor_world_roster_end(3);
  anchor_world_register(actor,source);anchor_world_post(actor);
  assert(s_actors[2].kind==WORLD_DOLL_CONTAINER);
  /* The same entity in an unrelated room stays untyped. */
  fixture(WORLD_DOLL_ENTITY,0);
  D_800C7AB2=0x161;
  anchor_world_roster_begin(0x161);
  anchor_world_roster_add(0,source,definition);
  anchor_world_roster_end(1);
  assert(!s_actors[0].kind);
  /* A self-owned record confirms this simulator without replaying scalars. */
  fixture(WORLD_DOLL_ENTITY,0);
  D_800C7AB2=WORLD_DOLL_ROOM_A;
  anchor_world_roster_begin(WORLD_DOLL_ROOM_A);
  anchor_world_roster_add(7,source,definition);
  anchor_world_roster_end(8);
  anchor_world_register(actor,source);anchor_world_post(actor);
  w=&s_actors[7];
  memset(row,0,sizeof(row));
  row[1]=WORLD_DOLL_ENTITY;row[2]=WORLD_DOLL_CONTAINER;
  row[WDC_CYCLE]=1;row[WDC_PHASE]=1;row[WDC_PITCH]=8;
  row[WORLD_INSTANCE]=(int)w->instance;
  memcpy(w->net,row,sizeof(row));
  w->have=1;w->owner=s_self;
  doll_apply_calls=0;doll_control_remote=doll_confirmed=doll_control_paused=0;
  tick();assert(doll_confirmed&&!doll_control_remote&&!doll_apply_calls);
  /* A peer-owned record turns the container into a waiting replica. */
  w->owner=s_self+1;w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|77;
  tick();
  assert(doll_control_remote&&doll_apply_calls==1&&
         w->receipt==(WORLD_BOOTSTRAP|77));
  w->net[38]=1;tick();assert(doll_control_paused);
  w->net[38]=0;
  /* A retained row is replayed before the normal offer path. */
  w->retained_valid=1;memcpy(w->retained,w->net,sizeof(w->net));
  doll_restore=1;w->applied_valid=0;doll_apply_calls=0;
  tick();assert(!doll_restore&&doll_apply_calls>=1);
  w->retained_valid=0;
  anchor_world_reset();assert(!w->have);
}

static WorldActor *spike_fixture(int subtype) {
  static unsigned short files[] = {0x24f,0};
  static unsigned int slots[] = {0x08000050,0x08000140,0x080000b4,0x08000180,0};
  static void *spike_model[] = {files,slots};
  fixture(WORLD_SPIKE_ENTITY,0);
  D_800C7AB2 = subtype == 2 ? 0x32 : subtype == 1 ? 0x34 : 0x3a;
  U16(definition,4) = (unsigned short)subtype;
  U16(definition,6) = (unsigned short)(subtype == 2 ? 20 : subtype == 1 ? 23 : 24);
  anchor_world_roster_begin(D_800C7AB2);
  anchor_world_roster_add(0,source,definition);anchor_world_roster_end(1);
  anchor_world_register(actor,source);anchor_world_post(actor);
  WorldActor *w = &s_actors[0];
  assert(w->kind == WORLD_PLATFORM && w->variant == WORLD_SPIKE_VARIANT);
  D_80236984_5F1E54[WORLD_SPIKE_ENTITY] = spike_model;
  U16(actor,0x5e) = WORLD_SPIKE_ENTITY;S16(actor,0xd0) = (short)subtype;
  U16(actor,0xd2) = U16(definition,6);U8(actor,0xd4) = 1;
  S16(actor,0xd6) = S16(actor,0x8a) = subtype ? 90 : 155;
  U32(actor,0x60) = 0xe0;U16(object,0x7e) = 12;
  F32(object,0x24) = subtype ? 1 : 1.2f;
  PTR(actor,0xc) = func_08000980_6ACED0;
  anchor_world_animation(actor,subtype == 1 ? 2 : 0);
  return w;
}
static void spike_checkpoint_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  for (int subtype = 0; subtype < 3; ++subtype) {
    WorldActor *w = spike_fixture(subtype);
    assert(capture(0,r) && r[18] == 76 && r[20] == 1);
    assert(phase((void *)func_08000980_6ACED0) == 76);
    assert(!platform_phase_valid(w,WORLD_PHYSICS_BROKEN));
    checkpoint(w);w->net[17] = 0;w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP|61;
    tick();assert(native_calls == 1 && U8(actor,0xd4) == 3 && S16(actor,0x8a) == -1);
    assert(w->receipt == (WORLD_BOOTSTRAP|61));
    assert(capture(0,r) && r[20] == 3 && r[29] == 17);
    /* An unchanged received row acknowledges once; it cannot reset the
     * predicted local timer on every intervening native update. */
    U8(object,0x7c) = 2;tick();assert(native_calls == 2 && U8(actor,0xd4) == 4);
    assert(capture(0,r) && r[23] == 1 && r[11] == 200);
    w->net[20] = 4;w->net[17] = -1;w->net[23] = 1;w->net[29] = 17;
    w->net[11] = 150;w->net[38] = 1;w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP|62;
    tick();assert(native_calls == 2 && F32(object,0x28) == 1.5f);
    unsigned int flags = U32(actor,0x60);
    anchor_world_post(actor);assert(!(U32(actor,0x60)&1));
    anchor_world_post_return();assert(U32(actor,0x60) == flags);
    /* Mutable phase is restored; immutable stagger/model and absent native
     * resources reject before changing the local task or acknowledging it. */
    w->net[21]--;assert(!apply(w));w->net[21]++;
    for (int i=0; i<3; ++i) {
      missing_file = i == 0 ? 24 : i == 1 ? 0x152 : 0x24f;
      assert(!apply(w));
    }
    missing_file = -1;U8(actor,0xd4) = 0;assert(!apply(w));U8(actor,0xd4) = 4;
    U16(actor,0x5e) = 1;assert(!apply(w));U16(actor,0x5e) = WORLD_SPIKE_ENTITY;
    if (subtype == 2) {
      D_801FC60C_5B851C = player_object;
      F32(player_object,0x10) = F32(object,0x10)+29;tick();
      assert((U32(actor,0x60)&0xc0) == 0xc0 && native_calls == 2);
      F32(player_object,0x10) = F32(object,0x10)+30;tick();
      assert(!(U32(actor,0x60)&0xc0) && native_calls == 2);
    }
    /* Same-room pool recycling resumes the retained cycle after the local
     * constructor completes; it does not restart from its initial stagger. */
    assert(capture(0,r));memcpy(w->retained,r,sizeof(r));w->retained_valid = 1;
    anchor_world_register(actor,source);U8(actor,0xd4) = 1;
    U32(actor,0x60) = 0xe0;S16(actor,0x8a) = subtype ? 90 : 155;
    anchor_world_animation(actor,subtype == 1 ? 2 : 0);anchor_world_post(actor);
    tick();assert(!w->restore_controller && U8(actor,0xd4) == 4);
    assert((U32(actor,0x60)&0x01000001u) == 0x01000001u);
  }
}
static void rope_checkpoint_test(void) {
  static unsigned short files[] = {0x1df,0};
  static unsigned int slots[] = {0x08000064,0x080000f0,0};
  static void *model[] = {files,slots};
  fixture(WORLD_ROPE_ENTITY,0);D_800C7AB2 = 0x41;U32(definition,4) = 10;
  anchor_world_roster_begin(D_800C7AB2);
  anchor_world_roster_add(0,source,definition);anchor_world_roster_end(1);
  anchor_world_register(actor,source);anchor_world_post(actor);
  WorldActor *w = &s_actors[0];
  assert(w->kind == WORLD_PLATFORM && w->variant == WORLD_ROPE_VARIANT);
  D_80236984_5F1E54[WORLD_ROPE_ENTITY] = model;
  U16(actor,0x5e) = WORLD_ROPE_ENTITY;U32(actor,0xd0) = 10;U32(actor,0x60) = 0xe0;
  U16(object,0x7e) = 512;U8(object,0x7c) = 1;
  U16(object,0x14) = 65530;PTR(actor,0xc) = func_0800074C_6ACC9C;
  anchor_world_animation(actor,0);
  int r[ANCHOR_WORLD_WORDS];
  assert(capture(0,r) && r[18] == 77 && r[7] == 1018 && r[19] == -6 && r[20] == 10);
  checkpoint(w);w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP|81;
  U16(object,0x14) = 100;tick();
  assert(native_calls == 1 && U16(object,0x14) == 4 && w->receipt == (WORLD_BOOTSTRAP|81));
  tick();assert(native_calls == 2 && U16(object,0x14) == 14);
  w->net[19] = -100;w->net[7] = (-100)&1023;w->net[38] = 1;
  w->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP|82;tick();
  assert(native_calls == 2 && U16(object,0x14) == 65436);
  assert(U32(actor,0xd0) == 10 && (U32(actor,0x60)&0xc0) == 0xc0);
  for (int i=0;i<3;++i) {
    missing_file = i == 0 ? 24 : i == 1 ? 0x152 : 0x1df;
    assert(!apply(w));
  }
  missing_file = -1;U32(actor,0xd0) = 11;assert(!apply(w));U32(actor,0xd0) = 10;
  U16(actor,0x5e) = WORLD_SPIKE_ENTITY;assert(!apply(w));U16(actor,0x5e) = WORLD_ROPE_ENTITY;
  w->saved_ai = (void *)func_08000980_6ACED0;assert(!apply(w));
  w->saved_ai = (void *)func_0800074C_6ACC9C;
  assert(capture(0,r));memcpy(w->retained,r,sizeof(r));w->retained_valid = 1;
  anchor_world_register(actor,source);U16(object,0x14) = 0;
  anchor_world_animation(actor,0);anchor_world_post(actor);tick();
  assert(!w->restore_controller && U16(object,0x14) == 65446);
  assert(native_calls == 3 && U32(actor,0xd0) == 10);
}

static WorldActor *file40_fixture(int top,int subtype,int radius,int step) {
  static unsigned short top_files[]={0x1e9,0x168},rotor_files[]={0x1ea,0x168};
  static unsigned int top_slots[]={0x08000348,0},rotor_slots[]={0x08000258,0};
  static void *top_model[]={top_files,top_slots},*rotor_model[]={rotor_files,rotor_slots};
  unsigned int entity=top ? WORLD_TOP_ENTITY : WORLD_ROTOR_ENTITY;
  fixture(entity,0);D_800C7AB2=top && subtype>=2 ? 0x3e : 0x3f;
  U32(definition,4)=(unsigned int)subtype;U32(definition,8)=(unsigned int)radius;
  U32(definition,12)=(unsigned int)step;
  S16(source,0)=-45;S16(source,4)=-230;
  anchor_world_roster_begin(D_800C7AB2);anchor_world_roster_add(0,source,definition);
  anchor_world_roster_end(1);anchor_world_register(actor,source);anchor_world_post(actor);
  WorldActor *w=&s_actors[0];
  assert(w->variant==(top ? WORLD_TOP_VARIANT : WORLD_ROTOR_VARIANT));
  D_80236984_5F1E54[entity]=top ? top_model : rotor_model;
  U16(actor,0x5e)=(unsigned short)entity;
  U32(actor,0xd0)=(unsigned int)subtype;U32(actor,0xd4)=(unsigned int)radius;
  U32(actor,0xd8)=(unsigned int)step;F32(actor,0xdc)=-45;F32(actor,0xe0)=-230;
  U32(actor,0x60)=top ? 0x80000220u : 0x80000020u;U32(actor,0x64)=0x4000;
  for(int j=0;j<3;++j) F32(object,0x1c+j*4)=0.1f;
  U32(object,0x2c)=top ? 0x48000348u : 0x48000258u;
  U16(object,0x34)=top ? 0x1e9 : 0x1ea;U16(object,0x3c)=0x168;U16(object,0x44)=0x152;
  PTR(actor,0xc)=top ? func_080018E4_6EAA34 : func_08001CC4_6EAE14;
  anchor_world_static_model(actor,object,0);
  return w;
}
static void file40_checkpoint_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  const int params[][3]={{0,130,2},{0,80,4},{2,160,1},{3,160,1},{0,0,0},{1,0,0}};
  for(int i=0;i<6;++i) {
    int top=i<4;
    WorldActor *w=file40_fixture(top,params[i][0],params[i][1],params[i][2]);
    assert(file40_resident(w) && capture(0,r) && r[18]==(top ? 78 : 79));
    assert(!interacting(w));checkpoint(w);
    w->net[8]=1022;w->net[19]=top ? 256 : params[i][0];
    w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|81;
    tick();assert(native_calls==1 && w->receipt==(WORLD_BOOTSTRAP|81));
    assert(U16(object,0x16)==(top || params[i][0] ? (top ? 2 : 0) : 1020));
    if(top) {
      assert(U16(actor,0xe4)==256+params[i][2]);
      assert(fabsf(F32(object,params[i][0]<2 ? 8 : 16)-(params[i][0]<2 ? -45.f : -230.f))<0.001f);
    }
    tick();assert(native_calls==2); /* No rewind on an unchanged receipt. */
    if(top) assert(U16(actor,0xe4)==256+params[i][2]*2);
    assert(capture(0,r));memcpy(w->net,r,sizeof(r));w->net[38]=1;
    w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|82;
    tick();assert(native_calls==2 && U16(object,0x16)==r[8]);
    unsigned int flags=U32(actor,0x60);
    anchor_world_post(actor);assert(!(U32(actor,0x60)&0x800300u));
    anchor_world_post_return();assert(U32(actor,0x60)==flags && U32(actor,0x64)==0x4000);
    /* Guards run even for an already-applied checkpoint. */
    for(int f=0;f<4;++f) {
      missing_file=f==0 ? 40 : f==1 ? (top ? 0x1e9 : 0x1ea) : f==2 ? 0x168 : 0x152;
      assert(!apply(w));
    }
    missing_file=-1;
    U32(object,0x2c)++;assert(!apply(w));U32(object,0x2c)--;
    U32(actor,0xd0)++;assert(!apply(w));U32(actor,0xd0)--;
    if(top) {F32(actor,0xdc)++;assert(!apply(w));F32(actor,0xdc)--;}
    w->saved_ai=(void *)idle;assert(!apply(w));
    w->saved_ai=top ? (void *)func_080018E4_6EAA34 : (void *)func_08001CC4_6EAE14;
    w->net[38]=0;w->net[WORLD_RECEIPT]=WORLD_BOOTSTRAP|83;
    assert(apply(w) && capture(0,r));memcpy(w->retained,r,sizeof(r));w->retained_valid=1;
    anchor_world_register(actor,source);U16(object,0x16)=100;
    if(top) U16(actor,0xe4)=0;
    anchor_world_static_model(actor,object,0);anchor_world_post(actor);tick();
    assert(!w->restore_controller && native_calls==3);
    assert(U16(object,0x16)==((r[8]+(top ? 4 : params[i][0] ? 2 : -2))&1023));
    if(top) assert(U16(actor,0xe4)==((r[19]+params[i][2])&1023));
  }
}

static unsigned short file30_ca_files[] = {0x1b3, 0x157};
static unsigned int file30_ca_clips[] = {0x08000fd8u};
static void *file30_ca_model[] = {file30_ca_files, file30_ca_clips};
static unsigned short file30_339_files[] = {0,0,0,0,0,0,0,0,0x205,0x161};
static unsigned int file30_339_clips[] = {0,0,0,0,0x08001690u};
static void *file30_339_model[] = {file30_339_files, file30_339_clips};

static WorldActor *file30_appearance_fixture(unsigned int room,
                                               unsigned int index,
                                               unsigned int entity,
                                               int initially_visible) {
  WorldActor *w = fixture((unsigned short)entity, 0);
  unsigned int visual_model = entity == WORLD_FILE30_CA_ENTITY ? entity : 0x24f;
  unsigned int flags = entity == WORLD_FILE30_CA_ENTITY ? 0x80000020u :
                                                        0x802006e1u;
  (void)w;
  D_800C7AB2 = (unsigned short)room;
  anchor_world_roster_begin(room);
  anchor_world_roster_add(index, source, definition);
  anchor_world_roster_end(index + 1);
  U16(actor, 0x5e) = initially_visible ? visual_model : entity;
  U32(actor, 0x60) = initially_visible ? flags : 0;
  PTR(actor, 0xc) = entity == WORLD_FILE30_CA_ENTITY ?
                    func_08000970_6C00C0 : func_08004AE8_6C4238;
  anchor_world_register(actor, source);
  anchor_world_post(actor);
  D_80236984_5F1E54[WORLD_FILE30_CA_ENTITY] = file30_ca_model;
  D_80236984_5F1E54[0x24f] = file30_339_model;
  return &s_actors[index];
}

static void file30_appearance_test(void) {
  WorldActor *w;
  unsigned int control = 0xaced1234u;
  int calls;
  /* The constructor saw an unset flag, then save sync arrived later. */
  w = file30_appearance_fixture(WORLD_FILE30_CA_ROOM,
                                 WORLD_FILE30_CA_INDEX,
                                 WORLD_FILE30_CA_ENTITY, 1);
  assert(w->file30_appearance == 1 && !w->kind);
  U32(actor, 0xd0) = control;
  save_flags[WORLD_FILE30_CA_SAVE >> 3] |=
      1u << (WORLD_FILE30_CA_SAVE & 7);
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0 && U32(actor, 0xd0) == control);
  assert(PTR(actor, 0xc) == func_08000970_6C00C0 && !flag_writes);
  calls = anim_calls;
  anchor_world_frame();
  assert(anim_calls == calls);
  /* Clearing a flag restores the exact visible branch, only when its mesh
   * resources and clip still match the verified File 30 descriptor. */
  save_flags[WORLD_FILE30_CA_SAVE >> 3] = 0;
  missing_file = 0x1b3;
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0 && anim_calls == calls);
  missing_file = -1;
  file30_ca_clips[0] = 0;
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0 && anim_calls == calls);
  file30_ca_clips[0] = 0x08000fd8u;
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0x80000020u && anim_calls == calls + 1);
  assert(F32(object, 0x1c) == 1.1f && F32(object, 0x20) == 1.1f &&
         F32(object, 0x24) == 1.1f && U32(actor, 0xd0) == control);
  assert(PTR(actor, 0xc) == func_08000970_6C00C0 && !flag_writes);

  /* A 0x339 initially hidden by its constructor can become visible after
   * a delayed shared flag clears, with the constructor's pose and slot 4. */
  for (unsigned int n = 0; n < 2; ++n) {
    unsigned int room = n ? WORLD_FILE30_339_ROOM_B : WORLD_FILE30_339_ROOM_A;
    unsigned int index = n ? WORLD_FILE30_339_INDEX_B : WORLD_FILE30_339_INDEX_A;
    w = file30_appearance_fixture(room, index, WORLD_FILE30_339_ENTITY, 0);
    assert(w->file30_appearance == 2 && !w->kind);
    U32(actor, 0xd0) = control;
    file30_bound_slot = 0;
    calls = anim_calls;
    anchor_world_frame();
    assert(anim_calls == calls + 1 && file30_bound_slot == 4);
    assert(U16(actor, 0x5e) == 0x24f && U32(actor, 0x60) == 0x802006e1u);
    assert(U8(actor, 0x6c) == 2 && U16(object, 0x16) == 0x37f);
    assert(F32(object, 8) == -602.0f && F32(object, 0xc) == -84.0f &&
           F32(object, 0x10) == -48.0f && F32(object, 0x1c) == 1.0f);
    assert(U32(actor, 0xd0) == control &&
           PTR(actor, 0xc) == func_08004AE8_6C4238 && !flag_writes);
    save_flags[WORLD_FILE30_339_SAVE >> 3] |=
        1u << (WORLD_FILE30_339_SAVE & 7);
    anchor_world_frame();
    assert(U32(actor, 0x60) == 0 && anim_calls == calls + 1);
    save_flags[WORLD_FILE30_339_SAVE >> 3] = 0;
    anchor_world_frame();
    assert(U32(actor, 0x60) == 0x802006e1u &&
           anim_calls == calls + 2 && U32(actor, 0xd0) == control);
  }
  /* A different placed record, an active callback, or a reused pool slot
   * must not receive this narrow appearance restoration. */
  w = file30_appearance_fixture(WORLD_FILE30_CA_ROOM,
                                 WORLD_FILE30_CA_INDEX - 1,
                                 WORLD_FILE30_CA_ENTITY, 1);
  assert(!w->file30_appearance);
  save_flags[WORLD_FILE30_CA_SAVE >> 3] |=
      1u << (WORLD_FILE30_CA_SAVE & 7);
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0x80000020u);
  w = file30_appearance_fixture(WORLD_FILE30_CA_ROOM,
                                 WORLD_FILE30_CA_INDEX,
                                 WORLD_FILE30_CA_ENTITY, 1);
  U32(definition, 8) = 1;
  anchor_world_roster_begin(WORLD_FILE30_CA_ROOM);
  anchor_world_roster_add(WORLD_FILE30_CA_INDEX, source, definition);
  anchor_world_roster_end(WORLD_FILE30_CA_INDEX + 1);
  assert(!s_actors[WORLD_FILE30_CA_INDEX].file30_appearance);
  anchor_world_register(actor, source);
  anchor_world_post(actor);
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0x80000020u);
  w = file30_appearance_fixture(WORLD_FILE30_CA_ROOM,
                                 WORLD_FILE30_CA_INDEX,
                                 WORLD_FILE30_CA_ENTITY, 1);
  PTR(actor, 0xc) = idle;
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0x80000020u);
  PTR(actor, 0xc) = func_08000970_6C00C0;
  ++U8(actor, 0x74);
  anchor_world_frame();
  assert(U32(actor, 0x60) == 0x80000020u && !flag_writes);
}

int main(void) {
  file30_appearance_test();
  file40_checkpoint_test();
  rope_checkpoint_test();
  spike_checkpoint_test();
  counterweight_core_test();
  gate64_core_test();
  doll_core_test();
  shutter_checkpoint_test();
  /* File51 bridge: cohort classification, latch delivery and the bootstrap
   * override that lets an established route replace a fresh save-1 pose. */
  bridge_roster_test();
  bridge_cohort_rejection_test();
  bridge_scheduler_test();
  bridge_frame_bootstrap_test();
  /* Coupled-controller scheduling: owner echoes carry input only, while a
   * real remote bootstrap/rebuilt child requires one native application. */
  WorldActor *crane = fixture(WORLD_CRANE_ENTITY, 0);
  assert(!crane->kind); /* Its entity is not a crane controller in other rooms. */
  D_800C7AB2 = 0x31;
  anchor_world_roster_begin(0x31); anchor_world_roster_add(0,source,definition);
  anchor_world_roster_end(1); anchor_world_register(actor,source); anchor_world_post(actor);
  crane = &s_actors[0]; assert(crane->kind == WORLD_CRANE);
  memset(crane_capture_row,0,sizeof(crane_capture_row));
  crane_capture_row[10] = crane_capture_row[18] = crane_capture_row[19] = 1;
  crane_capture_row[22] = crane_capture_row[37] = 1;
  crane_capture_row[WORLD_CRANE_INPUT] = 1;
  crane_capture_ready = 1; crane_inputs = 1; crane_apply_calls = 0;
  int captured[ANCHOR_WORLD_WORDS]; assert(capture(0,captured));
  assert(anchor_world_row_valid(captured));
  memcpy(crane->net,captured,sizeof(captured));
  crane->have = 1; crane->owner = s_self; crane->net[WORLD_CRANE_AGGREGATE] = 3;
  tick(); assert(!crane_apply_calls && crane_control_inputs == 3 && !crane_control_remote);
  crane->owner = 1; crane->net[WORLD_RECEIPT] = WORLD_BOOTSTRAP | 1;
  tick(); assert(crane_apply_calls == 1 && crane->receipt == (WORLD_BOOTSTRAP | 1));
  crane->net[WORLD_CRANE_AGGREGATE] = 2;
  tick(); assert(crane_apply_calls == 1 && crane_control_inputs == 2);
  crane_restore = 1;
  tick(); assert(crane_apply_calls == 2 && !crane_restore);
  crane_capture_ready = 0;
  assert(capture(0,captured) && captured[38] && !captured[WORLD_CRANE_INPUT]);
  assert(crane->restore_controller);
  crane->have = 0; tick(); assert(crane_apply_calls == 3 && !crane->restore_controller);
  crane->actor = 0;
  assert(capture(0,captured) && !captured[WORLD_CRANE_INPUT] && !captured[WORLD_CRANE_AGGREGATE]);
  anchor_world_reset(); assert(!capture(0,captured));
  physics_checkpoint_test();
  physics_carry_and_retry_test();
  physics_completion_and_reward_test();
  physics_reconnect_after_break_test();
  bootstrap_receipt_test();
  linked_platform_checkpoint_test();
  linked_lifetime_and_local_fire_test();
  switch_checkpoint_test();
  puzzle_mechanism_checkpoint_test();
  controller_culling_checkpoint_test();
  save_aware_constructor_does_not_finish_remote_motion_test();
  npc_checkpoint_test();
  remote_travel_door_closes_and_reopens_test();
  npc_test();
  platform_test();
  mechanism_test();
  doors_and_walls_test();
  emitter_test();
  safety_test();
  lifecycle_test();
  puts("world native: NPC/dialogue, platform prediction/handoff, "
       "pickup/culling and pool reuse passed");
  return 0;
}
