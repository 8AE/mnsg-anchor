/* Native child lifecycle and portable reconstruction. Constructors are never
 * replayed on late entry: bind resident native assets and restore the current
 * scalar checkpoint instead. Collision/contact runs once in the stock post. */
#ifndef WORLD_DYNAMIC_HOST_TEST
#include "anchor.h"
#include "anchor_dialog.h"
#include "enemy_sync.h"
#include "item_sync.h"
#include "modding.h"
#include "recomputils.h"
#endif
#include "anchor_world_dynamic.h"
#include "anchor_world_paths.inc"

#define B(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define H(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define S(p, o) (*(short *)((char *)(p) + (o)))
#define W(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define F(p, o) (*(float *)((char *)(p) + (o)))
#ifndef DPTR
#define DPTR(p, o) (*(void **)((char *)(p) + (o)))
#endif
#ifndef DDISABLED
#define DDISABLED 0x00800000ul
#endif
typedef void (*DynamicCallback)(void *, void *);
extern void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
extern void *D_80236984_5F1E54[];
extern unsigned short D_800C7AB2;
extern void *func_802171A8_5D2678(void *, DynamicCallback, unsigned char);
extern int func_800141C4_14DC4(unsigned int);
extern int func_80220F70_5DC440(void *);
extern void func_802268A8_5E1D78(void *);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern void func_80216DF8_5D22C8(void *, unsigned int);
extern void func_80218DCC_5D429C(void *, unsigned int);
extern float func_8001B5AC_1C1AC(void *);
extern void func_802145F0_5CFAC0(void *);
extern void func_80213FF0_5CF4C0(void *);
extern int func_801DCD48_598C58(signed char);
extern void func_80038B98_39798(unsigned int);
extern void *func_8021804C_5D351C(void *, unsigned int);
extern void func_80219E70_5D5340(void *, unsigned char, unsigned char);
extern void func_80224ABC_5DFF8C(void *, int, float, int);

/* Exact native continuation addresses, resolved in code for overlay imports. */
/* clang-format off */
#define DYNAMIC_PHASES(X) \
  X(func_80214498_5CF968) \
  X(func_802148E0_5CFDB0) \
  X(func_80214054_5CF524) \
  X(func_08000434_6AEC14) \
  X(func_08000310_6D4650) \
  X(func_08000000_6D4340) \
  X(func_08000EE0_6D5220) \
  X(func_08000BC0_6D4F00) \
  X(func_08001318_6D5658) \
  X(func_0800107C_6D53BC) \
  X(func_080024F8_6FD6F8) \
  X(func_08002578_6FD778) \
  X(func_80214768_5CFC38) \
  X(func_80214AEC_5CFFBC)
/* clang-format on */
#define DDECLARE(f) extern void f(void *, void *);
DYNAMIC_PHASES(DDECLARE)
static DynamicCallback d_phases[14];
static void addresses(void) {
  unsigned int i = 0;
#define DADDRESS(f) d_phases[i++] = f;
  DYNAMIC_PHASES(DADDRESS)
#undef DADDRESS
}
typedef struct {
  void *actor, *saved, *native;
  unsigned int serial, generation, owner;
  unsigned char used, ready, eligible, proxy, have, dirty, claimed, granted;
  unsigned char talkable, path, animated, kind, clip, sphere, placed;
  unsigned short parent;
  int row[WORLD_DYNAMIC_WORDS];
  int net[WORLD_DYNAMIC_WORDS];
} DynamicActor;
static DynamicActor d_actors[WORLD_DYNAMIC_MAX];
static unsigned int d_loot_ordinals[257][3];
static int d_rows[WORLD_DYNAMIC_MAX][WORLD_DYNAMIC_WORDS];
static int d_incoming[WORLD_DYNAMIC_MAX][WORLD_DYNAMIC_WORDS];
static char d_json[WORLD_DYNAMIC_JSON];
static unsigned int d_serial, d_leader, d_self, d_room = 0xffff, d_visit;
static unsigned int d_tick;
static int d_active, d_building, d_binding;

static void dynamic_callback(void *, void *);
static void npc_native(void *, void *);
static void clear(void *p, unsigned int size) {
  unsigned int i;
  for (i = 0; i < size; ++i)
    ((volatile unsigned char *)p)[i] = 0;
}
static DynamicActor *lookup(void *a) {
  unsigned int i;
  if (!a)
    return 0;
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
    if (d_actors[i].used && d_actors[i].actor == a)
      return &d_actors[i];
  return 0;
}
static int alive(DynamicActor *d) {
  return d->actor && d->ready && DPTR(d->actor, 0x18) &&
         B(d->actor, 0x74) == d->generation &&
         H(d->actor, 0x5c) == d->row[WD_ENTITY];
}
static void restore(void) {
  unsigned int i;
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
    DynamicActor *d = &d_actors[i];
    if (d->saved && alive(d) &&
        ((unsigned long)DPTR(d->actor, 0xc) & ~DDISABLED) ==
            (unsigned long)dynamic_callback)
      DPTR(d->actor, 0xc) = d->saved;
    d->saved = 0;
  }
}
void anchor_world_dynamic_room(void) {
  restore();
  clear(d_actors, sizeof(d_actors));
  clear(d_loot_ordinals, sizeof(d_loot_ordinals));
  d_active = 0;
  d_leader = 0;
  d_visit = 0;
  d_room = 0xffff;
}
static int quantize(float value, float scale, int lo, int hi, int *out) {
  volatile union {
    float f;
    unsigned int u;
  } v;
  v.f = value;
  if ((v.u & 0x7f800000u) == 0x7f800000u)
    return 0;
  value *= scale;
  if (!(value >= (float)lo && value <= (float)hi))
    return 0;
  *out = (int)value;
  return 1;
}
static DynamicActor *allocate(void *a) {
  unsigned int i, j;
  DynamicActor *d = lookup(a);
  if (d)
    return d;
  /* Unclassified children include short-lived effects. They must not occupy
   * every tracking slot and prevent a supported actor from registering. This
   * only discards bookkeeping; it never destroys an unknown native task. */
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
    if (!d_actors[i].used)
      break;
  if (i == WORLD_DYNAMIC_MAX) {
    for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
      if (!d_actors[i].kind && !d_actors[i].ready) {
        clear(&d_actors[i], sizeof(d_actors[i]));
        break;
      }
  }
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
    if (!d_actors[i].used) {
      d = &d_actors[i];
      clear(d, sizeof(*d));
      d->used = 1;
      d->actor = a;
      if (++d_serial > 0x7fffffffu)
        d_serial = 1;
      d->serial = d_serial;
      d->row[WD_SERIAL] = (int)d_serial;
      d->row[WD_ROUTE] = 163;
      d->eligible = !d_active || !d_leader || d_leader == d_self;
      if (a && DPTR(a, 0x18))
        for (j = 0; j < 3; ++j)
          quantize(F(DPTR(a, 0x18), 8 + j * 4), 100, -3276800, 3276700,
                   &d->row[WD_BIRTH_X + j]);
      return d;
    }
  return 0;
}
/* Registration precedes a child's initializer, which may inherit its parent's
 * entity number. Classify by real continuation/setup, never by that number. */
RECOMP_HOOK("func_80218C28_5D40F8")
void world_dynamic_child(void *parent, void *child) {
  DynamicActor *d, *p;
  unsigned int index = 0;
  int authority, enemy;
  if (d_building || !child)
    return;
  d = allocate(child);
  if (!d)
    return;
  authority = anchor_world_actor_authority(parent, &index);
  enemy = enemy_sync_actor_authority(parent);
  if (enemy >= 0)
    authority = enemy;
  p = lookup(parent);
  if (p && p->kind)
    authority = p->eligible && (!p->have || p->owner == d_self);
  d->parent = (unsigned short)index;
  if (authority >= 0)
    d->eligible = (unsigned char)authority;
}
RECOMP_HOOK("func_80034A10_35610")
void world_dynamic_reuse(void *actor) {
  DynamicActor *d = lookup(actor);
  if (!d)
    return;
  d->actor = 0;
  d->saved = 0;
  if (d->kind && d->eligible && d->ready && !d->placed) {
    d->row[WD_LIFE] = WD_REMOVED;
    d->row[WD_OWNER] = 0;
  } else
    clear(d, sizeof(*d));
}
RECOMP_HOOK("func_80221A90_5DCF60")
void world_dynamic_npc(void *actor) {
  DynamicActor *d;
  if (d_building || anchor_world_actor_placed(actor))
    return;
  d = allocate(actor);
  if (!d)
    return;
  d->kind = WD_NPC;
  d->talkable = 1;
  d->eligible = 1;
}
RECOMP_HOOK("func_80226840_5E1D10")
void world_dynamic_path(void *actor, unsigned short route) {
  DynamicActor *d;
  if (d_building || !actor || route >= 163 ||
      anchor_world_actor_placed(actor) || H(actor, 0x5c) == 0x2bc)
    return;
  d = lookup(actor);
  if (!d && (H(actor, 0x5c) == 0x2c2 || H(actor, 0x5c) == 0x2c3 ||
             H(actor, 0x5c) == 0x2c4))
    d = allocate(actor);
  if (!d)
    return;
  if (H(actor, 0x5c) >= 0x2c2 && H(actor, 0x5c) <= 0x2c4) {
    d->kind = WD_NPC;
    d->eligible = 1;
  }
  if (d->kind == WD_NPC) {
    d->path = 1;
    d->row[WD_ROUTE] = route;
  }
}
RECOMP_HOOK("func_8021664C_5D1B1C")
void world_dynamic_animation(void *actor, unsigned int clip) {
  DynamicActor *d = lookup(actor);
  if (d && !d_binding && clip <= 255) {
    d->clip = (unsigned char)clip;
    d->animated = 1;
  }
}
RECOMP_HOOK("func_80216CE0_5D21B0")
void world_dynamic_static(void *actor, void *object, unsigned int clip) {
  DynamicActor *d = lookup(actor);
  /* The same binder builds linked objects: shadow setup temporarily selects
   * model 1, clip 0 on this actor but supplies a different object. Only the
   * primary object describes the actor we replicate. */
  if (d && object == DPTR(actor, 0x18) && !d_binding && clip <= 255) {
    d->clip = (unsigned char)clip;
    d->animated = 0;
  }
}
RECOMP_HOOK("func_80218DCC_5D429C")
void world_dynamic_sphere(void *actor, unsigned int sphere) {
  DynamicActor *d = lookup(actor);
  if (d && !d_binding && sphere <= 16)
    d->sphere = (unsigned char)sphere;
}
static unsigned int phase(void *callback) {
  unsigned int i;
  addresses();
  for (i = 0; i < 14; ++i)
    if (((unsigned long)callback & ~DDISABLED) == (unsigned long)d_phases[i])
      return i + 1;
  return 0;
}
static void detect(DynamicActor *d) {
  unsigned int p;
  void *a = d->actor;
  if (d->kind == WD_NPC)
    return;
  p = phase(DPTR(a, 0xc));
  if ((p == 1 || p == 2 || p == 13) && H(a, 0x5e) == 1 && d->clip == 4 &&
      !d->animated)
    d->kind = WD_COIN;
  else if ((p == 3 || p == 14) && H(a, 0x5e) == 1 && d->clip == 3 &&
           !d->animated)
    d->kind = WD_HEALTH;
  else if (p == 4 && H(a, 0x5e) == 0x85 && !d->animated && !d->clip &&
           func_800141C4_14DC4(26) != -1)
    d->kind = WD_FOOD;
  else if (p >= 5 && p <= 12 && d->animated &&
           H(a, 0x5e) == (p <= 6    ? 0x191
                          : p <= 10 ? 0x1a4
                                    : 0x19b) &&
           d->clip == ((p == 9 || p == 10) ? 3 : 0) &&
           func_800141C4_14DC4(p < 11 ? 34 : 44) != -1)
    d->kind = WD_HAZARD;
  if (d->kind)
    d->row[WD_PHASE] = (int)p;
}
RECOMP_HOOK("func_80218F30_5D4400")
void world_dynamic_post(void *actor) {
  DynamicActor *d = lookup(actor);
  if (!d && actor && DPTR(actor, 0x18) && anchor_world_actor_placed(actor) &&
      (H(actor, 0x5c) == 0x82 || H(actor, 0x5c) == 0x83 ||
       H(actor, 0x5c) == 0x84 || H(actor, 0x5c) == 0x85)) {
    unsigned int index = 0;
    anchor_world_actor_authority(actor, &index);
    d = allocate(actor);
    if (d) {
      d->parent = (unsigned short)index;
      d->placed = 1;
      d->eligible = 1;
      d->row[WD_ORDINAL] = 0x7fffffff;
      d->clip = (H(actor, 0x5c) == 0x85) ? 0 : (H(actor, 0x5c) == 0x84) ? 3 : 4;
    }
  }
  if (!d || d_building || !DPTR(actor, 0x18))
    return;
  if (!d->ready) {
    detect(d);
    if (!d->kind) {
      if (W(actor, 0x68) & 2u)
        clear(d, sizeof(*d));
      return;
    }
    d->ready = 1;
    d->generation = B(actor, 0x74);
    d->row[WD_ENTITY] = H(actor, 0x5c);
    d->row[WD_MODEL] = H(actor, 0x5e);
    d->row[WD_KIND] = d->kind;
    if (!d->placed && d->parent && d->parent <= 256 && d->kind >= WD_COIN &&
        d->kind <= WD_FOOD)
      d->row[WD_ORDINAL] = (int)d_loot_ordinals[d->parent][d->kind - WD_COIN]++;
    if (d->kind == WD_NPC) {
      unsigned int i, j;
      for (j = 0; j < 3; ++j)
        quantize(F(DPTR(actor, 0x18), 8 + j * 4), 100, -3276800, 3276700,
                 &d->row[WD_BIRTH_X + j]);
      for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
        DynamicActor *other = &d_actors[i];
        if (other == d || !other->ready || other->kind != WD_NPC ||
            other->parent != d->parent ||
            other->row[WD_ENTITY] != d->row[WD_ENTITY])
          continue;
        for (j = 0; j < 3; ++j)
          if (other->row[WD_BIRTH_X + j] != d->row[WD_BIRTH_X + j])
            break;
        if (j == 3 && other->row[WD_ORDINAL] >= d->row[WD_ORDINAL])
          d->row[WD_ORDINAL] = other->row[WD_ORDINAL] + 1;
      }
    }
    d->native = DPTR(actor, 0xc);
  }
  if (d_active && !d->eligible && !d->have && d->kind != WD_NPC)
    W(actor, 0x68) |= 2u; /* a locally repeated remote death/emission */
  if (W(actor, 0x68) & 2u) {
    if (d->placed && d->row[WD_LIFE] != WD_REMOVED) {
      clear(d, sizeof(*d)); /* placement culling is not a shared collection */
      return;
    }
    if (d->eligible && !d->claimed && (!d->have || d->owner == d_self)) {
      d->row[WD_LIFE] = WD_REMOVED;
      d->row[WD_OWNER] = 0;
    }
    d->actor = 0;
    d->saved = 0;
    if (!d->eligible)
      clear(d, sizeof(*d));
  }
}
RECOMP_HOOK("func_80034ED4_35AD4")
void world_dynamic_delete(void) { world_dynamic_reuse(D_8016DAB4_16E6B4); }
static int capture(DynamicActor *d) {
  unsigned int j;
  void *a = d->actor, *o;
  int *r = d->row;
  if (!d->kind || !d->eligible)
    return 0;
  if (r[WD_LIFE] == WD_REMOVED)
    return anchor_world_dynamic_row_valid(r);
  if (!alive(d))
    return 0;
  o = DPTR(a, 0x18);
  r[WD_KIND] = d->kind;
  r[WD_PARENT] = d->parent;
  r[WD_OWNER] = (int)d->owner;
  r[WD_LIFE] = d->claimed ? WD_CLAIM : WD_LIVE;
  r[WD_ENTITY] = H(a, 0x5c);
  r[WD_MODEL] = H(a, 0x5e);
  r[WD_CLIP] = d->clip;
  r[WD_ANIMATED] = d->animated;
  for (j = 0; j < 3; ++j) {
    if (!quantize(F(o, 8 + j * 4), 100, -3276800, 3276700, &r[WD_X + j]) ||
        !quantize(F(a, 0x78 + j * 4), 1000, -100000, 100000, &r[WD_VX + j]) ||
        !quantize(F(o, 0x1c + j * 4), 1000, 0, 64000, &r[WD_SX + j]))
      return 0;
    r[WD_PITCH + j] = H(o, 0x14 + j * 2) & 1023;
  }
  if (!quantize(F(o, 0x28), 100, 0, 1000000, &r[WD_FRAME]) ||
      !quantize(F(o, 0x68), 1000, 0, 100000, &r[WD_OBJECT_RADIUS]) ||
      !quantize(F(o, 0x6c), 1000, 0, 100000, &r[WD_OBJECT_HEIGHT]))
    return 0;
  r[WD_RATE] = H(o, 0x7e);
  r[WD_ANIM_FLAGS] = B(o, 0x7c) & 7;
  r[WD_FLAGS_LO] = (int)(W(a, 0x60) & 65535u);
  r[WD_FLAGS_HI] = (int)(W(a, 0x60) >> 16);
  r[WD_AUX_LO] = (int)(W(a, 0x64) & 65535u);
  r[WD_AUX_HI] = (int)(W(a, 0x64) >> 16);
  r[WD_TIMER] = S(a, 0x8a);
  r[WD_BUSY] = d->talkable && (W(a, 0x68) & 0x100u) != 0;
  r[WD_PAUSED] = anchor_world_is_paused() != 0;
  if (d->kind == WD_NPC) {
    r[WD_PHASE] = 0;
    r[WD_ROUTE] = d->path ? H(a, 0xc4) : 163;
    r[WD_TALKABLE] = d->talkable;
    r[WD_DIALOG] = H(a, 0xa4);
    if (d->path) {
      r[WD_PATH_TIMER] = S(a, 0xc6);
      r[WD_PATH_STATE] = B(a, 0xce);
      r[WD_PATH_PC] = B(a, 0xcf);
      for (j = 0; j < 3; ++j)
        r[WD_ORIGIN_X + j] = S(a, 0xc8 + j * 2);
      r[WD_FACING] = B(a, 0xaa);
    }
  } else {
    unsigned int p = phase(d->saved ? d->saved : DPTR(a, 0xc));
    if (p)
      r[WD_PHASE] = (int)p;
    if (d->kind == WD_COIN || d->kind == WD_HEALTH) {
      if (r[WD_PHASE] < 13 &&
          !quantize(F(a, 0xd0), 1000, -100000, 100000, &r[WD_BOUNCE]))
        return 0;
    } else if (d->kind == WD_HAZARD) {
      if (r[WD_PHASE] < 11) {
        if (!quantize(F(a, 0xd8), 1000, -100000, 100000, &r[WD_BOUNCE]))
          return 0;
        r[WD_LANDED] = S(a, 0xdc) != 0;
      } else {
        r[WD_BASE_Y] = S(a, 0xd0);
        r[WD_ANGLE] = H(a, 0xd8) & 1023;
      }
    }
  }
  r[WD_RADIUS] = H(a, 0x3c);
  r[WD_HEIGHT] = H(a, 0x3e);
  r[WD_OFFSET] = S(a, 0x40);
  r[WD_ATTACK_RADIUS] = H(a, 0x4e);
  r[WD_ATTACK_HEIGHT] = H(a, 0x50);
  r[WD_ATTACK_OFFSET] = S(a, 0x52);
  r[WD_ATTACK] = B(a, 0x4c);
  r[WD_MASK] = H(a, 0x96);
  for (j = 0; j < 4; ++j)
    r[WD_DIM0 + j] = B(a, 0x98 + j);
  r[WD_SPHERE] = W(a, 0x48) == 0xffffffffu ? 17
                 : W(a, 0x48) == 0         ? 0
                                           : d->sphere;
  r[WD_GRAVITY] = B(a, 0x75);
  r[WD_SHADOW_SCALE] = B(a, 0x6e);
  r[WD_SHADOW_OFFSET] = B(a, 0x6f);
  r[WD_MASK94] = H(a, 0x94);
  r[WD_BODY_OFFSET2] = S(a, 0x42);
  return anchor_world_dynamic_row_valid(r);
}
/* Both asset waves and every requested list entry must be resident. Calling
 * the native binder with a missing resource deliberately faults the game. */
static int resource_valid(const int *r) {
  void *model;
  const unsigned short *files;
  const unsigned int *clips;
  unsigned int i, highest = (unsigned int)r[WD_CLIP];
  if (r[WD_MODEL] > 1025 || r[WD_MODEL] < 0)
    return 0;
  model = D_80236984_5F1E54[r[WD_MODEL]];
  if (!model)
    return 0;
  files = (const unsigned short *)DPTR(model, 0);
  clips = (const unsigned int *)DPTR(model, 4);
  if (!files || !clips || func_800141C4_14DC4(files[0]) == -1 ||
      func_800141C4_14DC4(files[1]) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  if (r[WD_SPHERE] != 17 && (unsigned int)r[WD_SPHERE] > highest)
    highest = (unsigned int)r[WD_SPHERE];
  for (i = 0; i <= highest; ++i)
    if (!clips[i])
      return 0;
  if (r[WD_KIND] == WD_HAZARD) {
    int p = r[WD_PHASE];
    if (p <= 6 && r[WD_MODEL] != 0x191)
      return 0;
    if (p >= 7 && p <= 10 && r[WD_MODEL] != 0x1a4)
      return 0;
    if (p >= 11 && r[WD_MODEL] != 0x19b)
      return 0;
    if (func_800141C4_14DC4(p < 11 ? 34 : 44) == -1)
      return 0;
  }
  if (r[WD_KIND] == WD_NPC && r[WD_ROUTE] < 163 &&
      !(world_path_pc[r[WD_ROUTE]][r[WD_PATH_PC] >> 3] &
        (1u << (r[WD_PATH_PC] & 7))))
    return 0;
  if (r[WD_KIND] == WD_NPC && r[WD_TALKABLE] && r[WD_DIALOG] >= 794)
    return 0;
  if ((r[WD_FLAGS_HI] & 0x800) && r[WD_MODEL] != 1) {
    model = D_80236984_5F1E54[1];
    if (!model || !(files = (const unsigned short *)DPTR(model, 0)) ||
        func_800141C4_14DC4(files[0]) == -1 ||
        func_800141C4_14DC4(files[1]) == -1)
      return 0;
  }
  return 1;
}
static void bind(DynamicActor *d, const int *r) {
  void *a = d->actor;
  d_binding = 1;
  H(a, 0x5e) = (unsigned short)r[WD_MODEL];
  if (r[WD_ANIMATED])
    func_8021664C_5D1B1C(a, (unsigned int)r[WD_CLIP],
                         (float)r[WD_RATE] / 256.0f, r[WD_ANIM_FLAGS] & 1);
  else
    func_80216DF8_5D22C8(a, (unsigned int)r[WD_CLIP]);
  d_binding = 0;
  d->clip = (unsigned char)r[WD_CLIP];
  d->animated = (unsigned char)r[WD_ANIMATED];
}
static unsigned short object_angle(const int *r, unsigned int axis) {
  /* Every supported loot constructor calls 8021A310. Its exact 0x8000
   * orientation cannot be represented by the ordinary 10-bit angle fields. */
  return r[WD_KIND] >= WD_COIN && r[WD_KIND] <= WD_FOOD
             ? 0x8000u
             : (unsigned short)r[WD_PITCH + axis];
}
static int apply(DynamicActor *d) {
  int *r = d->net;
  void *a = d->actor, *o;
  unsigned int j;
  if (!d->dirty)
    return 1;
  if (!alive(d) || !resource_valid(r))
    return 0;
  if (d->kind == WD_NPC && (W(a, 0x68) & 0x100u))
    return 1;
  o = DPTR(a, 0x18);
  if (d->clip != r[WD_CLIP] || d->animated != r[WD_ANIMATED] ||
      H(a, 0x5e) != r[WD_MODEL])
    bind(d, r);
  for (j = 0; j < 3; ++j) {
    F(o, 8 + j * 4) = (float)r[WD_X + j] / 100.0f;
    H(o, 0x14 + j * 2) = object_angle(r, j);
    F(a, 0x78 + j * 4) = (float)r[WD_VX + j] / 1000.0f;
    F(o, 0x1c + j * 4) = (float)r[WD_SX + j] / 1000.0f;
  }
  F(o, 0x28) = (float)r[WD_FRAME] / 100.0f;
  if (r[WD_ANIMATED]) {
    float length = func_8001B5AC_1C1AC(o);
    if (length > 0 && F(o, 0x28) >= length)
      F(o, 0x28) = length - 1;
  }
  H(o, 0x7e) = (unsigned short)r[WD_RATE];
  B(o, 0x7c) = (B(o, 0x7c) & ~7u) | (unsigned char)r[WD_ANIM_FLAGS];
  /* These families use common scalar flags, not mesh/actor-pointer modes. */
  {
    unsigned int shadow = W(a, 0x60) & 0x08000000u;
    /* Coin spin is a local texture sequence (+AB..+AE), not the lifetime
     * timer at +8A. Preserve its native loop flag so the common post keeps
     * advancing it instead of stopping the sequence after one revolution. */
    unsigned int texture_loop = d->kind == WD_COIN ? 4u : 0;
    W(a, 0x60) =
        (((unsigned int)r[WD_FLAGS_LO] | ((unsigned int)r[WD_FLAGS_HI] << 16)) &
         (0x16e00febu | shadow)) | texture_loop;
  }
  W(a, 0x64) =
      ((unsigned int)r[WD_AUX_LO] | ((unsigned int)r[WD_AUX_HI] << 16)) &
      0x1022u;
  S(a, 0x8a) = (short)r[WD_TIMER];
  B(a, 0x75) = (unsigned char)r[WD_GRAVITY];
  H(a, 0x3c) = (unsigned short)r[WD_RADIUS];
  H(a, 0x3e) = (unsigned short)r[WD_HEIGHT];
  S(a, 0x40) = (short)r[WD_OFFSET];
  H(a, 0x4e) = (unsigned short)r[WD_ATTACK_RADIUS];
  H(a, 0x50) = (unsigned short)r[WD_ATTACK_HEIGHT];
  S(a, 0x52) = (short)r[WD_ATTACK_OFFSET];
  B(a, 0x4c) = (unsigned char)r[WD_ATTACK];
  H(a, 0x96) = (unsigned short)r[WD_MASK];
  H(a, 0x94) = (unsigned short)r[WD_MASK94];
  S(a, 0x42) = (short)r[WD_BODY_OFFSET2];
  B(a, 0x6e) = (unsigned char)r[WD_SHADOW_SCALE];
  B(a, 0x6f) = (unsigned char)r[WD_SHADOW_OFFSET];
  for (j = 0; j < 4; ++j)
    B(a, 0x98 + j) = (unsigned char)r[WD_DIM0 + j];
  F(o, 0x68) = (float)r[WD_OBJECT_RADIUS] / 1000.0f;
  F(o, 0x6c) = (float)r[WD_OBJECT_HEIGHT] / 1000.0f;
  if (d->proxy) {
    W(a, 0x48) = r[WD_SPHERE] == 17 ? 0xffffffffu : 0;
    if (r[WD_SPHERE] && r[WD_SPHERE] != 17) {
      d_binding = 1;
      func_80218DCC_5D429C(a, (unsigned int)r[WD_SPHERE]);
      d_binding = 0;
    }
  }
  if (d->kind == WD_NPC) {
    d->path = r[WD_ROUTE] < 163;
    d->talkable = (unsigned char)r[WD_TALKABLE];
    H(a, 0xa4) = (unsigned short)r[WD_DIALOG];
    if (d->path) {
      H(a, 0xc4) = (unsigned short)r[WD_ROUTE];
      S(a, 0xc6) = (short)r[WD_PATH_TIMER];
      B(a, 0xce) = (unsigned char)r[WD_PATH_STATE];
      B(a, 0xcf) = (unsigned char)r[WD_PATH_PC];
      for (j = 0; j < 3; ++j)
        S(a, 0xc8 + j * 2) = (short)r[WD_ORIGIN_X + j];
      B(a, 0xaa) = (unsigned char)r[WD_FACING];
      DPTR(a, 0x9c) = a;
    }
  } else {
    addresses();
    d->native = (void *)d_phases[r[WD_PHASE] - 1];
    if (d->saved)
      d->saved = d->native;
    else
      DPTR(a, 0xc) = d->native;
    if (d->kind == WD_COIN || d->kind == WD_HEALTH)
      F(a, 0xd0) = (float)r[WD_BOUNCE] / 1000.0f;
    if (d->kind == WD_HAZARD) {
      if (r[WD_PHASE] < 11) {
        F(a, 0xd8) = (float)r[WD_BOUNCE] / 1000.0f;
        S(a, 0xdc) = (short)r[WD_LANDED];
      } else {
        S(a, 0xd0) = (short)r[WD_BASE_Y];
        H(a, 0xd8) = (unsigned short)r[WD_ANGLE];
      }
    }
  }
  d->sphere = (unsigned char)r[WD_SPHERE];
  d->dirty = 0;
  return 1;
}
static void npc_native(void *a, void *o) {
  DynamicActor *d = lookup(a);
  (void)o;
  if (!d)
    return;
  if (d->talkable && func_80220F70_5DC440(a))
    return;
  if (d->path && !(W(a, 0x68) & 0x100u))
    func_802268A8_5E1D78(a);
}
static void award(DynamicActor *d) {
  void *a = d->actor;
  unsigned int hp, ryo, after_hp, after_ryo;
  if (!a || d->granted)
    return;
  d->granted = 1;
  hp = item_sync_local_player_health();
  ryo = item_sync_local_player_ryo();
  if (d->kind == WD_COIN)
    func_802145F0_5CFAC0(a);
  else if (d->kind == WD_HEALTH)
    func_80213FF0_5CF4C0(a);
  else if (d->kind == WD_FOOD) {
    func_80038B98_39798(0x26c);
    func_801DCD48_598C58(40);
    func_8021804C_5D351C(a, 0);
  }
  after_hp = item_sync_local_player_health();
  after_ryo = item_sync_local_player_ryo();
  /* Removal is shared; native rewards belong only to the winning collector.
   * Exclude the actual capped gains from legacy health/ryo delta sharing. */
  item_sync_exclude_loot_reward(after_hp > hp ? after_hp - hp : 0,
                                after_ryo > ryo ? after_ryo - ryo : 0);
}
static void dynamic_callback(void *actor, void *object) {
  DynamicActor *d = lookup(actor);
  DynamicCallback real;
  if (!d)
    return;
  if (d_room != D_800C7AB2)
    return;
  if (d->row[WD_LIFE] == WD_REMOVED) {
    if ((unsigned int)d->row[WD_OWNER] == d_self && d->claimed)
      award(d);
    if (d->kind == WD_FOOD && DPTR(actor, 0xe4))
      B(DPTR(actor, 0xe4), 0xd0) = 1;
    W(actor, 0x68) |= 2u;
    return;
  }
  if (d->have && !apply(d)) {
    d->have = 0;
  }
  if (d_active && d->kind >= WD_COIN && d->kind <= WD_FOOD &&
      (W(actor, 0x68) & 0x200u))
    d->claimed = 1;
  if (d->claimed) {
    W(actor, 0x68) &= ~0x200u;
    F(actor, 0x78) = F(actor, 0x7c) = F(actor, 0x80) = 0;
    return;
  }
  real = (DynamicCallback)((unsigned long)(d->saved ? d->saved : d->native) &
                           ~DDISABLED);
  if (d->have && d->owner != d_self && !(W(actor, 0x68) & 0x100u)) {
    if (d->kind == WD_NPC && d->talkable) {
      /* Dialogue saves +0x0C as its continuation. Give it the real callback,
       * otherwise it would restore this temporary scheduler wrapper. */
      DPTR(actor, 0xc) = (void *)real;
      func_80220F70_5DC440(actor);
      if (DPTR(actor, 0xc) == (void *)real)
        DPTR(actor, 0xc) = (void *)dynamic_callback;
      else
        d->native = DPTR(actor, 0xc);
    }
    if (d->net[WD_BUSY] || d->net[WD_PAUSED])
      F(actor, 0x78) = F(actor, 0x7c) = F(actor, 0x80) = 0;
    return; /* Native post integrates velocity/animation/contact exactly once.
             */
  }
  if (real) {
    DPTR(actor, 0xc) = (void *)real;
    real(actor, object);
    if (DPTR(actor, 0xc) == (void *)real)
      DPTR(actor, 0xc) = (void *)dynamic_callback;
    else
      d->native = DPTR(actor, 0xc);
  }
}
RECOMP_HOOK("func_80034734_35334")
void world_dynamic_scheduler_begin(void) {
  unsigned int i;
  if (!d_active || d_room != D_800C7AB2)
    return;
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
    DynamicActor *d = &d_actors[i];
    if (!d->eligible || !alive(d))
      continue;
    d->saved = DPTR(d->actor, 0xc);
    if (((unsigned long)d->saved & ~DDISABLED) ==
        (unsigned long)dynamic_callback)
      d->saved = d->native;
    else
      d->native = d->saved;
    DPTR(d->actor, 0xc) = (void *)((unsigned long)dynamic_callback |
                                   ((unsigned long)d->saved & DDISABLED));
  }
}
RECOMP_HOOK_RETURN("func_80034734_35334")
void world_dynamic_scheduler_end(void) { restore(); }
static int equal_key(const int *a, const int *b) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}
static DynamicActor *match(const int *r) {
  unsigned int i, j;
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
    DynamicActor *d = &d_actors[i];
    if (!d->used)
      continue;
    if (equal_key(d->row, r) || (!d->row[WD_CID] && r[WD_CID] == (int)d_self &&
                                 r[WD_SERIAL] == (int)d->serial))
      return d;
    if (!d->row[WD_CID] && d->placed && r[WD_ORDINAL] == 0x7fffffff &&
        r[WD_PARENT] == d->parent)
      return d;
    if (!d->row[WD_CID] && r[WD_CID] == 0x7ffffffd && r[WD_KIND] == d->kind &&
        r[WD_KIND] >= WD_COIN && r[WD_KIND] <= WD_FOOD &&
        r[WD_PARENT] == d->parent && r[WD_ORDINAL] == d->row[WD_ORDINAL])
      return d;
  }
  /* Bind an existing scripted NPC on a follower instead of duplicating it or
   * destroying pointers held by its local scene controller. */
  if (r[WD_KIND] == WD_NPC)
    for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
      DynamicActor *d = &d_actors[i];
      if (!d->used || d->row[WD_CID] || !alive(d) || d->kind != WD_NPC ||
          d->row[WD_ENTITY] != r[WD_ENTITY] || d->parent != r[WD_PARENT] ||
          d->row[WD_ORDINAL] != r[WD_ORDINAL])
        continue;
      for (j = 0; j < 3; ++j) {
        int delta = d->row[WD_BIRTH_X + j] - r[WD_BIRTH_X + j];
        if (delta > 100 || delta < -100)
          break;
      }
      if (j == 3)
        return d;
    }
  return 0;
}
static DynamicActor *reconstruct(const int *r) {
  void *a;
  DynamicActor *d;
  unsigned int j;
  if (r[WD_LIFE] != WD_LIVE || r[WD_ORDINAL] == 0x7fffffff ||
      !D_801FC604_5B8514 || !resource_valid(r))
    return 0;
  d = allocate(0);
  if (!d)
    return 0;
  d_building = 1;
  a = func_802171A8_5D2678(D_801FC604_5B8514, dynamic_callback,
                           (unsigned char)(r[WD_KIND] == WD_NPC      ? 1
                                           : r[WD_KIND] == WD_HAZARD ? 12
                                                                     : 9));
  d_building = 0;
  if (!a || !DPTR(a, 0x18)) {
    clear(d, sizeof(*d));
    return 0;
  }
  d->actor = a;
  d->proxy = 1;
  d->ready = 1;
  d->eligible = 1;
  d->generation = B(a, 0x74);
  d->kind = (unsigned char)r[WD_KIND];
  d->native = (void *)npc_native;
  H(a, 0x5c) = (unsigned short)r[WD_ENTITY];
  H(a, 0x5e) = (unsigned short)r[WD_MODEL];
  W(a, 0x70) = 0;
  W(a, 0x68) = 0;
  DPTR(a, 0x90) = 0;
  DPTR(a, 0x34) = 0;
  DPTR(a, 0x38) = 0;
  DPTR(a, 0xe4) = 0;
  B(a, 0x8d) = 1;
  for (j = 0; j < WORLD_DYNAMIC_WORDS; ++j)
    d->row[j] = d->net[j] = r[j];
  d->dirty = 1;
  bind(d, r);
  /* Linked effects inherit the parent's pose when constructed. Establish the
   * remote pose first so food shine/shadows do not start at the local player.
   */
  for (j = 0; j < 3; ++j) {
    void *o = DPTR(a, 0x18);
    F(o, 8 + j * 4) = (float)r[WD_X + j] / 100.0f;
    H(o, 0x14 + j * 2) = object_angle(r, j);
    F(o, 0x1c + j * 4) = (float)r[WD_SX + j] / 1000.0f;
  }
  if (r[WD_FLAGS_HI] & 0x800)
    func_80219E70_5D5340(a, (unsigned char)r[WD_SHADOW_SCALE],
                         (unsigned char)r[WD_SHADOW_OFFSET]);
  if (d->kind == WD_COIN && r[WD_MODEL] == 1 && r[WD_CLIP] == 4)
    func_80224ABC_5DFF8C(a, 0, 1.0f, 1);
  if (d->kind == WD_FOOD) {
    d_building = 1;
    DPTR(a, 0xe4) = func_8021804C_5D351C(a, 1);
    d_building = 0;
  }
  return d;
}
void anchor_world_dynamic_frame(unsigned int room, unsigned int signature,
                                unsigned int visit, int active) {
  unsigned int i, j, n = 0, received = 0, leader = 0;
  char *reply;
  (void)signature;
  restore();
  ++d_tick;
  if (!active) {
    if (d_active) {
      d_active = 0;
      for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
        d_actors[i].have = 0;
        d_actors[i].claimed = 0;
        if (alive(&d_actors[i]) && d_actors[i].proxy)
          DPTR(d_actors[i].actor, 0xc) = d_actors[i].native;
      }
    }
    return;
  }
  d_active = 1;
  d_self = anchor_get_client_id();
  d_room = room;
  d_visit = visit;
  for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
    if (d_actors[i].used && capture(&d_actors[i])) {
      for (j = 0; j < WORLD_DYNAMIC_WORDS; ++j)
        d_rows[n][j] = d_actors[i].row[j];
      ++n;
    }
  if (!anchor_world_dynamic_encode(d_rows, n, d_json, sizeof(d_json)))
    return;
  reply = anchor_update_world_actors(d_json);
  if (reply &&
      anchor_world_dynamic_decode(reply, d_incoming, &received, &leader)) {
    d_leader = leader;
    for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
      d_actors[i].have = 0;
    for (i = 0; i < received; ++i) {
      int *r = d_incoming[i];
      DynamicActor *d = match(r);
      if (d && r[WD_LIFE] == WD_LIVE && !alive(d) &&
          d->row[WD_LIFE] != WD_REMOVED) {
        /* A replica may have been culled or its pool slot reused locally.
         * Presence at the authority requires a new local task, not a stale
         * bookkeeping match that silently prevents reconstruction forever. */
        clear(d, sizeof(*d));
        d = 0;
      }
      if (!d)
        d = reconstruct(r);
      if (!d)
        continue;
      for (j = 0; j < 4; ++j)
        d->row[j] = r[j];
      if (r[WD_LIFE] == WD_REMOVED) {
        if (!r[WD_COMMITTER] && !r[WD_OWNER]) {
          /* Unpublished speculative emission rejected after room ownership
           * became known. Retire only this local copy, without a tombstone. */
          if (alive(d))
            W(d->actor, 0x68) |= 2u;
          clear(d, sizeof(*d));
          continue;
        }
        d->row[WD_LIFE] = WD_REMOVED;
        d->row[WD_OWNER] = r[WD_OWNER];
        if (!d->actor)
          clear(d, sizeof(*d));
        continue;
      }
      if (r[WD_KIND] != d->kind || r[WD_ENTITY] != d->row[WD_ENTITY])
        continue;
      d->eligible = 1;
      d->parent = (unsigned short)r[WD_PARENT];
      if (r[WD_CID] == 0x7ffffffd && r[WD_KIND] >= WD_COIN &&
          r[WD_KIND] <= WD_FOOD &&
          (unsigned int)r[WD_ORDINAL] >=
              d_loot_ordinals[d->parent][d->kind - WD_COIN])
        d_loot_ordinals[d->parent][d->kind - WD_COIN] =
            (unsigned int)r[WD_ORDINAL] + 1;
      for (j = 0; j < WORLD_DYNAMIC_WORDS; ++j) {
        if (d->net[j] != r[j])
          d->dirty = 1;
        d->net[j] = r[j];
      }
      d->have = 1;
      d->owner = (unsigned int)r[WD_OWNER];
      if (d->proxy && !d->saved)
        DPTR(d->actor, 0xc) = (void *)dynamic_callback;
      if (d->owner != d_self || d->proxy)
        apply(d);
    }
  } else
    for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
      d_actors[i].have = 0;
  if (reply)
    recomp_free(reply);
}
