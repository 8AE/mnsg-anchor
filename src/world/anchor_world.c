/* Shared placed NPCs, native moving platforms, and ordinary pickups.
 * Identity comes from the guarded normal/partition/resident roster, never the
 * allocation order. Native callbacks retain dialogue, collision and cleanup.
 */
#ifndef ANCHOR_WORLD_HOST_TEST
#include "core/anchor.h"
#include "core/anchor_dialog.h"
#include "progression/item_sync.h"
#include "platform/modding.h"
#include "platform/recomputils.h"
#endif
#include "world/anchor_world.h"
#include "world/anchor_world_dynamic.h"
#include "world/anchor_world_quest.h"
#include "world/anchor_world_npc.h"
#include "world/anchor_world_crane.h"
#include "world/anchor_world_bridge.h"
#include "world/anchor_world_gate64.h"
#include "world/anchor_world_doll.h"
#include "world/anchor_world_counterweight.h"
#include "world/anchor_world_paths.inc"

static int quest_status_rows[WORLD_QUEST_MAX][WORLD_QUEST_WORDS];
static int quest_incoming_rows[WORLD_QUEST_MAX][WORLD_QUEST_WORDS];
static char quest_sources_json[WORLD_QUEST_JSON];
static char quest_status_json[WORLD_QUEST_JSON];

static void world_quest_frame(unsigned int room, unsigned int signature,
                              unsigned int visit, int active) {
  unsigned int count, status_count, received;
  const int (*rows)[WORLD_QUEST_WORDS];
  char *reply;
  if (!active) {
    anchor_world_quest_frame(0, 0, 0, 0);
    return;
  }
  anchor_world_quest_set_self(anchor_get_client_id());
  anchor_world_quest_frame(room, signature, visit, 1);
  count = anchor_world_quest_row_count();
  status_count = anchor_world_quest_status(quest_status_rows, WORLD_QUEST_MAX);
  if (count > WORLD_QUEST_MAX || status_count > WORLD_QUEST_MAX)
    return;
  rows = (const int (*)[WORLD_QUEST_WORDS])anchor_world_quest_rows();
  if (!anchor_world_quest_encode(rows, count, quest_sources_json,
                                  sizeof(quest_sources_json)) ||
      !anchor_world_quest_encode((const int (*)[WORLD_QUEST_WORDS])quest_status_rows,
                                  status_count, quest_status_json,
                                  sizeof(quest_status_json)))
    return;
  reply = anchor_update_world_quest(quest_sources_json, quest_status_json);
  if (reply && reply[0] &&
      anchor_world_quest_decode(reply, quest_incoming_rows, &received))
    anchor_world_quest_receive((const int (*)[WORLD_QUEST_WORDS])quest_incoming_rows,
                               received, anchor_get_client_id());
  if (reply)
    recomp_free(reply);
}

extern unsigned short D_800C7AB2;
extern unsigned int D_8015C5E4;
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned char *D_8015C5C8_15D1C8;
extern int func_80220F70_5DC440(void *);
extern void *D_80236984_5F1E54[];
extern float func_8001B5AC_1C1AC(void *);
extern float func_80003E10_4A10(unsigned short);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern int func_80023E94_24A94(int);
extern void func_80023DF0_249F0(int);
extern int func_800240DC_24CDC(int);
extern void func_80024038_24C38(int);
extern int func_800141C4_14DC4(unsigned int);
extern void *func_8021DF60_5D9430(void *, unsigned int, unsigned int,
                                unsigned int, unsigned int);
extern void *func_8021DDE8_5D92B8(void *, void (*)(void *, void *), unsigned char,
                                float, float, float, unsigned short);
extern void func_80214314_5CF7E4(void *, void *);
extern void func_802141AC_5CF67C(void *, void *);
extern void func_80216DF8_5D22C8(void *, unsigned int);
extern void func_80216E1C_5D22EC(void *, unsigned int);
extern void func_08000970_6C00C0(void *, void *);
extern void func_08004AE8_6C4238(void *, void *);
extern void func_8021A4E4_5D59B4(void *, float);
extern void func_80038B98_39798(unsigned int);
extern void func_801E55A0_5A14B0(void *);
extern void func_801DACDC_596BEC(void *, unsigned char);
extern void func_08000980_6ACED0(void *, void *);
extern void func_0800074C_6ACC9C(void *, void *);
extern void func_080018E4_6EAA34(void *, void *);
extern void func_08001CC4_6EAE14(void *, void *);
extern void anchor_race_boulder_room_reset(void);

#define U8(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define S16(p, o) (*(short *)((char *)(p) + (o)))
#define U16(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define U32(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define F32(p, o) (*(float *)((char *)(p) + (o)))
#ifndef PTR
#define PTR(p, o) (*(void **)((char *)(p) + (o)))
#endif
#ifndef DISABLED
#define DISABLED 0x00800000ul
#endif

typedef void (*Callback)(void *, void *);
/* File 44's platform state callbacks. Wire values are indices into this
 * allowlist, not addresses. Every entry is an actual native continuation. */
/* clang-format off */
#define PLATFORM_STATES(X) \
    X(func_08001368_6FC568) \
    X(func_080012DC_6FC4DC) \
    X(func_080012C8_6FC4C8) \
    X(func_08000E28_6FC028) \
    X(func_08000CCC_6FBECC) \
    X(func_08000C80_6FBE80) \
    X(func_08001224_6FC424) \
    X(func_0800119C_6FC39C) \
    X(func_080010A8_6FC2A8) \
    X(func_08000FFC_6FC1FC) \
    X(func_08000F74_6FC174) \
    X(func_08000E64_6FC064) \
    X(func_08000C10_6FBE10) \
    X(func_08000BB8_6FBDB8) \
    X(func_08000B34_6FBD34) \
    X(func_08000894_6FBA94) \
    X(func_080007C0_6FB9C0) \
    X(func_08000A48_6FBC48) \
    X(func_080009E4_6FBBE4) \
    X(func_08000980_6FBB80) \
    X(func_080008D4_6FBAD4) \
    X(func_08000784_6FB984) \
    X(func_08000714_6FB914) \
    X(func_080005D0_6FB7D0) \
    X(func_08000534_6FB734) \
    X(func_08000480_6FB680) \
    X(func_080003AC_6FB5AC) \
    X(func_08000ADC_6FBCDC) \
    X(func_08000A98_6FBC98) \
    X(func_0800091C_6FBB1C) \
    X(func_08005988_6C50D8) \
    X(func_08005A0C_6C515C) \
    X(func_08005BC0_6C5310) \
    X(func_08005C34_6C5384) \
    X(func_08005CA8_6C53F8) \
    X(func_0800204C_6FD24C) \
    X(func_08002180_6FD380) \
    X(func_0800220C_6FD40C) \
    X(func_08002298_6FD498) \
    X(func_080022E8_6FD4E8) \
    X(func_08001C70_6FCE70) \
    X(func_08000108_6BF858) \
    X(func_08000760_6BFEB0) \
    X(func_08004F74_6C46C4) \
    X(func_08004FDC_6C472C) \
    X(func_080054A4_6C4BF4) \
    X(func_0800551C_6C4C6C) \
    X(func_08006968_6C60B8) \
    X(func_080069C8_6C6118) \
    X(func_08006A38_6C6188) \
    X(func_08006AA8_6C61F8) \
    X(func_08006B00_6C6250) \
    X(func_08003418_6C2B68) \
    X(func_080036E0_6C2E30) \
    X(func_08003A74_6C31C4) \
    X(func_08003B48_6C3298) \
    X(func_08003C7C_6C33CC) \
    X(func_08003EE0_6C3630) \
    X(func_080040B8_6C3808) \
    X(func_08002600_6FD800) \
    X(func_080026D0_6FD8D0) \
    X(func_08002788_6FD988) \
    X(func_08002820_6FDA20) \
    X(func_08002E28_6FE028) \
    X(func_08003074_6FE274) \
    X(func_08003148_6FE348) \
    X(func_0800321C_6FE41C) \
    X(func_0800329C_6FE49C) \
    X(func_08006C18_6C6368) \
    X(func_08006F48_6C6698) \
    X(func_08007040_6C6790) \
    X(func_08007094_6C67E4) \
    X(func_080070E4_6C6834) \
    X(func_080072C4_6C6A14)
#define SWITCH_STATES(X) \
    X(func_08000204_70CA24) \
    X(func_08000298_70CAB8) \
    X(func_0800032C_70CB4C) \
    X(func_08000A8C_70D2AC) \
    X(func_080003AC_70CBCC)
/* clang-format on */
#define DECLARE(f) extern void f(void *, void *);
PLATFORM_STATES(DECLARE)
SWITCH_STATES(DECLARE)
#define SHUTTER_STATES(X) \
    X(func_08005624_6C4D74) \
    X(func_08005684_6C4DD4) \
    X(func_080056FC_6C4E4C) \
    X(func_080057D0_6C4F20)
SHUTTER_STATES(DECLARE)
static Callback shutter_states[4];
static int shutter_phase(void *callback) {
  unsigned int i = 0;
#define SHUTTER_ADDRESS(f) shutter_states[i++] = f;
  SHUTTER_STATES(SHUTTER_ADDRESS)
#undef SHUTTER_ADDRESS
  for (i = 0; i < 4; ++i)
    if (((unsigned long)callback & ~DISABLED) == (unsigned long)shutter_states[i])
      return (int)i + 1;
  return 0;
}
extern void func_08002DBC_6FDFBC(void *, void *);
extern void *func_802171A8_5D2678(void *, Callback, unsigned char);
static Callback platform_states[74], switch_states[5];
static void platform_addresses(void) {
  unsigned int i = 0;
  /* Overlay imports must be resolved by running code. Static relocations
   * to relocatable native sections cannot be packaged by RecompModTool. */
#define ADDRESS(f) platform_states[i++] = f;
  PLATFORM_STATES(ADDRESS)
#undef ADDRESS
  i = 0;
#define ADDRESS(f) switch_states[i++] = f;
  SWITCH_STATES(ADDRESS)
#undef ADDRESS
}

typedef struct {
  void *source, *actor, *saved_ai;
  unsigned short entity;
  unsigned char kind, ready, generation, have;
  unsigned char has_path, talkable, variant, animated, door_local, local_motion;
  unsigned char door_remote, door_closing, bridge_member;
  unsigned char switch_fx, switch_fx_pending;
  unsigned char initialized_complete;
  unsigned char file30_appearance;
  unsigned char file30_hidden_by_sync;
  short target[4];
  unsigned int definition_flags;
  unsigned char model_variant, start_sound;
  void *linked_child;
  unsigned char child_generation;
  unsigned int clip, owner, cycle;
  unsigned int instance, receipt;
  unsigned int physics_round;
  unsigned char physics_broken;
  unsigned int physics_post_flags;
  float physics_post_velocity[3];
  unsigned char emit_pending;
  int net[ANCHOR_WORLD_WORDS];
  int applied[ANCHOR_WORLD_WORDS];
  unsigned char applied_valid;
  int retained[ANCHOR_WORLD_WORDS];
  unsigned char retained_valid, restore_controller;
} WorldActor;
static WorldActor s_actors[ANCHOR_WORLD_MAX];
static unsigned int s_count, s_room = 0xffff, s_hash, s_signature, s_visit,
                             s_next_visit;
static unsigned int s_old_signature, s_old_room;
static unsigned char s_dead[32], s_previous_dead[32];
static unsigned char s_incoming_dead[32];
static int s_rows[ANCHOR_WORLD_MAX][ANCHOR_WORLD_WORDS];
static int s_incoming[ANCHOR_WORLD_MAX][ANCHOR_WORLD_WORDS + 2];
static char s_json[ANCHOR_WORLD_JSON];
static int s_active, s_applying;
static unsigned int s_self;
static unsigned int s_next_instance;

static void world_callback(void *, void *);
static int capture(unsigned int, int *);
static int phase(void *);
static void linked_detach(void *actor, int retire);
static int physics(const WorldActor *w) {
  return w->kind == WORLD_PLATFORM && w->variant == 26;
}
/* Exact flag values written by File_30's constructor and continuations. */
static const unsigned int physics_flags[] = {
    0x8ea007e1u, 0x02a007e1u, 0x08000020u, 0x0e800320u,
    0x8e800320u, 0x06a007e1u, 0x21u};
static void *s_physics_emitter;
static unsigned int s_physics_ordinal;
static int s_physics_post_index = -1;
int anchor_world_loot_ordinal(void *parent, unsigned int *ordinal) {
  if (!parent || parent != s_physics_emitter)
    return 0;
  *ordinal = s_physics_ordinal;
  return 1;
}
static int physics_held(const WorldActor *w) {
  void *work = D_801FC604_5B8514 ? PTR(D_801FC604_5B8514, 0x5c) : 0;
  return work && PTR(work, 0x8c) == w->actor;
}
static void physics_release(WorldActor *w) {
  if (physics_held(w)) {
    func_801E55A0_5A14B0(D_801FC604_5B8514);
    /* A carry continuation can dereference the held pointer on the next
     * throw. Leave that local action together with detaching the object. */
    func_801DACDC_596BEC(D_801FC604_5B8514, 0);
  }
  w->local_motion = 0;
}
static int flag_mechanism(const WorldActor *w) {
  return w->kind == WORLD_PLATFORM && (w->variant == 22 || w->variant == 23);
}
static int controller(const WorldActor *w) {
  return w->kind == WORLD_SWITCH || flag_mechanism(w);
}
static int linked_platform(const WorldActor *w) {
  return w->kind == WORLD_PLATFORM && (w->variant == 24 || w->variant == 25);
}
static int file40_rotor(const WorldActor *w) {
  return w->kind == WORLD_PLATFORM &&
         (w->variant == WORLD_TOP_VARIANT || w->variant == WORLD_ROTOR_VARIANT);
}
static int autonomous_platform(const WorldActor *w) {
  return w->kind == WORLD_PLATFORM &&
         (w->variant == WORLD_SPIKE_VARIANT || w->variant == WORLD_ROPE_VARIANT ||
          file40_rotor(w));
}
static int retained_actor(const WorldActor *w) {
  return controller(w) || linked_platform(w) || physics(w) ||
         autonomous_platform(w) ||
         w->kind == WORLD_CRANE || w->kind == WORLD_SHUTTER || w->kind == WORLD_BRIDGE ||
         w->kind == WORLD_GATE64 || w->kind == WORLD_DOLL_CONTAINER ||
         w->kind == WORLD_COUNTERWEIGHT;
}
static int controller_progress(const int *r) {
  if (r[2] == WORLD_COUNTERWEIGHT) return 0;
  if (r[2] == WORLD_PLATFORM && (r[1] == WORLD_SPIKE_ENTITY || r[1] == WORLD_ROPE_ENTITY ||
                               r[1] == WORLD_TOP_ENTITY || r[1] == WORLD_ROTOR_ENTITY))
    return 0; /* A repeated wait/open/retract cycle has no permanent progress. */
  if (r[2] == WORLD_GATE64) return r[WG64_PHASE] >= 3 ? r[WG64_PHASE] : 0;
  if (r[2] == WORLD_DOLL_CONTAINER) {
    /* The container cycle is repeatable, so a completed close must rank below
     * the next opening: phase 0 of a new cycle is one step past phase 3. */
    return r[WDC_CYCLE] * 4 + (r[WDC_PHASE] == 0 ? 3 : r[WDC_PHASE] - 1);
  }
  if (r[2] == WORLD_BRIDGE)
    return r[WB_FLAGS] * 16 + (r[WB_GUARD_0] & 3) + (r[WB_GUARD_1] & 3);
  if (r[2] == WORLD_SHUTTER)
    return r[20];
  if (r[2] == WORLD_CRANE)
    return 0; /* Its complete shared cycle can return to the initial phase. */
  if (r[2] == WORLD_PLATFORM && r[1] == 0x3d0) {
    /* Completion is permanent for this room visit, including when a lagging
     * participant has already broken/reloaded an older attempt. */
    if (r[18] >= 70 && r[18] <= 74)
      return 0x40000000 + (r[18] <= 72 ? r[18] - 69 :
                           r[18] == 73 ? 204 - r[17] : 205);
    return r[23] * 128 + (r[18] == WORLD_PHYSICS_BROKEN);
  }
  /* Activation is one-way; movement phases are cyclic. */
  if (r[2] == WORLD_PLATFORM && (r[1] == 0x228 || r[1] == 0x1fe))
    return r[23] * 2 + (r[1] == 0x228 && r[18] == 61);
  /* A save-aware constructor may start at the completed pose even while
   * another client is still animating the newly set flag. That fresh pose
   * outranks an idle copy, but not an observed move or real completion. */
  return r[18] * 2 - (r[1] == 0x326 && r[18] == 59 && r[24] ? 5 : 0);
}
static int valid(unsigned int i) {
  WorldActor *w = &s_actors[i];
  void *a = w->actor;
  return a && w->ready && U16(a, 0x5c) == w->entity &&
         U8(a, 0x74) == w->generation && PTR(a, 0x18);
}
static int find(void *actor) {
  unsigned int i;
  for (i = 0; i < s_count; ++i)
    if (s_actors[i].actor == actor)
      return (int)i;
  return -1;
}
static int travel_door(const WorldActor *w) {
  /* 0x23A is a progression barrier that can permanently disappear. The
   * other animated door families use local room-travel callbacks. */
  return w->kind == WORLD_DOOR && w->animated && w->entity != 0x23a;
}
int anchor_world_actor_placed(void *actor) { return find(actor) >= 0; }
int anchor_world_source_index(const void *source, unsigned int *placed_index) {
  unsigned int i;
  if (!source || s_room != D_800C7AB2) return 0;
  for (i = 0; i < s_count; ++i)
    if (s_actors[i].source == source) {
      if (placed_index) *placed_index = i + 1;
      return 1;
    }
  return 0;
}
int anchor_world_source_position(unsigned int placed_index, short out[3]) {
  const void *source;
  unsigned int j;
  if (!out || !placed_index || placed_index>s_count ||
      s_room!=D_800C7AB2 ||
      !(source=s_actors[placed_index-1].source)) return 0;
  for (j=0;j<3;++j) out[j]=S16(source,j*2);
  return 1;
}
int anchor_world_actor_authority(void *actor, unsigned int *placed_index) {
  int i = find(actor);
  if (i < 0)
    return -1;
  if (placed_index)
    *placed_index = (unsigned int)i + 1;
  if (!s_actors[i].kind)
    return -1;
  return !s_active || !s_actors[i].have || s_actors[i].owner == s_self;
}
static void unhold(void) {
  unsigned int i;
  for (i = 0; i < s_count; ++i) {
    WorldActor *w = &s_actors[i];
    void *a = w->actor;
    if (w->saved_ai && valid(i) &&
        ((unsigned long)PTR(a, 0x0c) & ~DISABLED) ==
            ((unsigned long)world_callback & ~DISABLED))
      PTR(a, 0x0c) = w->saved_ai;
    w->saved_ai = 0;
  }
}
void anchor_world_reset(void) {
  unsigned int i;
  unhold();
  anchor_world_crane_reset(0);
  anchor_world_bridge_reset(0);
  anchor_world_gate64_reset(0);
  anchor_world_doll_reset(0);
  anchor_world_counterweight_reset(0);
  s_active = 0;
  for (i = 0; i < s_count; ++i) {
    if (physics(&s_actors[i])) {
      if (valid(i) && !physics_held(&s_actors[i]) &&
          phase(PTR(s_actors[i].actor, 0xc)) == WORLD_PHYSICS_FIRST)
        U32(s_actors[i].actor, 0x68) &= ~0x88000u;
      s_actors[i].physics_round = 0;
      s_actors[i].physics_broken = 0;
      s_actors[i].local_motion = 0;
    }
    s_actors[i].have = 0;
    s_actors[i].applied_valid = 0;
    s_actors[i].receipt = 0;
    s_actors[i].retained_valid = s_actors[i].restore_controller = 0;
  }
  for (i = 0; i < 32; ++i)
    s_dead[i] = 0;
  s_visit = 0;
}
static unsigned int mix(unsigned int h, unsigned int v) {
  return ((h ^ (v & 65535u)) * 257u + 17u) & 65535u;
}
void anchor_world_roster_begin(unsigned int room) {
  anchor_race_boulder_room_reset();
  unsigned int i;
  unhold();
  anchor_world_crane_reset(1);
  anchor_world_bridge_reset(1);
  anchor_world_gate64_reset(1);
  anchor_world_doll_reset(1);
  anchor_world_counterweight_reset(1);
  if (s_room != room)
    anchor_world_dynamic_room();
  s_old_signature = s_signature;
  s_old_room = s_room;
  for (i = 0; i < 32; ++i) {
    s_previous_dead[i] = s_dead[i];
    s_dead[i] = 0;
  }
  /* Volatile byte stores avoid a compiler-generated libc memset import. */
  for (i = 0; i < sizeof(s_actors); ++i)
    ((volatile unsigned char *)s_actors)[i] = 0;
  s_room = room;
  s_count = 0;
  s_signature = 0;
  s_hash = mix(0x7931u, room);
}
void anchor_world_roster_add(unsigned int index, void *source,
                             const void *definition) {
  unsigned int j;
  WorldActor *w;
  if (index >= ANCHOR_WORLD_MAX || !source || !definition)
    return;
  w = &s_actors[index];
  w->source = source;
  w->entity = U16(definition, 0);
  if (!U32(definition, 4) && !U32(definition, 8) &&
      !U32(definition, 12)) {
    if (s_room == WORLD_FILE30_CA_ROOM && index == WORLD_FILE30_CA_INDEX &&
        w->entity == WORLD_FILE30_CA_ENTITY)
      w->file30_appearance = 1;
    else if (((s_room == WORLD_FILE30_339_ROOM_A &&
               index == WORLD_FILE30_339_INDEX_A) ||
              (s_room == WORLD_FILE30_339_ROOM_B &&
               index == WORLD_FILE30_339_INDEX_B)) &&
             w->entity == WORLD_FILE30_339_ENTITY)
      w->file30_appearance = 2;
  }
  if (s_room == WORLD_COUNTERWEIGHT_ROOM && w->entity == WORLD_COUNTERWEIGHT_ENTITY &&
      index >= 13 && index <= 15 && !U32(definition,4) &&
      !U32(definition,8) && !U32(definition,12))
    w->kind = WORLD_COUNTERWEIGHT;
  if (s_room == WORLD_GATE64_ROOM && w->entity == WORLD_GATE64_ENTITY &&
      !U32(definition,4) && !U32(definition,8) && !U32(definition,12))
    w->kind = WORLD_GATE64;
  /* File62's placed 0x3D6 container carries three known-zero parameter words;
   * its nested File_26 Doll is created by the dynamic-world API, not placed. */
  if ((s_room == WORLD_DOLL_ROOM_A || s_room == WORLD_DOLL_ROOM_B) &&
      w->entity == WORLD_DOLL_ENTITY && !U32(definition,4) &&
      !U32(definition,8) && !U32(definition,12))
    w->kind = WORLD_DOLL_CONTAINER;
  if (s_room == 0x31 && w->entity == WORLD_CRANE_ENTITY)
    w->kind = WORLD_CRANE;
  if (s_room == WORLD_BRIDGE_ROOM) {
    if (w->entity == WORLD_BRIDGE_ENTITY && !U32(definition,4) &&
        !U32(definition,8) && U32(definition,12) == 0x00010000u) {
      w->kind = WORLD_BRIDGE;w->bridge_member = 1;
    } else if (w->entity == 0x311 && !U32(definition,4) &&
               !U32(definition,8) && !U32(definition,12))
      w->bridge_member = 1;
    else if (w->entity == 0x2d0 && !U32(definition,4) &&
             ((U32(definition,8) == 0x10eu && !U32(definition,12)) ||
              (U32(definition,8) == 0x110u && U32(definition,12) == 0x00010000u))) {
      w->bridge_member = 1;w->variant = U32(definition,8) == 0x110u;
    }
  }
  if (s_room == 0xb2 && w->entity == WORLD_SHUTTER_ENTITY && U32(definition, 4) <= 1) {
    w->kind = WORLD_SHUTTER;
    w->variant = (unsigned char)U32(definition, 4);
  }
  if (w->entity == WORLD_SPIKE_ENTITY && U16(definition,4) <= 2 &&
      U16(definition,6) <= (U16(definition,4) == 2 ? 20 :
                            U16(definition,4) == 1 ? 23 : 24) &&
      !U32(definition,8) && !U32(definition,12)) {
    w->kind = WORLD_PLATFORM;w->variant = WORLD_SPIKE_VARIANT;
    w->target[0] = (short)U16(definition,4);
    w->target[1] = (short)U16(definition,6);
  }
  if (w->entity == WORLD_ROPE_ENTITY && U32(definition,4) == 10 &&
      !U32(definition,8) && !U32(definition,12)) {
    w->kind = WORLD_PLATFORM;w->variant = WORLD_ROPE_VARIANT;
    w->target[0] = 10;
  }
  if (w->entity == WORLD_TOP_ENTITY &&
      ((s_room == 0x3e && (U32(definition,4) == 2 || U32(definition,4) == 3) &&
        U32(definition,8) == 160 && U32(definition,12) == 1) ||
       (s_room == 0x3f && !U32(definition,4) &&
        ((U32(definition,8) == 130 && U32(definition,12) == 2) ||
         (U32(definition,8) == 80 && U32(definition,12) == 4))))) {
    w->kind = WORLD_PLATFORM;w->variant = WORLD_TOP_VARIANT;
    for (j=0;j<3;++j) w->target[j] = (short)U32(definition,4+j*4);
  }
  if (s_room == 0x3f && w->entity == WORLD_ROTOR_ENTITY && U32(definition,4) <= 1 &&
      !U32(definition,8) && !U32(definition,12)) {
    w->kind = WORLD_PLATFORM;w->variant = WORLD_ROTOR_VARIANT;
    w->target[0] = (short)U32(definition,4);
  }
  s_hash = mix(s_hash, index);
  for (j = 0; j < 12; j += 2)
    s_hash = mix(s_hash, U16(source, j));
  for (j = 0; j < 16; j += 2)
    s_hash = mix(s_hash, U16(definition, j));
  /* NPCs are discovered by their native initializer, including animals and
   * named NPCs; no enemy/boss ID range is treated as an NPC. */
  if (w->entity == 0x3e0) {
    w->variant = U8(definition, 4);
    if (w->variant == 11 && U8(definition, 5) > 1)
      w->variant = 12;
    if (w->variant <= 13)
      w->kind = WORLD_PLATFORM;
  }
  if (w->entity == 0x82 || w->entity == 0x83 || w->entity == 0x84 ||
      w->entity == 0x85 || w->entity == 0x192)
    w->kind = WORLD_PICKUP;
  if (w->entity == 0x19a && U8(definition, 8) <= 2)
    w->kind = WORLD_EMITTER;
  /* Other physical mechanisms have their own native continuation families.
   * Their pointer-bearing private work is never treated as rotor state. */
  if (w->entity == 0x197) {
    w->kind = WORLD_PLATFORM;
    w->variant = 14;
  }
  if (w->entity == 0x1fc) {
    w->kind = WORLD_PLATFORM;
    w->variant = 15;
  }
  if (w->entity == 0x1fd) {
    w->kind = WORLD_PLATFORM;
    w->variant = 16;
  }
  if (w->entity == 0x1f7) {
    w->kind = WORLD_PLATFORM;
    w->variant = 17;
  }
  if (w->entity == 0x245) {
    w->kind = WORLD_PLATFORM;
    w->variant = 18;
  }
  if (w->entity == 0x34a) {
    w->kind = WORLD_PLATFORM;
    w->variant = 19;
  }
  if (w->entity == 0x356) {
    w->kind = WORLD_PLATFORM;
    w->variant = 20;
  }
  if (w->entity == 0x3b4) {
    w->kind = WORLD_PLATFORM;
    w->variant = 21;
  }
  if (w->entity == 0x3d0) {
    w->kind = WORLD_PLATFORM;
    w->variant = 26;
  }
  if ((w->entity == 0x324 && U8(definition, 4) <= 11 && U16(definition, 8) < 800) ||
      (w->entity == 0x326 && U8(definition, 4) <= 7 && U16(definition, 8) < 2048)) {
    w->kind = WORLD_PLATFORM;
    w->variant = w->entity == 0x324 ? 22 : 23;
  }
  if ((w->entity == 0x228 && U8(definition, 13) <= 4) || w->entity == 0x1fe) {
    w->kind = WORLD_PLATFORM;
    w->variant = w->entity == 0x228 ? 24 : 25;
    for (j = 0; j < 4; ++j)
      w->target[j] = (short)(U16(definition, 4 + j * 2) - 0x8000);
    w->definition_flags = w->variant == 24 ? U8(definition, 12) : U32(definition, 12);
    w->model_variant = U8(definition, 13);
    w->start_sound = U8(definition, 14);
  }
  /* These are the two switch modes present in the placed US-ROM roster.
   * Their definition selects one latched room bit or one save bit. */
  if (w->entity == 0x226 && U8(definition, 8) <= 1 &&
      U16(definition, 4) < (U8(definition, 8) ? 2048 : 800)) {
    w->kind = WORLD_SWITCH;
    w->variant = U8(definition, 8);
  }
  if (w->entity == 0x23a || w->entity == 0x23c || w->entity == 0x23e ||
      w->entity == 0x23f || w->entity == 0x241 || w->entity == 0x242 ||
      w->entity == 0x24d || w->entity == 0x31f || w->entity == 0x321 ||
      w->entity == 0x32f)
    w->kind = WORLD_DOOR;
  if (w->entity == 0x23d && (U8(definition, 4) == 1 || U8(definition, 4) == 3 ||
                             U8(definition, 4) == 4))
    w->kind = WORLD_PICKUP;
}
void anchor_world_roster_end(unsigned int count) {
  unsigned int i;
  if (count > ANCHOR_WORLD_MAX)
    return;
  s_count = count;
  if (s_room == WORLD_BRIDGE_ROOM) {
    unsigned int members[4] = {0,0,0,0};
    for (i = 0; i < count; ++i) if (s_actors[i].bridge_member) {
      WorldActor *w = &s_actors[i];
      unsigned int member = w->entity == WORLD_BRIDGE_ENTITY ? 0 :
                            w->entity == 0x311 ? 3 : 1u+w->variant;
      ++members[member];
    }
    if (members[0] != 1 || members[1] != 1 || members[2] != 1 || members[3] != 1)
      for (i = 0; i < count; ++i) {
        s_actors[i].bridge_member = 0;
        if (s_actors[i].kind == WORLD_BRIDGE) s_actors[i].kind = 0;
      }
  }
  s_signature = mix(s_hash, count);
  if (!s_signature)
    s_signature = 1;
  if (s_room == s_old_room && s_signature == s_old_signature) {
    for (i = 0; i < 32; ++i)
      s_dead[i] = s_previous_dead[i];
  } else
    s_visit = 0;
  if (!s_visit) {
    if (++s_next_visit > 0x7fffffffu)
      s_next_visit = 1;
    s_visit = s_next_visit;
  }
  recomp_printf("[WorldSync] room=0x%X actors=%u signature=0x%X visit=%u\n",
                s_room, s_count, s_signature, s_visit);
}
RECOMP_HOOK("func_80218A54_5D3F24")
void anchor_world_register(void *actor, void *source) {
  unsigned int i;
  linked_detach(actor, 1);
  /* A freed pool slot can be reused by an unrelated native child. */
  for (i = 0; i < s_count; ++i)
    if (s_actors[i].actor == actor) {
      s_actors[i].actor = 0;
      s_actors[i].saved_ai = 0;
      s_actors[i].ready = 0;
    }
  if (s_room != D_800C7AB2 || !actor)
    return;
  for (i = 0; i < s_count; ++i)
    if (s_actors[i].source == source) {
      anchor_world_crane_register(actor, s_actors[i].entity);
      if (s_actors[i].bridge_member)
        anchor_world_bridge_register(actor, s_actors[i].entity, s_actors[i].variant);
      s_actors[i].actor = actor;
      s_actors[i].ready = 0;
      s_actors[i].clip = 0;
      s_actors[i].applied_valid = 0;
      s_actors[i].receipt = 0;
      if (++s_next_instance > 0x7fffffffu)
        s_next_instance = 1;
      s_actors[i].instance = s_next_instance;
      s_actors[i].emit_pending = 0;
      s_actors[i].door_local = 0;
      s_actors[i].door_remote = 0;
      s_actors[i].door_closing = 0;
      s_actors[i].switch_fx = s_actors[i].switch_fx_pending = 0;
      s_actors[i].restore_controller = 0;
      s_actors[i].initialized_complete = 0;
      s_actors[i].file30_hidden_by_sync = 0;
      s_actors[i].linked_child = 0;
      if (physics(&s_actors[i])) {
        WorldActor *w = &s_actors[i];
        w->local_motion = 0;
        w->have = 0;
        /* Native placement is a new live task. Only a retained checkpoint
         * may restore a break; a disconnected scope must not poison it. */
        w->physics_broken = 0;
        w->physics_round = w->retained_valid ? (unsigned int)w->retained[23] : 0;
        if (w->retained_valid && w->retained[18] == WORLD_PHYSICS_BROKEN &&
            w->physics_round < WORLD_PHYSICS_ROUND_MAX) {
          /* A fresh native placement after the proximity cooldown starts
           * the next shared attempt. A first-time entrant has no such receipt. */
          ++w->physics_round;
          w->physics_broken = 0;
          w->retained_valid = 0;
        }
        w->restore_controller = w->retained_valid;
      }
      if (s_actors[i].kind == WORLD_COUNTERWEIGHT) {
        anchor_world_counterweight_register(actor);
        s_actors[i].have = 0;s_actors[i].restore_controller = s_actors[i].retained_valid;
      }
      if (s_actors[i].kind == WORLD_GATE64) {
        anchor_world_gate64_register(actor);
        s_actors[i].have = 0;s_actors[i].restore_controller = s_actors[i].retained_valid;
      }
      if (s_actors[i].kind == WORLD_DOLL_CONTAINER) {
        /* The parent index is the one-based placed roster slot; the dynamic
         * Doll registration resolves its owner through this root. */
        anchor_world_doll_register(actor, i + 1);
        s_actors[i].have = 0;s_actors[i].restore_controller = s_actors[i].retained_valid;
      }
      if (linked_platform(&s_actors[i]) ||
          autonomous_platform(&s_actors[i]) ||
          s_actors[i].kind == WORLD_CRANE ||
          s_actors[i].kind == WORLD_SHUTTER || s_actors[i].kind == WORLD_BRIDGE) {
        s_actors[i].local_motion = 0;
        s_actors[i].have = 0;
        s_actors[i].restore_controller = s_actors[i].retained_valid;
      }
      return;
    }
}
RECOMP_HOOK("func_80221A90_5DCF60")
void anchor_world_npc_init(void *actor) {
  int i = find(actor);
  if (i >= 0 && !s_actors[i].bridge_member) {
    s_actors[i].kind = WORLD_NPC;
    s_actors[i].talkable = 1;
  }
}
RECOMP_HOOK("func_80226840_5E1D10")
void anchor_world_path_init(void *actor, unsigned short route) {
  int i = find(actor);
  if (i < 0 || route >= 163)
    return;
  /* The bird and pickpocket constructors use paths without the talkable-NPC
   * initializer. Benkei's combat actor (0x2BC) remains boss-owned. */
  if (s_actors[i].entity == 0x2c2 || s_actors[i].entity == 0x2c3 ||
      s_actors[i].entity == 0x2c4)
    s_actors[i].kind = WORLD_NPC;
  if (s_actors[i].kind == WORLD_NPC || s_actors[i].entity == 0x1f7)
    s_actors[i].has_path = 1;
}
RECOMP_HOOK("func_8021664C_5D1B1C")
void anchor_world_animation(void *actor, unsigned int clip) {
  int i = find(actor);
  if (i >= 0 && !s_applying && clip <= 255) {
    s_actors[i].clip = clip;
    s_actors[i].animated = 1;
  }
}
RECOMP_HOOK("func_80216CE0_5D21B0")
void anchor_world_static_model(void *actor, void *object, unsigned int clip) {
  int i = find(actor);
  if (i >= 0 && object == PTR(actor, 0x18) && !s_applying && clip <= 255) {
    s_actors[i].clip = clip;
    s_actors[i].animated = 0;
  }
}
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_post(void *actor) {
  int i = find(actor);
  WorldActor *w;
  s_physics_post_index = -1;
  if (i < 0) {
    if (actor && (U32(actor, 0x68) & 2u))
      linked_detach(actor, 0);
    return;
  }
  w = &s_actors[i];
  if (U16(actor, 0x5c) != w->entity)
    return;
  if (!w->ready) {
    w->generation = U8(actor, 0x74);
    w->ready = 1;
  }
  if (s_active && w->kind == WORLD_PICKUP &&
      (s_dead[i >> 3] & (1u << (i & 7)))) {
    /* The large-food actor owns a shine child. Retire that child through
     * its native flag; its full pickup cleanup would reset local controls. */
    if (w->entity == 0x85 && PTR(actor, 0xe4))
      U8(PTR(actor, 0xe4), 0xd0) = 1;
    U32(actor, 0x68) |= 2u;
  }
  if (U32(actor, 0x68) & 2u) {
    if (physics(w) && PTR(actor, 0x70) &&
        (!w->have || w->owner == s_self || w->local_motion))
      w->physics_broken = 1;
    if (retained_actor(w)) {
      int checkpoint[ANCHOR_WORLD_WORDS];
      capture((unsigned int)i, checkpoint);
    }
    linked_detach(actor, 1);
    /* Distance culling is NOT a pickup. Only the actual collection hook
     * below creates a tombstone. The native post owns physical removal. */
    w->actor = 0;
    w->saved_ai = 0;
    w->ready = 0;
  } else if ((physics(w) || w->kind == WORLD_SHUTTER ||
              autonomous_platform(w)) &&
             w->have && w->owner != s_self && w->net[38] &&
             !w->local_motion) {
    /* The common post still runs once for local presentation/contact. During
     * an owner pause, hold only its motion/geometry integration and restore
     * the checkpoint velocity afterward so a handoff can resume it. */
    s_physics_post_index = i;
    w->physics_post_flags = U32(actor, 0x60);
    U32(actor, 0x60) &= ~0x800300u;
    if (w->kind == WORLD_SHUTTER || autonomous_platform(w))
      U32(actor, 0x60) &= ~1u;
    for (unsigned int j = 0; j < 3; ++j) {
      w->physics_post_velocity[j] = F32(actor, 0x78 + j * 4);
      F32(actor, 0x78 + j * 4) = 0;
    }
  }
}
RECOMP_HOOK_RETURN("func_80218F30_5D4400")
void anchor_world_post_return(void) {
  int i = s_physics_post_index;
  s_physics_post_index = -1;
  if (i < 0 || !valid((unsigned int)i))
    return;
  WorldActor *w = &s_actors[i];
  U32(w->actor, 0x60) = w->physics_post_flags;
  for (unsigned int j = 0; j < 3; ++j)
    F32(w->actor, 0x78 + j * 4) = w->physics_post_velocity[j];
  w->physics_post_flags = 0;
}
RECOMP_HOOK("func_80034ED4_35AD4")
void anchor_world_delete(void) {
  int i = find(D_8016DAB4_16E6B4);
  if (i >= 0) {
    if (physics(&s_actors[i]) &&
        (!s_actors[i].have || s_actors[i].owner == s_self || s_actors[i].local_motion) &&
        (U32(D_8016DAB4_16E6B4, 0x68) & 2u) &&
        PTR(D_8016DAB4_16E6B4, 0x70))
      s_actors[i].physics_broken = 1;
    if (retained_actor(&s_actors[i])) {
      int checkpoint[ANCHOR_WORLD_WORDS];
      capture((unsigned int)i, checkpoint);
    }
    linked_detach(D_8016DAB4_16E6B4, 1);
    s_actors[i].actor = 0;
    s_actors[i].saved_ai = 0;
    s_actors[i].ready = 0;
  } else
    linked_detach(D_8016DAB4_16E6B4, 0);
}
static void collected(void *actor) {
  int i = find(actor);
  if (s_active && i >= 0 && s_actors[i].kind == WORLD_PICKUP) {
    s_dead[i >> 3] |= (unsigned char)(1u << (i & 7));
    /* A locally broken container owns the contents it is about to create. */
    s_actors[i].owner = s_self;
    s_actors[i].have = 1;
  }
}
RECOMP_HOOK("func_802145F0_5CFAC0")
void anchor_world_coin(void *actor) { collected(actor); }
RECOMP_HOOK("func_80214AEC_5CFFBC")
void anchor_world_health(void *actor) {
  if (actor && (U32(actor, 0x68) & 0x200u))
    collected(actor);
}
RECOMP_HOOK("func_08000434_6AEC14")
void anchor_world_food(void *actor) {
  if (actor && (U32(actor, 0x68) & 0x200u))
    collected(actor);
}
RECOMP_HOOK("func_08000150_6B30F0")
void anchor_world_container(void *actor) { collected(actor); }
static void *s_wall_actor;
RECOMP_HOOK("func_08003410_6F68F0")
void anchor_world_wall_begin(void *actor) { s_wall_actor = actor; }
RECOMP_HOOK_RETURN("func_08003410_6F68F0")
void anchor_world_wall_end(void) {
  void *actor = s_wall_actor;
  s_wall_actor = 0;
  if (actor && find(actor) >= 0 && (U32(actor, 0x68) & 2u))
    collected(actor);
}
RECOMP_HOOK("func_801FB240_5B7150")
void anchor_world_door_interaction(void) {
  int i = find(D_8016DAB4_16E6B4);
  if (i >= 0 && s_actors[i].kind == WORLD_DOOR) {
    WorldActor *w = &s_actors[i];
    if (travel_door(w) && (w->door_remote || w->door_closing)) {
      /* A local traveller can reopen a closing door from its current pose.
       * Only this native interaction may acquire player-control ownership. */
      U32(w->actor, 0x60) &= ~0x01000000u;
      U8(PTR(w->actor, 0x18), 0x7c) &= ~7u;
    }
    w->door_remote = w->door_closing = 0;
    w->door_local = 30;
  }
}
/* File 34 emitter callbacks fire exactly when their old countdown is zero.
 * Children carry independent live checkpoints; this counter identifies the
 * emitter phase without replaying its child constructors on replicas. */
static void emitter_cycle(void *actor) {
  int i = find(actor);
  if (!s_applying && s_active && i >= 0 && s_actors[i].kind == WORLD_EMITTER &&
      S16(actor, 0x8a) == 0) {
    if (++s_actors[i].cycle > 0x7fffffffu)
      s_actors[i].cycle = 1;
  }
}
RECOMP_HOOK("func_08000534_6D4874")
void anchor_world_emitter_rock(void *actor) { emitter_cycle(actor); }
RECOMP_HOOK("func_08000594_6D48D4")
void anchor_world_emitter_slow(void *actor) { emitter_cycle(actor); }
RECOMP_HOOK("func_08000628_6D4968")
void anchor_world_emitter_fast(void *actor) { emitter_cycle(actor); }
RECOMP_HOOK("func_080056FC_6C4E4C")
void anchor_world_shutter_emission(void *actor) {
  int i = find(actor);
  if (i >= 0 && s_actors[i].kind == WORLD_SHUTTER &&
      S16(actor, 0x8a) == 30 && U32(actor, 0xd0) == 1 &&
      s_actors[i].cycle < 0x7fffffffu)
    ++s_actors[i].cycle;
}
int anchor_world_shutter_birth(void *parent, unsigned int *index,
                               unsigned int *ordinal) {
  int i = find(parent);
  if (i < 0 || s_actors[i].kind != WORLD_SHUTTER ||
      !s_actors[i].cycle || U32(parent, 0xd0) != 1 || S16(parent, 0x8a) != 30)
    return 0;
  *index = (unsigned int)i + 1;
  *ordinal = s_actors[i].cycle;
  return 1;
}
RECOMP_HOOK("func_080007C0_6FB9C0")
void anchor_world_platform_emitter(void *actor) {
  int i = find(actor);
  if (!s_applying && s_active && i >= 0 && s_actors[i].kind == WORLD_PLATFORM &&
      s_actors[i].variant == 9 && ((U16(actor, 0xd8) + 8) & 1023) == 0) {
    s_actors[i].cycle = (s_actors[i].cycle + 1) & 32767u;
  }
}

static int phase(void *callback) {
  unsigned int i;
  unsigned long p = (unsigned long)callback & ~DISABLED;
  if (p == ((unsigned long)func_08000980_6ACED0 & ~DISABLED))
    return WORLD_SPIKE_PHASE;
  if (p == ((unsigned long)func_0800074C_6ACC9C & ~DISABLED))
    return WORLD_ROPE_PHASE;
  if (p == ((unsigned long)func_080018E4_6EAA34 & ~DISABLED))
    return WORLD_TOP_PHASE;
  if (p == ((unsigned long)func_08001CC4_6EAE14 & ~DISABLED))
    return WORLD_ROTOR_PHASE;
  platform_addresses();
  for (i = 0; i < sizeof(platform_states) / sizeof(platform_states[0]); ++i)
    if (p == ((unsigned long)platform_states[i] & ~DISABLED))
      return (int)i + 1;
  return 0;
}
static int switch_phase(void *callback) {
  unsigned int i;
  unsigned long p = (unsigned long)callback & ~DISABLED;
  platform_addresses();
  for (i = 0; i < 5; ++i)
    if (p == ((unsigned long)switch_states[i] & ~DISABLED))
      return (int)i + 1;
  return 0;
}
static int switch_trigger(const WorldActor *w) {
  return switch_phase(w->saved_ai ? w->saved_ai : PTR(w->actor, 0xc)) == 1 &&
         (U32(w->actor, 0x68) & 0x80u);
}
RECOMP_HOOK("func_08000A8C_70D2AC")
void anchor_world_switch_sparks(void *actor) {
  int i = find(actor);
  if (i >= 0 && s_actors[i].kind == WORLD_SWITCH)
    s_actors[i].switch_fx = 1;
}
RECOMP_HOOK("func_08003A8C_6C31DC")
void anchor_world_puzzle_constructor(void *actor) {
  int i = find(actor);
  if (i >= 0 && s_actors[i].entity == 0x326 && U16(actor, 0xd4) < 2048)
    s_actors[i].initialized_complete = func_800240DC_24CDC(U16(actor, 0xd4)) != 0;
}
static int paused(void) {
  return anchor_dialog_world_paused() || !D_8015C5C8_15D1C8 ||
         (U16(D_8015C5C8_15D1C8, 0x3ae24) & 1u) ||
         U16(D_8015C5C8_15D1C8, 0x3ae26) != 0;
}
int anchor_world_is_paused(void) { return paused(); }
static void *linked_child(WorldActor *w) {
  void *a = w->actor, *child;
  if (!a || w->variant != 25 || U16(a, 0x5c) != w->entity ||
      U8(a, 0x74) != w->generation)
    return 0;
  child = PTR(a, 0x9c);
  if (!child)
    return 0;
  if (U16(child, 0x5c) != 0x1fe || U16(child, 0x5e) != 0x1f9 ||
      !PTR(child, 0x18) || (U32(child, 0x68) & 2u) ||
      ((unsigned long)PTR(child, 0xc) & ~DISABLED) !=
          ((unsigned long)func_08002DBC_6FDFBC & ~DISABLED) ||
      (w->linked_child && (w->linked_child != child ||
                          U8(child, 0x74) != w->child_generation))) {
    PTR(a, 0x9c) = 0;
    w->linked_child = 0;
    return 0;
  }
  w->linked_child = child;
  w->child_generation = U8(child, 0x74);
  return child;
}
static void linked_detach(void *actor, int retire) {
  for (unsigned int i = 0; actor && i < s_count; ++i) {
    WorldActor *w = &s_actors[i];
    if (w->variant != 25 || !valid(i))
      continue;
    if (w->actor == actor) {
      void *child = linked_child(w);
      if (retire && child)
        U32(child, 0x68) |= 2u;
      PTR(actor, 0x9c) = 0;
      w->linked_child = 0;
    } else if (w->linked_child == actor || PTR(w->actor, 0x9c) == actor) {
      PTR(w->actor, 0x9c) = 0;
      w->linked_child = 0;
    }
  }
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_reuse(void *actor) {
  int i = find(actor);
  if (i >= 0 && retained_actor(&s_actors[i])) {
    int checkpoint[ANCHOR_WORLD_WORDS];
    capture((unsigned int)i, checkpoint);
  }
  linked_detach(actor, 1);
  if (i >= 0) {
    s_actors[i].actor = s_actors[i].saved_ai = 0;
    s_actors[i].ready = 0;
  }
}
static int linked_fire(WorldActor *w) {
  void *child = linked_child(w), *hitter;
  return child && S16(w->actor, 0xe2) == 0 &&
         (U32(child, 0x68) & 1u) && (hitter = PTR(child, 0xdc)) &&
         U8(hitter, 0x4c) == 0x1a;
}
RECOMP_HOOK("func_080028B4_6FDAB4")
void anchor_world_platform_fire(void *actor) {
  int i = find(actor);
  if (i >= 0 && s_actors[i].variant == 25 && linked_fire(&s_actors[i]))
    s_actors[i].local_motion = 1;
}
static int interacting(WorldActor *w) {
  void *player_object;
  unsigned int p;
  if (flag_mechanism(w) || autonomous_platform(w) ||
      (w->kind == WORLD_PLATFORM && w->variant == 24))
    return 0; /* Flag-driven mechanisms have no rider-controlled phase. */
  if (!w->actor || !PTR(w->actor, 0x18))
    return 0;
  if (physics(w)) {
    int p = phase(w->saved_ai ? w->saved_ai : PTR(w->actor, 0xc));
    if (w->physics_broken || p != WORLD_PHYSICS_FIRST) {
      w->local_motion = 0;
      return 0;
    }
    if (physics_held(w))
      w->local_motion = 1;
    else if (!U16(w->actor, 0xdc))
      w->local_motion = 0;
    return w->local_motion;
  }
  if (w->variant == 25) {
    int state = S16(w->actor, 0xe2);
    if (state == 0 || state == 5)
      w->local_motion = 0;
    return w->local_motion || linked_fire(w);
  }
  if (D_801FC604_5B8514 && PTR(D_801FC604_5B8514, 0xa0) == PTR(w->actor, 0x18))
    return 1;
  /* The elevator can be called from the other floor without standing on it.
   * Its local table is native immutable data, not imported private state. */
  if (w->variant == 15 && D_801FC604_5B8514 &&
      (player_object = PTR(D_801FC604_5B8514, 0x18)) && PTR(w->actor, 0xe0)) {
    void *table = PTR(w->actor, 0xe0), *o = PTR(w->actor, 0x18);
    p = (unsigned int)phase(w->saved_ai ? w->saved_ai : PTR(w->actor, 0xc));
    if (w->local_motion && p >= 37 && p <= 40)
      return 1;
    if (p == 36 && ((F32(o, 0xc) == F32(table, 20) &&
                     F32(player_object, 0xc) > S16(table, 0)) ||
                    (F32(o, 0xc) == F32(table, 8) &&
                     F32(player_object, 0xc) < S16(table, 0))))
      return 1;
  }
  return (w->variant == 17 || w->variant == 18) &&
         (U32(w->actor, 0x68) & 0x10000u);
}
static int scaled(float f, float scale, int lo, int hi, int *out) {
  /* Volatile bit access keeps this check intact under the mod's fast-math
   * flags, which otherwise allow the compiler to assume finite operands. */
  volatile union {
    float f;
    unsigned int bits;
  } input;
  input.f = f;
  if ((input.bits & 0x7f800000u) == 0x7f800000u)
    return 0;
  float v = f * scale;
  /* Ordered comparisons reject infinities/NaN before the integer cast. */
  if (!(v >= (float)lo && v <= (float)hi))
    return 0;
  *out = (int)v;
  return 1;
}
static int capture(unsigned int i, int *r) {
  WorldActor *w = &s_actors[i];
  void *a = w->actor, *o;
  unsigned int j;
  /* Completed File64 roots have been natively removed. Their typed durable
   * result must still publish and acknowledge without dereferencing a task. */
  if (w->kind == WORLD_GATE64) {
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) r[j] = 0;
    r[0] = (int)i;r[1] = w->entity;r[2] = w->kind;
    if (anchor_world_gate64_capture(a,r)) goto capture_complete;
  }
  if (w->kind == WORLD_COUNTERWEIGHT) {
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) r[j] = 0;
    r[0] = (int)i;r[1] = w->entity;r[2] = w->kind;
    if (anchor_world_counterweight_capture(a,r)) goto capture_complete;
    if (!w->retained_valid) return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) r[j] = w->retained[j];
    r[3] = 0;r[38] = 1;r[46] = r[47] = 0;
    r[WORLD_INSTANCE] = (int)w->instance;r[WORLD_RECEIPT] = (int)w->receipt;
    w->restore_controller = 1;return 1;
  }
  if (w->kind == WORLD_DOLL_CONTAINER) {
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) r[j] = 0;
    r[0] = (int)i;r[1] = w->entity;r[2] = w->kind;
    if (anchor_world_doll_capture(a,r)) goto capture_complete;
  }
  if (!w->kind || !valid(i) || ((U32(a, 0x68) & 2u) && !retained_actor(w))) {
    if (!retained_actor(w) || !w->retained_valid)
      return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
      r[j] = w->retained[j];
    r[38] = 1; /* Remember an unloaded controller without simulating it. */
    if (w->kind == WORLD_CRANE || w->kind == WORLD_BRIDGE) {
      r[WORLD_CRANE_INPUT] = r[WORLD_CRANE_AGGREGATE] = 0;
      if (w->kind == WORLD_BRIDGE) r[WB_INPUT] = (int)anchor_world_bridge_local_inputs();
      w->restore_controller = 1;
    }
    if (linked_platform(w) || physics(w))
      r[3] = 0;
    r[WORLD_INSTANCE] = (int)w->instance;
    r[WORLD_RECEIPT] = (int)w->receipt;
    return 1;
  }
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    r[j] = 0;
  o = PTR(a, 0x18);
  r[0] = (int)i;
  r[1] = w->entity;
  r[2] = w->kind;
  if (w->kind == WORLD_GATE64) {
    if (!w->retained_valid) return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) r[j] = w->retained[j];
    r[38] = 1;r[WORLD_INSTANCE] = (int)w->instance;r[WORLD_RECEIPT] = (int)w->receipt;
    w->restore_controller = 1;return 1;
  }
  if (w->kind == WORLD_DOLL_CONTAINER) {
    /* A culled container is remembered without a task: the cycle is durable
     * room fact, so an unloaded root still publishes its last row. */
    if (!w->retained_valid) return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) r[j] = w->retained[j];
    r[38] = 1;r[WORLD_INSTANCE] = (int)w->instance;r[WORLD_RECEIPT] = (int)w->receipt;
    w->restore_controller = 1;return 1;
  }
  if (w->kind == WORLD_CRANE || w->kind == WORLD_BRIDGE) {
    if (!(w->kind == WORLD_CRANE ? anchor_world_crane_capture(a, r) :
                                  anchor_world_bridge_capture(a, r))) {
      if (!w->retained_valid)
        return 0;
      for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
        r[j] = w->retained[j];
      r[38] = 1;
      r[WORLD_CRANE_INPUT] = r[WORLD_CRANE_AGGREGATE] = 0;
      if (w->kind == WORLD_BRIDGE) r[WB_INPUT] = (int)anchor_world_bridge_local_inputs();
      r[WORLD_INSTANCE] = (int)w->instance;
      r[WORLD_RECEIPT] = (int)w->receipt;
      w->restore_controller = 1;
      return 1;
    }
    goto capture_complete;
  }
  r[3] = w->kind == WORLD_NPC && (U32(a, 0x68) & 0x100u) != 0;
  for (j = 0; j < 3; ++j) {
    if (!scaled(F32(o, 8 + j * 4), 100, -3276800, 3276700, &r[4 + j]) ||
        !scaled(F32(a, 0x78 + j * 4), 1000, -100000, 100000, &r[14 + j]) ||
        !scaled(F32(o, 0x1c + j * 4), 1000, 0, 64000, &r[26 + j]))
      return 0;
    r[7 + j] = U16(o, 0x14 + j * 2) & 1023u;
  }
  r[10] = (int)w->clip;
  if (!scaled(F32(o, 0x28), 100, 0, 1000000, &r[11]))
    return 0;
  r[12] = U16(o, 0x7e);
  r[13] = U8(o, 0x7c) & 7u;
  r[29] = (U32(a, 0x60) & 0x20u) != 0;
  if (w->kind == WORLD_SHUTTER) {
    r[17] = S16(a, 0x8a);
    r[18] = shutter_phase(w->saved_ai ? w->saved_ai : PTR(a, 0xc));
    r[19] = w->variant;
    r[20] = (int)w->cycle;
    r[29] = (U32(a, 0x60) & 1u) | ((U32(a, 0x60) >> 23) & 2u);
    goto capture_complete;
  }
  if (w->kind == WORLD_DOOR) {
    r[3] = w->door_local != 0;
    r[29] |= (U32(a, 0x60) & 1u) ? 16 : 0;
    r[29] |= (U32(a, 0x60) & 0x80000000u) ? 4 : 0;
    r[29] |= (U32(a, 0x60) & 0x01000000u) ? 2 : 0;
    if (w->door_local) {
      if ((U32(a, 0x60) & 1u) ||
          (D_8015C5C8_15D1C8 && U8(D_8015C5C8_15D1C8, 0x3ae23)))
        w->door_local = 30;
      else
        --w->door_local;
    }
  }
  if (w->kind == WORLD_PLATFORM) {
    if (w->variant == 15 && phase(PTR(a, 0xc)) == 36)
      w->local_motion = 0;
    r[3] = interacting(w) != 0;
    r[29] |= (U32(a, 0x60) & 0x800000u) ? 2 : 0;
    r[29] |= (U32(a, 0x60) & 0x80000000u) ? 4 : 0;
    r[29] |= (U32(a, 0x60) & 0x100u) ? 8 : 0;
    for (j = 0; j < 3; ++j)
      r[34 + j] = S16(a, 0xc8 + j * 2);
    r[17] = S16(a, 0x8a);
    r[18] = phase(w->saved_ai &&
        ((unsigned long)PTR(a, 0xc) & ~DISABLED) == (unsigned long)world_callback
        ? w->saved_ai : PTR(a, 0xc));
    if (physics(w) && w->physics_broken)
      r[18] = WORLD_PHYSICS_BROKEN;
    if (!r[18])
      return 0; /* Native initializer has not reached a supported state. */
    if (w->variant == WORLD_SPIKE_VARIANT) {
      if (!w->animated || U8(a,0xd4) == 0) return 0;
      r[19] = S16(a,0xd0);r[20] = U8(a,0xd4);
      r[21] = U16(a,0xd2);r[22] = S16(a,0xd6);
      r[23] = (U32(a,0x60)&0x01000000u) != 0;
      r[29] |= (U32(a,0x60)&1u) ? 16 : 0;
      for (j = 34; j <= 36; ++j) r[j] = 0;
      goto capture_complete;
    }
    if (w->variant == WORLD_ROPE_VARIANT) {
      if (!w->animated || U32(a,0xd0) != 10) return 0;
      r[17] = 0;r[19] = S16(o,0x14);r[20] = 10;
      for (j = 34; j <= 36; ++j) r[j] = 0;
      goto capture_complete;
    }
    if (file40_rotor(w)) {
      if (w->animated || w->clip || U32(a,0xd0) != (unsigned int)w->target[0]) return 0;
      r[11]=r[12]=r[13]=r[17]=0; /* Static model: no animation clock. */
      r[19] = w->target[0];
      if (w->variant == WORLD_TOP_VARIANT) {
        if (U32(a,0xd4) != (unsigned int)w->target[1] ||
            U32(a,0xd8) != (unsigned int)w->target[2] ||
            F32(a,0xdc) != (float)S16(w->source,0) ||
            F32(a,0xe0) != (float)S16(w->source,4)) return 0;
        r[19]=U16(a,0xe4)&1023;r[20]=w->target[0];
        r[21]=w->target[1];r[22]=w->target[2];
        r[23]=S16(w->source,0);r[24]=S16(w->source,4);
      }
      for (j=34;j<=36;++j) r[j]=0;
      goto capture_complete;
    }
    if (flag_mechanism(w)) {
      r[19] = U8(a, 0xd0);
      r[20] = U16(a, 0xd4);
      if (r[19] > (w->variant == 22 ? 11 : 7) ||
          r[20] >= (w->variant == 22 ? 800 : 2048))
        return 0;
      r[21] = (w->variant == 22 ? func_80023E94_24A94(r[20])
                                : func_800240DC_24CDC(r[20])) != 0;
      r[24] = w->variant == 23 && w->initialized_complete;
      /* An idle late entrant cannot take ownership from a mechanism that
       * has already started, even when the entrant has a lower client ID. */
      r[3] |= r[18] != (w->variant == 22 ? 53 : 56);
    }
    if (linked_platform(w)) {
      for (j = 0; j < 4; ++j)
        r[19 + j] = w->target[j];
      r[23] = r[18] >= (w->variant == 24 ? 62 : 65);
      r[17] = 0;
      if (w->variant == 24) {
        r[24] = (int)w->definition_flags;
        r[25] = w->model_variant;
        r[31] = w->start_sound;
      } else {
        r[30] = (int)w->definition_flags;
        r[29] |= (U32(a, 0x60) & 1u) ? 16 : 0;
        if (r[23]) {
          void *child = linked_child(w);
          unsigned int rgba = U32(o, 0x8c);
          r[24] = S16(a, 0xe2);
          r[25] = r[18] == 67 ? U16(a, 0xee) : 0;
          /* Phase angle and travel direction are independent native values. */
          r[31] = (U16(a, 0xe0) & 1023u) | (S16(a, 0xec) ? 1024u : 0u);
          r[42] = U32(a, 0x60) & 0x240u;
          r[43] = (short)(rgba >> 16);
          r[44] = (short)rgba;
          if (r[24] >= 1 && r[24] <= 4) {
            r[17] = S16(a, 0x8a);
            if (!scaled(F32(a, 0xdc), 100, 0, 25500, &r[40]) ||
                !scaled(F32(a, 0xe4), 100, 0, 8000, &r[41]))
              return 0;
          }
          if (child) {
            r[33] = 1;
            r[32] = (U32(child, 0x60) & 0x600080u) != 0;
            r[37] = U8(child, 0x8d);
            for (j = 0; j < 3; ++j)
              if (!scaled(F32(child, 0xd0 + j * 4) - F32(o, 8 + j * 4),
                          100, -32768, 32767, &r[45 + j]))
                return 0;
          }
        }
      }
    }
    if (w->variant < 14) {
      for (j = 0; j < 7; ++j)
        r[19 + j] = S16(a, 0xd0 + j * 2);
    } else if (w->variant == 14)
      r[19] = S16(a, 0xa0);
    else if (w->variant == 16)
      r[19] = S16(a, 0xe4);
    else if (w->variant == 20 &&
             !scaled(F32(a, 0xdc), 1000, 1000, 8000, &r[19]))
      return 0;
    if (w->variant == 9)
      r[31] = (int)w->cycle;
    if (physics(w)) {
      unsigned int status = U32(a, 0x68), f = U32(a, 0x60);
      if (!scaled(F32(a, 0xd0), 1000, -32768, 32767, &r[19]) ||
          !scaled(F32(a, 0xd4), 1000, -32768, 32767, &r[20]))
        return 0;
      r[17] = r[18] == WORLD_PHYSICS_REWARD ? U16(a, 0x8a) : 0;
      r[21] = U16(a, 0xdc);
      r[22] = ((status >> 5) & 3u) | ((status >> 10) & 12u) |
              ((status >> 11) & 16u);
      r[23] = (int)w->physics_round;
      r[24] = U8(a, 0x8d);
      r[25] = U8(a, 0x8c);
      for (j = 0; j < sizeof(physics_flags) / sizeof(*physics_flags); ++j)
        if (physics_flags[j] == f)
          break;
      if (j == sizeof(physics_flags) / sizeof(*physics_flags))
        return 0;
      r[30] = (int)j;
      r[29] |= (f & 1u) ? 16 : 0;
      for (j = 34; j <= 36; ++j)
        r[j] = 0;
    }
  }
  if (w->kind == WORLD_EMITTER) {
    r[17] = S16(a, 0x8a);
    r[18] = (int)w->cycle;
  }
  if (w->kind == WORLD_SWITCH) {
    r[18] = switch_phase(PTR(a, 0xc));
    if (!r[18] || U8(a, 0xd4) != w->variant)
      return 0;
    r[19] = w->variant;
    r[20] = U16(a, 0xd0);
    if (r[20] >= (r[19] ? 2048 : 800))
      return 0;
    r[21] = (r[19] ? func_800240DC_24CDC(r[20]) : func_80023E94_24A94(r[20])) != 0;
    r[22] = U8(a, 0x6c);
    r[3] = r[18] > 1 || switch_trigger(w);
    r[29] |= (U32(a, 0x60) & 0x01000000u) ? 2 : 0;
    r[29] |= (U32(a, 0x60) & 1u) ? 16 : 0;
  }
  if (w->kind == WORLD_NPC || w->has_path) {
    r[30] = 163;
    if (w->has_path) {
      r[30] = U16(a, 0xc4);
      r[31] = S16(a, 0xc6);
      r[32] = U8(a, 0xce);
      r[33] = U8(a, 0xcf);
      for (j = 0; j < 3; ++j)
        r[34 + j] = S16(a, 0xc8 + j * 2);
      r[37] = U8(a, 0xaa);
    }
  }
  if (w->kind == WORLD_NPC) {
    if (!anchor_world_npc_capture(a, PTR(a, 0xc), r + WORLD_NPC_CHECKPOINT))
      return 0;
    if (r[WORLD_NPC_CHECKPOINT]) {
      r[17] = S16(a, 0x8a);
      r[19] = (U32(a, 0x60) >> 16) & 0x60u;
      r[20] = S16(a, 0x96);
      r[21] = U8(a, 0x75);
      r[22] = U32(a, 0x60) & 1u;
    }
  }
capture_complete:
  r[38] = paused();
  if ((w->kind == WORLD_CRANE || w->kind == WORLD_COUNTERWEIGHT) && r[38])
    r[WORLD_CRANE_INPUT] = 0;
  if (physics(w) && r[18] == WORLD_PHYSICS_FIRST && !D_8015C5E4)
    r[38] = 1;
  r[WORLD_INSTANCE] = (int)w->instance;
  r[WORLD_RECEIPT] = (int)w->receipt;
  if (!anchor_world_row_valid(r))
    return 0;
  if (retained_actor(w)) {
    if (w->retained_valid && (w->restore_controller ||
        controller_progress(r) < controller_progress(w->retained))) {
      for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
        r[j] = w->retained[j];
      r[38] = paused();
      if (w->kind == WORLD_CRANE) {
        r[WORLD_CRANE_INPUT] = r[38] ? 0 : (int)anchor_world_crane_local_inputs();
        r[WORLD_CRANE_AGGREGATE] = 0;
      }
      if (w->kind == WORLD_BRIDGE) {
        r[WB_INPUT] = (int)anchor_world_bridge_local_inputs();
        r[WB_AGGREGATE] = 0;
      }
      if (w->kind == WORLD_COUNTERWEIGHT) {
        r[46] = r[38] ? 0 : (int)anchor_world_counterweight_local_inputs(a);
        r[47] = 0;r[3] = r[46]!=0;
      }
      w->restore_controller = 1;
    } else {
      for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
        w->retained[j] = r[j];
      w->retained_valid = 1;
    }
  }
  r[WORLD_INSTANCE] = (int)w->instance;
  r[WORLD_RECEIPT] = (int)w->receipt;
  return 1;
}
static int changed(WorldActor *w) {
  unsigned int j;
  if (!w->applied_valid)
    return 1;
  for (j = 0; j < WORLD_INSTANCE; ++j)
    if ((w->kind != WORLD_CRANE && w->kind != WORLD_BRIDGE &&
         w->kind != WORLD_COUNTERWEIGHT) ||
        (j != WORLD_CRANE_INPUT && j != WORLD_CRANE_AGGREGATE))
    if (w->applied[j] != w->net[j])
      return 1;
  return 0;
}
static int platform_phase_valid(const WorldActor *w, int phase) {
  if (file40_rotor(w))
    return phase == (w->variant == WORLD_TOP_VARIANT ? WORLD_TOP_PHASE : WORLD_ROTOR_PHASE);
  if (w->variant == WORLD_SPIKE_VARIANT)
    return phase == WORLD_SPIKE_PHASE;
  if (w->variant == WORLD_ROPE_VARIANT)
    return phase == WORLD_ROPE_PHASE;
  if (physics(w))
    return phase >= WORLD_PHYSICS_FIRST && phase <= WORLD_PHYSICS_BROKEN;
  if (w->variant == 24)
    return phase >= 60 && phase <= 63;
  if (w->variant == 25)
    return phase >= 64 && phase <= 68;
  if (w->variant == 22)
    return phase >= 53 && phase <= 55;
  if (w->variant == 23)
    return phase >= 56 && phase <= 59;
  if (w->variant == 14)
    return phase >= 31 && phase <= 35;
  if (w->variant == 15)
    return phase >= 36 && phase <= 40;
  if (w->variant == 16)
    return phase == 41;
  if (w->variant == 17)
    return phase == 42;
  if (w->variant == 18)
    return phase == 43;
  if (w->variant == 19)
    return phase == 44 || phase == 45;
  if (w->variant == 20)
    return phase == 46 || phase == 47;
  if (w->variant == 21)
    return phase >= 48 && phase <= 52;
  static const unsigned char first[14] = {1, 4,  3,  7,  7,  10, 1,
                                          3, 13, 16, 18, 22, 24, 27};
  static const unsigned char last[14] = {2, 6,  3,  9,  9,  12, 2,
                                         3, 15, 17, 21, 23, 26, 27};
  if (w->variant == 8 && (phase == 28 || phase == 29))
    return 1;
  if (w->variant == 10 && phase == 30)
    return 1;
  return w->variant < 14 && phase >= first[w->variant] &&
         phase <= last[w->variant];
}
static int clip_valid(void *actor, unsigned int clip) {
  void **model;
  const unsigned int *clips;
  unsigned int i;
  /* Model identity is local native state. Native animation lists are
   * zero-terminated; never index them directly with a network clip. */
  if (clip > 255)
    return 0;
  model = (void **)D_80236984_5F1E54[U16(actor, 0x5e)];
  if (!model || !model[1])
    return 0;
  clips = (const unsigned int *)model[1];
  for (i = 0; i <= clip; ++i)
    if (!clips[i])
      return 0;
  return 1;
}
static int file24_resident(const WorldActor *w) {
  void **model;
  const unsigned short *files;
  void *a = w->actor;
  if (!a || U16(a,0x5e) != w->entity ||
      func_800141C4_14DC4(24) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  model = D_80236984_5F1E54[w->entity];
  if (!model || !model[0] || !model[1]) return 0;
  files = model[0];
  return func_800141C4_14DC4(files[0]) != -1 &&
         func_800141C4_14DC4(files[1]) != -1 &&
         clip_valid(a,w->variant == WORLD_SPIKE_VARIANT && w->target[0] == 1 ? 3 : 1);
}
static int file40_resident(const WorldActor *w) {
  void *a=w->actor, *o=a ? PTR(a,0x18) : 0;
  unsigned int file=w->variant == WORLD_TOP_VARIANT ? 0x1e9 : 0x1ea;
  unsigned int command=w->variant == WORLD_TOP_VARIANT ? 0x48000348u : 0x48000258u;
  return a && o && U16(a,0x5e)==w->entity && !w->animated && !w->clip &&
         func_800141C4_14DC4(40)!=-1 && func_800141C4_14DC4(file)!=-1 &&
         func_800141C4_14DC4(0x168)!=-1 && func_800141C4_14DC4(0x152)!=-1 &&
         U32(o,0x2c)==command && U16(o,0x34)==file &&
         U16(o,0x3c)==0x168 && U16(o,0x44)==0x152 && clip_valid(a,0);
}
/* The File 30 constructors skip their static bind once the shared save flag
 * is set. A flag arriving after construction needs only that presentation
 * branch; their idle continuations and local controls remain untouched. */
static int file30_appearance_resident(unsigned int variant) {
  unsigned int model_id = variant == 1 ? WORLD_FILE30_CA_ENTITY : 0x24f;
  unsigned int slot = variant == 1 ? 0 : 4;
  unsigned short *files;
  unsigned int *clips;
  void **model = (void **)D_80236984_5F1E54[model_id];
  if (!model || !model[0] || !model[1] ||
      func_800141C4_14DC4(30) == -1 ||
      func_800141C4_14DC4(0x152) == -1)
    return 0;
  files = (unsigned short *)model[0];
  clips = (unsigned int *)model[1];
  if (files[slot * 2] != (variant == 1 ? 0x1b3 : 0x205) ||
      files[slot * 2 + 1] != (variant == 1 ? 0x157 : 0x161) ||
      clips[slot] != (variant == 1 ? 0x08000fd8u : 0x08001690u))
    return 0;
  return func_800141C4_14DC4(files[slot * 2]) != -1 &&
         func_800141C4_14DC4(files[slot * 2 + 1]) != -1;
}
static void file30_appearance_frame(void) {
  unsigned int i;
  if (s_room != D_800C7AB2)
    return;
  for (i = 0; i < s_count; ++i) {
    WorldActor *w = &s_actors[i];
    void *a, *o;
    unsigned int visible_flags, visual_model, save_flag, saved;
    Callback idle;
    if (!w->file30_appearance || !valid(i))
      continue;
    a = w->actor;
    o = PTR(a, 0x18);
    if (U32(a, 0x68) & 2u)
      continue;
    idle = w->file30_appearance == 1 ? func_08000970_6C00C0 :
                                         func_08004AE8_6C4238;
    if (((unsigned long)PTR(a, 0xc) & ~DISABLED) != (unsigned long)idle)
      continue;
    visible_flags = w->file30_appearance == 1 ? 0x80000020u : 0x802006e1u;
    visual_model = w->file30_appearance == 1 ? WORLD_FILE30_CA_ENTITY : 0x24f;
    save_flag = w->file30_appearance == 1 ? WORLD_FILE30_CA_SAVE :
                                              WORLD_FILE30_339_SAVE;
    saved = func_800240DC_24CDC((int)save_flag) != 0;
    if (saved) {
      if (U32(a, 0x60) == visible_flags && U16(a, 0x5e) == visual_model) {
        U32(a, 0x60) = 0;
        w->file30_hidden_by_sync = 1;
      }
      continue;
    }
    if (U32(a, 0x60) != 0 ||
        (U16(a, 0x5e) != w->entity &&
         !(w->file30_hidden_by_sync && U16(a, 0x5e) == visual_model)) ||
        !file30_appearance_resident(w->file30_appearance))
      continue;
    if (w->file30_appearance == 1) {
      U32(a, 0x60) = visible_flags;
      func_80216DF8_5D22C8(a, 0);
      U8(a, 0x6c) = 0;
      F32(o, 0x1c) = F32(o, 0x20) = F32(o, 0x24) = 1.1f;
    } else {
      U16(a, 0x5e) = 0x24f;
      U32(a, 0x60) = visible_flags;
      func_80216E1C_5D22EC(a, 4);
      U8(a, 0x6c) = 2;
      F32(o, 0x1c) = F32(o, 0x20) = F32(o, 0x24) = 1.0f;
      U16(o, 0x16) = 0x37f;
      F32(o, 8) = -602.0f;
      F32(o, 0xc) = -84.0f;
      F32(o, 0x10) = -48.0f;
    }
    w->file30_hidden_by_sync = 0;
  }
}
static void spike_local_contact(void *a, void *o) {
  if (S16(a,0xd0) == 2 && D_801FC60C_5B851C) {
    float distance = F32(D_801FC60C_5B851C,0x10)-F32(o,0x10);
    if (distance < 0) distance = -distance;
    U32(a,0x60) = (U32(a,0x60)&~0xc0u) | (distance < 30 ? 0xc0u : 0u);
  }
}
static int linked_resident(const WorldActor *w) {
  static const unsigned short models[5] = {0x1f4, 0x1a0, 0x1f9, 0x3e2, 0x1f4};
  unsigned int id = w->variant == 25 ? 0x1f9 : models[w->model_variant];
  unsigned int highest = w->variant == 25 ? 3 : w->model_variant == 2 ? 2 : 0;
  void **model = (void **)D_80236984_5F1E54[id];
  const unsigned short *files;
  const unsigned int *clips;
  if (!model || !model[0] || !model[1] ||
      func_800141C4_14DC4(44) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  files = (const unsigned short *)model[0];
  clips = (const unsigned int *)model[1];
  if (func_800141C4_14DC4(files[0]) == -1 || func_800141C4_14DC4(files[1]) == -1)
    return 0;
  for (unsigned int j = 0; j <= highest; ++j)
    if (!clips[j])
      return 0;
  return 1;
}
static int physics_resident(WorldActor *w) {
  void **model = (void **)D_80236984_5F1E54[0x1c1];
  const unsigned short *files;
  const unsigned int *clips;
  if (U16(w->actor, 0x5e) != 0x1c1 || !model || !model[0] || !model[1] ||
      func_800141C4_14DC4(30) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  files = (const unsigned short *)model[0];
  clips = (const unsigned int *)model[1];
  return func_800141C4_14DC4(files[0]) != -1 &&
         func_800141C4_14DC4(files[1]) != -1 && clips[0] && clips[1] && clips[2];
}
static void physics_reward(WorldActor *w) {
  void *a = w->actor;
  unsigned int timer = U16(a, 0x8a);
  if (timer > 200)
    return;
  if (!(timer % 10)) {
    unsigned int slot = (200 - timer) / 10;
    /* The same native drop opportunity must choose the same item and launch
     * angle after a handoff. Do not consume or replace the local input RNG. */
    unsigned int seed = (s_signature ^ ((slot + 1) * 0x85ebca6bu)) + 0x6d2b79f5u;
    seed ^= seed >> 16;
    seed *= 0x7feb352du;
    seed ^= seed >> 15;
    unsigned short angle = (unsigned short)(seed & 1023u);
    Callback init = ((seed >> 10) % 100 < 80) ? func_80214314_5CF7E4
                                             : func_802141AC_5CF67C;
    func_80038B98_39798(0x23c);
    s_physics_emitter = a;
    /* Ordinary native drops use increasing per-kind ordinals. This producer
     * reserves the last 256 slots; completion happens once per room visit. */
    s_physics_ordinal = 0xff00u + slot;
    void *child = func_8021DDE8_5D92B8(a, init, 9, 0, 8, 0, angle);
    s_physics_emitter = 0;
    if (child) {
      F32(child, 0x7c) = 5;
      F32(child, 0x78) = (float)((double)func_80003E10_4A10(angle) * 0.9);
      F32(child, 0x80) = (float)((double)func_80003E10_4A10((angle + 256) & 1023) * 0.9);
    }
  }
  U16(a, 0x8a) = (unsigned short)(timer - 1);
  if (!timer) {
    platform_addresses();
    w->saved_ai = platform_states[WORLD_PHYSICS_DONE - 1];
  }
}
static void *linked_build_child(WorldActor *w) {
  void *a = w->actor, *o = PTR(a, 0x18), *child = linked_child(w), *co;
  if (child)
    return child;
  if (!linked_resident(w))
    return 0;
  child = func_802171A8_5D2678(a, func_08002DBC_6FDFBC, 2);
  if (!child)
    return 0;
  co = PTR(child, 0x18);
  if (!co) {
    U32(child, 0x68) |= 2u;
    return 0;
  }
  U16(child, 0x28) = 44;
  U32(child, 0x2c) = (unsigned int)func_800141C4_14DC4(44);
  U16(child, 0x5e) = 0x1f9;
  for (unsigned int j = 0; j < 3; ++j) {
    F32(co, 0x1c + j * 4) = F32(o, 0x1c);
    F32(child, 0xd0 + j * 4) = F32(o, 8 + j * 4);
  }
  PTR(a, 0x9c) = child;
  w->linked_child = child;
  w->child_generation = U8(child, 0x74);
  /* This callback only sets up the child's local collider and pose. */
  func_08002DBC_6FDFBC(child, co);
  return child;
}
static int linked_prepare(WorldActor *w) {
  void *a = w->actor;
  int *r = w->net, local_phase = phase(w->saved_ai ? w->saved_ai : PTR(a, 0xc));
  unsigned int j;
  if (!anchor_world_row_valid(r) || !platform_phase_valid(w, r[18]))
    return 0;
  for (j = 0; j < 4; ++j)
    if (r[19 + j] != w->target[j])
      return 0;
  if (w->variant == 24) {
    if ((unsigned int)r[24] != w->definition_flags || r[25] != w->model_variant ||
        r[31] != w->start_sound)
      return 0;
  } else if ((unsigned int)r[30] != w->definition_flags)
    return 0;
  if (!r[23]) {
    /* The gate has opened even if its model initializer has not run yet. */
    if (w->variant == 24 && r[18] == 61 && w->definition_flags &&
        !func_80023E94_24A94((int)w->definition_flags - 1))
      func_80023DF0_249F0((int)w->definition_flags - 1);
    return 1;
  }
  if (!linked_resident(w))
    return 0;
  if (local_phase < (w->variant == 24 ? 62 : 65)) {
    void *callback = PTR(a, 0xc);
    /* The native schedule helper writes the current task, not an argument. */
    if (D_8016DAB4_16E6B4 != a)
      return 0;
    if (w->variant == 24) {
      if (w->definition_flags)
        func_80023DF0_249F0((int)w->definition_flags - 1);
      func_080026D0_6FD8D0(a, PTR(a, 0x18));
    } else {
      if ((w->definition_flags & 2u) && !func_800240DC_24CDC(0x1a4))
        func_80024038_24C38(0x1a4);
      func_08002E28_6FE028(a, PTR(a, 0x18));
    }
    w->saved_ai = PTR(a, 0xc);
    PTR(a, 0xc) = callback;
  }
  for (j = 0; j < 4; ++j)
    S16(a, 0xd0 + j * 2) = w->target[j];
  if (w->variant == 25 && r[33] && !linked_build_child(w))
    return 0;
  return 1;
}
static int apply(WorldActor *w) {
  void *a = w->actor, *o = 0;
  int *r = w->net;
  unsigned int j;
  if ((unsigned int)r[WORLD_INSTANCE] != w->instance)
    return 0;
  if (w->kind == WORLD_COUNTERWEIGHT) {
    if (!anchor_world_row_valid(r)) return 0;
    int bootstrap = (r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                    w->receipt != (unsigned int)r[WORLD_RECEIPT];
    if ((changed(w) || bootstrap || w->restore_controller ||
         anchor_world_counterweight_needs_restore(a)) &&
        !anchor_world_counterweight_apply(a,r)) return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->applied[j] = r[j];
    w->applied_valid = 1;w->receipt = (unsigned int)r[WORLD_RECEIPT];return 1;
  }
  if (w->kind == WORLD_GATE64) {
    if (!anchor_world_row_valid(r)) return 0;
    int bootstrap = (r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                    w->receipt != (unsigned int)r[WORLD_RECEIPT];
    if ((changed(w) || bootstrap || w->restore_controller ||
         anchor_world_gate64_needs_restore()) && !anchor_world_gate64_apply(a,r)) return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->applied[j] = r[j];
    w->applied_valid = 1;w->receipt = (unsigned int)r[WORLD_RECEIPT];return 1;
  }
  if (w->kind == WORLD_DOLL_CONTAINER) {
    if (!anchor_world_row_valid(r)) return 0;
    int bootstrap = (r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                    w->receipt != (unsigned int)r[WORLD_RECEIPT];
    if ((changed(w) || bootstrap || w->restore_controller ||
         anchor_world_doll_needs_restore()) && !anchor_world_doll_apply(a,r)) return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->applied[j] = r[j];
    w->applied_valid = 1;w->receipt = (unsigned int)r[WORLD_RECEIPT];return 1;
  }
  o = PTR(a,0x18);
  if (w->kind == WORLD_BRIDGE) {
    if (!anchor_world_row_valid(r)) return 0;
    int bootstrap = (r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                    w->receipt != (unsigned int)r[WORLD_RECEIPT];
    if ((changed(w) || bootstrap || w->restore_controller ||
         anchor_world_bridge_needs_restore()) && !anchor_world_bridge_apply(a,r))
      return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->applied[j] = r[j];
    w->applied_valid = 1;w->receipt = (unsigned int)r[WORLD_RECEIPT];
    return 1;
  }
  if (w->kind == WORLD_CRANE) {
    if (!anchor_world_row_valid(r))
      return 0;
    int bootstrap = (r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                    w->receipt != (unsigned int)r[WORLD_RECEIPT];
    if ((changed(w) || bootstrap || w->restore_controller ||
         anchor_world_crane_needs_restore()) && !anchor_world_crane_apply(a, r))
      return 0;
    for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
      w->applied[j] = r[j];
    w->applied_valid = 1;
    w->receipt = (unsigned int)r[WORLD_RECEIPT];
    return 1;
  }
  if (w->kind == WORLD_PLATFORM && w->variant == WORLD_SPIKE_VARIANT) {
    /* Wait for this task's scheduled constructor to bind local resources.
     * A remote row cannot stand in for native initialization. */
    if (!anchor_world_row_valid(r) || !file24_resident(w) || !w->animated ||
        phase(w->saved_ai ? w->saved_ai : PTR(a,0xc)) != WORLD_SPIKE_PHASE ||
        (U8(a,0xd4) != 1 && U8(a,0xd4) != 3 && U8(a,0xd4) != 4) ||
        r[19] != w->target[0] || r[21] != w->target[1] ||
        S16(a,0xd0) != w->target[0] || U16(a,0xd2) != w->target[1])
      return 0;
  }
  if (w->kind == WORLD_PLATFORM && w->variant == WORLD_ROPE_VARIANT) {
    if (!anchor_world_row_valid(r) || !file24_resident(w) || !w->animated ||
        phase(w->saved_ai ? w->saved_ai : PTR(a,0xc)) != WORLD_ROPE_PHASE ||
        U32(a,0xd0) != 10 || r[20] != w->target[0]) return 0;
  }
  if (file40_rotor(w)) {
    if (!anchor_world_row_valid(r) || !file40_resident(w) ||
        !platform_phase_valid(w,phase(w->saved_ai ? w->saved_ai : PTR(a,0xc))) ||
        U32(a,0xd0)!=(unsigned int)w->target[0]) return 0;
    if (w->variant == WORLD_TOP_VARIANT) {
      if (r[20]!=w->target[0] || r[21]!=w->target[1] || r[22]!=w->target[2] ||
          r[23]!=S16(w->source,0) || r[24]!=S16(w->source,4) ||
          U32(a,0xd4)!=(unsigned int)w->target[1] ||
          U32(a,0xd8)!=(unsigned int)w->target[2] ||
          F32(a,0xdc)!=(float)r[23] || F32(a,0xe0)!=(float)r[24]) return 0;
    } else if (r[19]!=w->target[0]) return 0;
  }
  /* A new bootstrap token requires a real application even if the numeric
   * checkpoint matches a formerly applied row; local simulation may have moved. */
  if (!changed(w) && (!(r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) ||
                      w->receipt == (unsigned int)r[WORLD_RECEIPT])) {
    w->receipt = (unsigned int)r[WORLD_RECEIPT];
    return 1;
  }
  if (w->kind == WORLD_DOOR && w->door_local)
    return 1;
  if (physics(w)) {
    if (!anchor_world_row_valid(r) || !physics_resident(w))
      return 0;
    if (w->retained_valid && controller_progress(w->retained) > controller_progress(r))
      return 1;
    if (interacting(w)) {
      /* A local grab beats an idle remote pose, but simultaneous grabs have
       * one deterministic winner. Never leave a detached carry callback live. */
      if (r[18] == WORLD_PHYSICS_FIRST &&
          !r[3] && r[23] == (int)w->physics_round)
        return 1;
      physics_release(w);
    }
    w->physics_round = (unsigned int)r[23];
    w->physics_broken = r[18] == WORLD_PHYSICS_BROKEN;
    if (w->physics_broken) {
      if (!(U32(a, 0x68) & 2u) && w->applied_valid)
        func_8021A4E4_5D59B4(a, 1);
      U32(a, 0x68) |= 2u;
    }
    if (r[18] <= 70 && w->animated) {
      s_applying = 1;
      func_80216DF8_5D22C8(a, 0);
      s_applying = 0;
      w->clip = 0;
      w->animated = 0;
    } else if (r[18] >= 71 && r[18] <= 74)
      w->animated = 1;
  }
  if (w->kind == WORLD_PLATFORM && !physics(w) && !(r[WORLD_RECEIPT] & WORLD_BOOTSTRAP) && interacting(w))
    return 1;
  if (linked_platform(w)) {
    int local_phase = phase(w->saved_ai ? w->saved_ai : PTR(a, 0xc));
    if (!r[23] && local_phase >= (w->variant == 24 ? 62 : 65))
      return 1;
    if (!linked_prepare(w))
      return 0;
  }
  if (flag_mechanism(w)) {
    if (!anchor_world_row_valid(r))
      return 0;
    int local_phase = phase(w->saved_ai ? w->saved_ai : PTR(a, 0xc));
    int local_progress = local_phase * 2 -
                         (w->variant == 23 && local_phase == 59 && w->initialized_complete ? 5 : 0);
    if (local_progress > controller_progress(r))
      return 1;
  }
  if (w->kind == WORLD_SHUTTER) {
    void **model = (void **)D_80236984_5F1E54[0x23f];
    if (!anchor_world_row_valid(r) || r[19] != w->variant ||
        U32(a, 0xd0) != w->variant || U16(a, 0x5e) != 0x23f ||
        func_800141C4_14DC4(30) == -1 || func_800141C4_14DC4(0x152) == -1 ||
        !model || !model[0] ||
        func_800141C4_14DC4(((unsigned short *)model[0])[0]) == -1 ||
        func_800141C4_14DC4(((unsigned short *)model[0])[1]) == -1)
      return 0;
    /* Never re-enter an older emission attempt after culling or handoff. */
    if ((unsigned int)r[20] < w->cycle)
      return 0;
  }
  if (w->kind == WORLD_SWITCH) {
    int local_phase = switch_phase(w->saved_ai ? w->saved_ai : PTR(a, 0xc));
    if (!anchor_world_row_valid(r) || r[19] != w->variant ||
        r[19] != U8(a, 0xd4) || r[20] != U16(a, 0xd0))
      return 0;
    /* A fresh local hit and its one-way continuation must survive the
     * older idle checkpoint until the ownership advertisement arrives. */
    if (local_phase > r[18] || (r[18] == 1 && switch_trigger(w)))
      return 1;
  }
  if ((w->kind == WORLD_NPC || w->animated) &&
      !clip_valid(a, (unsigned int)r[10]))
    return 0;
  if (w->kind == WORLD_NPC &&
      (!anchor_world_npc_valid(w->entity, U16(a, 0x5e),
                               r + WORLD_NPC_CHECKPOINT) ||
       !anchor_world_npc_resident(r + WORLD_NPC_CHECKPOINT)))
    return 0;
  if (w->kind == WORLD_NPC && r[WORLD_NPC_CHECKPOINT] &&
      ((r[19] & ~0x60) || r[21] < 0 || r[21] > 255 || r[22] < 0 || r[22] > 1))
    return 0;
  if (w->kind == WORLD_NPC || w->has_path) {
    unsigned int route = w->has_path ? U16(a, 0xc4) : 163,
                 pc = (unsigned int)r[33];
    if (route != (unsigned int)r[30] || route > 163 ||
        (w->has_path && !(world_path_pc[route][pc >> 3] & (1u << (pc & 7)))))
      return 0;
    /* The native conversation owns its task/target pointers and script.
     * Never import another machine's dialogue callback or target. */
    if (U32(a, 0x68) & 0x100u)
      return 1;
    if (w->has_path) {
      S16(a, 0xc6) = (short)r[31];
      U8(a, 0xce) = (unsigned char)r[32];
      U8(a, 0xcf) = (unsigned char)pc;
      for (j = 0; j < 3; ++j)
        S16(a, 0xc8 + j * 2) = (short)r[34 + j];
      U8(a, 0xaa) = (unsigned char)r[37];
    }
  }
  if (w->kind == WORLD_NPC && r[WORLD_NPC_CHECKPOINT]) {
    S16(a, 0x8a) = (short)r[17];
    S16(a, 0x96) = (short)r[20];
    U8(a, 0x75) = (unsigned char)r[21];
    U32(a, 0x60) = (U32(a, 0x60) & ~0x00600001u) | ((unsigned int)r[19] << 16) |
                   (unsigned int)r[22];
    w->saved_ai = anchor_world_npc_restore(a, r + WORLD_NPC_CHECKPOINT, 0);
  }
  if (w->kind == WORLD_SHUTTER) {
    shutter_phase(0);
    w->saved_ai = (void *)((unsigned long)shutter_states[r[18] - 1] |
                           ((unsigned long)w->saved_ai & DISABLED));
    S16(a, 0x8a) = (short)r[17];
    w->cycle = (unsigned int)r[20];
  }
  if (w->kind == WORLD_PLATFORM) {
    if (!platform_phase_valid(w, r[18]))
      return 0;
    platform_addresses();
    /* Variant 1 divides by its immutable nonzero duration byte. */
    if (w->variant == 1 && ((r[19] & 255) == 0 || (r[19] & 255) != U8(a, 0xd1)))
      return 0;
    if (w->variant == 14 && r[19] != S16(a, 0xa0))
      return 0;
    if (w->variant == 16 && (r[19] < -50 || r[19] > 50))
      return 0;
    if (w->variant == 20 && (r[19] < 1000 || r[19] > 8000 || r[19] % 1000))
      return 0;
    if (flag_mechanism(w) &&
        (r[19] != U8(a, 0xd0) || r[20] != U16(a, 0xd4) ||
         r[19] < 0 || r[19] > (w->variant == 22 ? 11 : 7) ||
         r[20] < 0 || r[20] >= (w->variant == 22 ? 800 : 2048) ||
         r[21] < 0 || r[21] > 1))
      return 0;
    if (w->variant == 9) {
      if (r[31] < 0)
        return 0;
      if (w->applied_valid && (unsigned int)r[31] != w->cycle)
        w->emit_pending = 1;
      w->cycle = (unsigned int)r[31];
    }
    S16(a, 0x8a) = (short)r[17];
    if (w->variant < 14) {
      for (j = 0; j < 7; ++j)
        S16(a, 0xd0 + j * 2) = (short)r[19 + j];
    } else if (w->variant == 16) {
      S16(a, 0xe4) = (short)r[19];
    } else if (w->variant == 20) {
      F32(a, 0xdc) = (float)r[19] / 1000.0f;
    }
    if (flag_mechanism(w) && r[21]) {
      if (w->variant == 22)
        func_80023DF0_249F0(r[20]);
      else if (!func_800240DC_24CDC(r[20]))
        func_80024038_24C38(r[20]);
    }
    if (w->variant == 23)
      w->initialized_complete = (unsigned char)r[24];
    if (w->variant == WORLD_SPIKE_VARIANT) {
      U8(a,0xd4) = (unsigned char)r[20];
      S16(a,0xd6) = (short)r[22];
      U32(a,0x60) = (U32(a,0x60)&~0x01000001u) |
          (r[23] ? 0x01000000u : 0u) | ((r[29]&16) ? 1u : 0u);
      w->saved_ai = (void *)func_08000980_6ACED0;
    } else if (w->variant == WORLD_ROPE_VARIANT) {
      w->saved_ai = (void *)func_0800074C_6ACC9C;
      /* The native rope rotates its complete object. Its bound looping clip
       * stays at frame zero because native play is never enabled. */
      U32(a,0x60) &= ~0x01000001u;
    } else if (file40_rotor(w)) {
      if (w->variant == WORLD_TOP_VARIANT) {
        U16(a,0xe4)=(unsigned short)r[19];
        w->saved_ai=(void *)func_080018E4_6EAA34;
      } else w->saved_ai=(void *)func_08001CC4_6EAE14;
    } else
      w->saved_ai = (void *)platform_states[(r[18] == WORLD_PHYSICS_BROKEN ?
                                           WORLD_PHYSICS_FIRST : r[18]) - 1];
    for (j = 0; j < 3; ++j)
      S16(a, 0xc8 + j * 2) = (short)r[34 + j];
    U32(a, 0x60) = (U32(a, 0x60) & ~0x800000u) | ((r[29] & 2) ? 0x800000u : 0u);
    U32(a, 0x60) = (U32(a, 0x60) & ~0x80000100u) |
                   ((r[29] & 4) ? 0x80000000u : 0u) |
                   ((r[29] & 8) ? 0x100u : 0u);
  }
  if (w->kind == WORLD_EMITTER) {
    if (r[17] < 0 || r[17] > 150)
      return 0;
    if (w->applied_valid && (unsigned int)r[18] > w->cycle)
      w->emit_pending = 1;
    w->cycle = (unsigned int)r[18];
    S16(a, 0x8a) = (short)r[17];
  }
  if (w->kind == WORLD_SWITCH) {
    platform_addresses();
    if (r[21]) {
      if (!r[19])
        func_80023DF0_249F0(r[20]);
      else if (!func_800240DC_24CDC(r[20]))
        func_80024038_24C38(r[20]);
    }
    U8(a, 0x6c) = (unsigned char)r[22];
    PTR(a, 0xbc) = (void *)switch_states[4];
    w->saved_ai = (void *)switch_states[r[18] - 1];
    if (r[18] >= 4 && !w->switch_fx) {
      /* Show a witnessed press once. A late join at the settled state
       * restores the object without replaying old spark bursts. */
      w->switch_fx_pending = w->applied_valid;
      w->switch_fx = 1;
    }
    if (r[18] == 4)
      w->saved_ai = (void *)switch_states[4];
  }
  for (j = 0; j < 3; ++j) {
    F32(o, 8 + j * 4) = (float)r[4 + j] / 100.0f;
    U16(o, 0x14 + j * 2) = (unsigned short)r[7 + j];
    F32(a, 0x78 + j * 4) = (float)r[14 + j] / 1000.0f;
    F32(o, 0x1c + j * 4) = (float)r[26 + j] / 1000.0f;
  }
  /* The rope's native add wraps at 16 bits. Generic pose words use 10-bit
   * rendering angles, so retain the complete accumulator separately. */
  if (w->kind == WORLD_PLATFORM && w->variant == WORLD_ROPE_VARIANT)
    U16(o,0x14) = (unsigned short)r[19];
  if ((w->kind == WORLD_NPC || w->animated) && (unsigned int)r[10] != w->clip) {
    s_applying = 1;
    func_8021664C_5D1B1C(a, (unsigned int)r[10], (float)r[12] / 256.0f,
                         r[13] & 1);
    s_applying = 0;
    w->clip = (unsigned int)r[10];
  }
  if (w->kind == WORLD_NPC || w->animated) {
    float frame = (float)r[11] / 100.0f, limit = func_8001B5AC_1C1AC(o);
    if (limit > 0.0f && frame >= limit)
      frame = limit - 1.0f;
    if (frame < 0.0f)
      frame = 0.0f;
    F32(o, 0x28) = frame;
  }
  if (!file40_rotor(w)) {
    U16(o, 0x7e) = (unsigned short)r[12];
    U8(o, 0x7c) = (unsigned char)((U8(o, 0x7c) & ~7u) | (unsigned int)r[13]);
  }
  U32(a, 0x60) = (U32(a, 0x60) & ~0x20u) | ((r[29] & 1) ? 0x20u : 0u);
  if (physics(w)) {
    F32(a, 0xd0) = (float)r[19] / 1000;
    F32(a, 0xd4) = (float)r[20] / 1000;
    U16(a, 0xdc) = (unsigned short)r[21];
    U8(a, 0x8d) = (unsigned char)r[24];
    U8(a, 0x8c) = (unsigned char)r[25];
    U32(a, 0x60) = physics_flags[r[30]];
    U32(a, 0x68) = (U32(a, 0x68) & ~0x8b060u) |
        ((unsigned int)(r[22] & 3) << 5) | ((unsigned int)(r[22] & 12) << 10) |
        ((unsigned int)(r[22] & 16) << 11) |
        ((r[18] >= 70 || (r[22] & 16)) ? 0x80000u : 0);
  }
  if (w->kind == WORLD_SWITCH)
    U32(a, 0x60) = (U32(a, 0x60) & ~0x01000001u) |
                   ((r[29] & 2) ? 0x01000000u : 0u) | ((r[29] & 16) ? 1u : 0u);
  if (w->kind == WORLD_SHUTTER)
    U32(a, 0x60) = (U32(a, 0x60) & ~0x01000021u) | 0x20u |
                   ((r[29] & 2) ? 0x01000000u : 0u) | (r[29] & 1u);
  if (w->kind == WORLD_DOOR) {
    U32(a, 0x60) = (U32(a, 0x60) & ~0x81000001u) |
                   ((r[29] & 4) ? 0x80000000u : 0u) |
                   ((r[29] & 2) ? 0x01000000u : 0u) | ((r[29] & 16) ? 1u : 0u);
    if (travel_door(w)) {
      w->door_closing = (r[29] & 2) != 0;
      w->door_remote = !w->door_closing && (r[3] || r[11] > 0);
    }
  }
  if (w->kind == WORLD_PLATFORM && w->variant == 25 && r[23]) {
    void *child = linked_child(w);
    unsigned int rgba = ((unsigned int)(unsigned short)r[43] << 16) |
                        (unsigned short)r[44];
    U16(a, 0xe0) = (unsigned short)(r[31] & 1023);
    S16(a, 0xec) = (short)(r[31] >> 10);
    S16(a, 0xe2) = (short)r[24];
    U16(a, 0xee) = (unsigned short)r[25];
    F32(a, 0xdc) = (float)r[40] / 100.0f;
    F32(a, 0xe4) = (float)r[41] / 100.0f;
    U32(a, 0x60) = (U32(a, 0x60) & ~0x241u) | (unsigned int)r[42] |
                   ((r[29] & 16) ? 1u : 0u);
    PTR(o, 0x30) = func_8021DF60_5D9430(a, rgba & 255u, rgba >> 24,
                                      (rgba >> 16) & 255u, (rgba >> 8) & 255u);
    if (child && !r[33]) {
      U32(child, 0x68) |= 2u;
      PTR(a, 0x9c) = 0;
      w->linked_child = 0;
    }
    if (child && r[33]) {
      void *co = PTR(child, 0x18);
      U32(child, 0x60) = r[32] ? 0x600080u : 0;
      U8(child, 0x8d) = (unsigned char)r[37];
      for (j = 0; j < 3; ++j) {
        F32(child, 0xd0 + j * 4) = F32(o, 8 + j * 4) + (float)r[45 + j] / 100.0f;
        F32(co, 8 + j * 4) = F32(child, 0xd0 + j * 4);
        F32(co, 0x1c + j * 4) = F32(o, 0x1c);
      }
    }
  }
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    w->applied[j] = r[j];
  w->applied_valid = 1;
  w->receipt = (unsigned int)r[WORLD_RECEIPT];
  return 1;
}
static void door_close(WorldActor *w, void *object) {
  void *a = w->actor;
  if (!travel_door(w) || w->door_local)
    return;
  if (w->door_remote && (!w->have || !w->net[3])) {
    /* The traveller stopped advertising this interaction (including leaving
     * the room). Reverse the existing clip through the native common post;
     * never run a foreign travel/animation-complete callback. */
    w->door_remote = 0;
    w->door_closing = 1;
    U32(a, 0x60) |= 0x01000001u;
    U8(object, 0x7c) &= ~7u;
  }
  if (w->door_closing && F32(object, 0x28) <= 0.0f) {
    F32(object, 0x28) = 0;
    U32(a, 0x60) &= ~0x01000001u;
    U8(object, 0x7c) &= ~7u;
    w->door_closing = 0;
  }
}
static void world_callback(void *actor, void *object) {
  int i = find(actor);
  WorldActor *w;
  Callback real;
  if (i < 0)
    return;
  w = &s_actors[i];
  if (w->restore_controller) {
    for (unsigned int j = 0; j < ANCHOR_WORLD_WORDS; ++j)
      w->net[j] = w->retained[j];
    w->net[WORLD_INSTANCE] = (int)w->instance;
    w->net[WORLD_RECEIPT] = 0; /* Local retention is not a bridge receipt. */
    w->applied_valid = 0;
    if (apply(w))
      w->restore_controller = 0;
  }
  if (w->kind == WORLD_PICKUP && (s_dead[i >> 3] & (1u << (i & 7)))) {
    U32(actor, 0x68) |= 2u;
    return;
  }
  if (w->have && !apply(w)) {
    w->have = 0;
    w->receipt = WORLD_APPLY_FAILED;
  }
  real = (Callback)((unsigned long)w->saved_ai & ~DISABLED);
  if (autonomous_platform(w)) {
    if (w->variant == WORLD_SPIKE_VARIANT) spike_local_contact(actor,object);
    if (w->restore_controller || !(file40_rotor(w) ? file40_resident(w) : file24_resident(w)) ||
        (w->have && w->owner != s_self && w->net[38])) return;
    /* These callbacks advance only their own timing/rotation, local sound
     * or collision admission. They never dispatch contact, damage or rewards.
     * The common post retains that separate, single local contact pass. */
    if (real) real(actor,object);
    return;
  }
  if (w->kind == WORLD_SHUTTER) {
    /* The emitting continuation also allocates gameplay children. Only the
     * simulator runs the graph; peers advance the model in native post. */
    if (w->restore_controller || (w->have && w->owner != s_self))
      return;
    if (shutter_phase((void *)real) == 3 && S16(actor, 0x8a) == 30 &&
        w->variant && w->cycle == 0x7fffffffu) {
      --S16(actor, 0x8a); /* Exhaustion cannot reuse an existing birth key. */
      return;
    }
    if (real)
      real(actor, object);
    return;
  }
  if (physics(w)) {
    if (w->physics_broken)
      return;
    int p = phase((void *)real);
    int local = !w->have || w->owner == s_self || interacting(w);
    if (local) {
      if (!physics_held(w)) {
        U32(actor, 0x68) &= ~0x8000u;
        if (p == WORLD_PHYSICS_FIRST)
          U32(actor, 0x68) &= ~0x80000u;
      }
      if (p == WORLD_PHYSICS_REWARD)
        physics_reward(w);
      else if (real)
        real(actor, object);
    } else if (!w->net[38] && p >= 70 && p <= 72) {
      /* These three continuations only shrink, bind animation, and play
       * local sound. Rolling/contact and the reward producer stay authoritative. */
      if (physics_resident(w) && real)
        real(actor, object);
    }
    return;
  }
  if (w->kind == WORLD_SWITCH) {
    if (w->switch_fx_pending) {
      void *callback = PTR(actor, 0xc), *resume = PTR(actor, 0xbc);
      w->switch_fx_pending = 0;
      PTR(actor, 0xbc) = (void *)func_080003AC_70CBCC;
      func_08000A8C_70D2AC(actor, object);
      PTR(actor, 0xc) = callback;
      PTR(actor, 0xbc) = resume;
    }
    if (!w->have || w->owner == s_self ||
        switch_phase((void *)real) > w->net[18] ||
        (w->net[18] == 1 && switch_trigger(w))) {
      if (real)
        real(actor, object);
    }
    return;
  }
  if (w->kind == WORLD_DOOR) {
    /* Only presentation and mesh state travel. Native door callbacks own
     * local input, travel, fade/camera and player-control work independently.
     */
    door_close(w, object);
    unsigned int was_animating = U32(actor, 0x60) & 1u;
    if (real)
      real(actor, object);
    if (!was_animating && (U32(actor, 0x60) & 1u))
      w->door_local = 30;
    return;
  }
  if (!w->have || w->owner == s_self ||
      (w->kind == WORLD_PLATFORM && interacting(w))) {
    if (w->kind == WORLD_PLATFORM && w->variant == 15 && interacting(w))
      w->local_motion = 1;
    if (real)
      real(actor, object);
    return;
  }
  if (w->kind == WORLD_EMITTER) {
    /* Child snapshots reconstruct the current live set, including hazards
     * already in flight when a peer enters. Never replay the birth here. */
    w->emit_pending = 0;
    if (!w->net[38] && S16(actor, 0x8a) > 0)
      --S16(actor, 0x8a);
    return;
  }
  if (w->kind == WORLD_NPC) {
    if (w->talkable) {
      if (U32(actor, 0x68) & 0x100u) {
        if (real)
          real(actor, object);
      } else {
        PTR(actor, 0x0c) = (void *)real;
        func_80220F70_5DC440(actor);
        if (PTR(actor, 0x0c) == (void *)real)
          PTR(actor, 0x0c) = (void *)world_callback;
      }
    }
    if (!(U32(actor, 0x68) & 0x100u) && !w->net[38])
      anchor_world_npc_face(actor, w->net + WORLD_NPC_CHECKPOINT);
    if (w->net[3] || w->net[38])
      F32(actor, 0x78) = F32(actor, 0x7c) = F32(actor, 0x80) = 0;
    /* Native post advances animation/velocity and contact once. A replica
     * never executes somebody else's path-script side effects. */
    return;
  }
  if (w->kind == WORLD_PLATFORM && !w->net[38]) {
    int p = phase(w->saved_ai);
    /* Continuous local collision for the rotating footholds in the clip. */
    if (p == 1) {
      short timer = S16(actor, 0x8a);
      S16(actor, 0x8a) = timer - 1;
      if (timer == 0)
        w->saved_ai = (void *)platform_states[1];
    } else if (p == 2 || p == 3) {
      int step = p == 3 || S16(actor, 0xd4) == 0 ? -8 : 8;
      U16(object, 0x14) = (unsigned short)((U16(object, 0x14) + step) & 1023);
      if (p == 2 && !(U16(object, 0x14) & 511)) {
        S16(actor, 0x8a) = 100;
        w->saved_ai = (void *)platform_states[0];
      }
    } else if (p == 17) {
      w->emit_pending = 0;
      F32(object, 0xc) = func_80003E10_4A10(U16(actor, 0xd8)) * 20.0f - 70.0f;
      U16(actor, 0xd8) = (U16(actor, 0xd8) + 8) & 1023;
    } else if (p >= 31) {
      /* Falling traps retain their one native local-contact pass. The other
       * allowed continuations here only integrate verified mechanical motion.
       * Elevator triggers, tilting under a rider, and path VM actions belong
       * to the interacting owner; they are not replayed from a checkpoint. */
      if ((p >= 31 && p <= 35) || (p >= 37 && p <= 39) || p == 43 || p == 45 ||
          p == 46 || p == 47 || (p >= 49 && p <= 59) ||
          (p >= 62 && p <= 63) || (p >= 65 && p <= 68))
        if (real)
          real(actor, object);
    } else if (p != 24) {
      /* These verified continuations only move/rotate the platform,
       * update its own phase, and play local mechanical audio. They
       * perform no attack/contact dispatch, item grant or child spawn. */
      if (real)
        real(actor, object);
    }
  }
}
RECOMP_HOOK("func_80034734_35334")
void anchor_world_scheduler_begin(void) {
  unsigned int i;
  anchor_world_crane_control(0, 0, 0, 0);
  anchor_world_bridge_control(0, 0, 0, 0);
  anchor_world_gate64_control(0, 0, 0, 0);
  anchor_world_doll_control(0, 0, 0, 0);
  anchor_world_counterweight_control(0, 0, 0, 0, 0);
  if (!s_active || s_room != D_800C7AB2 || paused())
    return;
  for (i = 0; i < s_count; ++i) {
    WorldActor *w = &s_actors[i];
    void *a = w->actor;
    if (w->kind == WORLD_GATE64) {
      if (w->retained_valid && (w->restore_controller || anchor_world_gate64_needs_restore())) {
        for (unsigned int j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->net[j] = w->retained[j];
        w->net[WORLD_INSTANCE] = (int)w->instance;
        if (!w->have) w->net[WORLD_RECEIPT] = 0;
        w->applied_valid = 0;
        if (apply(w)) w->restore_controller = 0;
      }
      int bootstrap = (w->net[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                      w->receipt != (unsigned int)w->net[WORLD_RECEIPT];
      if (w->have && (w->owner != s_self || bootstrap) && !apply(w)) {
        w->have = 0;w->receipt = WORLD_APPLY_FAILED;
      }
      /* Both a remote offer and the transport's self-owner echo confirm a
       * simulator. Initial membership waiting deliberately withholds this. */
      anchor_world_gate64_control(a, w->have && w->owner != s_self,
                                  w->have && w->net[38],w->have);
      continue;
    }
    if (!valid(i))
      continue;
    if (w->kind == WORLD_COUNTERWEIGHT) {
      if (w->retained_valid &&
          (w->restore_controller || anchor_world_counterweight_needs_restore(a))) {
        for (unsigned int j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->net[j] = w->retained[j];
        w->net[WORLD_INSTANCE] = (int)w->instance;
        if (!w->have) w->net[WORLD_RECEIPT] = 0;
        w->applied_valid = 0;
        if (apply(w)) w->restore_controller = 0;
      }
      int bootstrap = (w->net[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                      w->receipt != (unsigned int)w->net[WORLD_RECEIPT];
      if (w->have && (w->owner != s_self || bootstrap) && !apply(w)) {
        w->have = 0;w->receipt = WORLD_APPLY_FAILED;
      }
      unsigned int inputs = w->have ? (unsigned int)w->net[47] : 0;
      anchor_world_counterweight_control(a, w->have && w->owner != s_self,
                                         w->have && w->net[38], w->have, inputs);
      continue;
    }
    if (w->kind == WORLD_BRIDGE) {
      if (w->retained_valid && (w->restore_controller || anchor_world_bridge_needs_restore())) {
        for (unsigned int j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->net[j] = w->retained[j];
        w->net[WORLD_INSTANCE] = (int)w->instance;
        if (!w->have) w->net[WORLD_RECEIPT] = 0;
        w->applied_valid = 0;
        if (apply(w)) w->restore_controller = 0;
      }
      int bootstrap = (w->net[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                      w->receipt != (unsigned int)w->net[WORLD_RECEIPT];
      if (w->have && (w->owner != s_self || bootstrap) && !apply(w)) {
        w->have = 0;w->receipt = WORLD_APPLY_FAILED;
      }
      unsigned int inputs = anchor_world_bridge_local_inputs();
      if (w->have) inputs = (unsigned int)w->net[WB_AGGREGATE];
      anchor_world_bridge_control(a, w->have && w->owner != s_self,
                                  w->have && w->net[38], inputs);
      continue;
    }
    if (w->kind == WORLD_CRANE) {
      if (w->retained_valid && (w->restore_controller || anchor_world_crane_needs_restore())) {
        for (unsigned int j = 0; j < ANCHOR_WORLD_WORDS; ++j)
          w->net[j] = w->retained[j];
        w->net[WORLD_INSTANCE] = (int)w->instance;
        /* Keep a bridge bootstrap receipt if this is the same incoming offer. */
        if (!w->have) w->net[WORLD_RECEIPT] = 0;
        w->applied_valid = 0;
        if (apply(w)) w->restore_controller = 0;
      }
      int bootstrap = (w->net[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                      w->receipt != (unsigned int)w->net[WORLD_RECEIPT];
      if (w->have && (w->owner != s_self || bootstrap) && !apply(w)) {
        w->have = 0;
        w->receipt = WORLD_APPLY_FAILED;
      }
      unsigned int inputs = anchor_world_crane_local_inputs();
      if (w->have) inputs = (unsigned int)w->net[WORLD_CRANE_AGGREGATE];
      anchor_world_crane_control(a, w->have && w->owner != s_self,
                                  w->have && w->net[38], inputs);
      continue;
    }
    if (w->kind == WORLD_DOLL_CONTAINER) {
      if (w->retained_valid &&
          (w->restore_controller || anchor_world_doll_needs_restore())) {
        for (unsigned int j = 0; j < ANCHOR_WORLD_WORDS; ++j) w->net[j] = w->retained[j];
        w->net[WORLD_INSTANCE] = (int)w->instance;
        if (!w->have) w->net[WORLD_RECEIPT] = 0;
        w->applied_valid = 0;
        if (apply(w)) w->restore_controller = 0;
      }
      int bootstrap = (w->net[WORLD_RECEIPT] & WORLD_BOOTSTRAP) &&
                      w->receipt != (unsigned int)w->net[WORLD_RECEIPT];
      if (w->have && (w->owner != s_self || bootstrap) && !apply(w)) {
        w->have = 0;w->receipt = WORLD_APPLY_FAILED;
      }
      /* Both a remote offer and the transport's self-owner echo confirm a
       * simulator; initial membership waiting withholds this. */
      anchor_world_doll_control(a, w->have && w->owner != s_self,
                                w->have && w->net[38], w->have);
      continue;
    }
    if (w->kind == WORLD_PICKUP) {
      if (!(s_dead[i >> 3] & (1u << (i & 7))))
        continue;
    } else if (!w->have && !w->restore_controller && !physics(w) &&
               !(w->kind == WORLD_DOOR && (w->door_remote || w->door_closing)))
      continue;
    w->saved_ai = PTR(a, 0x0c);
    PTR(a, 0x0c) = (void *)((unsigned long)world_callback |
                            ((unsigned long)w->saved_ai & DISABLED));
  }
  anchor_world_crane_begin();
  anchor_world_bridge_begin();
  anchor_world_gate64_begin();
  anchor_world_doll_begin();
  anchor_world_counterweight_begin();
}
RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_world_scheduler_end(void) {
  unhold();anchor_world_crane_end();anchor_world_bridge_end();
  anchor_world_counterweight_end();
}
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_world_frame(void) {
  unsigned int i, j, n = 0, received = 0;
  int active;
  char *reply;
  unhold();
  anchor_world_crane_end();
  active = anchor_is_connected() && !anchor_is_disabled() &&
           item_sync_save_is_loaded() && s_signature && s_room == D_800C7AB2;
  if (!active) {
    anchor_world_dynamic_frame(0, 0, 0, 0);
    world_quest_frame(0, 0, 0, 0);
    if (s_active) {
      reply = anchor_update_world(0, 0, 0, "{}");
      if (reply)
        recomp_free(reply);
      anchor_world_reset();
    }
    if (s_room != D_800C7AB2) {
      s_count = 0;
      s_signature = 0;
    }
    return;
  }
  file30_appearance_frame();
  s_active = 1;
  s_self = anchor_get_client_id();
  if (!s_visit) {
    if (++s_next_visit > 0x7fffffffu)
      s_next_visit = 1;
    s_visit = s_next_visit;
  }
  for (i = 0; i < s_count; ++i)
    if (capture(i, s_rows[n]))
      ++n;
  if (!anchor_world_encode(s_rows, n, s_dead, s_json, sizeof(s_json)))
    return;
  reply = anchor_update_world(s_room, s_signature, s_visit, s_json);
  if (reply &&
      anchor_world_decode(reply, s_incoming, &received, s_incoming_dead)) {
    for (i = 0; i < s_count; ++i)
      s_actors[i].have = 0;
    for (i = 0; i < received; ++i) {
      int *row = s_incoming[i] + 2;
      WorldActor *w = &s_actors[row[0]];
      if ((unsigned int)row[0] >= s_count || row[1] != w->entity ||
          row[2] != w->kind || (unsigned int)row[WORLD_INSTANCE] != w->instance)
        continue;
      if (retained_actor(w) && w->retained_valid &&
          controller_progress(row) < controller_progress(w->retained) &&
          !(w->kind == WORLD_BRIDGE && w->retained[WB_FRESH] &&
            (row[WORLD_RECEIPT] & WORLD_BOOTSTRAP)))
        continue;
      for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
        w->net[j] = row[j];
      if (retained_actor(w)) {
        for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
          w->retained[j] = row[j];
        w->retained_valid = 1;
      }
      w->owner = (unsigned int)s_incoming[i][0];
      w->have = 1;
    }
    for (i = 0; i < s_count; ++i)
      if (s_actors[i].kind == WORLD_PICKUP &&
          (s_incoming_dead[i >> 3] & (1u << (i & 7))))
        s_dead[i >> 3] |= (unsigned char)(1u << (i & 7));
  } else {
    /* A broken/stale bridge cannot leave native callbacks frozen. */
    for (i = 0; i < s_count; ++i)
      s_actors[i].have = 0;
  }
  if (reply)
    recomp_free(reply);
  anchor_world_dynamic_frame(s_room, s_signature, s_visit, 1);
  world_quest_frame(s_room, s_signature, s_visit, 1);
}
