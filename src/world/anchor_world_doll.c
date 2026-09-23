/* File62's placed 0x3D6 container in rooms 0x16A/0x182. The placed root is a
 * repeatable hit trigger whose pitch walks a close cycle; the nested File_26
 * Silver Doll is born through the dynamic-world API. This controller owns the
 * container cycle, its durable row and the local-vs-owner arbitration. */
#ifndef WORLD_DOLL_HOST_TEST
#include "platform/modding.h"
#endif
#include "world/anchor_world.h"
#include "world/anchor_world_doll.h"

#define DB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define DH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define DW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define DF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef DP
#define DP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef WDC_DISABLED
#define WDC_DISABLED 0x00800000ul
#endif
typedef void (*DollCallback)(void *, void *);

extern unsigned short D_800C7AB2;
extern void *D_8016DAB4_16E6B4;
extern void *D_80236984_5F1E54[];
extern int func_800141C4_14DC4(unsigned int);
extern int func_800240DC_24CDC(int);
extern int func_80023E94_24A94(int);
extern void func_80023DF0_249F0(int);
extern void func_80038BC8_397C8(unsigned int);
/* The dynamic-world parent API births the nested File_26 task from this root. */
extern int anchor_world_dynamic_doll_spawn(void *root,
                                           unsigned int index_onebased);

#define WDC_DECLARE(f) extern void f(void *, void *);
WDC_DECLARE(func_080025A8_723BC8) /* idle: waits for a fresh contact */
WDC_DECLARE(func_08002618_723C38) /* opening: pitch +8, clamps to 70 */
WDC_DECLARE(func_0800266C_723C8C) /* birth: allocates the nested Doll */
WDC_DECLARE(func_08002740_723D60) /* closing: pitch -16, clamps to 0 */

#define DOLL_IDLE 0
#define DOLL_OPENING 1
#define DOLL_BIRTH 2
#define DOLL_CLOSING 3

typedef struct {
  void *actor, *object;
  unsigned char generation, ready;
} DollNode;
static DollNode doll_node;
static DollCallback doll_real;
static DollCallback volatile doll_callbacks[4];
static void doll_callback(void *, void *);

static int doll_enabled, doll_remote, doll_paused, doll_confirmed;
static int doll_restore, doll_observed, doll_suspend;
static int doll_birth_known;
static int doll_registered;
static unsigned int doll_index, doll_cycle;
static int doll_post_active;
static unsigned int doll_post_flags;
static float doll_post_velocity[3];

static void addresses(void) {
  /* Runtime stores keep relocatable overlay imports out of a static table. */
  doll_callbacks[0] = func_080025A8_723BC8;
  doll_callbacks[1] = func_08002618_723C38;
  doll_callbacks[2] = func_0800266C_723C8C;
  doll_callbacks[3] = func_08002740_723D60;
}
static int room_ok(unsigned int room) {
  return room == WORLD_DOLL_ROOM_A || room == WORLD_DOLL_ROOM_B;
}
static int same(void) {
  DollNode *n = &doll_node;
  return n->actor && DP(n->actor,0x18) == n->object && n->object &&
         DB(n->actor,0x74) == n->generation &&
         DH(n->actor,0x5c) == WORLD_DOLL_ENTITY;
}
static int live(void) {
  return same() && doll_node.ready && !(DW(doll_node.actor,0x68)&2u);
}
static DollCallback raw(void *a) {
  return (DollCallback)((unsigned long)DP(a,0xc)&~WDC_DISABLED);
}
static void bind(void *a) {
  doll_node.actor = a;
  doll_node.object = a ? DP(a,0x18) : 0;
  doll_node.generation = a ? DB(a,0x74) : 0;
  doll_node.ready = 0;
}
static void install(void) {
  if (live())
    DP(doll_node.actor,0xc) = (void *)((unsigned long)doll_callback |
        ((unsigned long)DP(doll_node.actor,0xc)&WDC_DISABLED));
}
static int cur_phase(void) {
  DollCallback c;
  if (!same()) return -1;
  c = doll_real;
  if (raw(doll_node.actor) != doll_callback) c = raw(doll_node.actor);
  if (c == func_080025A8_723BC8) return DOLL_IDLE;
  if (c == func_08002618_723C38) return DOLL_OPENING;
  if (c == func_0800266C_723C8C) return DOLL_BIRTH;
  if (c == func_08002740_723D60) return DOLL_CLOSING;
  return -1;
}
static void set_phase(int p) {
  addresses();
  doll_real = doll_callbacks[p];
  install();
}
static unsigned int pitch(void) {
  return (unsigned int)DH(doll_node.object,0x14)&1023u;
}
static void set_pitch(unsigned int v) {
  DH(doll_node.object,0x14) = (unsigned short)(v&1023u);
}
static int resident(void) {
  void **m;
  const unsigned short *files;
  const unsigned int *slots;
  /* A missing resource must never schedule the binder on a stale pointer. */
  if (func_800141C4_14DC4(62) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  m = D_80236984_5F1E54[WORLD_DOLL_ENTITY];
  if (!m || !m[0] || !m[1]) return 0;
  files = m[0]; slots = m[1];
  if (func_800141C4_14DC4(files[0]) == -1 ||
      func_800141C4_14DC4(files[1]) == -1) return 0;
  return slots[0] != 0;
}
static void observe(void) {
  /* A root that already advanced along the vanilla callbacks before the
   * wrapper was active still needs a positive cycle floor. */
  int p = cur_phase();
  if (p < 0) return;
  if (p != DOLL_IDLE && !doll_cycle) doll_cycle = 1;
  if (func_80023E94_24A94(2) && !doll_cycle) doll_cycle = 1;
}
static int birth(void *a) {
  /* 0 = retry. The refusal simply leaves the container on its birth phase, so
   * the next update asks again; there is no separate pending flag. */
  if (!anchor_world_dynamic_doll_spawn(a, doll_index)) return 0;
  doll_birth_known = 1;
  return 1;
}
static void hit(void *a) {
  /* The native trigger repeats forever, so every accepted contact opens a new
   * cycle. The consumed latch is cleared so one contact cannot re-arm next
   * frame and a new hit needs a new contact. */
  if (doll_cycle >= 1000000u) return; /* Saturated: never publish an invalid row. */
  ++doll_cycle;
  DB(a,0x8d) = 100;
  func_80038BC8_397C8(0x24f);
  DW(a,0x68) &= ~0x80u;
  set_phase(DOLL_OPENING);
}
static void doll_callback(void *a, void *o) {
  int p;
  (void)o;
  if (!doll_enabled || !live() || a != doll_node.actor ||
      !room_ok(D_800C7AB2)) return;
  p = cur_phase();
  if (p < 0) return;
  if (doll_suspend) return;
  /* A remote pause or an unacknowledged atomic restore freezes the cycle. */
  if (doll_restore || doll_paused) return;
  if (doll_remote || !doll_confirmed) {
    /* A replica, or an authority the transport has not confirmed yet, still
     * accepts a fresh idle contact so the local proposal can reach the owner.
     * It must never run the native/helper birth itself: an unowned second
     * Doll duplicates the toy and desyncs the cycle. Every later phase waits
     * for a confirmed owner checkpoint. */
    if (p == DOLL_IDLE && (DW(a,0x68)&0x80u) && DP(a,0x38)) hit(a);
    return;
  }
  if (p == DOLL_IDLE) {
    if ((DW(a,0x68)&0x80u) && DP(a,0x38)) hit(a);
    return;
  }
  if (p == DOLL_OPENING) {
    unsigned int v = (pitch()+8u)&1023u;
    if (v > 69u) { set_pitch(70); set_phase(DOLL_BIRTH); }
    else set_pitch(v);
    return;
  }
  if (p == DOLL_BIRTH) {
    /* A refused/unknown birth stays pending on this phase and retries. */
    if (!birth(a)) return;
    set_phase(DOLL_CLOSING);
    return;
  }
  {
    unsigned int v = (pitch()-16u)&1023u;
    if (v > 0x300u) {
      set_pitch(0);
      func_80038BC8_397C8(0x273);
      set_phase(DOLL_IDLE);
    } else set_pitch(v);
  }
}
void anchor_world_doll_reset(int room_changed) {
  if (!room_changed && same() && raw(doll_node.actor) == doll_callback)
    DP(doll_node.actor,0xc) = (void *)doll_real;
  doll_enabled = doll_remote = doll_paused = doll_confirmed = 0;
  doll_restore = doll_suspend = 0;
  if (room_changed) {
    bind(0);
    doll_real = 0; doll_observed = 0; doll_index = 0; doll_cycle = 0;
    doll_registered = 0;
    doll_birth_known = 0;
  }
}
void anchor_world_doll_register(void *a, unsigned int index_onebased) {
  if (!room_ok(D_800C7AB2) || !a) return;
  if (same() && doll_node.actor != a && raw(doll_node.actor) == doll_callback)
    DP(doll_node.actor,0xc) = (void *)doll_real;
  /* A culled root that is re-placed in the same visit keeps its cycle so the
   * published progress cannot regress. */
  bind(a);
  doll_real = raw(a);
  doll_index = index_onebased;
  doll_registered = 1;
  doll_restore = doll_observed;
  doll_suspend = 0;
  observe();
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_doll_reuse(void *a) {
  if (a != doll_node.actor) return;
  if (raw(doll_node.actor) == doll_callback)
    DP(doll_node.actor,0xc) = (void *)doll_real;
  /* The nested category-9 Doll is owned by the scheduler list, not by this
   * root, and survives the root being culled. Keep the roster identity so the
   * parent mapping and the Doll's own cleanup stay valid until room reset. */
  bind(0);
  if (doll_observed) doll_restore = 1;
}
RECOMP_HOOK("func_80034ED4_35AD4")
void anchor_world_doll_delete(void) {
  if (!room_ok(D_800C7AB2) || D_8016DAB4_16E6B4 != doll_node.actor) return;
  doll_node.ready = 0;
}
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_doll_post(void *a) {
  doll_post_active = 0;
  if (!room_ok(D_800C7AB2)) return;
  if (a == doll_node.actor && !doll_node.ready &&
      DH(a,0x5c) == WORLD_DOLL_ENTITY && DP(a,0x18)) {
    doll_node.object = DP(a,0x18);
    doll_node.generation = DB(a,0x74);
    doll_real = raw(a);
  }
  if (same() && a == doll_node.actor) {
    if (cur_phase() >= 0) {
      doll_node.ready = 1;
      if (raw(a) != doll_callback) doll_real = raw(a);
      observe();
    }
  }
  /* The freeze mask and its restoring return hook must act on the same task.
   * live() only proves the tracked root, not that this update is that task. */
  if (a != doll_node.actor) return;
  if (!doll_suspend && (!doll_enabled || (!doll_restore && !doll_paused)))
    return;
  if (!live()) return;
  doll_post_active = 1;
  doll_post_flags = DW(a,0x60);
  DW(a,0x60) &= ~0x800301u;
  for (unsigned int j = 0; j < 3; ++j) {
    doll_post_velocity[j] = DF(a,0x78+j*4);
    DF(a,0x78+j*4) = 0;
  }
}
RECOMP_HOOK_RETURN("func_80218F30_5D4400")
void anchor_world_doll_post_return(void) {
  if (!doll_post_active) return;
  doll_post_active = 0;
  if (!live()) return;
  DW(doll_node.actor,0x60) = doll_post_flags;
  for (unsigned int j = 0; j < 3; ++j)
    DF(doll_node.actor,0x78+j*4) = doll_post_velocity[j];
}
static int quantize(float f, float scale, int lo, int hi, int *out) {
  volatile union { float f; unsigned int u; } value;
  value.f = f;
  if ((value.u&0x7f800000u) == 0x7f800000u) return 0;
  f *= scale;
  if (!(f >= (float)lo && f <= (float)hi)) return 0;
  *out = (int)f;
  return 1;
}
/* phase 0 -> pitch 0, phase 1 -> 0..64 step 8, phase 2 -> pitch 70,
 * phase 3 -> 70,54,38,22,6; cycle 0 exists only at idle. */
static int phase_pitch_ok(int p, int cyc, int pit) {
  if (cyc < 0 || cyc > 1000000) return 0;
  if (cyc == 0 && p != DOLL_IDLE) return 0;
  if (p == DOLL_IDLE) return !pit;
  if (p == DOLL_OPENING) return pit >= 0 && pit <= 64 && !(pit&7);
  if (p == DOLL_BIRTH) return pit == 70;
  if (p == DOLL_CLOSING)
    return pit == 70 || pit == 54 || pit == 38 || pit == 22 || pit == 6;
  return 0;
}
int anchor_world_doll_capture(void *a, int *r) {
  int p, cyc, pit;
  void *o;
  if (!room_ok(D_800C7AB2)) return 0;
  if (a != doll_node.actor || doll_restore || doll_suspend || !live()) return 0;
  p = cur_phase();
  if (p < 0) return 0;
  /* A root that opened along the vanilla callbacks before the first capture
   * still owns a positive cycle floor; phase 1 with cycle 0 is not a row. */
  observe();
  cyc = (int)doll_cycle;
  pit = (int)pitch();
  if (!phase_pitch_ok(p,cyc,pit)) return 0;
  o = doll_node.object;
  if (!quantize(DF(o,8),100,-3276800,3276700,&r[WDC_X]) ||
      !quantize(DF(o,0xc),100,-3276800,3276700,&r[WDC_Y]) ||
      !quantize(DF(o,0x10),100,-3276800,3276700,&r[WDC_Z]) ||
      !resident())
    return 0;
  r[WDC_BUSY] = 0;
  r[WDC_PITCH] = pit;
  r[WDC_YAW] = (int)(DH(o,0x16)&1023u);
  r[WDC_ROLL] = (int)(DH(o,0x18)&1023u);
  r[WDC_CYCLE] = cyc;
  r[WDC_PHASE] = p;
  /* The room temporary is volatile and save 0xEE is the durable collected
   * record. Neither suppresses a live reconstruction. */
  r[WDC_SPAWNED] = (func_80023E94_24A94(2) || doll_birth_known) ? 1 : 0;
  r[WDC_PAUSED] = 0;
  doll_observed = 1;
  return 1;
}
int anchor_world_doll_needs_restore(void) { return doll_restore; }
int anchor_world_doll_apply(void *a, const int *r) {
  int p, cyc, pit;
  void *o;
  if (!room_ok(D_800C7AB2) || r[2] != WORLD_DOLL_CONTAINER) return 0;
  if (!anchor_world_row_valid(r)) return 0;
  if (a != doll_node.actor || !live()) return 0;
  p = r[WDC_PHASE];
  cyc = r[WDC_CYCLE];
  pit = r[WDC_PITCH];
  if (!phase_pitch_ok(p,cyc,pit)) return 0;
  if (!resident()) return 0;
  doll_restore = 1;
  o = doll_node.object;
  DF(o,8) = (float)r[WDC_X]/100;
  DF(o,0xc) = (float)r[WDC_Y]/100;
  DF(o,0x10) = (float)r[WDC_Z]/100;
  DH(o,0x16) = (unsigned short)r[WDC_YAW];
  DH(o,0x18) = (unsigned short)r[WDC_ROLL];
  set_pitch((unsigned int)pit);
  set_phase(p);
  doll_cycle = (unsigned int)cyc;
  /* A durable "a Doll was born this visit" fact is monotone: it never clears a
   * live Doll and never by itself forces a reconstruction. */
  if (r[WDC_SPAWNED]) {
    doll_birth_known = 1;
    func_80023DF0_249F0(2);
  }
  doll_restore = 0;
  doll_observed = 1;
  return 1;
}
void anchor_world_doll_control(void *a, int remote, int paused, int confirmed) {
  int enabled = room_ok(D_800C7AB2) && a && a == doll_node.actor;
  /* The scheduler clears control before its pause/active checks. Preserve a
   * paused session's suspension, but never re-suspend a disconnected proxy
   * that reset(0) already released to finish without a record. */
  doll_suspend = enabled ? 0 : (doll_enabled || doll_suspend);
  doll_enabled = enabled;
  doll_remote = remote;
  doll_paused = paused;
  doll_confirmed = confirmed;
}
void anchor_world_doll_begin(void) {
  if (!doll_enabled) {
    /* Offline or untracked: hand the root back to its vanilla continuation. */
    if (room_ok(D_800C7AB2) && same() && raw(doll_node.actor) == doll_callback)
      DP(doll_node.actor,0xc) = (void *)doll_real;
    return;
  }
  if (!live()) return;
  if (!resident()) { doll_suspend = 1; return; }
  doll_suspend = 0;
  if (raw(doll_node.actor) != doll_callback) doll_real = raw(doll_node.actor);
  install();
}
int anchor_world_doll_parent_valid(unsigned int index_onebased) {
  /* The verified placed roster slot in the current room, independent of
   * whether the root task is still live. */
  if (!room_ok(D_800C7AB2) || !doll_registered) return 0;
  if (doll_index != index_onebased) return 0;
  if (D_800C7AB2 == WORLD_DOLL_ROOM_A) return index_onebased == 8;
  return index_onebased == 3;
}
unsigned int anchor_world_doll_parent(void) {
  if (!room_ok(D_800C7AB2) || !doll_registered) return 0;
  return doll_index;
}
