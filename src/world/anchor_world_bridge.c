/* File51's placed Super Pass Bridge. One checkpoint owns the two guard
 * routes, opening animation and blocker. Dialogue and control locks are local. */
#ifndef WORLD_BRIDGE_HOST_TEST
#include "platform/modding.h"
#endif
#include "world/anchor_world.h"
#include "world/anchor_world_bridge.h"
#include "world/anchor_world_paths.inc"

#define BB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define BH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define BS(p,o) (*(short *)((char *)(p)+(o)))
#define BW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define BF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef BP
#define BP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef BR_DISABLED
#define BR_DISABLED 0x00800000ul
#endif
typedef void (*BridgeCallback)(void *, void *);
extern unsigned short D_800C7AB2;
extern unsigned char D_800C7AE2;
extern void *D_8016DAB4_16E6B4;
extern void *D_80236984_5F1E54[];
extern int func_800141C4_14DC4(unsigned int);
extern int func_800240DC_24CDC(int);
extern int func_80023E94_24A94(int);
extern void func_80023DF0_249F0(int);
extern void func_80038B98_39798(unsigned int);
extern void *func_802171A8_5D2678(void *, BridgeCallback, unsigned char);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern void func_80216E1C_5D22EC(void *, unsigned int);
extern float func_8001B5AC_1C1AC(void *);
extern void func_80224D50_5E0220(void *, int);
extern void func_80226840_5E1D10(void *, unsigned short);
extern int func_802268A8_5E1D78(void *);
#define BR_DECLARE(f) extern void f(void *, void *);
BR_DECLARE(func_08000174_70D8B4)
BR_DECLARE(func_08000088_70D7C8)
BR_DECLARE(func_08000000_70D740)
BR_DECLARE(func_080002D8_70DA18)
BR_DECLARE(func_08000598_70DCD8)
BR_DECLARE(func_08000530_70DC70)
BR_DECLARE(func_080004AC_70DBEC)
BR_DECLARE(func_080006B4_70DDF4)

typedef struct {
  void *actor, *parent;
  BridgeCallback saved;
  unsigned char generation, parent_generation, ready, proxy, clip;
  unsigned int post_flags;
  float post_velocity[3];
} BridgeNode;
/* Gate, guard with dialogue10E, guard with dialogue110, blocker. */
static BridgeNode br_nodes[4];
static BridgeCallback br_guard[3], br_gate[3];
static unsigned int br_flags, br_inputs;
static int br_enabled, br_remote, br_paused, br_restore, br_observed;
static int br_removed, br_fresh, br_gate_fx, br_guard_fx[2], br_post_slot = -1;
static int br_frozen, br_owns_lock;
static int br_building;
static void *br_talk_actor;
static void *br_talk_callback;
static unsigned char br_talk_lock;
static void bridge_callback(void *, void *);
static void blocker_init(void *, void *);

static void addresses(void) {
  br_guard[0] = func_08000174_70D8B4;
  br_guard[1] = func_08000088_70D7C8;
  br_guard[2] = func_08000000_70D740;
  br_gate[0] = func_08000598_70DCD8;
  br_gate[1] = func_08000530_70DC70;
  br_gate[2] = func_080004AC_70DBEC;
}
static void clear(void *p, unsigned int size) {
  for (unsigned int i = 0; i < size; ++i) ((volatile unsigned char *)p)[i] = 0;
}
static int same_actor(const BridgeNode *n) {
  static const unsigned short entities[4] = {0x240,0x2d0,0x2d0,0x311};
  unsigned int index = (unsigned int)(n-br_nodes);
  return n->actor && BP(n->actor,0x18) && BB(n->actor,0x74) == n->generation &&
         BH(n->actor,0x5c) == entities[index];
}
static int live(const BridgeNode *n) {
  return same_actor(n) && !(BW(n->actor,0x68)&2u) &&
         (!n->parent || BB(n->parent,0x74) == n->parent_generation);
}
static int alive(const BridgeNode *n) { return n->ready && live(n); }
static int find(void *a) {
  for (unsigned int i = 0; i < 4; ++i) if (br_nodes[i].actor == a) return (int)i;
  return -1;
}
static BridgeCallback callback(const BridgeNode *n) {
  void *p = n->actor ? BP(n->actor,0xc) : 0;
  if (((unsigned long)p&~BR_DISABLED) == (unsigned long)bridge_callback)
    p = (void *)n->saved;
  if (n->actor && (BW(n->actor,0x68)&0x100u)) p = BP(n->actor,0xb4);
  return (BridgeCallback)((unsigned long)p&~BR_DISABLED);
}
static int phase(const BridgeNode *n, BridgeCallback *table) {
  BridgeCallback p = callback(n);
  for (unsigned int i = 0; i < 3; ++i) if (p == table[i]) return (int)i+1;
  return 0;
}
static void schedule(BridgeNode *n, BridgeCallback next) {
  BP(n->actor,0xc) = (void *)((unsigned long)next |
                             ((unsigned long)BP(n->actor,0xc)&BR_DISABLED));
}
static void detach(BridgeNode *n, int retire) {
  /* A lost parent makes a child unusable for capture, but it still owns its
   * own native slot and must be unwrapped/retired before the binding is lost. */
  if (!same_actor(n)) return;
  if (n->saved && ((unsigned long)BP(n->actor,0xc)&~BR_DISABLED) ==
                   (unsigned long)bridge_callback) schedule(n,n->saved);
  if (retire) BW(n->actor,0x68) |= 2u;
}
void anchor_world_bridge_end(void) {
  for (unsigned int i = 0; i < 4; ++i) {
    detach(&br_nodes[i],0);br_nodes[i].saved = 0;
  }
}
void anchor_world_bridge_reset(int room_changed) {
  anchor_world_bridge_end();
  br_enabled = br_remote = br_paused = br_restore = br_observed = br_frozen = 0;
  br_flags = br_inputs = 0;br_post_slot = -1;br_talk_actor = 0;br_building = 0;
  /* An already-owned native sequence keeps its ordinary cleanup on disconnect. */
  br_owns_lock = 0;
  if (room_changed) {
    clear(br_nodes,sizeof(br_nodes));br_removed = br_fresh = br_gate_fx = 0;
    br_guard_fx[0] = br_guard_fx[1] = 0;
  }
}
void anchor_world_bridge_register(void *a, unsigned int entity, unsigned int selector) {
  int i = entity == 0x240 ? 0 : entity == 0x311 ? 3 :
          entity == 0x2d0 && selector < 2 ? (int)selector+1 : -1;
  if (D_800C7AB2 != WORLD_BRIDGE_ROOM || i < 0 || !a) return;
  BridgeNode *n = &br_nodes[i];
  detach(n,n->actor != a && n->proxy);
  if (i == 0 && n->actor != a)
    for (unsigned int j = 1; j < 4; ++j) if (br_nodes[j].parent == n->actor) {
      detach(&br_nodes[j],1);clear(&br_nodes[j],sizeof(br_nodes[j]));
    }
  if (br_observed) br_restore = 1;
  clear(n,sizeof(*n));n->actor = a;n->generation = BB(a,0x74);
  n->clip = (unsigned char)(i == 1 || i == 2);
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_bridge_reuse(void *actor) {
  for (unsigned int i = 0; i < 4; ++i) {
    BridgeNode *n = &br_nodes[i];
    if (n->actor == actor) { detach(n,0);clear(n,sizeof(*n));if (br_observed) br_restore = 1; }
    else if (n->parent == actor) { detach(n,1);clear(n,sizeof(*n));if (br_observed) br_restore = 1; }
  }
}
RECOMP_HOOK("func_80034ED4_35AD4")
void anchor_world_bridge_delete(void) {
  int i = find(D_8016DAB4_16E6B4);
  if (D_800C7AB2 != WORLD_BRIDGE_ROOM || i < 0) return;
  if (i == 3 && (func_80023E94_24A94(1) || func_800240DC_24CDC(1))) br_removed = 1;
  br_nodes[i].actor = 0;br_nodes[i].ready = 0;
  if (i != 3 && br_observed) br_restore = 1;
}
RECOMP_HOOK("func_8021664C_5D1B1C")
void anchor_world_bridge_animation(void *a, unsigned int clip) {
  int i = find(a);
  if (D_800C7AB2 == WORLD_BRIDGE_ROOM && i >= 0 && clip <= 2)
    br_nodes[i].clip = (unsigned char)clip;
}
static unsigned int flags(void) {
  return !!func_80023E94_24A94(0) | (!!func_80023E94_24A94(1)<<1);
}
unsigned int anchor_world_bridge_local_inputs(void) {
  return D_800C7AB2 == WORLD_BRIDGE_ROOM ? flags() : 0;
}
int anchor_world_bridge_needs_restore(void) { return br_restore; }
int anchor_world_bridge_owns(void *a) {
  if (D_800C7AB2 != WORLD_BRIDGE_ROOM || !a) return 0;
  int i = find(a);
  return br_building || (i >= 0 && same_actor(&br_nodes[i]));
}
static int resident(void) {
  static const unsigned short ids[4] = {0x240,0x2cc,0x311,1};
  static const unsigned char clips[4] = {0,2,0,0};
  if (func_800141C4_14DC4(51) == -1 || func_800141C4_14DC4(0x152) == -1 ||
      func_800141C4_14DC4(0x493) == -1) return 0;
  for (unsigned int i = 0; i < 4; ++i) {
    void **m = D_80236984_5F1E54[ids[i]];
    if (!m || !m[0] || !m[1]) return 0;
    const unsigned short *files = m[0];const unsigned int *c = m[1];
    if (func_800141C4_14DC4(files[0]) == -1 ||
        func_800141C4_14DC4(files[1]) == -1) return 0;
    for (unsigned int j = 0; j <= clips[i]; ++j) if (!c[j]) return 0;
  }
  return 1;
}
static int busy(void) {
  for (unsigned int i = 1; i < 3; ++i)
    if (alive(&br_nodes[i]) && ((BW(br_nodes[i].actor,0x68)&0x4100u) ||
                               !phase(&br_nodes[i],br_guard))) return 1;
  return 0;
}
static void blocker_init(void *a, void *o) {
  /* Pure body of the native311 binder, without its save1 early deletion.
   * A live checkpoint can still contain the blocker after dialogue committed
   * save1 but before the route's temp1 opening milestone. */
  BW(a,0x60) = 0x80000000u;BB(a,0x6c) = 0;
  func_80216E1C_5D22EC(a,0);
  for (unsigned int j = 0; j < 3; ++j) BF(o,0x1c+j*4) = 1;
  BF(o,8) = BF(o,12) = 0;BF(o,16) = -9;
  BP(a,0xc) = (void *)func_080006B4_70DDF4;
}
static int prepare(int removed) {
  if (!resident() || !alive(&br_nodes[0])) return 0;
  for (unsigned int i = 1; i < 4; ++i) {
    BridgeNode *n = &br_nodes[i];
    if (i == 3 && removed) continue;
    if (alive(n) || (n->actor && !n->ready && live(n))) continue;
    detach(n,1);clear(n,sizeof(*n));
    void *parent = br_nodes[0].actor;
    br_building = 1;
    void *a = func_802171A8_5D2678(parent,
                  i == 3 ? blocker_init : func_080002D8_70DA18,
                  (unsigned char)(i == 3 ? 3 : 0));
    br_building = 0;
    if (!a) continue;
    BH(a,0x5c) = BH(a,0x5e) = (unsigned short)(i == 3 ? 0x311 : 0x2d0);
    BH(a,0x28) = 51;BW(a,0x2c) = (unsigned int)func_800141C4_14DC4(51);
    BW(a,0xd0) = 0;BW(a,0xd4) = i == 3 ? 0 : i == 1 ? 0x10e : 0x110;
    BW(a,0xd8) = i == 2 ? 0x00010000u : 0;
    void *o = BP(a,0x18);
    if (!o) { BW(a,0x68) |= 2u;continue; }
    BF(o,8) = i == 1 ? -431 : i == 2 ? -451 : 0;
    BF(o,12) = 0;BF(o,16) = i == 3 ? 0 : 29;
    BH(o,0x14) = BH(o,0x18) = 0;BH(o,0x16) = i == 3 ? 0 : 0x200;
    n->actor = a;n->parent = parent;n->proxy = 1;
    n->generation = BB(a,0x74);n->parent_generation = BB(parent,0x74);
    n->clip = (unsigned char)(i != 3);if (br_observed) br_restore = 1;
  }
  return alive(&br_nodes[1]) && alive(&br_nodes[2]) &&
         (removed || alive(&br_nodes[3]));
}
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_bridge_post(void *a) {
  br_post_slot = -1;
  int i = find(a);
  if (D_800C7AB2 != WORLD_BRIDGE_ROOM || i < 0 || !BP(a,0x18)) return;
  addresses();BridgeNode *n = &br_nodes[i];
  if (!n->ready) {
    int p = i == 0 ? phase(n,br_gate) : i < 3 ? phase(n,br_guard) :
            callback(n) == func_080006B4_70DDF4;
    if (!p) return;
    n->generation = BB(a,0x74);n->ready = 1;
    if (i == 0 && !br_observed) br_fresh = p == 3;
  }
  if (br_enabled && br_frozen && alive(n) && i < 3 &&
      !(BW(a,0x68)&0x100u)) {
    br_post_slot = i;n->post_flags = BW(a,0x60);BW(a,0x60) &= ~0x800301u;
    for (unsigned int j = 0; j < 3; ++j) {
      n->post_velocity[j] = BF(a,0x78+j*4);BF(a,0x78+j*4) = 0;
    }
  }
}
RECOMP_HOOK_RETURN("func_80218F30_5D4400")
void anchor_world_bridge_post_return(void) {
  int i = br_post_slot;br_post_slot = -1;
  if (i < 0 || !alive(&br_nodes[i])) return;
  BridgeNode *n = &br_nodes[i];BW(n->actor,0x60) = n->post_flags;
  for (unsigned int j = 0; j < 3; ++j) BF(n->actor,0x78+j*4) = n->post_velocity[j];
}
static int quantize(float f, float scale, int lo, int hi, int *out) {
  volatile union { float f; unsigned int u; } value;value.f = f;
  if ((value.u&0x7f800000u) == 0x7f800000u) return 0;
  f *= scale;if (!(f >= (float)lo && f <= (float)hi)) return 0;
  *out = (int)f;return 1;
}
int anchor_world_bridge_capture(void *root, int *r) {
  if (D_800C7AB2 != WORLD_BRIDGE_ROOM || root != br_nodes[0].actor ||
      br_restore || !alive(&br_nodes[0]) || !alive(&br_nodes[1]) ||
      !alive(&br_nodes[2])) return 0;
  addresses();void *o = BP(root,0x18);
  r[3] = busy();br_flags |= flags();r[WB_FLAGS] = (int)br_flags;
  r[WB_FRESH] = br_fresh;r[WB_GATE_PHASE] = phase(&br_nodes[0],br_gate);
  if (!r[WB_GATE_PHASE] || !quantize(BF(o,0x28),100,0,1000000,&r[WB_GATE_FRAME])) return 0;
  r[WB_GATE_ANIMATION] = BH(o,0x7e) | ((BB(o,0x7c)&7u)<<16) | ((BW(root,0x60)&1u)<<19);
  r[WB_BLOCKER_REMOVED] = br_removed;
  if (!br_removed && !alive(&br_nodes[3])) return 0;
  for (unsigned int i = 0; i < 2; ++i) {
    BridgeNode *n = &br_nodes[i+1];void *a = n->actor;void *go = BP(a,0x18);
    int k = i ? WB_GUARD_1 : WB_GUARD_0, v = i ? WB_VELOCITY_1 : WB_VELOCITY_0;
    int p = phase(n,br_guard);if (!p) return 0;
    r[k] = p | ((BW(a,0x60)&1u)<<2) | ((BW(a,0x68)&0x400u) ? 8 : 0);
    for (unsigned int j = 0; j < 3; ++j)
      if (!quantize(BF(go,8+j*4),100,-3276800,3276700,&r[k+1+j]) ||
          !quantize(BF(a,0x78+j*4),1000,-100000,100000,&r[v+j])) return 0;
    r[k+4] = (BH(go,0x16)&1023u) | (p == 2 ? (unsigned int)BB(a,0xaa)<<10 : 0);
    r[k+5] = n->clip;
    if (!quantize(BF(go,0x28),100,0,1000000,&r[k+6])) return 0;
    r[k+7] = BH(go,0x7e) | ((BB(go,0x7c)&7u)<<16);
    if (p == 2) {
      if (BH(a,0xc4) != (i ? 0x43 : 0x44)) return 0;
      r[k+8] = BS(a,0xc6);
      for (unsigned int j = 0; j < 3; ++j) r[k+9+j] = BS(a,0xc8+j*2);
      r[k+12] = BB(a,0xce);r[k+13] = BB(a,0xcf);
    }
  }
  r[WB_INPUT] = (int)anchor_world_bridge_local_inputs();r[WB_AGGREGATE] = 0;
  br_observed = 1;return 1;
}

/* The following apply/control functions keep the bridge row atomic while the
 * per-guard native task retains ownership of every local talk continuation. */
static void guard_scalars(void *a, int p) {
  /* Common NPC outcome without its Y probe or non-idempotent shadow append.
   * Call only at a stable boundary, never during dialogue/result cleanup. */
  unsigned int shadow = BW(a,0x60)&0x08000000u;
  BW(a,0x60) = (p == 2 ? 0x02800761u : 0x12800761u) | shadow;
  BW(a,0x64) = 0x20;BB(a,0x4c) = 2;BW(a,0x68) = 0x400;
  BH(a,0xa4) = (unsigned short)BW(a,0xd4);BW(a,0x48) = 0xffffffffu;
  BS(a,0x4e) = 100;BH(a,0x50) = 100;BH(a,0x52) = 0;
}
static void release_lock(unsigned int guard) {
  if (br_owns_lock & (1 << guard)) {
    br_owns_lock &= ~(1 << guard);
    if (!br_owns_lock) D_800C7AE2 = 0;
  }
}
RECOMP_HOOK("func_8022125C_5DC72C")
void anchor_world_bridge_talk(void *a) {
  br_talk_actor = 0;
  if (br_enabled && a) {
    br_talk_actor = a;br_talk_lock = D_800C7AE2;br_talk_callback = BP(a,0xc);
  }
}
RECOMP_HOOK_RETURN("func_8022125C_5DC72C")
void anchor_world_bridge_talk_return(void) {
  void *a = br_talk_actor;br_talk_actor = 0;
  if (!a || !D_800C7AE2 || BP(a,0xc) == br_talk_callback) return;
  /* Both acceptance helpers change the task continuation only on success.
   * A different local sequence supersedes any earlier bridge ownership. */
  int i = find(a);br_owns_lock = 0;
  if (i > 0 && i < 3 && alive(&br_nodes[i]) && !br_talk_lock &&
      (BW(a,0x68)&0x100u)) br_owns_lock = 1 << (i-1);
}
RECOMP_HOOK("func_8022075C_5DBC2C")
void anchor_world_bridge_other_talk(void *a) { anchor_world_bridge_talk(a); }
RECOMP_HOOK_RETURN("func_8022075C_5DBC2C")
void anchor_world_bridge_other_talk_return(void) { anchor_world_bridge_talk_return(); }
RECOMP_HOOK("func_80221F70_5DD440")
void anchor_world_bridge_other_lock(void) { br_owns_lock = 0; }
RECOMP_HOOK("func_80221FB0_5DD480")
void anchor_world_bridge_other_unlock(void) { br_owns_lock = 0; }
RECOMP_HOOK("func_8000B4A0_C0A0")
void anchor_world_bridge_system_reset(void) { br_owns_lock = 0; }
static void animation(BridgeNode *n, int clip, int frame, int packed) {
  void *a = n->actor,*o = BP(a,0x18);
  if (n->clip != clip) {
    func_8021664C_5D1B1C(a,(unsigned int)clip,(float)(packed&65535)/256,
                       ((unsigned int)packed>>16)&7u);
    n->clip = (unsigned char)clip;
  }
  float limit = func_8001B5AC_1C1AC(o), f = (float)frame/100;
  if (f >= limit) f = limit-1;if (f < 0) f = 0;
  BF(o,0x28) = f;BH(o,0x7e) = (unsigned short)packed;
  BB(o,0x7c) = (BB(o,0x7c)&~7u)|(((unsigned int)packed>>16)&7u);
}
static int valid_route(const int *r, int k, unsigned int route) {
  if ((r[k]&3) != 2) return 1;
  unsigned int pc = (unsigned int)r[k+13];
  return pc < 256 && (world_path_pc[route][pc>>3] & (1u<<(pc&7)));
}
int anchor_world_bridge_apply(void *root, const int *r) {
  if (D_800C7AB2 != WORLD_BRIDGE_ROOM || r[2] != WORLD_BRIDGE ||
      root != br_nodes[0].actor || !anchor_world_row_valid(r) ||
      !valid_route(r,WB_GUARD_0,0x44) || !valid_route(r,WB_GUARD_1,0x43)) return 0;
  addresses();br_restore = 1;
  /* Allocation may queue missing children, but no checkpoint scalars or
   * receipt change until the whole cohort has finished initializing. */
  if (busy() || !prepare(r[WB_BLOCKER_REMOVED])) return 0;
  for (unsigned int i = 0; i < 3; ++i)
    if (!phase(&br_nodes[i],i ? br_guard : br_gate) ||
        !(func_8001B5AC_1C1AC(BP(br_nodes[i].actor,0x18)) > 0)) return 0;
  int witnessed = br_observed && !(r[WORLD_RECEIPT]&WORLD_BOOTSTRAP);
  int gp = phase(&br_nodes[0],br_gate);
  for (unsigned int i = 0; i < 2; ++i) {
    BridgeNode *n = &br_nodes[i+1];void *a = n->actor,*o = BP(a,0x18);
    int k = i ? WB_GUARD_1 : WB_GUARD_0, v = i ? WB_VELOCITY_1 : WB_VELOCITY_0;
    int p = r[k]&3, old = phase(n,br_guard);
    if (p != old) guard_scalars(a,p);
    for (unsigned int j = 0; j < 3; ++j) {
      BF(o,8+j*4) = (float)r[k+1+j]/100;
      BF(a,0x78+j*4) = (float)r[v+j]/1000;
    }
    BH(o,0x16) = (unsigned short)(r[k+4]&1023);
    BW(a,0x60) = (BW(a,0x60)&~1u)|((unsigned int)(r[k]>>2)&1u);
    BW(a,0x68) = (BW(a,0x68)&~0x400u)|((r[k]&8) ? 0x400u : 0);
    animation(n,r[k+5],r[k+6],r[k+7]);
    if (p == 2) {
      BH(a,0xc4) = (unsigned short)(i ? 0x43 : 0x44);BS(a,0xc6) = (short)r[k+8];
      for (unsigned int j = 0; j < 3; ++j) BS(a,0xc8+j*2) = (short)r[k+9+j];
      BB(a,0xce) = (unsigned char)r[k+12];BB(a,0xcf) = (unsigned char)r[k+13];
      BB(a,0xaa) = (unsigned char)((unsigned int)r[k+4]>>10);BP(a,0x9c) = a;
    }
    if (p == 3 || (p == 2 && r[k+13] >= 20)) {
      if (witnessed && !br_guard_fx[i] && old == 2) func_80038B98_39798(0x162);
      br_guard_fx[i] = 1;
    }
    if (p == 3) release_lock(i);
    schedule(n,br_guard[p-1]);
  }
  animation(&br_nodes[0],0,r[WB_GATE_FRAME],r[WB_GATE_ANIMATION]);
  BW(root,0x60) = (BW(root,0x60)&~1u)|((unsigned int)r[WB_GATE_ANIMATION]>>19&1u);
  schedule(&br_nodes[0],br_gate[r[WB_GATE_PHASE]-1]);
  if (r[WB_GATE_PHASE] != 1) {
    if (witnessed && !br_gate_fx && gp == 1 && r[WB_GATE_PHASE] == 2)
      func_80038B98_39798(0x15e);
    br_gate_fx = 1;
  }
  br_flags |= (unsigned int)r[WB_FLAGS];br_fresh = r[WB_FRESH];
  for (unsigned int i = 0; i < 2; ++i) if (br_flags&(1u<<i)) func_80023DF0_249F0((int)i);
  br_removed = r[WB_BLOCKER_REMOVED];
  if (br_removed) detach(&br_nodes[3],1);
  br_restore = 0;br_observed = 1;return 1;
}
void anchor_world_bridge_control(void *root, int remote, int paused, unsigned int inputs) {
  br_enabled = D_800C7AB2 == WORLD_BRIDGE_ROOM && root && root == br_nodes[0].actor;
  br_remote = remote;br_paused = paused;br_inputs = inputs&3u;
}
void anchor_world_bridge_begin(void) {
  if (!br_enabled || D_800C7AB2 != WORLD_BRIDGE_ROOM) return;
  addresses();br_frozen = !prepare(br_removed) || br_restore || (br_remote && br_paused);
  br_flags |= flags() | br_inputs;
  /* Save1 can commit before dialogue returns its accepted latch. If that
   * client departs, the new authority can finish the already accepted opening.
   * A live local dialogue or native cleanup continuation keeps its own turn. */
  if (!br_frozen && !br_remote && !busy() && !br_fresh &&
      phase(&br_nodes[0],br_gate) == 1 && func_800240DC_24CDC(1)) br_flags |= 1u;
  for (unsigned int i = 0; i < 2; ++i) if (br_flags&(1u<<i)) func_80023DF0_249F0((int)i);
  for (unsigned int i = 0; i < 4; ++i) {
    BridgeNode *n = &br_nodes[i];if (!alive(n)) continue;
    /* A dialogue helper saves task+C into B4. Never replace a running local
     * continuation, including the frame between clearing100 and setting4000. */
    if (i > 0 && i < 3 && ((BW(n->actor,0x68)&0x100u) || !phase(n,br_guard))) continue;
    n->saved = callback(n);if (!n->saved) continue;
    schedule(n,bridge_callback);
  }
}
static void bridge_callback(void *a, void *o) {
  int i = find(a);if (i < 0) return;
  BridgeNode *n = &br_nodes[i];if (!alive(n) || !n->saved) return;
  int p = i == 0 ? phase(n,br_gate) : i < 3 ? phase(n,br_guard) : 1;
  /* Consume an existing local result before an imported start latch can move
   * this guard into ROUTE. This also runs while an atomic apply is deferred. */
  if (i > 0 && i < 3 && (p == 1 || p == 3) && (BW(a,0x68)&0x4000u)) {
    BW(a,0x68) &= ~0x4000u;
    if (p == 1 && func_800240DC_24CDC(0)) {
      func_80023DF0_249F0(0);br_flags |= 1;
    }
    func_8021664C_5D1B1C(a,1,.25f,1);
  }
  if (br_frozen) return;
  if (i > 0 && i < 3 && p == 2) {
    int event = func_802268A8_5E1D78(a);func_80224D50_5E0220(a,1);
    if (event == 3 && !br_guard_fx[i-1]) {
      br_guard_fx[i-1] = 1;func_80038B98_39798(0x162);
    }
    if (event == 2) { func_80023DF0_249F0(1);br_flags |= 2; }
    if (event == 1) {
      guard_scalars(a,3);BH(o,0x16) = 0x200;
      for (unsigned int j = 0; j < 3; ++j) BF(a,0x78+j*4) = 0;
      func_8021664C_5D1B1C(a,1,.25f,1);
      release_lock((unsigned int)i-1);schedule(n,br_guard[2]);
    }
    return;
  }
  if (i > 0 && i < 3 && p == 1 && (br_flags&1u)) {
    /* A remote accepted latch must not invoke a local talk/contact check. */
    BW(a,0x60) &= ~0x10000000u;
    func_80226840_5E1D10(a,(unsigned short)(i == 1 ? 0x44 : 0x43));
    schedule(n,br_guard[1]);return;
  }
  if (i == 0 && p == 1 && (br_flags&2u)) {
    BW(a,0x60) |= 1u;
    if (!br_gate_fx) func_80038B98_39798(0x15e);
    br_gate_fx = 1;schedule(n,br_gate[1]);return;
  }
  /* Expose the real continuation before native talk captures task+C in B4. */
  BridgeCallback native = n->saved;schedule(n,native);native(a,o);
  if (live(n) && callback(n) == native && !(BW(a,0x68)&0x100u)) schedule(n,bridge_callback);
}
