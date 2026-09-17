/* Shared placed NPCs, native moving platforms, and ordinary pickups.
 * Identity comes from the guarded normal+partition actor roster, never the
 * allocation order. Native callbacks retain dialogue, collision and cleanup.
 */
#ifndef ANCHOR_WORLD_HOST_TEST
#include "anchor.h"
#include "anchor_dialog.h"
#include "item_sync.h"
#include "modding.h"
#include "recomputils.h"
#endif
#include "anchor_world.h"
#include "anchor_world_dynamic.h"
#include "anchor_world_paths.inc"

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC604_5B8514;
extern unsigned char *D_8015C5C8_15D1C8;
extern int func_80220F70_5DC440(void *);
extern void *D_80236984_5F1E54[];
extern float func_8001B5AC_1C1AC(void *);
extern float func_80003E10_4A10(unsigned short);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);

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
    X(func_08004FDC_6C472C)
/* clang-format on */
#define DECLARE(f) extern void f(void *, void *);
PLATFORM_STATES(DECLARE)
static Callback platform_states[45];
static void platform_addresses(void) {
  unsigned int i = 0;
  /* Overlay imports must be resolved by running code. Static relocations
   * to relocatable native sections cannot be packaged by RecompModTool. */
#define ADDRESS(f) platform_states[i++] = f;
  PLATFORM_STATES(ADDRESS)
#undef ADDRESS
}

typedef struct {
  void *source, *actor, *saved_ai;
  unsigned short entity;
  unsigned char kind, ready, generation, have;
  unsigned char has_path, talkable, variant, animated, door_local, local_motion;
  unsigned char door_remote, door_closing;
  unsigned int clip, owner, cycle;
  unsigned char emit_pending;
  int net[ANCHOR_WORLD_WORDS];
  int applied[ANCHOR_WORLD_WORDS];
  unsigned char applied_valid;
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

static void world_callback(void *, void *);
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
  s_active = 0;
  for (i = 0; i < s_count; ++i) {
    s_actors[i].have = 0;
    s_actors[i].applied_valid = 0;
  }
  for (i = 0; i < 32; ++i)
    s_dead[i] = 0;
  s_visit = 0;
}
static unsigned int mix(unsigned int h, unsigned int v) {
  return ((h ^ (v & 65535u)) * 257u + 17u) & 65535u;
}
void anchor_world_roster_begin(unsigned int room) {
  unsigned int i;
  unhold();
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
      s_actors[i].actor = actor;
      s_actors[i].ready = 0;
      s_actors[i].clip = 0;
      s_actors[i].applied_valid = 0;
      s_actors[i].emit_pending = 0;
      s_actors[i].door_local = 0;
      s_actors[i].door_remote = 0;
      s_actors[i].door_closing = 0;
      return;
    }
}
RECOMP_HOOK("func_80221A90_5DCF60")
void anchor_world_npc_init(void *actor) {
  int i = find(actor);
  if (i >= 0) {
    s_actors[i].kind = WORLD_NPC;
    s_actors[i].talkable = 1;
  }
}
RECOMP_HOOK("func_80226840_5E1D10")
void anchor_world_path_init(void *actor, unsigned short route) {
  int i = find(actor);
  if (i < 0 || route >= 163)
    return;
  /* The bird and dog constructors use paths without the talkable-NPC
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
  if (i < 0)
    return;
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
    /* Distance culling is NOT a pickup. Only the actual collection hook
     * below creates a tombstone. The native post owns physical removal. */
    w->actor = 0;
    w->saved_ai = 0;
    w->ready = 0;
  }
}
RECOMP_HOOK("func_80034ED4_35AD4")
void anchor_world_delete(void) {
  int i = find(D_8016DAB4_16E6B4);
  if (i >= 0) {
    s_actors[i].actor = 0;
    s_actors[i].saved_ai = 0;
    s_actors[i].ready = 0;
  }
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
  platform_addresses();
  for (i = 0; i < sizeof(platform_states) / sizeof(platform_states[0]); ++i)
    if (p == ((unsigned long)platform_states[i] & ~DISABLED))
      return (int)i + 1;
  return 0;
}
static int paused(void) {
  return anchor_dialog_world_paused() || !D_8015C5C8_15D1C8 ||
         (U16(D_8015C5C8_15D1C8, 0x3ae24) & 1u) ||
         U16(D_8015C5C8_15D1C8, 0x3ae26) != 0;
}
int anchor_world_is_paused(void) { return paused(); }
static int interacting(const WorldActor *w) {
  void *player_object;
  unsigned int p;
  if (!w->actor || !PTR(w->actor, 0x18))
    return 0;
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
  if (!w->kind || !valid(i) || (U32(a, 0x68) & 2u))
    return 0;
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    r[j] = 0;
  o = PTR(a, 0x18);
  r[0] = (int)i;
  r[1] = w->entity;
  r[2] = w->kind;
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
    r[18] = phase(PTR(a, 0x0c));
    if (!r[18])
      return 0; /* Native initializer has not reached a supported state. */
    if (w->variant < 14) {
      for (j = 0; j < 7; ++j)
        r[19 + j] = S16(a, 0xd0 + j * 2);
    } else if (w->variant == 14)
      r[19] = S16(a, 0xa0);
    else if (w->variant == 16)
      r[19] = S16(a, 0xe4);
    if (w->variant == 9)
      r[31] = (int)w->cycle;
  }
  if (w->kind == WORLD_EMITTER) {
    r[17] = S16(a, 0x8a);
    r[18] = (int)w->cycle;
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
  r[38] = paused();
  return anchor_world_row_valid(r);
}
static int changed(WorldActor *w) {
  unsigned int j;
  if (!w->applied_valid)
    return 1;
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    if (w->applied[j] != w->net[j])
      return 1;
  return 0;
}
static int platform_phase_valid(const WorldActor *w, int phase) {
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
static int apply(WorldActor *w) {
  void *a = w->actor, *o = PTR(a, 0x18);
  int *r = w->net;
  unsigned int j;
  if (!changed(w))
    return 1;
  if (w->kind == WORLD_DOOR && w->door_local)
    return 1;
  if (w->kind == WORLD_PLATFORM && interacting(w))
    return 1;
  if ((w->kind == WORLD_NPC || w->animated) &&
      !clip_valid(a, (unsigned int)r[10]))
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
    }
    w->saved_ai = (void *)platform_states[r[18] - 1];
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
  for (j = 0; j < 3; ++j) {
    F32(o, 8 + j * 4) = (float)r[4 + j] / 100.0f;
    U16(o, 0x14 + j * 2) = (unsigned short)r[7 + j];
    F32(a, 0x78 + j * 4) = (float)r[14 + j] / 1000.0f;
    F32(o, 0x1c + j * 4) = (float)r[26 + j] / 1000.0f;
  }
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
  U16(o, 0x7e) = (unsigned short)r[12];
  U8(o, 0x7c) = (unsigned char)((U8(o, 0x7c) & ~7u) | (unsigned int)r[13]);
  U32(a, 0x60) = (U32(a, 0x60) & ~0x20u) | ((r[29] & 1) ? 0x20u : 0u);
  if (w->kind == WORLD_DOOR) {
    U32(a, 0x60) = (U32(a, 0x60) & ~0x81000001u) |
                   ((r[29] & 4) ? 0x80000000u : 0u) |
                   ((r[29] & 2) ? 0x01000000u : 0u) |
                   ((r[29] & 16) ? 1u : 0u);
    if (travel_door(w)) {
      w->door_closing = (r[29] & 2) != 0;
      w->door_remote = !w->door_closing && (r[3] || r[11] > 0);
    }
  }
  for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
    w->applied[j] = r[j];
  w->applied_valid = 1;
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
  if (w->kind == WORLD_PICKUP && (s_dead[i >> 3] & (1u << (i & 7)))) {
    U32(actor, 0x68) |= 2u;
    return;
  }
  if (w->have && !apply(w))
    w->have = 0;
  real = (Callback)((unsigned long)w->saved_ai & ~DISABLED);
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
      if ((p >= 31 && p <= 35) || (p >= 37 && p <= 39) || p == 43 || p == 45)
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
  if (!s_active || s_room != D_800C7AB2 || paused())
    return;
  for (i = 0; i < s_count; ++i) {
    WorldActor *w = &s_actors[i];
    void *a = w->actor;
    if (!valid(i))
      continue;
    if (w->kind == WORLD_PICKUP) {
      if (!(s_dead[i >> 3] & (1u << (i & 7))))
        continue;
    } else if (!w->have &&
               !(w->kind == WORLD_DOOR && (w->door_remote || w->door_closing)))
      continue;
    w->saved_ai = PTR(a, 0x0c);
    PTR(a, 0x0c) = (void *)((unsigned long)world_callback |
                            ((unsigned long)w->saved_ai & DISABLED));
  }
}
RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_world_scheduler_end(void) { unhold(); }
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_world_frame(void) {
  unsigned int i, j, n = 0, received = 0;
  int active;
  char *reply;
  unhold();
  active = anchor_is_connected() && !anchor_is_disabled() &&
           item_sync_save_is_loaded() && s_signature && s_room == D_800C7AB2;
  if (!active) {
    anchor_world_dynamic_frame(0, 0, 0, 0);
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
          row[2] != w->kind)
        continue;
      for (j = 0; j < ANCHOR_WORLD_WORDS; ++j)
        w->net[j] = row[j];
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
}
