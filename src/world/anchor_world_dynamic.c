/* Native child lifecycle and portable reconstruction. Restore scalar state
 * without replaying emitting AI. The typed route robot additionally runs its
 * verified pure model initializer in its own scheduler slot before restoring
 * that checkpoint. Collision/contact runs once in the stock post. */
#ifndef WORLD_DYNAMIC_HOST_TEST
#include "core/anchor.h"
#include "core/anchor_dialog.h"
#include "world/enemy_sync.h"
#include "progression/item_sync.h"
#include "platform/modding.h"
#include "platform/recomputils.h"
#endif
#include "world/anchor_world_dynamic.h"
#include "world/anchor_world_bridge.h"
#include "world/anchor_world_doll.h"
#include "world/anchor_world.h"
#include "world/anchor_castle_return_sign.h"
#ifdef WORLD_DYNAMIC_HOST_TEST
#ifndef WORLD_DYNAMIC_CASTLE_SIGN_OWNS
#define WORLD_DYNAMIC_CASTLE_SIGN_OWNS(actor) 0
#endif
#else
#define WORLD_DYNAMIC_CASTLE_SIGN_OWNS(actor) anchor_castle_return_sign_owns_task(actor)
#endif
#include "world/anchor_world_npc.h"
#include "world/anchor_world_slicer.h"
#include "world/anchor_world_random.h"
#include "world/anchor_world_bomb.h"
#include "world/anchor_world_wave.h"
#include "world/anchor_world_fragile.h"
#include "world/anchor_world_boulder.h"
#include "world/anchor_world_fish.h"
#include "world/anchor_world_paths.inc"

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
extern void func_08001EA4_6D0F84(void *, void *);
extern void func_08001FF4_6D10D4(void *, void *);
extern void func_802130C8_5CE598(void *, void *);
extern void func_8021332C_5CE7FC(void *, void *);
extern unsigned char D_800C7AE2, D_8015CD00[];
extern int func_800240DC_24CDC(int);
extern void func_80023DF0_249F0(int);
extern void func_80035020_35C20(void);
extern void func_8021925C_5D472C(void *, void *);
extern void func_80218F30_5D4400(void *, void *);
extern void func_080027AC_723DCC(void *, void *);
extern void func_08000514_6AECF4(void *, void *);
extern void func_080005F8_6AEDD8(void *, void *);
extern void func_080006E0_6AEEC0(void *, void *);
extern void func_08004550_6C3CA0(void *, void *);
extern void func_08004594_6C3CE4(void *, void *);
extern void func_08004654_6C3DA4(void *, void *);
extern void func_08004694_6C3DE4(void *, void *);
extern void func_0800488C_6C3FDC(void *, void *);
extern void func_08007B28_6C7278(void *, void *);
extern void func_08007BDC_6C732C(void *, void *);
extern void func_0800028C_6BF9DC(void *, void *);
extern void func_08004CEC_6C443C(void *, void *);
extern void func_08004D64_6C44B4(void *, void *);
extern void func_08004AA0_6C41F0(void *, void *);
extern void func_08004AB4_6C4204(void *, void *);
extern void func_0800664C_6C5D9C(void *, void *);
extern void func_08006778_6C5EC8(void *, void *);
extern void func_0800676C_6C5EBC(void *, void *);
extern void func_08004AE8_6C4238(void *, void *);
extern void func_08007984_6C70D4(void *, void *);
extern void func_80214314_5CF7E4(void *, void *);
extern void func_802141AC_5CF67C(void *, void *);
extern void func_080014B4_6D57F4(void *, void *);
extern void func_080002C0_72ACF0(void *, void *);
extern void func_08000668_72B098(void *, void *);
extern void func_08000928_72B358(void *, void *);
extern int anchor_race_boulder_duplicate(void *, unsigned int *, unsigned int *);
extern void anchor_race_boulder_forget(void *);

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
  unsigned char talkable, path, animated, kind, clip, sphere, placed, npc_bound;
  unsigned char stable_ordinal;
  unsigned char shutter_birth, enemy_initialized, death_started;
  unsigned char doll_birth, doll_initialized, doll_scene;
  unsigned char slicer_birth, slicer_initialized, slicer_frozen;
  unsigned char random_birth, random_initialized, random_frozen;
  unsigned char bomb_birth, bomb_initialized, bomb_frozen, bomb_particles;
  unsigned char wave_birth, wave_initialized, wave_frozen;
  unsigned char fragile_birth, fragile_initialized;
  unsigned char boulder_initialized, boulder_frozen;
  unsigned char fish_awarded, fish_admit;
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
static void *d_doll_helper;

static void dynamic_callback(void *, void *);
static void npc_native(void *, void *);
static void doll_callback(void *, void *);
static int resource_valid(const int *);
static int apply(DynamicActor *);
static DynamicActor *reconstruct(const int *);
static int capture(DynamicActor *);
static DynamicActor *slicer_register_root(void *);
static int slicer_capture(DynamicActor *);
static int slicer_callback(DynamicActor *, void *, void *);
static void slicer_native(void *, void *);
static DynamicActor *random_register_root(void *);
static void random_child_birth(DynamicActor *, DynamicActor *);
static int random_capture(DynamicActor *);
static int random_callback(DynamicActor *, void *, void *);
static void random_native(void *, void *);
static DynamicActor *bomb_register_root(void *);
static DynamicActor *bomb_child(DynamicActor *, DynamicActor *, DynamicCallback);
static int bomb_capture(DynamicActor *);
static int bomb_resource_valid(const int *);
static int bomb_apply_extra(DynamicActor *,const int *);
static int bomb_callback(DynamicActor *,void *,void *);
static void bomb_native(void *,void *);
static void bomb_damage(void *);
static void bomb_detach(DynamicActor *);
static DynamicActor *wave_register_root(void *);
static unsigned int wave_role_of(void *);
static void wave_child_birth(DynamicActor *, DynamicActor *);
static int wave_capture(DynamicActor *);
static int wave_callback(DynamicActor *, void *, void *);
static void wave_hold(DynamicActor *);
static void wave_apply(DynamicActor *);
static void wave_native(void *, void *);
static int wave_gate(const int *, const DynamicActor *);
static int wave_resource(const int *);
static void wave_detach(DynamicActor *);
static void wave_damage(void *);
static DynamicActor *fragile_register_root(void *);
static void fragile_root_begin(void *, unsigned int);
static void fragile_child_birth(DynamicActor *, DynamicActor *);
static int fragile_capture(DynamicActor *);
static int fragile_apply(DynamicActor *);
static int fragile_callback(DynamicActor *, void *, void *);
static void fragile_damage(void *);
static int fragile_native_ok(unsigned int, unsigned int, void *);
static DynamicActor *boulder_register_root(void *);
static int boulder_capture(DynamicActor *);
static int boulder_apply(DynamicActor *);
static void boulder_callback(DynamicActor *, void *, void *);
static DynamicActor *fish_register_root(void *);
static int fish_capture(DynamicActor *);
static void fish_callback(DynamicActor *, void *, void *);
static int family_actor(const DynamicActor *d) {
  return d->kind==WD_SLICER || d->kind==WD_RANDOM ||
         d->kind==WD_BOMB || d->kind==WD_WAVE || d->kind==WD_FRAGILE ||
         d->kind==WD_BOULDER || d->kind==WD_FISH;
}
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
  if (d_room==D_800C7AB2) {
    for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
      if (d_actors[i].used && d_actors[i].kind==WD_BOMB)
        bomb_detach(&d_actors[i]);
      else if (d_actors[i].used && d_actors[i].kind==WD_WAVE)
        wave_detach(&d_actors[i]);
    }
  }
  clear(d_actors, sizeof(d_actors));
  clear(d_loot_ordinals, sizeof(d_loot_ordinals));
  d_active = 0;
  d_leader = 0;
  d_visit = 0;
  d_room = 0xffff;
  d_doll_helper = 0;
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
  if (d_building || !child || anchor_world_bridge_owns(child))
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
  if (p && p->kind==WD_BOMB && !p->row[WB_ROLE]) {
    d->parent=p->parent;d->bomb_birth=d->stable_ordinal=1;
    bomb_child(p,d,0);
    authority=p->row[WD_LIFE]==WD_REMOVED && (unsigned int)p->row[WD_COMMITTER]==d_self;
  }
  if (p && p->kind==WD_RANDOM) {
    d->parent=p->parent;
    if (!p->row[WR_ROLE] &&
        ((unsigned long)DPTR(child,0xc)&~DDISABLED)==(unsigned long)func_08007B28_6C7278) {
      random_child_birth(p,d);
      d->row[WR_ROLE]=1;
    } else if (p->row[WR_ROLE]) {
      d->stable_ordinal=1;
      d->row[WD_BASE_Y]=3;d->row[WD_ORDINAL]=p->row[WD_ORDINAL];
      if (p->row[WD_LIFE]==WD_REMOVED)
        authority=(unsigned int)p->row[WD_COMMITTER]==d_self;
    }
  }
  if (p && p->kind==WD_WAVE && !p->row[WW_ROLE] &&
      D_800C7AB2==WW_ROOM && wave_role_of(child)) {
    wave_child_birth(p,d);
    authority=p->eligible && (!p->have || p->owner==d_self);
  }
  if (p && p->kind==WD_FRAGILE && !p->row[WF_ROLE] &&
      (p->row[WD_ENTITY]==0x331 || p->row[WD_ENTITY]==0x332)) {
    fragile_child_birth(p,d);
    if (d->fragile_birth) authority=1;
  }
  if (p && p->kind==WD_FRAGILE && p->row[WD_LIFE]==WD_REMOVED &&
      (unsigned int)p->row[WD_COMMITTER]==d_self &&
      (((unsigned long)DPTR(child,0xc)&~DDISABLED)==(unsigned long)func_80214314_5CF7E4 ||
       ((unsigned long)DPTR(child,0xc)&~DDISABLED)==(unsigned long)func_802141AC_5CF67C)) {
    d->parent=p->parent;
    d->stable_ordinal=1;
    d->row[WD_BASE_Y]=4+p->row[WF_ROLE];
    d->row[WD_ORDINAL]=p->row[WF_ROLE];
    authority=1;
  }
  if (p && p->kind == WD_SLICER) {
    d->parent = p->parent;
    d->stable_ordinal = 1;
    d->row[WD_ORDINAL] = p->row[WS_ROLE] ? p->row[WD_ORDINAL] : 0;
    /* Mark loot before generic placed-loot identity packing. A blade birth
     * has a different initializer and carries the emitter's current ordinal. */
    d->row[WD_BASE_Y] = p->row[WS_ROLE] + 2;
    if (!p->row[WS_ROLE] &&
        ((unsigned long)DPTR(child,0xc)&~DDISABLED)==(unsigned long)func_08004694_6C3DE4) {
      d->slicer_birth = 1;
      d->row[WD_BASE_Y] = 0;
      d->row[WD_ORDINAL] = p->row[WD_ORDINAL];
      d->row[WS_ROLE] = 1;
      for (unsigned int j=WS_SUBTYPE;j<=WS_INITIAL;++j) d->row[j]=p->row[j];
    } else if (p->row[WD_LIFE] == WD_REMOVED)
      authority = (unsigned int)p->row[WD_COMMITTER] == d_self;
  }
  if (parent == d_doll_helper && anchor_world_doll_parent()) {
    d->parent = (unsigned short)anchor_world_doll_parent();
    d->doll_birth = d->stable_ordinal = d->eligible = 1;
    d->row[WD_ORDINAL] = 1;
    authority = 1;
  }
  unsigned int ordinal;
  if (anchor_world_shutter_birth(parent, &index, &ordinal)) {
    d->parent = (unsigned short)index;
    d->shutter_birth = 1;
    d->row[WD_ORDINAL] = (int)ordinal;
  }
  if (p && p->kind == WD_SHUTTER_ENEMY) {
    /* One native drop at most per robot. Its emission identity survives
     * concurrent kills and simulator changes, independent of kill order. */
    d->parent = p->parent;
    d->stable_ordinal = 1;
    d->row[WD_ORDINAL] = p->row[WD_ORDINAL];
    if (p->row[WD_LIFE] == WD_REMOVED)
      authority = (unsigned int)p->row[WD_COMMITTER] == d_self;
  }
  if (anchor_world_loot_ordinal(parent, &ordinal)) {
    d->stable_ordinal = 1;
    d->row[WD_ORDINAL] = (int)ordinal;
  }
  if (authority >= 0)
    d->eligible = (unsigned char)authority;
}
RECOMP_HOOK("func_80034A10_35610")
void world_dynamic_reuse(void *actor) {
  DynamicActor *d = lookup(actor);
  anchor_race_boulder_forget(actor);
  if (!d)
    return;
  if (d->kind==WD_FRAGILE) {
    if (alive(d) && d->row[WD_LIFE]!=WD_REMOVED) capture(d);
    d->actor=0;d->saved=0;d->row[WF_PRESENT]=0;
    return; /* Culling is not a kill; preserve a committed tombstone too. */
  }
  if (family_actor(d) && !d->row[WS_ROLE]) {
    if (alive(d) && d->row[WD_LIFE] != WD_REMOVED) capture(d);
    d->actor=0;d->saved=0;d->row[WS_PRESENT]=0;d->row[WD_PAUSED]=1;
    return; /* A placed emitter's proximity unload is not a shared kill. */
  }
  d->actor = 0;
  d->saved = 0;
  if (d->kind == WD_DOLL) {
    /* A recycled native slot is not a collection. Keep its last checkpoint
     * available for reconstruction instead of manufacturing a tombstone. */
    d->doll_initialized = 0;
    return;
  }
  if (d->kind && d->eligible && d->ready && !d->placed &&
      ((d->kind != WD_SHUTTER_ENEMY && !family_actor(d)) || !d->have || d->owner == d_self ||
       d->row[WD_LIFE] == WD_REMOVED)) {
    if (d->kind==WD_WAVE && d->row[WW_ROLE] && !d->claimed &&
        d->row[WD_LIFE]!=WD_REMOVED)
      d->row[WD_LANDED]=0; /* native expiry/cull has no hit presentation */
    if (family_actor(d) && d->row[WD_LIFE]!=WD_REMOVED)
      d->row[WD_COMMITTER]=(int)d_self;
    d->row[WD_LIFE] = WD_REMOVED;
    d->row[WD_OWNER] = 0;
    if ((d->kind == WD_SHUTTER_ENEMY || family_actor(d)) && !d->row[WD_COMMITTER])
      d->row[WD_COMMITTER] = (int)d_self;
  } else
    clear(d, sizeof(*d));
}
RECOMP_HOOK("func_80221A90_5DCF60")
void world_dynamic_npc(void *actor) {
  DynamicActor *d;
  if (d_building || WORLD_DYNAMIC_CASTLE_SIGN_OWNS(actor) ||
      anchor_world_actor_placed(actor) || anchor_world_bridge_owns(actor))
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
      anchor_world_actor_placed(actor) || anchor_world_bridge_owns(actor) || H(actor, 0x5c) == 0x2bc)
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
  if (d->random_birth && H(a,0x5c)==WR_ENTITY && H(a,0x5e)==WR_CHILD_MODEL &&
      d->animated && !d->clip &&
      ((unsigned long)DPTR(a,0xc)&~DDISABLED)==(unsigned long)func_08007BDC_6C732C) {
    d->kind=WD_RANDOM;d->random_initialized=1;
    d->row[WD_PHASE]=WR_CHILD;d->row[WR_ROLE]=1;
    d->row[WR_INSTANCE]=(int)d->serial;d->row[WR_PRESENT]=1;return;
  }
  if (d->fragile_birth && H(a,0x5c)==d->row[WD_ENTITY] &&
      H(a,0x5e)==d->row[WD_MODEL] && B(a,0x8d)==1 &&
      func_800141C4_14DC4(30)!=-1 &&
      fragile_native_ok((unsigned int)d->row[WD_ENTITY],1,DPTR(a,0xc))) {
    d->kind=WD_FRAGILE;d->fragile_initialized=1;
    d->row[WD_PHASE]=WF_PHASE;d->row[WF_INSTANCE]=(int)d->serial;
    d->row[WF_PRESENT]=1;return;
  }
  if (d->slicer_birth && H(a,0x5c)==0x19d && H(a,0x5e)==0x19d &&
      d->animated && !d->clip &&
      ((unsigned long)DPTR(a,0xc)&~DDISABLED)==(unsigned long)func_0800488C_6C3FDC) {
    d->kind=WD_SLICER;d->slicer_initialized=1;
    d->row[WD_PHASE]=WS_FLIGHT;d->row[WS_INSTANCE]=(int)d->serial;
    d->row[WS_PRESENT]=1;
    return;
  }
  if (d->doll_birth && !H(a, 0x5c) && !H(a, 0x5e) &&
      H(a, 0xd0) == 0xee && d->clip == 2 && !d->animated &&
      ((unsigned long)DPTR(a, 0xc) & ~DDISABLED) == (unsigned long)func_080005F8_6AEDD8) {
    d->kind = WD_DOLL;
    d->row[WD_PHASE] = F(DPTR(a, 0x18), 12) < 35 ? 17 : 16;
    d->doll_initialized = 1;
    DPTR(a, 0xc) = (void *)doll_callback;
    return;
  }
  if (d->shutter_birth && H(a, 0x5c) == 0xfc && H(a, 0x5e) == 0xfb &&
      d->animated && !d->clip && H(a, 0xc4) == 57 &&
      func_800141C4_14DC4(32) != -1 &&
      ((unsigned long)DPTR(a, 0xc) & ~DDISABLED) == (unsigned long)func_08001FF4_6D10D4) {
    d->kind = WD_SHUTTER_ENEMY;
    d->row[WD_PHASE] = 15;
    d->enemy_initialized = 1;
    return;
  }
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
  if (d && (d->kind==WD_BOULDER || d->kind==WD_FISH) && !alive(d)) {
    /* The native pool may reuse a boulder task address before this post hook.
     * Preserve its room checkpoint, but never trust the stale generation. */
    d->actor=0;d->saved=0;d=0;
  }
  if (d && d->kind==WD_FRAGILE && d->placed && !d->ready &&
      !fragile_register_root(actor)) return;
  if (!d && !d_building) d=slicer_register_root(actor);
  if (!d && !d_building) d=random_register_root(actor);
  if (!d && !d_building) d=bomb_register_root(actor);
  if (!d && !d_building) d=wave_register_root(actor);
  if (!d && !d_building) d=fragile_register_root(actor);
  if (!d && !d_building) d=boulder_register_root(actor);
  if (!d && !d_building) d=fish_register_root(actor);
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
  if (d->kind==WD_BOMB && d->bomb_birth && !d->proxy) d->bomb_initialized=1;
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
    if (!d->placed && !d->stable_ordinal && d->parent && d->parent <= 256 && d->kind >= WD_COIN &&
        d->kind <= WD_FOOD)
      d->row[WD_ORDINAL] = (int)d_loot_ordinals[d->parent][d->kind - WD_COIN]++;
    if (d->stable_ordinal) {
      for (unsigned int i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
        DynamicActor *other = &d_actors[i];
        if (other != d && other->used && other->ready &&
            (other->eligible || other->have) && other->kind == d->kind &&
            (!family_actor(d) || other->row[WS_ROLE]==d->row[WS_ROLE]) &&
            other->parent == d->parent && other->row[WD_ORDINAL] == d->row[WD_ORDINAL]) {
          /* A takeover may revisit an already published reward boundary.
           * Keep the existing native copy and its claim, without a new death. */
          W(actor, 0x68) |= 2u;
          clear(d, sizeof(*d));
          return;
        }
      }
    }
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
  if (d->kind == WD_DOLL && !(W(actor,0x68)&2u)) capture(d);
  if (W(actor, 0x68) & 2u) {
    if (d->kind==WD_FRAGILE && d->row[WD_LIFE]!=WD_REMOVED) {
      world_dynamic_reuse(actor);
      return;
    }
    if (family_actor(d) && !d->row[WS_ROLE]) {
      world_dynamic_reuse(actor);
      return;
    }
    if (d->kind == WD_DOLL && d->row[WD_LIFE] != WD_REMOVED) {
      /* A durable save bit can precede the room tombstone. Do not turn a
       * local unavailability or that packet race into a second award. */
      world_dynamic_reuse(actor);
      return;
    }
    if (d->placed && d->row[WD_LIFE] != WD_REMOVED) {
      clear(d, sizeof(*d)); /* placement culling is not a shared collection */
      return;
    }
    if (d->row[WD_LIFE] != WD_REMOVED && d->eligible && !d->claimed &&
        (!d->have || d->owner == d_self)) {
      d->row[WD_LIFE] = WD_REMOVED;
      d->row[WD_OWNER] = 0;
      if (d->kind == WD_SHUTTER_ENEMY || family_actor(d))
        d->row[WD_COMMITTER] = (int)d_self;
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
  if (d->kind==WD_FISH) return fish_capture(d);
  if (d->kind==WD_BOMB) return bomb_capture(d);
  if (d->kind==WD_BOULDER && (!alive(d) || d->boulder_frozen))
    return d->ready && anchor_world_dynamic_row_valid(r);
  if (d->kind==WD_WAVE && (!alive(d) || !d->wave_initialized))
    return d->ready && anchor_world_dynamic_row_valid(r);
  if (d->kind==WD_RANDOM && (!alive(d) || !d->random_initialized))
    return d->ready && anchor_world_dynamic_row_valid(r);
  if (d->kind==WD_SLICER && (!alive(d) || !d->slicer_initialized))
    return d->ready && anchor_world_dynamic_row_valid(r);
  if (d->kind==WD_FRAGILE && r[WD_ENTITY]==0x339 &&
      r[WD_LIFE]!=WD_REMOVED && func_800240DC_24CDC(0x32))
    return 0;
  if (d->kind==WD_FRAGILE && (!alive(d) || !d->fragile_initialized))
    return d->ready && anchor_world_dynamic_row_valid(r);
  if (family_actor(d) && !r[WS_ROLE] && r[WD_CID] && !r[WS_RECEIPT])
    return anchor_world_dynamic_row_valid(r);
  if (d->kind == WD_DOLL && !alive(d) && func_800240DC_24CDC(0xee) &&
      r[WD_LIFE] != WD_REMOVED) {
    /* A Doll collected during offline play has already finished its local
     * scene. Publish the explicit save-backed removal when reconnecting. */
    r[WD_LIFE]=WD_REMOVED;r[WD_OWNER]=0;r[WD_COMMITTER]=(int)d_self;
  }
  if (r[WD_LIFE] == WD_REMOVED) {
    if (d->kind == WD_DOLL && !r[WD_COMMITTER] && d_active && func_800240DC_24CDC(0xee))
      r[WD_COMMITTER]=(int)d_self;
    return anchor_world_dynamic_row_valid(r);
  }
  if ((d->kind==WD_SLICER && d->slicer_frozen) ||
      (d->kind==WD_RANDOM && d->random_frozen) ||
      (d->kind==WD_WAVE && d->wave_frozen)) {
    r[WD_LIFE]=d->claimed ? WD_CLAIM : WD_LIVE;
    r[WD_LANDED]=d->claimed!=0;r[WD_PAUSED]=anchor_world_is_paused()!=0;
    r[WS_PRESENT]=alive(d);r[WS_INSTANCE]=(int)d->serial;
    if (d->kind==WD_WAVE) wave_hold(d);
    return anchor_world_dynamic_row_valid(r);
  }
  if (d->kind == WD_SHUTTER_ENEMY && d->proxy && !d->enemy_initialized)
    return anchor_world_dynamic_row_valid(r);
  if (d->kind == WD_DOLL && (!alive(d) || !d->doll_initialized))
    return d->ready && anchor_world_dynamic_row_valid(r);
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
  if (d->kind == WD_DOLL) {
    r[WD_PHASE] = r[WD_Y] < 3500 ? 17 : 16;
    r[WD_ROUTE] = 163;
    r[WD_TALKABLE] = r[WD_DIALOG] = r[WD_BUSY] = 0;
  }
  if (d->kind == WD_NPC || d->kind == WD_SHUTTER_ENEMY) {
    int enemy = d->kind == WD_SHUTTER_ENEMY;
    r[WD_PHASE] = enemy ? 15 : 0;
    if (!enemy && !anchor_world_npc_capture(a, d->saved ? d->saved : DPTR(a, 0xc),
                                  r + WD_NPC_CHECKPOINT))
      return 0;
    r[WD_ROUTE] = d->path || enemy ? H(a, 0xc4) : 163;
    r[WD_TALKABLE] = d->talkable;
    r[WD_DIALOG] = H(a, 0xa4);
    if (enemy) {
      r[WD_TALKABLE] = r[WD_DIALOG] = r[WD_BUSY] = 0;
      r[WD_BOUNCE] = B(a, 0x8c);
      r[WD_LANDED] = d->claimed != 0;
      r[WD_BASE_Y] = (W(a, 0x68) & 0x400u) != 0;
    }
    if (d->path || enemy) {
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
  if (d->kind==WD_SLICER && !slicer_capture(d)) return 0;
  if (d->kind==WD_FRAGILE && !fragile_capture(d)) return 0;
  if (d->kind==WD_RANDOM && !random_capture(d)) return 0;
  if (d->kind==WD_WAVE && !wave_capture(d)) return 0;
  if (d->kind==WD_BOULDER && !boulder_capture(d)) return 0;
  return anchor_world_dynamic_row_valid(r);
}
#include "anchor_world_slicer.inc"
#include "anchor_world_random.inc"
#include "anchor_world_bomb.inc"
#include "anchor_world_wave.inc"
#include "anchor_world_fragile.inc"
#include "anchor_world_boulder.inc"
#include "anchor_world_fish.inc"

RECOMP_HOOK("func_080002C0_72ACF0")
void world_dynamic_fish_active_pre(void *actor, void *object) {
  DynamicActor *d;
  (void)object;
  if (!d_active || d_room!=D_800C7AB2 || !actor) return;
  /* The first active native tick may precede the common post hook. Claim
   * that contact here, before File68 can increment its color counter. */
  d=fish_register_root(actor);
  if (!d || d->fish_admit) return;
  if (W(actor,0x68)&0x200u) {
    if (!d->fish_awarded) d->claimed=1;
    W(actor,0x68)&=~0x200u;
  }
}

static void bomb_child_traits(void *a,DynamicCallback initializer) {
  DynamicActor *d=lookup(a);
  if (d_building || !d || !d->bomb_birth) return;
  for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *root=&d_actors[i];
    if (root->used && root->kind==WD_BOMB && !root->row[WB_ROLE] && root->parent==d->parent) {
      bomb_child(root,d,initializer);
      return;
    }
  }
}
RECOMP_HOOK("func_08000984_6E9AD4")
void world_dynamic_bomb_ring_traits(void *a) { bomb_child_traits(a,func_08000984_6E9AD4); }
RECOMP_HOOK("func_08000C74_6E9DC4")
void world_dynamic_bomb_particle_traits(void *a) { bomb_child_traits(a,func_08000C74_6E9DC4); }
/* Both asset waves and every requested list entry must be resident. Calling
 * the native binder with a missing resource deliberately faults the game. */
static int resource_valid(const int *r) {
  void *model;
  const unsigned short *files;
  const unsigned int *clips;
  unsigned int i, highest = (unsigned int)r[WD_CLIP];
  if (r[WD_KIND]==WD_BOMB) return bomb_resource_valid(r);
  if (r[WD_KIND]==WD_BOULDER) {
    if (func_800141C4_14DC4(WBO_FILE)==-1 ||
        !anchor_world_boulder_valid(r)) return 0;
    if (r[WD_PHASE]<3) return 1;
    model=D_80236984_5F1E54[WBO_ACTIVE_MODEL];
    if (!model || !(files=(const unsigned short *)DPTR(model,0)) ||
        !(clips=(const unsigned int *)DPTR(model,4)) || !clips[0]) return 0;
    if ((files[0] && func_800141C4_14DC4(files[0])==-1) ||
        (files[1] && func_800141C4_14DC4(files[1])==-1)) return 0;
    return 1;
  }
  if (r[WD_KIND]==WD_FISH)
    return anchor_world_fish_valid(r) &&
           anchor_world_fish_placement(D_800C7AB2,r[WD_PARENT],0,0);
  if (r[WD_KIND]==WD_WAVE) return wave_resource(r);
  if (r[WD_KIND]==WD_RANDOM) {
    if (!anchor_world_random_scope(r,D_800C7AB2) || func_800141C4_14DC4(30)==-1) return 0;
    if (!r[WR_ROLE]) return 1;
  }
  if (r[WD_KIND]==WD_SLICER) {
    if (!anchor_world_slicer_scope(r,D_800C7AB2) ||
        func_800141C4_14DC4(30)==-1) return 0;
    /* Placed emitters have no model/clip binding. */
    if (!r[WS_ROLE]) return 1;
  }
  if (r[WD_KIND] == WD_SHUTTER_ENEMY)
    highest = 4; /* Live clip0 and the two native death fragment resources3/4. */
  if (r[WD_MODEL] > 1025 || r[WD_MODEL] < 0)
    return 0;
  /* The nested Doll restores its inherited selector to zero after binding
   * model1/slot2. Its native selector is not its appearance resource. */
  model = D_80236984_5F1E54[r[WD_KIND] == WD_DOLL ? 1 : r[WD_MODEL]];
  if (!model)
    return 0;
  files = (const unsigned short *)DPTR(model, 0);
  clips = (const unsigned int *)DPTR(model, 4);
  if (!files || !clips || func_800141C4_14DC4(files[0]) == -1 ||
      func_800141C4_14DC4(files[1]) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  if (r[WD_KIND]==WD_RANDOM &&
      (files[0]!=0x18f || files[1]!=0x19f || clips[0]!=0x080009f8u)) return 0;
  if (r[WD_KIND]==WD_SLICER &&
      (files[0]!=0x1d6 || files[1]!=0x180 || clips[0]!=0x0800001cu ||
       clips[1]!=0x080001d8u)) return 0;
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
  if (r[WD_KIND] == WD_NPC &&
      (!anchor_world_npc_valid(r[WD_ENTITY], r[WD_MODEL],
                               r + WD_NPC_CHECKPOINT) ||
       !anchor_world_npc_resident(r + WD_NPC_CHECKPOINT)))
    return 0;
  if (r[WD_KIND] == WD_SHUTTER_ENEMY &&
      (func_800141C4_14DC4(32) == -1 || r[WD_ROUTE] != 57 ||
       r[WD_MODEL] != 0xfb || r[WD_ENTITY] != 0xfc))
    return 0;
  if (r[WD_KIND] == WD_DOLL &&
      (!anchor_world_doll_parent_valid((unsigned int)r[WD_PARENT]) ||
       func_800141C4_14DC4(26) == -1 || func_800141C4_14DC4(62) == -1))
    return 0;
  if ((r[WD_KIND] == WD_NPC || r[WD_KIND] == WD_SHUTTER_ENEMY) && r[WD_ROUTE] < 163 &&
      !(world_path_pc[r[WD_ROUTE]][r[WD_PATH_PC] >> 3] &
        (1u << (r[WD_PATH_PC] & 7))))
    return 0;
  if (r[WD_KIND] == WD_NPC && r[WD_TALKABLE] && r[WD_DIALOG] >= 794)
    return 0;
  if (((r[WD_FLAGS_HI] & 0x800) || r[WD_KIND] == WD_SHUTTER_ENEMY) && r[WD_MODEL] != 1) {
    model = D_80236984_5F1E54[1];
    if (!model || !(files = (const unsigned short *)DPTR(model, 0)) ||
        func_800141C4_14DC4(files[0]) == -1 ||
        func_800141C4_14DC4(files[1]) == -1)
      return 0;
    if (r[WD_KIND] == WD_SHUTTER_ENEMY &&
        (!DPTR(model, 4) || !((unsigned int *)DPTR(model, 4))[0]))
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
  return (r[WD_KIND] >= WD_COIN && r[WD_KIND] <= WD_FOOD) || r[WD_KIND] == WD_DOLL
             ? 0x8000u
             : (unsigned short)r[WD_PITCH + axis];
}
static int apply(DynamicActor *d) {
  int *r = d->net;
  void *a = d->actor, *o;
  unsigned int j;
  if (!d->dirty)
    return 1;
  if (d->kind==WD_FRAGILE) return fragile_apply(d);
  if (d->kind==WD_BOULDER) return boulder_apply(d);
  if (d->kind==WD_FISH) {
    if (!anchor_world_fish_valid(r) ||
        !anchor_world_fish_placement(D_800C7AB2,r[WD_PARENT],0,0) ||
        r[WFISH_FLAG]!=d->row[WFISH_FLAG] ||
        r[WFISH_VARIANT]!=d->row[WFISH_VARIANT]) return 0;
    d->dirty=0;
    return 1;
  }
  if (!alive(d) || !resource_valid(r))
    return 0;
  if (d->kind==WD_BOMB) {
    int claimed_cause=d->row[WB_CAUSE];
    if (!r[WB_RECEIPT] || r[WB_INSTANCE]!=(int)d->serial || r[WB_ROLE]!=d->row[WB_ROLE] ||
        (!d->bomb_initialized && D_8016DAB4_16E6B4!=a) || !bomb_apply_extra(d,r)) return 0;
    for (j=0;j<WORLD_DYNAMIC_WORDS;++j) d->row[j]=r[j];
    if (d->claimed) {
      d->row[WD_LIFE]=WD_CLAIM;d->row[WD_LANDED]=1;
      d->row[WB_CAUSE]=claimed_cause;d->row[WD_PHASE]=WB_EXPLODE;
    }
    d->row[WB_PRESENT]=1;d->row[WB_INSTANCE]=(int)d->serial;
    B(a,0x75)=(unsigned char)r[WD_GRAVITY];
    H(a,0x3c)=(unsigned short)r[WD_RADIUS];H(a,0x3e)=(unsigned short)r[WD_HEIGHT];
    S(a,0x40)=(short)r[WD_OFFSET];H(a,0x96)=(unsigned short)r[WD_MASK];
    H(a,0x94)=(unsigned short)r[WD_MASK94];S(a,0x42)=(short)r[WD_BODY_OFFSET2];
    for (j=0;j<4;++j) B(a,0x98+j)=(unsigned char)r[WD_DIM0+j];
    o=DPTR(a,0x18);
    F(o,0x68)=(float)r[WD_OBJECT_RADIUS]/1000.f;F(o,0x6c)=(float)r[WD_OBJECT_HEIGHT]/1000.f;
    d->native=(void *)bomb_native;
    if (d->saved) d->saved=d->native;else DPTR(a,0xc)=d->native;
    if (!r[WD_OWNER] || r[WD_PAUSED] || d->claimed) {
      d->bomb_frozen=1;W(a,0x60)=0;F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    }
    d->dirty=0;
    return 1;
  }
  if (d->kind==WD_RANDOM &&
      (!d->random_initialized || !r[WR_RECEIPT] || r[WR_INSTANCE]!=(int)d->serial ||
       r[WR_ROLE]!=d->row[WR_ROLE] || !anchor_world_random_scope(r,D_800C7AB2) ||
       (!r[WR_ROLE] && d->row[WD_ORDINAL]>r[WD_ORDINAL]))) return 0;
  if (d->kind==WD_SLICER &&
      (!d->slicer_initialized || !r[WS_RECEIPT] || r[WS_INSTANCE]!=(int)d->serial ||
       r[WS_ROLE]!=d->row[WS_ROLE] || !anchor_world_slicer_scope(r,D_800C7AB2) ||
       (!r[WS_ROLE] && d->row[WD_ORDINAL]>r[WD_ORDINAL])))
    return 0;
  if (d->kind==WD_WAVE && !wave_gate(r,d)) return 0;
  if (d->kind == WD_DOLL) {
    if (!d->doll_initialized || d->doll_scene) return 0;
    o = DPTR(a, 0x18);
    /* A delayed checkpoint or a newly arrived copy cannot lift it back up. */
    float y = (float)r[WD_Y] / 100.0f;
    if (F(o,12) > y) F(o,12) = y;
    for (j=0;j<3;++j) F(a,0x78+j*4)=0;
    d->dirty=0;
    return 1;
  }
  if (d->kind == WD_SHUTTER_ENEMY && !d->enemy_initialized)
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
    if (d->kind==WD_RANDOM) W(a,0x60)=r[WR_ROLE] ? 0x002006e7u : 0;
    else if (d->kind==WD_SLICER) W(a,0x60)=0x002006e1u;
    else if (d->kind == WD_SHUTTER_ENEMY)
      W(a, 0x60) = 0x002e8261u | shadow;
    else W(a, 0x60) =
        (((unsigned int)r[WD_FLAGS_LO] | ((unsigned int)r[WD_FLAGS_HI] << 16)) &
         (0x16e00febu | shadow)) |
        texture_loop;
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
  if (d->kind==WD_WAVE) {
    wave_apply(d);
  } else if (d->kind==WD_RANDOM) {
    d->row[WD_COMMITTER]=r[WD_COMMITTER];
    for (j=WR_ROLE;j<=WR_RESERVED1;++j) d->row[j]=r[j];
    d->row[WD_PHASE]=r[WD_PHASE];d->row[WD_ORDINAL]=r[WD_ORDINAL];
    for (j=WD_X;j<=WD_SZ;++j) d->row[j]=r[j];
    d->row[WD_TIMER]=r[WD_TIMER];d->row[WR_RECEIPT]=r[WR_RECEIPT];d->row[WR_PRESENT]=1;
    d->native = (void *)(r[WR_ROLE] && d->proxy ? random_native : random_phase(r[WD_PHASE]));
    if (d->saved) d->saved=d->native;
    else DPTR(a,0xc)=d->native;
    if (!r[WD_OWNER] || r[WD_PAUSED] || d->claimed) {
      d->random_frozen=1;W(a,0x60)=0;F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    }
  } else if (d->kind==WD_SLICER) {
    d->row[WD_COMMITTER]=r[WD_COMMITTER];
    for (j=WS_ROLE;j<=WS_INITIAL;++j) d->row[j]=r[j];
    d->row[WD_PHASE]=r[WD_PHASE];d->row[WD_ORDINAL]=r[WD_ORDINAL];
    /* Keep an exact checkpoint while native prediction is frozen; do not
     * publish the zero velocity used only to suppress the local post. */
    for (j=WD_X;j<=WD_SZ;++j) d->row[j]=r[j];
    d->row[WD_TIMER]=r[WD_TIMER];
    d->row[WS_RECEIPT]=r[WS_RECEIPT];d->row[WS_PRESENT]=1;
    B(a,0xd0)=(unsigned char)r[WS_SUBTYPE];B(a,0xd1)=(unsigned char)r[WS_REPEAT];
    B(a,0xd2)=(unsigned char)r[WS_INITIAL];B(a,0xd3)=(unsigned char)r[WS_SPEED];
    if (r[WS_ROLE]) DPTR(a,0xdc)=a;
    d->native = (void *)slicer_phase(r[WD_PHASE]);
    if (d->saved) d->saved=d->native;
    else DPTR(a,0xc)=d->native;
    if (!r[WD_OWNER] || r[WD_PAUSED] || d->claimed) {
      d->slicer_frozen=1;
      W(a,0x60)=0;
      F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    }
  } else if (d->kind == WD_SHUTTER_ENEMY) {
    H(a, 0xc4) = 57;
    S(a, 0xc6) = (short)r[WD_PATH_TIMER];
    B(a, 0xce) = (unsigned char)r[WD_PATH_STATE];
    B(a, 0xcf) = (unsigned char)r[WD_PATH_PC];
    for (j = 0; j < 3; ++j)
      S(a, 0xc8 + j * 2) = (short)r[WD_ORIGIN_X + j];
    B(a, 0xaa) = (unsigned char)r[WD_FACING];
    DPTR(a, 0x9c) = a;
    W(a, 0x68) = (W(a, 0x68) & ~0x400u) | (r[WD_BASE_Y] ? 0x400u : 0);
    /* The immunity clock is local contact state. Never reopen a pending
     * local hit by importing a peer's older immunity byte. */
    if (!d->claimed && B(a, 0x8c) < r[WD_BOUNCE])
      B(a, 0x8c) = (unsigned char)r[WD_BOUNCE];
    d->native = (void *)func_08001FF4_6D10D4;
    if (d->saved)
      d->saved = (void *)((unsigned long)d->native |
                          ((unsigned long)d->saved & DDISABLED));
    else
      DPTR(a, 0xc) = (void *)((unsigned long)d->native |
                              ((unsigned long)DPTR(a, 0xc) & DDISABLED));
  } else if (d->kind == WD_NPC) {
    if (r[WD_NPC_CHECKPOINT]) {
      d->native = anchor_world_npc_restore(a, r + WD_NPC_CHECKPOINT,
                                           d->proxy && !d->npc_bound);
      d->npc_bound = 1;
      d->saved = d->native;
    }
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
      /* Textured town NPCs use +9C as their mouth selector; the native
       * texture initializer intentionally replaces the path self-target. */
      if (!r[WD_NPC_CHECKPOINT] || r[WD_NPC_CHECKPOINT] == 1 ||
          r[WD_NPC_CHECKPOINT] == 7)
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
/* The helper only owns the descent, not the category9 Doll. Once the child
 * is tracked, remove the helper in its own scheduler slot so it can never
 * dereference a collected/recycled child. 35020 unlinks only the current
 * task; the descendant-deleting 34EF8 is intentionally not used. */
static void doll_helper_retire(void *a, void *o) {
  (void)o;
  if (D_8016DAB4_16E6B4 == a) func_80035020_35C20();
}
RECOMP_HOOK("func_080027AC_723DCC")
void world_dynamic_doll_helper(void *a) {
  d_doll_helper = anchor_world_doll_parent() ? a : 0;
}
RECOMP_HOOK_RETURN("func_080027AC_723DCC")
void world_dynamic_doll_helper_return(void) {
  void *helper = d_doll_helper;
  d_doll_helper = 0;
  if (!helper) return;
  DynamicActor *d = lookup(DPTR(helper,0xec));
  if (d && d->doll_birth) DPTR(helper,0xc) = (void *)doll_helper_retire;
}
static void doll_callback(void *a, void *o) {
  DynamicActor *d = lookup(a);
  if (!d || d->kind != WD_DOLL || !alive(d) ||
      D_8016DAB4_16E6B4 != a || !anchor_world_doll_parent_valid(d->parent)) return;
  void *wrapped = DPTR(a,0xc);
  if (d->doll_scene) {
    /* Cleanup is local, even after disconnect or a competing save update.
     * Only this task can release the controls acquired by its pickup. */
    func_080006E0_6AEEC0(a,o);
    if (alive(d)) DPTR(a,0xc) = wrapped;
    return;
  }
  if (func_800240DC_24CDC(0xee)) {
    /* Durable quest collection may arrive before the room removal packet. */
    d->row[WD_LIFE] = WD_REMOVED;
    d->row[WD_OWNER] = 0;
    if (!d->row[WD_COMMITTER]) d->row[WD_COMMITTER] = (int)d_self;
    W(a,0x68) |= 2u;
    return;
  }
  if (!d->doll_initialized) {
    if (!resource_valid(d->net)) return;
    /* 00514 schedules its next callback on the implicit current task. */
    d_building = d_binding = 1;
    func_08000514_6AECF4(a,o);
    d_building = d_binding = 0;
    d->doll_initialized = 1;
    d->native = (void *)doll_callback;
    if (d->saved) d->saved = (void *)((unsigned long)doll_callback |
                                      ((unsigned long)d->saved & DDISABLED));
    DPTR(a,0xc) = wrapped;
    d->dirty = 1;
    apply(d);
    return;
  }
  if (d->row[WD_LIFE] == WD_REMOVED) {
    if ((unsigned int)d->row[WD_OWNER] != d_self || !d->claimed) {
      W(a,0x68) |= 2u;
      return;
    }
    if (D_800C7AE2 || anchor_world_is_paused()) return;
    d->granted = d->doll_scene = 1;
    W(a,0x68) |= 0x200u;
    func_080005F8_6AEDD8(a,o);
    DPTR(a,0xc) = wrapped;
    return;
  }
  if (anchor_world_is_paused()) return;
  if (d->have) apply(d);
  if (W(a,0x68) & 0x200u) {
    if (!d_active) {
      if (D_800C7AE2) return;
      d->granted = d->doll_scene = 1;
      d->row[WD_LIFE]=WD_REMOVED;d->row[WD_OWNER]=d->row[WD_COMMITTER]=0;
      func_080005F8_6AEDD8(a,o);
      DPTR(a,0xc) = wrapped;
      return;
    }
    d->claimed = 1;
    W(a,0x68) &= ~0x200u;
  }
  if (d->claimed) return;
  if (d_active && (!d->have || d->owner != d_self || d->net[WD_PAUSED])) return;
  /* Exact native 28D8 trajectory: 125 down to 34 over 91 updates. */
  if (F(o,12) >= 35.0f) F(o,12) -= 1.0f;
}
int anchor_world_dynamic_doll_spawn(void *root, unsigned int parent) {
  if (!root || !anchor_world_doll_parent_valid(parent)) return 0;
  if (func_800240DC_24CDC(0xee)) return 1;
  for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *d=&d_actors[i];
    if (d->used && (d->kind==WD_DOLL || d->doll_birth) && d->parent==parent) return 1;
  }
  int r[WORLD_DYNAMIC_WORDS];clear(r,sizeof(r));
  r[WD_SERIAL]=1;r[WD_KIND]=WD_DOLL;r[WD_PARENT]=(int)parent;
  r[WD_CLIP]=2;r[WD_X]=-600;r[WD_Y]=12500;r[WD_Z]=-10200;
  r[WD_SX]=r[WD_SY]=r[WD_SZ]=1000;r[WD_ROUTE]=163;r[WD_PHASE]=16;
  r[WD_ORDINAL]=1;
  DynamicActor *d=reconstruct(r);
  if (!d) return 0;
  /* Local births still get a fresh pre-canonical serial; the Python bridge
   * coalesces it with the sole placed parent identity. */
  d->row[WD_SERIAL]=d->net[WD_SERIAL]=(int)d->serial;
  func_80023DF0_249F0(2);
  return 1;
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
  if (d->kind == WD_DOLL) { doll_callback(actor,object);return; }
  if (d->kind == WD_SLICER) { slicer_callback(d,actor,object);return; }
  if (d->kind == WD_RANDOM) { random_callback(d,actor,object);return; }
  if (d->kind == WD_BOMB) { bomb_callback(d,actor,object);return; }
  if (d->kind == WD_WAVE) { wave_callback(d,actor,object);return; }
  if (d->kind == WD_FRAGILE) { fragile_callback(d,actor,object);return; }
  if (d->kind == WD_BOULDER) { boulder_callback(d,actor,object);return; }
  if (d->kind == WD_FISH) { fish_callback(d,actor,object);return; }
  if (d->kind == WD_SHUTTER_ENEMY && !d->enemy_initialized) {
    /* This pure D4=1 setup binds model/route/display-list/shadow resources.
     * It uses the current native task when scheduling its continuation, so
     * initialize here, never during the frame/network callback. */
    if (!resource_valid(d->net) || D_8016DAB4_16E6B4 != actor)
      return;
    void *wrapped = DPTR(actor, 0xc);
    W(actor, 0xd0) = 57; W(actor, 0xd4) = 1;
    W(actor, 0x70) = 0; B(actor, 0x8d) = 1;
    d_building = d_binding = 1;
    func_08001EA4_6D0F84(actor, object);
    d_building = d_binding = 0;
    d->enemy_initialized = 1;
    d->clip = 0; d->animated = 1;
    d->native = (void *)func_08001FF4_6D10D4;
    d->saved = (void *)((unsigned long)d->native | ((unsigned long)wrapped & DDISABLED));
    DPTR(actor, 0xc) = wrapped;
    d->dirty = 1;
    if (!apply(d))
      return;
  }
  if (d->row[WD_LIFE] == WD_REMOVED) {
    if (d->kind == WD_SHUTTER_ENEMY && d->row[WD_LANDED])
      return; /* The next common pre performs one committed native death. */
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
    if (d->kind == WD_NPC && !(W(actor, 0x68) & 0x100u) && !d->net[WD_PAUSED])
      anchor_world_npc_face(actor, d->net + WD_NPC_CHECKPOINT);
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
/* Native 18350 calls death/drop *inside* the damage function. Defer this
 * verified one-HP route actor before native mutation, never undo death from a
 * return hook. Cooldown matches the native surviving-hit bookkeeping. */
RECOMP_HOOK("func_80218350_5D3820")
void world_dynamic_enemy_damage(void *actor) {
  bomb_damage(actor);
  wave_damage(actor);
  fragile_damage(actor);
  DynamicActor *d = lookup(actor);
  unsigned int status;
  if (!d_active || d_room != D_800C7AB2 || !d || !alive(d) ||
      d->row[WD_LIFE] == WD_REMOVED || B(actor,0x8d)!=1)
    return;
  if (d->kind==WD_FRAGILE) return; /* handled by its typed admission */
  if (d->kind==WD_SHUTTER_ENEMY) {
    if (!d->enemy_initialized || H(actor,0x5e)!=0xfb || H(actor,0xc4)!=57 ||
        W(actor,0xd4)!=1) return;
  } else if (d->kind==WD_RANDOM) {
    if (!d->random_initialized || !d->row[WR_ROLE] ||
        !anchor_world_random_scope(d->row,D_800C7AB2)) return;
  } else if (d->kind!=WD_SLICER || !d->slicer_initialized ||
             !anchor_world_slicer_scope(d->row,D_800C7AB2)) return;
  status = W(actor, 0x68);
  if (!(status & 0x40000u) &&
      !((status & 0x80u) && !(status & 1u) && DPTR(actor, 0x38)))
    return;
  d->claimed = 1;
  W(actor, 0x68) = (status & ~0x40080u) | 1u;
  B(actor, 0x8c) = 60;
  capture(d);
}
RECOMP_HOOK("func_80218E7C_5D434C")
void world_dynamic_enemy_commit(void *actor) {
  DynamicActor *d = lookup(actor);
  if (!d_active || d_room != D_800C7AB2 || !d || !alive(d) || d->death_started ||
      d->row[WD_LIFE] != WD_REMOVED || !d->row[WD_LANDED])
    return;
  if (family_actor(d)) {
    if (d->kind==WD_BOMB || d->kind==WD_WAVE) return;
    /* Bombs explode in their own callback; waves retire without a native
     * common death, preserving the local hit effect and no-loot contract. */
    if (d->kind==WD_SLICER ? !d->slicer_initialized :
        d->kind==WD_FRAGILE ? !d->fragile_initialized :
        (!d->random_initialized || !d->row[WR_ROLE])) return;
    if (d->kind==WD_FRAGILE && d->row[WD_ENTITY]==0x339 &&
        func_800240DC_24CDC(0x32)) return;
    d->death_started=1;B(actor,0x8d)=1;W(actor,0x68)|=0x40000u;
    if ((unsigned int)d->row[WD_COMMITTER]!=d_self) W(actor,0x64)|=0x8000u;
    return; /* Native common fast death, no robot-specific fragments. */
  }
  if (d->kind!=WD_SHUTTER_ENEMY || !d->enemy_initialized) return;
  d->death_started = 1;
  /* Fast native death has no presentation. Reuse the two non-attacking
   * category8 fragment constructors from the ordinary robot death branch;
   * selector bytes are written after allocation, before their scheduled init. */
  func_80038B98_39798(0x222);
  if (resource_valid(d->row)) {
    DynamicCallback effects[2] = {func_802130C8_5CE598, func_8021332C_5CE7FC};
    d_building = 1;
    for (unsigned int i = 0; i < 2; ++i) {
      void *child = func_802171A8_5D2678(actor, effects[i], 8);
      if (child && DPTR(child, 0x18)) {
        for (unsigned int j = 0; j < 3; ++j)
          F(DPTR(child, 0x18), 0x1c + j * 4) = F(DPTR(actor, 0x18), 0x1c + j * 4);
        B(child, 0x8e + i) = (unsigned char)(3 + i);
      }
    }
    d_building = 0;
  }
  B(actor, 0x8d) = 1;
  W(actor, 0x68) |= 0x40000u;
  /* Native fast deaths roll at most one coin/health child. Only the arbiter
   * rolls it; every other peer uses the verified native suppression flag. */
  if ((unsigned int)d->row[WD_COMMITTER] != d_self)
    W(actor, 0x64) |= 0x8000u;
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
    if (!d->row[WD_CID] && family_actor(d) && r[WD_KIND]==d->kind &&
        d->parent==r[WD_PARENT] && d->row[WS_ROLE]==r[WS_ROLE] &&
        ((d->kind==WD_BOULDER || d->kind==WD_FISH) ? d->row[WD_ORDINAL]==r[WD_ORDINAL] :
         (!r[WS_ROLE] || d->row[WD_ORDINAL]==r[WD_ORDINAL]))) return d;
    if (!d->row[WD_CID] && d->placed && r[WD_ORDINAL] == 0x7fffffff &&
        r[WD_PARENT] == d->parent)
      return d;
    if (!d->row[WD_CID] && (r[WD_CID] == 0x7ffffffd || r[WD_CID] == 0x7ffffffb ||
        ((r[WD_CID]==WS_LOOT_ORIGIN || r[WD_CID]==WR_LOOT_ORIGIN ||
          r[WD_CID]==WF_LOOT_ORIGIN) &&
         r[WD_ENTITY]==d->row[WD_ENTITY] && r[WD_BASE_Y]==d->row[WD_BASE_Y])) && r[WD_KIND] == d->kind &&
        r[WD_KIND] >= WD_COIN && r[WD_KIND] <= WD_FOOD &&
        r[WD_PARENT] == d->parent && r[WD_ORDINAL] == d->row[WD_ORDINAL])
      return d;
    if (!d->row[WD_CID] && d->kind == WD_SHUTTER_ENEMY &&
        r[WD_CID] == 0x7ffffffc && r[WD_KIND] == WD_SHUTTER_ENEMY &&
        r[WD_PARENT] == d->parent && r[WD_ORDINAL] == d->row[WD_ORDINAL])
      return d;
    if (!d->row[WD_CID] && d->kind == WD_DOLL &&
        r[WD_CID] == 0x7ffffffa && r[WD_KIND] == WD_DOLL &&
        r[WD_PARENT] == d->parent) return d;
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
  if (r[WD_LIFE] != WD_LIVE ||
      ((r[WD_KIND]==WD_SLICER || r[WD_KIND]==WD_RANDOM ||
        r[WD_KIND]==WD_BOMB || r[WD_KIND]==WD_WAVE) && !r[WS_ROLE]) ||
      r[WD_KIND]==WD_FRAGILE || r[WD_KIND]==WD_BOULDER || r[WD_KIND]==WD_FISH ||
      (r[WD_ORDINAL] == 0x7fffffff && r[WD_KIND] != WD_SHUTTER_ENEMY &&
       r[WD_KIND]!=WD_SLICER && r[WD_KIND]!=WD_RANDOM && r[WD_KIND]!=WD_WAVE &&
       r[WD_CID]!=WR_LOOT_ORIGIN && r[WD_CID]!=WS_LOOT_ORIGIN &&
       r[WD_CID]!=WF_LOOT_ORIGIN && r[WD_CID] != 0x7ffffffb) ||
      !D_801FC604_5B8514 || !resource_valid(r))
    return 0;
  d = allocate(0);
  if (!d)
    return 0;
  d_building = 1;
  a = func_802171A8_5D2678(D_801FC604_5B8514, dynamic_callback,
                           (unsigned char)(r[WD_KIND]==WD_BOMB ?
                                             (r[WB_ROLE]==WB_ROLE_V4 || r[WB_ROLE]==WB_ROLE_V8 ? 1 : 8)
                                           : r[WD_KIND]==WD_WAVE ? WW_CHILD_CATEGORY
                                           : r[WD_KIND] == WD_NPC || r[WD_KIND]==WD_SLICER || r[WD_KIND]==WD_RANDOM ? 1
                                           : r[WD_KIND] == WD_SHUTTER_ENEMY ? 6
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
  d->parent = (unsigned short)r[WD_PARENT];
  d->native = d->kind == WD_DOLL ? (void *)doll_callback : (void *)npc_native;
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
  if (family_actor(d)) {
    if (d->kind==WD_SLICER) d->slicer_birth=1;
    else if (d->kind==WD_BOMB) d->bomb_birth=1;
    else if (d->kind==WD_WAVE) d->wave_birth=1;
    else d->random_birth=1;
    d->stable_ordinal=1;
    d->row[WS_INSTANCE]=(int)d->serial;d->row[WS_RECEIPT]=0;
    d->row[WS_ESTABLISHED]=0;d->row[WS_PRESENT]=1;
    unsigned int code=d->kind==WD_BOMB ? 40 : d->kind==WD_WAVE ? WW_FILE_CODE : 30;
    H(a,0x28)=(unsigned short)code;W(a,0x2c)=(unsigned int)func_800141C4_14DC4(code);
    DPTR(a,0x84)=0;DPTR(a,0x90)=0;
    DPTR(a,0x8)=(void *)func_8021925C_5D472C;
    DPTR(a,0x10)=(void *)func_80218F30_5D4400;
    /* Inherited collision fields are restored from the typed row by apply;
     * suppress common work until scheduled native initialization and receipt. */
    W(a,0x60)=0;W(a,0x64)=0;W(a,0x48)=0;
    d->native=(void *)(d->kind==WD_SLICER ? slicer_native :
                       d->kind==WD_BOMB ? bomb_native :
                       d->kind==WD_WAVE ? wave_native : random_native);
    DPTR(a,0xc)=(void *)dynamic_callback;
  } else if (d->kind == WD_DOLL) {
    /* Exact fields written by File62 before scheduled File26 initialization.
     * CD00+8 is the native byte generation counter at 8015CD08. */
    H(a,0x28)=26;W(a,0x2c)=(unsigned int)func_800141C4_14DC4(26);
    B(a,0x74)=D_8015CD00[8]++;d->generation=B(a,0x74);
    DPTR(a,0x8)=(void *)func_8021925C_5D472C;
    DPTR(a,0x10)=(void *)func_80218F30_5D4400;
    H(a,0xd0)=0xee;H(a,0xd2)=0;DPTR(a,0x84)=0;
    /* 80218C28 copies these from its allocator parent. The native producer's
     * helper has zero dimensions/body collider/mask; our category-list anchor
     * is the player, whose collision context must not leak into the Doll. */
    H(a,0x3c)=H(a,0x3e)=H(a,0x40)=H(a,0x96)=0;
    for (j=0;j<4;++j) B(a,0x98+j)=0;
    F(DPTR(a,0x18),0x68)=F(DPTR(a,0x18),0x6c)=0;
    d->clip=2;d->doll_birth=d->stable_ordinal=1;
    DPTR(a,0xc)=(void *)doll_callback;
    func_80023DF0_249F0(2);
  } else if (d->kind != WD_SHUTTER_ENEMY)
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
  if ((r[WD_FLAGS_HI] & 0x800) && d->kind != WD_SHUTTER_ENEMY && d->kind != WD_DOLL && !family_actor(d))
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
static void doll_recover(DynamicActor *d) {
  if (!d->used || d->kind != WD_DOLL || alive(d) || d->doll_scene ||
      d->row[WD_LIFE] == WD_REMOVED || func_800240DC_24CDC(0xee) ||
      !anchor_world_dynamic_row_valid(d->row)) return;
  int r[WORLD_DYNAMIC_WORDS];
  for (unsigned int j=0;j<WORLD_DYNAMIC_WORDS;++j) r[j]=d->row[j];
  r[WD_LIFE]=WD_LIVE;
  DynamicActor *fresh=reconstruct(r);
  if (!fresh) return;
  fresh->claimed=d->claimed;fresh->have=d->have;fresh->owner=d->owner;
  clear(d,sizeof(*d));
}
void anchor_world_dynamic_frame(unsigned int room, unsigned int signature,
                                unsigned int visit, int active) {
  unsigned int i, j, n = 0, received = 0, leader = 0;
  char *reply;
  (void)signature;
  restore();
  ++d_tick;
  for (i=0;i<WORLD_DYNAMIC_MAX;++i) doll_recover(&d_actors[i]);
  if (!active) {
    if (d_active) {
      d_active = 0;
      for (i = 0; i < WORLD_DYNAMIC_MAX; ++i) {
        d_actors[i].have = 0;
        d_actors[i].claimed = 0;
        if (d_actors[i].kind==WD_BOMB) {
          bomb_detach(&d_actors[i]);
          continue;
        }
        if (d_actors[i].kind==WD_WAVE) {
          wave_detach(&d_actors[i]);
          continue;
        }
        if (alive(&d_actors[i]) && d_actors[i].proxy) {
          if ((d_actors[i].kind==WD_SLICER && !d_actors[i].slicer_initialized) ||
              (d_actors[i].kind==WD_RANDOM && !d_actors[i].random_initialized))
            W(d_actors[i].actor,0x68)|=2u;
          else DPTR(d_actors[i].actor, 0xc) = d_actors[i].native;
        }
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
      d->kind!=WD_FRAGILE &&
          !(family_actor(d) && !d->row[WS_ROLE]) &&
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
        d->row[WD_COMMITTER] = r[WD_COMMITTER];
        if (d->kind == WD_SHUTTER_ENEMY || family_actor(d))
          d->row[WD_LANDED] = r[WD_LANDED];
        if (!d->actor && !family_actor(d))
          clear(d, sizeof(*d));
        continue;
      }
      if (r[WD_KIND] != d->kind || r[WD_ENTITY] != d->row[WD_ENTITY])
        continue;
      if (family_actor(d) &&
          (d->kind==WD_SLICER ? d->slicer_initialized :
           d->kind==WD_BOMB ? d->bomb_initialized :
           d->kind==WD_WAVE ? d->wave_initialized :
           d->kind==WD_BOULDER ? d->boulder_initialized :
           d->kind==WD_FRAGILE ? d->fragile_initialized : d->random_initialized) &&
          (r[WS_INSTANCE]!=(int)d->serial || !r[WS_RECEIPT] ||
           (!r[WS_ROLE] && r[WD_ORDINAL]<d->row[WD_ORDINAL])))
        continue; /* Do not replace a usable checkpoint with a stale offer. */
      d->eligible = 1;
      d->parent = (unsigned short)r[WD_PARENT];
      if (r[WD_CID] == 0x7ffffffd && r[WD_KIND] >= WD_COIN &&
          r[WD_KIND] <= WD_FOOD &&
          !(r[WD_ENTITY] == 0x3d0 && r[WD_ORDINAL] >= 0xff00) &&
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
      if (d->proxy && !d->saved && d->actor)
        DPTR(d->actor, 0xc) = (void *)dynamic_callback;
      if (d->owner != d_self || d->proxy || family_actor(d))
        apply(d);
    }
  } else
    for (i = 0; i < WORLD_DYNAMIC_MAX; ++i)
      d_actors[i].have = 0;
  if (reply)
    recomp_free(reply);
}
