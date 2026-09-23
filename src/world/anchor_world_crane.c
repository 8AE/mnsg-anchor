/* File_30 room 0x31: one atomic crane/pad/reward checkpoint. Native resources,
 * camera tasks, scene/fade handles and player control remain process-local. */
#ifndef WORLD_CRANE_HOST_TEST
#include "platform/modding.h"
#endif
#include "world/anchor_world.h"
#include "world/anchor_world_crane.h"

#define CB(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define CH(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define CS(p,o) (*(short *)((char *)(p)+(o)))
#define CW(p,o) (*(unsigned int *)((char *)(p)+(o)))
#define CF(p,o) (*(float *)((char *)(p)+(o)))
#ifndef CP
#define CP(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef CR_DISABLED
#define CR_DISABLED 0x00800000ul
#endif
typedef void (*CraneCallback)(void *, void *);
extern unsigned short D_800C7AB2;
extern void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
extern unsigned char D_8015C562_15D162;
extern float D_8015CDB4[3];
extern void *D_80236984_5F1E54[];
extern int func_800141C4_14DC4(unsigned int);
extern int func_800240DC_24CDC(int);
extern void func_80024038_24C38(int);
extern int func_80023E94_24A94(int);
extern void func_80023DF0_249F0(int);
extern void func_80023E40_24A40(int);
extern void func_80038B98_39798(unsigned int);
extern void func_801DACDC_596BEC(void *, unsigned char);
extern void func_801FA3FC_5B630C(unsigned char, unsigned char, unsigned char);
extern void func_80216DF8_5D22C8(void *, unsigned int);
extern float func_8001B5AC_1C1AC(void *);
extern void *func_802171A8_5D2678(void *, CraneCallback, unsigned char);

#define CRANE_PHASES(X) \
 X(func_08000BE8_6C0338) X(func_08000DB4_6C0504) \
 X(func_08000E2C_6C057C) X(func_08000F34_6C0684) \
 X(func_08001018_6C0768) X(func_080010B8_6C0808) \
 X(func_08001174_6C08C4) X(func_08001228_6C0978) \
 X(func_08001268_6C09B8) X(func_080012B8_6C0A08) \
 X(func_08001324_6C0A74) X(func_08001388_6C0AD8) \
 X(func_080013E4_6C0B34) X(func_08001488_6C0BD8) \
 X(func_080014C8_6C0C18) X(func_0800152C_6C0C7C) \
 X(func_08001594_6C0CE4) X(func_08001600_6C0D50) \
 X(func_0800166C_6C0DBC)
#define PAD0_PHASES(X) \
 X(func_08002848_6C1F98) X(func_08002930_6C2080) \
 X(func_080029A0_6C20F0) X(func_08002A20_6C2170) X(func_08002A98_6C21E8)
#define PAD1_PHASES(X) \
 X(func_08002B4C_6C229C) X(func_08002BF8_6C2348) \
 X(func_08002C68_6C23B8) X(func_08002CEC_6C243C) X(func_08002D64_6C24B4)
#define REWARD_PHASES(X) \
 X(func_08001874_6C0FC4) X(func_080018C8_6C1018) \
 X(func_08001920_6C1070) X(func_08001974_6C10C4) \
 X(func_080019E4_6C1134) X(func_08001A24_6C1174) \
 X(func_08001A7C_6C11CC) X(func_08001B04_6C1254) \
 X(func_08001B60_6C12B0) X(func_08001BBC_6C130C) \
 X(func_08001C10_6C1360) X(func_08001CCC_6C141C)
#define CR_DECLARE(f) extern void f(void *, void *);
CRANE_PHASES(CR_DECLARE)
PAD0_PHASES(CR_DECLARE)
PAD1_PHASES(CR_DECLARE)
REWARD_PHASES(CR_DECLARE)
CR_DECLARE(func_08000ACC_6C021C)
CR_DECLARE(func_080027CC_6C1F1C)
CR_DECLARE(func_08002ACC_6C221C)
CR_DECLARE(func_08000AC0_6C0210)
CR_DECLARE(func_08002624_6C1D74)
CR_DECLARE(func_0800221C_6C196C)
CR_DECLARE(func_08002474_6C1BC4)
CR_DECLARE(func_08002488_6C1BD8)
CR_DECLARE(func_0800178C_6C0EDC)

typedef struct {
  void *actor, *parent;
  CraneCallback saved;
  unsigned char generation, parent_generation, ready, proxy;
  unsigned int post_flags;
  float post_velocity[3];
} CraneNode;
/* Roots: crane, pads, power, reward. Nodes: moving crane, pad0, pad1. */
static CraneNode cr_roots[4], cr_nodes[3];
static struct { void *actor, *parent; unsigned char parent_generation; } cr_pending[8];
static CraneCallback cr_phases[19], cr_pad[2][5], cr_reward[12];
static int cr_enabled, cr_remote, cr_paused, cr_complete, cr_post_slot = -1;
static int cr_observed, cr_restore;
static int cr_frozen;
static unsigned int cr_inputs, cr_previous_local, cr_local_history;
static void crane_callback(void *, void *);

static void addresses(void) {
  unsigned int i = 0;
#define CR_MAIN(f) cr_phases[i++] = f;
  CRANE_PHASES(CR_MAIN)
#undef CR_MAIN
  i = 0;
#define CR_P0(f) cr_pad[0][i++] = f;
  PAD0_PHASES(CR_P0)
#undef CR_P0
  i = 0;
#define CR_P1(f) cr_pad[1][i++] = f;
  PAD1_PHASES(CR_P1)
#undef CR_P1
  i = 0;
#define CR_REWARD(f) cr_reward[i++] = f;
  REWARD_PHASES(CR_REWARD)
#undef CR_REWARD
}
static void clear(void *p, unsigned int size) {
  for (unsigned int i = 0; i < size; ++i) ((volatile unsigned char *)p)[i] = 0;
}
static int alive(const CraneNode *n) {
  return n->actor && n->ready && CP(n->actor,0x18) &&
         CB(n->actor,0x74) == n->generation && !(CW(n->actor,0x68)&2u) &&
         (!n->parent || (CB(n->parent,0x74) == n->parent_generation &&
                        CP(n->actor,0xdc) == n->parent));
}
static CraneCallback callback(const CraneNode *n) {
  void *p = n->actor ? CP(n->actor,0xc) : 0;
  if (((unsigned long)p & ~CR_DISABLED) == (unsigned long)crane_callback)
    return n->saved;
  return (CraneCallback)((unsigned long)p & ~CR_DISABLED);
}
static int phase(const CraneNode *n, CraneCallback *table, unsigned int count) {
  CraneCallback p = callback(n);
  for (unsigned int i = 0; i < count; ++i) if (p == table[i]) return (int)i+1;
  return 0;
}
static void schedule(CraneNode *n, CraneCallback next) {
  CP(n->actor,0xc) = (void *)((unsigned long)next |
                            ((unsigned long)CP(n->actor,0xc)&CR_DISABLED));
}
static void detach(CraneNode *n, int retire) {
  /* Parent identity may already be invalid. Only touch this still-owned task
   * generation; a pool reset hook clears its binding before reuse. */
  if (!n->actor || CB(n->actor,0x74) != n->generation) return;
  if (n->saved && ((unsigned long)CP(n->actor,0xc)&~CR_DISABLED) ==
                  (unsigned long)crane_callback)
    schedule(n,n->saved);
  if (retire) CW(n->actor,0x68) |= 2u;
}
static CraneNode *slot(unsigned int i) {
  return i < 3 ? &cr_nodes[i] : &cr_roots[i-3];
}
void anchor_world_crane_end(void) {
  for (unsigned int i = 0; i < 7; ++i) {
    CraneNode *n = slot(i);
    detach(n,0);
    n->saved = 0;
  }
}
void anchor_world_crane_reset(int room_changed) {
  anchor_world_crane_end();
  cr_enabled = cr_remote = cr_paused = cr_complete = 0;
  cr_observed = cr_restore = cr_frozen = 0;
  cr_inputs = cr_previous_local = cr_local_history = 0;
  cr_post_slot = -1;
  if (room_changed) {
    clear(cr_roots,sizeof(cr_roots));clear(cr_nodes,sizeof(cr_nodes));
    clear(cr_pending,sizeof(cr_pending));
  }
}
void anchor_world_crane_register(void *a, unsigned int entity) {
  int r = entity == 0x1b9 ? 0 : entity == 0x1bb ? 1 :
          entity == 0x1ba ? 2 : entity == 0x3d3 ? 3 : -1;
  if (D_800C7AB2 != 0x31 || r < 0) return;
  addresses();
  if ((r == 0 || r == 1) && cr_roots[r].actor) {
    void *previous = cr_roots[r].actor;
    for (unsigned int i = 0; i < 3; ++i)
      if (cr_nodes[i].parent == previous) {
        detach(&cr_nodes[i],1);clear(&cr_nodes[i],sizeof(cr_nodes[i]));
      }
    for (unsigned int i = 0; i < 8; ++i)
      if (cr_pending[i].parent == previous) {
        void *child = cr_pending[i].actor;
        if (child && CP(child,0xdc) == previous &&
            CB(child,0x74) == cr_pending[i].parent_generation) CW(child,0x68) |= 2u;
        cr_pending[i].actor = 0;
      }
  }
  if (r == 3 && cr_roots[r].proxy && alive(&cr_roots[r])) {
    /* Placement can bind during the scheduler. An older proxy's already
     * started local scene must keep its real cleanup callback after detaching. */
    CraneCallback previous = callback(&cr_roots[r]);
    schedule(&cr_roots[r],previous);
    if (previous != cr_reward[11]) CW(cr_roots[r].actor,0x68) |= 2u;
  }
  if (cr_observed) cr_restore = 1;
  clear(&cr_roots[r],sizeof(cr_roots[r]));
  cr_roots[r].actor = a;
  cr_roots[r].generation = CB(a,0x74);
}
RECOMP_HOOK("func_80218C28_5D40F8")
void anchor_world_crane_child(void *parent, void *child) {
  if (D_800C7AB2 != 0x31 || !child ||
      (parent != cr_roots[0].actor && parent != cr_roots[1].actor)) return;
  /* 802171A8 has installed the initializer before this hook. Decorations are
   * not puzzle nodes and must not gate their readiness. */
  CraneCallback init = (CraneCallback)((unsigned long)CP(child,0xc)&~CR_DISABLED);
  if (init != func_08000ACC_6C021C && init != func_080027CC_6C1F1C &&
      init != func_08002ACC_6C221C) return;
  for (unsigned int i = 0; i < 8; ++i) if (!cr_pending[i].actor) {
    cr_pending[i].actor = child;cr_pending[i].parent = parent;
    cr_pending[i].parent_generation = CB(parent,0x74);return;
  }
}
RECOMP_HOOK("func_80034A10_35610")
void anchor_world_crane_reuse(void *actor) {
  for (unsigned int i = 0; i < 7; ++i) {
    CraneNode *n = slot(i);
    if (n->actor == actor) { detach(n,0);clear(n,sizeof(*n)); }
    else if (n->parent == actor) { detach(n,1);clear(n,sizeof(*n)); }
  }
  for (unsigned int i = 0; i < 8; ++i)
    if (cr_pending[i].actor == actor || cr_pending[i].parent == actor) cr_pending[i].actor = 0;
}
unsigned int anchor_world_crane_local_inputs(void) {
  unsigned int bits = 0;
  if (!D_801FC604_5B8514 || !func_800240DC_24CDC(0x15a)) return 0;
  for (unsigned int i = 0; i < 2; ++i)
    if (alive(&cr_nodes[i+1]) &&
        CP(D_801FC604_5B8514,0xa0) == CP(cr_nodes[i+1].actor,0x18)) bits |= 1u<<i;
  return bits;
}
int anchor_world_crane_needs_restore(void) { return cr_restore; }
static int resident(void) {
  static const unsigned short ids[] = {0x1b9,0x1bb,0x1ba,0x3d3};
  static const unsigned char clips[] = {5,2,1,0};
  if (func_800141C4_14DC4(30) == -1 || func_800141C4_14DC4(0x152) == -1) return 0;
  for (unsigned int i = 0; i < 4; ++i) {
    void **m = D_80236984_5F1E54[ids[i]];
    if (!m || !m[0] || !m[1]) return 0;
    const unsigned short *f = m[0]; const unsigned int *c = m[1];
    if (func_800141C4_14DC4(f[0]) == -1 || func_800141C4_14DC4(f[1]) == -1) return 0;
    for (unsigned int j = 0; j <= clips[i]; ++j) if (!c[j]) return 0;
  }
  return 1;
}
static int prepare(void) {
  if (D_800C7AB2 != 0x31 || !resident() || !alive(&cr_roots[0]) ||
      !alive(&cr_roots[1])) return 0;
  if (callback(&cr_roots[0]) != func_08000AC0_6C0210 ||
      callback(&cr_roots[1]) != func_08002624_6C1D74) return 0;
  /* The root may finish before its newly queued children run their first
   * initializer. Let those births identify themselves before reconstructing. */
  for (unsigned int i = 0; i < 8; ++i) if (cr_pending[i].actor) {
    void *a = cr_pending[i].actor, *parent = cr_pending[i].parent;
    if ((CW(a,0x68)&2u) || !CP(a,0x18) || CP(a,0xdc) != parent ||
        CB(parent,0x74) != cr_pending[i].parent_generation)
      cr_pending[i].actor = 0;
    else return 0;
  }
  for (unsigned int i = 0; i < 3; ++i) {
    CraneNode *n = &cr_nodes[i];
    if (alive(n) || (n->actor && !n->ready && !(CW(n->actor,0x68)&2u) &&
        CP(n->actor,0x18) && CB(n->actor,0x74) == n->generation &&
        n->parent && CB(n->parent,0x74) == n->parent_generation)) continue;
    detach(n,1);clear(n,sizeof(*n));
    void *parent = cr_roots[i ? 1 : 0].actor;
    CraneCallback init = i == 0 ? func_08000ACC_6C021C :
                         i == 1 ? func_080027CC_6C1F1C : func_08002ACC_6C221C;
    void *a = func_802171A8_5D2678(parent,init,3);
    if (!a) continue;
    CH(a,0x28) = 30;CP(a,0x2c) = (void *)(unsigned long)(unsigned int)func_800141C4_14DC4(30);
    CP(a,0xdc) = parent;
    clear(n,sizeof(*n));n->actor = a;n->parent = parent;
    n->generation = CB(a,0x74);
    n->parent_generation = CB(parent,0x74);
    if (cr_observed) cr_restore = 1;
  }
  if (!func_800240DC_24CDC(0x1a3) && !alive(&cr_roots[3]) &&
      !(cr_roots[3].actor && !cr_roots[3].ready &&
        CP(cr_roots[3].actor,0x18) && !(CW(cr_roots[3].actor,0x68)&2u))) {
    /* The coupled placed reward may be outside this client's placement box.
     * Its pure initializer can reconstruct it as a parent-owned local task. */
    void *parent = cr_roots[0].actor;
    detach(&cr_roots[3],1);clear(&cr_roots[3],sizeof(cr_roots[3]));
    void *a = func_802171A8_5D2678(parent,func_0800178C_6C0EDC,9);
    if (a) {
      CH(a,0x5c) = 0x3d3;CH(a,0x28) = 30;
      CP(a,0x2c) = (void *)(unsigned long)(unsigned int)func_800141C4_14DC4(30);
      CP(a,0xdc) = parent;
      clear(&cr_roots[3],sizeof(cr_roots[3]));cr_roots[3].actor = a;
      cr_roots[3].generation = CB(a,0x74);
      cr_roots[3].parent = parent;cr_roots[3].parent_generation = CB(parent,0x74);
      cr_roots[3].proxy = 1;if (cr_observed) cr_restore = 1;
    }
  }
  return alive(&cr_nodes[0]) && alive(&cr_nodes[1]) && alive(&cr_nodes[2]) &&
         (alive(&cr_roots[3]) || func_800240DC_24CDC(0x1a3));
}
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_crane_post(void *a) {
  cr_post_slot = -1;
  if (D_800C7AB2 != 0x31 || !a) return;
  addresses();
  for (unsigned int i = 0; i < 8; ++i) if (cr_pending[i].actor == a) {
    CraneNode probe = {0};probe.actor = a;
    if (!CP(a,0x18) || (CW(a,0x68)&2u) ||
        CB(cr_pending[i].parent,0x74) != cr_pending[i].parent_generation) {
      cr_pending[i].actor = 0;break;
    }
    int k = cr_pending[i].parent == cr_roots[0].actor && phase(&probe,cr_phases,19) ? 0 :
            cr_pending[i].parent == cr_roots[1].actor && phase(&probe,cr_pad[0],5) ? 1 :
            cr_pending[i].parent == cr_roots[1].actor && phase(&probe,cr_pad[1],5) ? 2 : -1;
    if (k >= 0) {
      if (cr_observed && cr_nodes[k].actor != a) cr_restore = 1;
      if (cr_nodes[k].actor != a) detach(&cr_nodes[k],1);
      clear(&cr_nodes[k],sizeof(cr_nodes[k]));cr_nodes[k].actor = a;
      cr_nodes[k].parent = cr_pending[i].parent;
      cr_nodes[k].parent_generation = cr_pending[i].parent_generation;
    }
    cr_pending[i].actor = 0;
  }
  if (!CP(a,0x18)) return;
  for (unsigned int i = 0; i < 7; ++i) {
    CraneNode *n = slot(i);
    if (n->actor != a) continue;
    if (!n->ready) { n->ready = 1;n->generation = CB(a,0x74); }
    if (cr_enabled && cr_frozen && (i < 3 || i == 6) && alive(n)) {
      cr_post_slot = (int)i;n->post_flags = CW(a,0x60);CW(a,0x60) &= ~0x800301u;
      for (unsigned int j = 0; j < 3; ++j) {
        n->post_velocity[j] = CF(a,0x78+j*4);CF(a,0x78+j*4) = 0;
      }
    }
    return;
  }
}
RECOMP_HOOK_RETURN("func_80218F30_5D4400")
void anchor_world_crane_post_return(void) {
  int i = cr_post_slot;cr_post_slot = -1;
  if (i < 0 || !alive(slot((unsigned int)i))) return;
  CraneNode *n = slot((unsigned int)i);CW(n->actor,0x60) = n->post_flags;
  for (unsigned int j = 0; j < 3; ++j) CF(n->actor,0x78+j*4) = n->post_velocity[j];
}
static int quantize(float f, float scale, int lo, int hi, int *out) {
  volatile union { float f; unsigned int u; } value;value.f = f;
  if ((value.u&0x7f800000u) == 0x7f800000u) return 0;
  f *= scale;if (!(f >= (float)lo && f <= (float)hi)) return 0;
  *out = (int)f;return 1;
}
static unsigned int shared_bits(void) {
  unsigned int bits = 0;
  for (unsigned int i = 0; i < 15; ++i)
    if ((0x4bcfu&(1u<<i)) && func_80023E94_24A94((int)i)) bits |= 1u<<i;
  return bits;
}
int anchor_world_crane_capture(void *root, int *r) {
  if (D_800C7AB2 != 0x31 || root != cr_roots[0].actor || cr_restore ||
      !alive(&cr_roots[0]) || !alive(&cr_roots[1]) ||
      !alive(&cr_nodes[0]) || !alive(&cr_nodes[1]) || !alive(&cr_nodes[2])) return 0;
  addresses();
  void *a = cr_nodes[0].actor, *o = CP(a,0x18);
  int p = phase(&cr_nodes[0],cr_phases,19);
  if (!p) return 0;
  for (unsigned int j = 0; j < 3; ++j) {
    if (!quantize(CF(o,8+j*4),100,-3276800,3276700,&r[4+j]) ||
        !quantize(CF(a,0x78+j*4),1000,-100000,100000,&r[14+j]) ||
        !quantize(CF(o,0x1c+j*4),1000,0,64000,&r[26+j])) return 0;
    r[7+j] = CH(o,0x14+j*2)&1023;
  }
  r[10] = 1;
  if (!quantize(CF(o,0x28),100,0,1000000,&r[11])) return 0;
  r[12] = CH(o,0x7e);r[13] = CB(o,0x7c)&7;
  r[17] = (p == 4 || p == 9 || p == 11 || p == 12 || p == 17) ? CS(a,0x8a) : 0;
  r[18] = p;
  for (unsigned int i = 0; i < 2; ++i) {
    CraneNode *n = &cr_nodes[i+1];int k = 19+(int)i*3;
    r[k] = phase(n,cr_pad[i],5);if (!r[k]) return 0;
    r[k+1] = (r[k] == 2 || r[k] == 4) ? CS(n->actor,0x8a) : 0;
    if (!quantize(CF(CP(n->actor,0x18),0xc),100,-32768,32767,&r[k+2])) return 0;
  }
  r[25] = (int)shared_bits();r[29] = (CW(a,0x60)&1u) | ((CW(a,0x60)>>23)&2u);
  r[30] = !!func_800240DC_24CDC(0x15a);
  r[32] = !!func_800240DC_24CDC(0x1a3);
  r[33] = !!func_800240DC_24CDC(0x15e) | (!!func_800240DC_24CDC(0x15f)<<1);
  CraneNode *reward = &cr_roots[3];
  r[37] = r[32] ? 13 : phase(reward,cr_reward,12);
  if (!r[32] && (!alive(reward) || !r[37] || r[37] == 12)) return 0;
  if (r[32]) r[44] = 2;
  else {
    void *ra = reward->actor, *ro = CP(ra,0x18);
    for (unsigned int j = 0; j < 3; ++j)
      if (!quantize(CF(ro,8+j*4),100,-32768,32767,&r[34+j]) ||
          !quantize(CF(ra,0x78+j*4),1000,-32768,32767,&r[40+j])) return 0;
    r[39] = CH(ro,0x16)&1023;r[43] = r[37] == 5 ? CS(ra,0x8a) : 0;
    if (CW(ra,0x60) != 0x2006e1u && CW(ra,0x60) != 0x2a007e1u) return 0;
    r[44] = CW(ra,0x60) == 0x2a007e1u;r[45] = !!(CW(ra,0x68)&0x20u);
  }
  r[WORLD_CRANE_INPUT] = (int)anchor_world_crane_local_inputs();
  r[WORLD_CRANE_AGGREGATE] = 0;
  cr_observed = 1;return 1;
}
static void release_local_crane_control(void) {
  if (cr_local_history && D_801FC604_5B8514)
    func_801DACDC_596BEC(D_801FC604_5B8514,0);
  cr_local_history = 0;
}
static void power_visual(void) {
  CraneNode *n = &cr_roots[2];
  if (!alive(n) || !func_800240DC_24CDC(0x15a)) return;
  CraneCallback p = callback(n);
  /* An active local fade must run its own release. Only replace idle state. */
  if (p == func_0800221C_6C196C) {
    func_80216DF8_5D22C8(n->actor,1);CB(n->actor,0x6c) = 1;
    func_801FA3FC_5B630C(255,255,255);
    schedule(n,func_08002488_6C1BD8);
  }
}
int anchor_world_crane_apply(void *root, const int *r) {
  if (r[2] != WORLD_CRANE || root != cr_roots[0].actor ||
      !anchor_world_row_valid(r) || !prepare()) return 0;
  addresses();
  CraneNode *main = &cr_nodes[0];void *a = main->actor,*o = CP(a,0x18);
  float frame = (float)r[11]/100,limit = func_8001B5AC_1C1AC(o);
  if (!(limit > 0)) return 0;
  if (phase(main,cr_phases,19) == 1 && r[18] != 1) release_local_crane_control();
  for (unsigned int j = 0; j < 3; ++j) {
    CF(o,8+j*4) = (float)r[4+j]/100;CH(o,0x14+j*2) = (unsigned short)r[7+j];
    CF(a,0x78+j*4) = (float)r[14+j]/1000;CF(o,0x1c+j*4) = (float)r[26+j]/1000;
    D_8015CDB4[j] = CF(o,8+j*4);
  }
  if (frame >= limit) frame = limit-1;if (frame < 0) frame = 0;
  CF(o,0x28) = frame;CH(o,0x7e) = (unsigned short)r[12];
  CB(o,0x7c) = (CB(o,0x7c)&~7u)|(unsigned int)r[13];CS(a,0x8a) = (short)r[17];
  CW(a,0x60) = 0x802006e0u | (unsigned int)(r[29]&1) | ((unsigned int)(r[29]&2)<<23);
  schedule(main,cr_phases[r[18]-1]);
  for (unsigned int i = 0; i < 2; ++i) {
    CraneNode *n = &cr_nodes[i+1];int k = 19+(int)i*3;
    CS(n->actor,0x8a) = (short)r[k+1];CF(CP(n->actor,0x18),0xc) = (float)r[k+2]/100;
    schedule(n,cr_pad[i][r[k]-1]);
  }
  for (unsigned int i = 0; i < 15; ++i) if (0x4bcfu&(1u<<i)) {
    if ((unsigned int)r[25]&(1u<<i)) func_80023DF0_249F0((int)i);
    else func_80023E40_24A40((int)i);
  }
  if (r[30]) func_80024038_24C38(0x15a);
  if (r[33]&1) func_80024038_24C38(0x15e);
  if (r[33]&2) func_80024038_24C38(0x15f);
  if (r[32]) func_80024038_24C38(0x1a3);
  cr_complete = !!func_800240DC_24CDC(0x1a3);
  CraneNode *reward = &cr_roots[3];
  if (alive(reward) && phase(reward,cr_reward,12) != 12) {
    void *ra = reward->actor,*ro = CP(ra,0x18);
    if (cr_complete) CW(ra,0x68) |= 2u;
    else {
      for (unsigned int j = 0; j < 3; ++j) {
        CF(ro,8+j*4) = (float)r[34+j]/100;CF(ra,0x78+j*4) = (float)r[40+j]/1000;
      }
      CH(ro,0x16) = (unsigned short)r[39];CS(ra,0x8a) = (short)r[43];
      CW(ra,0x60) = r[44] == 1 ? 0x2a007e1u : 0x2006e1u;
      CW(ra,0x68) = (CW(ra,0x68)&~0x20u) | ((unsigned int)r[45]<<5);
      schedule(reward,cr_reward[r[37]-1]);
    }
  }
  power_visual();cr_restore = 0;cr_observed = 1;return 1;
}
void anchor_world_crane_control(void *root, int remote, int paused,
                                unsigned int inputs) {
  cr_enabled = D_800C7AB2 == 0x31 && root && root == cr_roots[0].actor;
  cr_remote = remote;cr_paused = paused;cr_inputs = inputs&3u;
}
void anchor_world_crane_begin(void) {
  if (!cr_enabled || D_800C7AB2 != 0x31) return;
  addresses();cr_frozen = !prepare() || cr_restore || (cr_remote && cr_paused);
  power_visual();
  unsigned int local = anchor_world_crane_local_inputs();
  if (!cr_frozen) {
    if (local & ~cr_previous_local) D_8015C562_15D162 = 1;
    if ((cr_previous_local & ~local)&1u) D_8015C562_15D162 = 0;
    if ((cr_previous_local & ~local)&2u) D_8015C562_15D162 = 1;
    cr_local_history |= local;
    cr_previous_local = local;
  }
  for (unsigned int i = 0; i < 4; ++i) {
    CraneNode *n = i < 3 ? &cr_nodes[i] : &cr_roots[3];
    if (!alive(n)) continue;
    int p = i == 0 ? phase(n,cr_phases,19) :
            i < 3 ? phase(n,cr_pad[i-1],5) : phase(n,cr_reward,12);
    if (!p) continue; /* Initializers must run once before a checkpoint binds. */
    n->saved = callback(n);
    CP(n->actor,0xc) = (void *)((unsigned long)crane_callback |
                         ((unsigned long)CP(n->actor,0xc)&CR_DISABLED));
  }
}
static void crane_callback(void *a, void *o) {
  CraneNode *n = 0;unsigned int i;
  for (i = 0; i < 4; ++i) {
    n = i < 3 ? &cr_nodes[i] : &cr_roots[3];
    if (n->actor == a) break;
  }
  if (i == 4 || !alive(n) || !n->saved) return;
  int p = i == 0 ? phase(n,cr_phases,19) :
          i < 3 ? phase(n,cr_pad[i-1],5) : phase(n,cr_reward,12);
  if (i == 3 && p == 12) {
    n->saved(a,o);return; /* An already-started local scene owns its cleanup. */
  }
  if (cr_frozen) return;
  if (i == 0 && p == 1) {
    if (!func_800240DC_24CDC(0x15a)) return;
    if (func_80023E94_24A94(0)) CF(o,8) += 1;
    if (func_80023E94_24A94(1)) CF(o,0x10) -= 1;
    if (CF(o,8) > -20) { CF(o,8) = -20;func_80038B98_39798(0x8154); }
    if (CF(o,0x10) < -100) { CF(o,0x10) = -100;func_80038B98_39798(0x8154); }
    if (!cr_remote && func_80023E94_24A94(2) && func_80023E94_24A94(3)) {
      func_80023DF0_249F0(14);func_80038B98_39798(0x154);
      release_local_crane_control();
      if (CF(o,8)-15 < -190) {
        schedule(n,cr_phases[13]);func_80038B98_39798(0x154);
      } else { CW(a,0x60) |= 1u;schedule(n,cr_phases[1]); }
    }
    return;
  }
  if (i == 1 || i == 2) {
    unsigned int pad = i-1, mask = 1u<<pad;
    if (p == 1) {
      if (!cr_remote && (cr_inputs&mask) && func_800240DC_24CDC(0x15a)) {
        func_80023DF0_249F0((int)pad);func_80038B98_39798(0x2a5);
        func_80038B98_39798(0x154);CS(a,0x8a) = 10;schedule(n,cr_pad[pad][1]);
      }
      return;
    }
    if (p == 3) {
      if (!cr_remote) {
        func_80023DF0_249F0((int)pad);
        if (!(cr_inputs&mask)) {
          func_80038B98_39798(0x155);CS(a,0x8a) = 10;schedule(n,cr_pad[pad][3]);
        }
      }
      return;
    }
    /* Lower/raise/rearm touch only this pad, timer, sound and typed room bits. */
    n->saved(a,o);return;
  }
  if (i == 3 && (cr_complete || func_800240DC_24CDC(0x1a3))) {
    CW(a,0x68) |= 2u;return;
  }
  /* Crane motion uses only its own transform/animation, camera target XYZ,
   * typed milestones and native sound. Reward phases1..10 likewise contain
   * no award; phase11 must keep local contact/scenario handling on each peer. */
  n->saved(a,o);
}
