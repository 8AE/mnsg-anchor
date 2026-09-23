/* File_40 room 0x6B: three placed six-piece counterweights (entity 0x3CB).
 *
 * Each placed root owns six sliding children. The portable state is scalar
 * only: the six child heights, the shared rider mask and its speed. The native
 * child update rewrites that shared mask from the local player alone, so every
 * child continuation is replaced by the same pure prediction on every peer,
 * driven by the aggregate rider mask. Native tasks, resources, cameras and the
 * X-distance render cull stay process-local.
 *
 * Verified native motion (File_40, ROM window 0x6E9150..0x6EE960):
 *   root init  08001CFC  allocs six children via 802171A8(init,3)
 *   child init 08001ECC/20CC/2328/2564/27A0/29FC
 *   child upd  08001F5C/2160/23BC/25F8/2834/2A90
 *   slot 0..2 extend upward, slot 3..5 extend downward; the shared speed is
 *   parent+0xD0 and the rider byte is parent+0xEC.
 */
#ifndef WORLD_COUNTERWEIGHT_HOST_TEST
#include "platform/modding.h"
#endif
#include "world/anchor_world.h"
#include "world/anchor_world_counterweight.h"

#define CB(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define CH(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define CW(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define CF(p, o) (*(float *)((char *)(p) + (o)))
#ifndef CP
#define CP(p, o) (*(void **)((char *)(p) + (o)))
#endif
#ifndef CW_DISABLED
#define CW_DISABLED 0x00800000ul
#endif
#ifndef CW_TASK_KIND
#define CW_TASK_KIND 3
#endif

typedef void (*CwCallback)(void *, void *);

extern unsigned short D_800C7AB2;
extern void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
extern void *D_80236984_5F1E54[];
extern int func_800141C4_14DC4(unsigned int);
extern void *func_802171A8_5D2678(void *, CwCallback, unsigned char);

#define CW_INITS(X)                                                           \
  X(func_08001ECC_6EB01C)                                                     \
  X(func_080020CC_6EB21C)                                                     \
  X(func_08002328_6EB478)                                                     \
  X(func_08002564_6EB6B4)                                                     \
  X(func_080027A0_6EB8F0)                                                     \
  X(func_080029FC_6EBB4C)
#define CW_UPDATES(X)                                                         \
  X(func_08001F5C_6EB0AC)                                                     \
  X(func_08002160_6EB2B0)                                                     \
  X(func_080023BC_6EB50C)                                                     \
  X(func_080025F8_6EB748)                                                     \
  X(func_08002834_6EB984)                                                     \
  X(func_08002A90_6EBBE0)
#define CW_DECLARE(f) extern void f(void *, void *);
CW_INITS(CW_DECLARE)
CW_UPDATES(CW_DECLARE)

/* Native per-slot constants. The rider arms of slots 1..4 evaluate the scale in
 * double precision as (speed * num) / den; slots 0 and 5 fold 1.0/1.0 away and
 * stay single precision. The no-rider arm always relaxes in double precision. */
static const float cw_speed[6] = {2.4f, 1.6f, 0.8f, 0.8f, 1.6f, 2.4f};
static const double cw_num[6] = {1.0, 3.0, 1.0, 1.0, 3.0, 1.0};
static const double cw_den[6] = {1.0, 5.0, 5.0, 5.0, 5.0, 1.0};
static const double cw_step[6] = {0.4, 0.24, 0.08, 0.08, 0.24, 0.4};
/* Verified room 0x6B placements, index 13..15 -> X, Z hundredths. */
static const float cw_px[3] = {0.0f, -300.0f, 300.0f};
static const float cw_pz[3] = {-20.0f, 0.0f, 0.0f};

typedef struct {
  void *actor, *object;
  void *child[6], *child_object[6];
  float child_x[6], child_z[6];
  float pending_y[6];
  unsigned char pending[6];
  unsigned char generation, child_generation[6];
  unsigned char ready, child_ready[6];
  int index;
} CwRoot;

static CwRoot cw_roots[3];
/* Runtime stores stop RecompModTool from emitting a static table of
 * relocatable overlay imports that it cannot resolve. */
static CwCallback volatile cw_inits[6], cw_updates[6];
static int cw_enabled[3], cw_remote[3], cw_paused[3], cw_confirmed[3];
static int cw_frozen[3];
static unsigned int cw_aggregate[3];
/* Per root: one root's reload must not gate the other two. */
static int cw_observed[3], cw_restore[3];
static void cw_callback(void *, void *);
static void cw_retired(void *, void *);

static void addresses(void) {
  unsigned int i = 0;
#define CW_I(f) cw_inits[i++] = f;
  CW_INITS(CW_I)
#undef CW_I
  i = 0;
#define CW_U(f) cw_updates[i++] = f;
  CW_UPDATES(CW_U)
#undef CW_U
}
static void clear(void *p, unsigned int size) {
  for (unsigned int i = 0; i < size; ++i)
    ((volatile unsigned char *)p)[i] = 0;
}
static CwCallback raw(void *a) {
  return (CwCallback)((unsigned long)CP(a, 0xc) & ~CW_DISABLED);
}
static int slot_of(const CwCallback volatile *table, CwCallback c) {
  for (unsigned int i = 0; i < 6; ++i)
    if (c == table[i])
      return (int)i;
  return -1;
}
static CwRoot *find(void *actor) {
  for (unsigned int i = 0; i < 3; ++i)
    if (cw_roots[i].actor && cw_roots[i].actor == actor)
      return &cw_roots[i];
  return 0;
}
static int index_of(const CwRoot *cw) { return (int)(cw - cw_roots); }
static int live_root(const CwRoot *cw) {
  return cw->actor && CP(cw->actor, 0x18) &&
         CB(cw->actor, 0x74) == cw->generation &&
         !(CW(cw->actor, 0x68) & 2u);
}
/* The child task exists and still belongs to this root, but its scheduled
 * initializer may not have run yet. Identity is re-checked against the live
 * object, the parent backlink and the task generation. */
static int bound_child(const CwRoot *cw, unsigned int slot) {
  void *a = cw->child[slot];
  return a && !(CW(a, 0x68) & 2u) && CP(a, 0x18) && CP(a, 0xe8) == cw->actor &&
         CB(a, 0x74) == cw->child_generation[slot];
}
static int live_child(const CwRoot *cw, unsigned int slot) {
  return bound_child(cw, slot) && cw->child_ready[slot] &&
         cw->child_object[slot] &&
         CP(cw->child[slot], 0x18) == cw->child_object[slot];
}
/* Child X/Z is inherited from the owner object and never moves, so the placed
 * index is recoverable from the immutable root pose. */
static int resolve(CwRoot *cw) {
  if (cw->index >= 13 && cw->index <= 15)
    return 1;
  if (!cw->actor || !CP(cw->actor, 0x18))
    return 0;
  float x = CF(CP(cw->actor, 0x18), 0x08);
  float z = CF(CP(cw->actor, 0x18), 0x10);
  for (unsigned int i = 0; i < 3; ++i)
    if (x > cw_px[i] - 0.5f && x < cw_px[i] + 0.5f && z > cw_pz[i] - 0.5f &&
        z < cw_pz[i] + 0.5f) {
      cw->index = 13 + (int)i;
      return 1;
    }
  return 0;
}
static void bind_root(CwRoot *cw, void *a) {
  clear(cw, sizeof(*cw));
  cw->actor = a;
  cw->generation = CB(a, 0x74);
}
static void bind_child(CwRoot *cw, unsigned int slot, void *a) {
  void *o = CP(a, 0x18);
  cw->child[slot] = a;
  cw->child_object[slot] = o;
  cw->child_generation[slot] = CB(a, 0x74);
  cw->child_ready[slot] = 0;
  if (o) {
    cw->child_x[slot] = CF(o, 0x08);
    cw->child_z[slot] = CF(o, 0x10);
  }
}
/* A retired child must never reach a released parent through +0xE8 and must
 * never keep predicting against a root that no longer owns it. */
static void retire_child(CwRoot *cw, unsigned int slot) {
  void *a = cw->child[slot];
  if (a && CB(a, 0x74) == cw->child_generation[slot]) {
    if (!(CW(a, 0x68) & 2u)) {
      CwCallback c = raw(a);
      if (slot_of(cw_updates, c) == (int)slot)
        CP(a, 0xc) = (void *)((unsigned long)cw_retired |
                              ((unsigned long)CP(a, 0xc) & CW_DISABLED));
      CW(a, 0x68) |= 2u;
    }
    if (CP(a, 0xe8) == cw->actor)
      CP(a, 0xe8) = 0;
  }
  cw->child[slot] = 0;
  cw->child_object[slot] = 0;
  cw->child_ready[slot] = 0;
  cw->pending[slot] = 0;
}
static void retire_root(CwRoot *cw) {
  for (unsigned int i = 0; i < 6; ++i)
    retire_child(cw, i);
  clear(cw, sizeof(*cw));
}
/* The descriptor's +0 pair table and +4 binder-command table are 32-bit
 * pointers, so they are read at their native 4-byte offsets rather than
 * through a host pointer stride. Both tables are pinned to the verified
 * entity 0x3CB recipe before any child is bound or restored. */
static int resident(void) {
  static const unsigned int want[6] = {0x08000198u, 0x080004a8u, 0x08000798u,
                                       0x08000a88u, 0x08000d78u, 0x080010a8u};
  void *desc = D_80236984_5F1E54[WORLD_COUNTERWEIGHT_ENTITY];
  const unsigned short *files;
  const unsigned int *slots;
  if (!desc)
    return 0;
  files = (const unsigned short *)CP(desc, 0);
  slots = (const unsigned int *)CP(desc, 4);
  if (!files || !slots || files[0] != 0x1eb || files[1] != 0x160)
    return 0;
  for (unsigned int i = 0; i < 6; ++i)
    if (slots[i] != want[i])
      return 0;
  if (func_800141C4_14DC4(0x28) == -1 || func_800141C4_14DC4(0x152) == -1)
    return 0;
  if (func_800141C4_14DC4(files[0]) == -1 ||
      func_800141C4_14DC4(files[1]) == -1)
    return 0;
  return 1;
}
/* Rebuild only missing children. The native initializer must run once through
 * the scheduler; the root initializer is never replayed. */
static int prepare(CwRoot *cw) {
  int ready = 1;
  if (D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM || !live_root(cw) || !resident())
    return 0;
  for (unsigned int i = 0; i < 6; ++i) {
    void *a;
    if (live_child(cw, i))
      continue;
    if (bound_child(cw, i)) {
      ready = 0; /* Its scheduled initializer still owns the first tick. */
      continue;
    }
    retire_child(cw, i);
    a = func_802171A8_5D2678(cw->actor, cw_inits[i], CW_TASK_KIND);
    if (!a) {
      ready = 0;
      continue;
    }
    CH(a, 0x28) = 0x28;
    CP(a, 0x2c) = (void *)(unsigned long)(unsigned int)func_800141C4_14DC4(0x28);
    CP(a, 0xe8) = cw->actor;
    bind_child(cw, i, a);
    if (cw_observed[index_of(cw)])
      cw_restore[index_of(cw)] = 1;
    ready = 0;
  }
  for (unsigned int i = 0; i < 6; ++i)
    if (!live_child(cw, i))
      ready = 0;
  return ready;
}
static void install(CwRoot *cw, unsigned int slot) {
  void *a = cw->child[slot];
  if (!a || !CP(a, 0x18) || CB(a, 0x74) != cw->child_generation[slot])
    return;
  cw->child_object[slot] = CP(a, 0x18);
  if (raw(a) != cw_updates[slot])
    return; /* The initializer still owns this tick. */
  cw->child_ready[slot] = 1;
  CP(a, 0xc) = (void *)((unsigned long)cw_callback |
                        ((unsigned long)CP(a, 0xc) & CW_DISABLED));
}
static void release(CwRoot *cw, unsigned int slot) {
  void *a = cw->child[slot];
  if (!a || CB(a, 0x74) != cw->child_generation[slot])
    return;
  if (((unsigned long)CP(a, 0xc) & ~CW_DISABLED) == (unsigned long)cw_callback)
    CP(a, 0xc) = (void *)((unsigned long)cw_updates[slot] |
                          ((unsigned long)CP(a, 0xc) & CW_DISABLED));
}
/* Native step 1: derived presentation only, never wire state. Task +0x84 is
 * the resolved object pointer itself, so its +0x8 is the target X. */
static void cull(void *a, void *o) {
  void *target = CP(a, 0x84);
  float d;
  if (!target)
    return;
  d = CF(o, 0x08) - CF(target, 0x08);
  if (d < 0)
    d = -d;
  if (d > 160.0f)
    CW(a, 0x60) &= ~0x80000000u;
  else
    CW(a, 0x60) |= 0x80000000u;
}
static unsigned int selected_bit(unsigned int mask) {
  static const unsigned int bit[6] = {1u, 2u, 4u, 8u, 16u, 32u};
  for (int i = 5; i >= 0; --i)
    if (mask & bit[i])
      return bit[i];
  return 0;
}
static int bit_slot(unsigned int selected) {
  switch (selected) {
  case 1u:
    return 0;
  case 2u:
    return 1;
  case 4u:
    return 2;
  case 8u:
    return 3;
  case 16u:
    return 4;
  case 32u:
    return 5;
  default:
    return -1;
  }
}
static float clamp_y(float y, float e0, float dc) {
  return y < e0 ? e0 : y > dc ? dc : y;
}
/* Exact native child motion for one slot. Slots 0..2 lower for rider bits
 * 1/2/4 and raise for 8/16/32; slots 3..5 are the inverse. With no rider the
 * child relaxes toward its rest position at the slot's own double step. */
static void advance(CwRoot *cw, unsigned int slot, unsigned int selected) {
  void *a = cw->child[slot], *o = cw->child_object[slot];
  float y, dc, e0;
  int top = slot < 3, down;
  if (!a || !o)
    return;
  y = CF(o, 0x0c);
  dc = CF(a, 0xdc);
  e0 = CF(a, 0xe0);
  if (!selected) {
    down = top;
    y = (float)(down ? (double)y - cw_step[slot] : (double)y + cw_step[slot]);
    CF(o, 0x0c) = clamp_y(y, e0, dc);
    return;
  }
  down = top ? (selected <= 4u) : (selected >= 8u);
  int k = bit_slot(selected);
  if (k < 0)
    return;
  if (cw_num[slot] == 1.0 && cw_den[slot] == 1.0) {
    /* Native folds the scale away for slots 0 and 5. */
    float s = cw_speed[k];
    y = down ? y - s : y + s;
  } else {
    double d = ((double)cw_speed[k] * cw_num[slot]) / cw_den[slot];
    y = (float)(down ? (double)y - d : (double)y + d);
  }
  CF(o, 0x0c) = clamp_y(y, e0, dc);
}
static unsigned int local_bits(CwRoot *cw) {
  unsigned int bits = 0;
  void *support;
  if (!D_801FC604_5B8514)
    return 0;
  support = CP(D_801FC604_5B8514, 0xa0);
  if (!support)
    return 0;
  for (unsigned int i = 0; i < 6; ++i)
    if (cw->child_object[i] && cw->child_object[i] == support)
      bits |= 1u << i;
  return bits;
}
static int quantize(float f, int *out) {
  volatile union {
    float f;
    unsigned int u;
  } v;
  v.f = f;
  if ((v.u & 0x7f800000u) == 0x7f800000u)
    return 0;
  f *= 100;
  if (!(f >= -3276800 && f <= 3276700))
    return 0;
  *out = (int)f;
  return 1;
}

void anchor_world_counterweight_end(void) {
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int s = 0; s < 6; ++s)
      release(&cw_roots[i], s);
}
void anchor_world_counterweight_reset(int room_changed) {
  anchor_world_counterweight_end();
  for (unsigned int i = 0; i < 3; ++i) {
    cw_enabled[i] = cw_remote[i] = cw_paused[i] = cw_confirmed[i] = 0;
    cw_frozen[i] = 0;
    cw_aggregate[i] = 0;
    cw_observed[i] = cw_restore[i] = 0;
  }
  if (room_changed)
    clear(cw_roots, sizeof(cw_roots));
}
void anchor_world_counterweight_register(void *a) {
  CwRoot *cw;
  unsigned int i;
  if (D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM || !a)
    return;
  addresses();
  cw = find(a);
  if (cw) {
    resolve(cw);
    return;
  }
  cw = 0;
  for (i = 0; i < 3; ++i)
    if (!cw_roots[i].actor) {
      cw = &cw_roots[i];
      break;
    }
  if (!cw) {
    /* Three placed roots share this module; reuse the first stale binding. */
    for (i = 0; i < 3; ++i)
      if (!live_root(&cw_roots[i])) {
        cw = &cw_roots[i];
        break;
      }
    if (!cw)
      return;
    retire_root(cw);
  }
  bind_root(cw, a);
  resolve(cw);
  if (cw_observed[index_of(cw)])
    cw_restore[index_of(cw)] = 1;
}
unsigned int anchor_world_counterweight_local_inputs(void *root) {
  CwRoot *cw = find(root);
  if (!cw || D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM)
    return 0;
  return local_bits(cw);
}
int anchor_world_counterweight_needs_restore(void *root) {
  CwRoot *cw = find(root);
  if (!cw || D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM)
    return 0;
  return cw_restore[index_of(cw)];
}
int anchor_world_counterweight_capture(void *root, int *r) {
  CwRoot *cw = find(root);
  void *o;
  unsigned int mask, selected;
  int idx;
  if (D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM || !cw || !r)
    return 0;
  idx = index_of(cw);
  if (cw_restore[idx] || !live_root(cw) || !resolve(cw) || cw->index != r[0])
    return 0;
  for (unsigned int i = 0; i < 6; ++i)
    if (!live_child(cw, i))
      return 0;
  o = cw->object ? cw->object : CP(cw->actor, 0x18);
  if (!o)
    return 0;
  /* The caller supplies only the header words; the pose is read from the live
   * root. The codec's typed validator then pins it to the placement recipe. */
  for (unsigned int i = 0; i < 3; ++i) {
    if (!quantize(CF(o, 0x08 + i * 4), &r[WC_X + i]))
      return 0;
    r[WC_PITCH + i] = (int)(CH(o, 0x14 + i * 2) & 1023);
  }
  for (unsigned int i = 0; i < 6; ++i) {
    if (!quantize(CF(cw->child_object[i], 0x0c), &r[WC_CHILD_Y + i]))
      return 0;
  }
  mask = cw_enabled[idx] ? cw_aggregate[idx] : local_bits(cw);
  selected = selected_bit(mask);
  r[WC_BUSY] = local_bits(cw) ? 1 : 0;
  r[WC_PRESENT] = 63;
  r[WC_SELECTED] = (int)selected;
  r[WC_SPEED] = selected ? (int)(cw_speed[bit_slot(selected)] * 1000.0f) : 0;
  r[WC_LOCAL] = (int)local_bits(cw);
  r[WC_AGGREGATE] = 0; /* The transport owns the aggregate word. */
  cw_observed[idx] = 1;
  return 1;
}
/* The placed root pose and the resource binding are validated before any child
 * is touched; a rejected row leaves every live task exactly as it was. */
int anchor_world_counterweight_apply(void *root, const int *r) {
  CwRoot *cw;
  void *o;
  int idx;
  if (!r || r[2] != WORLD_COUNTERWEIGHT || !anchor_world_row_valid(r) ||
      !anchor_world_counterweight_valid(r))
    return 0;
  cw = find(root);
  if (!cw || D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM)
    return 0;
  idx = index_of(cw);
  if (!live_root(cw) || !resolve(cw) || cw->index != r[0])
    return 0;
  if (!resident())
    return 0;
  o = cw->object ? cw->object : CP(cw->actor, 0x18);
  if (!o)
    return 0;
  for (unsigned int i = 0; i < 3; ++i) {
    int q;
    if (!quantize(CF(o, 0x08 + i * 4), &q) || q != r[WC_X + i])
      return 0;
  }
  for (unsigned int i = 0; i < 3; ++i)
    if ((int)(CH(o, 0x14 + i * 2) & 1023) != r[WC_PITCH + i])
      return 0;
  if (!prepare(cw)) {
    /* Children whose initializer has not run yet take their height when they
     * become ready, before their first prediction tick. The checkpoint is not
     * confirmed until all six pieces are live, so the caller retries. */
    for (unsigned int i = 0; i < 6; ++i)
      if (!live_child(cw, i) && bound_child(cw, i)) {
        cw->pending[i] = 1;
        cw->pending_y[i] = (float)r[WC_CHILD_Y + i] / 100.0f;
      }
    return 0;
  }
  for (unsigned int i = 0; i < 6; ++i) {
    void *co = cw->child_object[i];
    CF(co, 0x0c) = (float)r[WC_CHILD_Y + i] / 100.0f;
    CF(co, 0x08) = cw->child_x[i];
    CF(co, 0x10) = cw->child_z[i];
  }
  cw_restore[idx] = 0;
  cw_observed[idx] = 1;
  return 1;
}
void anchor_world_counterweight_control(void *root, int remote, int paused,
                                        int confirmed, unsigned int aggregate) {
  CwRoot *cw;
  int i;
  if (!root || D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM) {
    for (i = 0; i < 3; ++i) {
      cw_enabled[i] = cw_remote[i] = cw_paused[i] = cw_confirmed[i] = 0;
      cw_frozen[i] = 0;
      cw_aggregate[i] = 0;
    }
    return;
  }
  cw = find(root);
  if (!cw)
    return;
  i = index_of(cw);
  cw_enabled[i] = 1;
  cw_remote[i] = remote;
  cw_paused[i] = paused;
  cw_confirmed[i] = confirmed;
  cw_aggregate[i] = aggregate & 63u;
}
void anchor_world_counterweight_begin(void) {
  if (D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM)
    return;
  addresses();
  for (unsigned int i = 0; i < 3; ++i) {
    CwRoot *cw = &cw_roots[i];
    int ok;
    if (!cw->actor)
      continue;
    /* Only a root this session owns is reconstructed. Every registered root
     * still gets the wrapper, so a waiting or restoring piece can never run
     * the native local-only update and diverge. */
    ok = cw_enabled[i] ? prepare(cw) : 0;
    cw_frozen[i] = !ok || !cw_enabled[i] || !cw_confirmed[i] || cw_paused[i] ||
                   cw_restore[i];
    for (unsigned int s = 0; s < 6; ++s)
      if (bound_child(cw, s))
        install(cw, s);
  }
}

RECOMP_HOOK("func_80218C28_5D40F8")
void anchor_world_counterweight_child(void *parent, void *child) {
  if (D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM || !child)
    return;
  for (unsigned int i = 0; i < 3; ++i) {
    CwRoot *cw = &cw_roots[i];
    if (cw->actor != parent)
      continue;
    CwCallback init = raw(child);
    int slot = slot_of(cw_inits, init);
    if (slot < 0)
      return; /* A decoration, not one of the six pieces. */
    for (unsigned int s = 0; s < 6; ++s)
      if (cw->child[s] == child) {
        cw->child[s] = 0;
        cw->child_ready[s] = 0;
      }
    bind_child(cw, (unsigned int)slot, child);
    return;
  }
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_counterweight_reuse(void *actor) {
  if (!actor)
    return;
  for (unsigned int i = 0; i < 3; ++i) {
    CwRoot *cw = &cw_roots[i];
    if (cw->actor == actor) {
      retire_root(cw);
      continue;
    }
    for (unsigned int s = 0; s < 6; ++s)
      if (cw->child[s] == actor) {
        cw->child[s] = 0;
        cw->child_object[s] = 0;
        cw->child_ready[s] = 0;
        cw->pending[s] = 0;
      }
  }
}
/* The common post runs once per task turn. It is the point where a scheduled
 * initializer has finished and the child's own update callback is installed. */
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_counterweight_post(void *a) {
  CwRoot *cw;
  if (D_800C7AB2 != WORLD_COUNTERWEIGHT_ROOM || !a || !CP(a, 0x18))
    return;
  addresses();
  cw = find(a);
  if (cw) {
    cw->object = CP(a, 0x18);
    cw->generation = CB(a, 0x74);
    cw->ready = 1;
    return;
  }
  for (unsigned int i = 0; i < 3; ++i) {
    CwRoot *r = &cw_roots[i];
    for (unsigned int s = 0; s < 6; ++s) {
      if (r->child[s] != a)
        continue;
      r->child_object[s] = CP(a, 0x18);
      r->child_generation[s] = CB(a, 0x74);
      if (r->child_ready[s])
        return;
      if (raw(a) != cw_updates[s])
        return; /* The initializer still owns this tick. */
      if (r->pending[s]) {
        CF(r->child_object[s], 0x0c) = r->pending_y[s];
        r->pending[s] = 0;
      }
      install(r, s);
      return;
    }
  }
}

static void cw_callback(void *a, void *o) {
  int idx = -1;
  unsigned int slot = 0, selected;
  for (unsigned int i = 0; i < 3 && idx < 0; ++i)
    for (unsigned int s = 0; s < 6; ++s)
      if (cw_roots[i].child[s] == a) {
        idx = (int)i;
        slot = s;
        break;
      }
  if (idx < 0)
    return;
  CwRoot *cw = &cw_roots[idx];
  if (!live_child(cw, slot) || cw->child_object[slot] != o)
    return;
  if (!cw_enabled[idx]) {
    /* Disabled: the native update keeps its own local-only behaviour. */
    cw_updates[slot](a, o);
    return;
  }
  /* The native step-1 cull is local presentation and stays on every frame. */
  cull(a, o);
  if (cw_frozen[idx] || cw_restore[idx])
    return;
  selected = selected_bit(cw_aggregate[idx]);
  if (!cw_remote[idx]) {
    /* Mirror the shared state the native children would have written, then run
     * the pure prediction so both peers produce the identical height. */
    CB(cw->actor, 0xec) = (unsigned char)selected;
    CF(cw->actor, 0xd0) = selected ? cw_speed[bit_slot(selected)] : 0.0f;
  }
  advance(cw, slot, selected);
}
static void cw_retired(void *a, void *o) {
  (void)a;
  (void)o;
}
