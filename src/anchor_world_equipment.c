/* File30 equipment rewards read their collection flag only in the constructor.
 * Reconcile an already-loaded idle reward before its local contact branch. The
 * collector's continuation owns dialogue/control cleanup and is never replayed
 * or removed here. Rotation and shine animation remain native/local. */
#ifndef WORLD_EQUIPMENT_HOST_TEST
#include "modding.h"
#endif

extern unsigned short D_800C7AB2;
extern int func_800240DC_24CDC(int);
extern void func_80218DA8_5D4278(void *, int, unsigned short, short);
extern void func_802139E0_5CEEB0(void *, void *);
extern void func_80213A9C_5CEF6C(void *);

#define EQ_B(p,o) (*(unsigned char *)((char *)(p)+(o)))
#define EQ_H(p,o) (*(unsigned short *)((char *)(p)+(o)))
#define EQ_W(p,o) (*(unsigned int *)((char *)(p)+(o)))
#ifndef EQ_PTR
#define EQ_PTR(p,o) (*(void **)((char *)(p)+(o)))
#endif
#ifndef EQ_DISABLED
#define EQ_DISABLED 0x00800000ul
#endif

static int equipment_pointer(const void *p) {
#ifdef WORLD_EQUIPMENT_HOST_TEST
  return p != 0;
#else
  unsigned long address = (unsigned long)p;
  return !(address & 3u) && address >= 0x80001000u && address < 0x80800000u;
#endif
}

static void equipment_collected(void *a, void *o, unsigned int entity,
                                unsigned int room, unsigned int flag) {
  if (!a || !o || D_800C7AB2 != room || EQ_PTR(a,0x18) != o ||
      EQ_H(a,0x5c) != entity || EQ_H(a,0x5e) != entity ||
      (EQ_W(a,0x68) & 2u) || !func_800240DC_24CDC((int)flag))
    return;

  /* 8021804C(a,1) allocates a category13 shine with inherited identity and
   * generation. Its initializer leaves D0 intact; 13A9C deletes on nonzero D0.
   * Validate the live local link even if a stale E4 survived slot reuse. */
  void *shine = EQ_PTR(a,0xe4);
  if (equipment_pointer(shine) && EQ_H(shine,0x5c) == entity &&
      EQ_B(shine,0x74) == EQ_B(a,0x74) && equipment_pointer(EQ_PTR(shine,0x18))) {
    unsigned long callback = (unsigned long)EQ_PTR(shine,0xc) & ~EQ_DISABLED;
    if (callback == (unsigned long)func_802139E0_5CEEB0 ||
        (callback == (unsigned long)func_80213A9C_5CEF6C && EQ_H(shine,0x5e) == 1))
      EQ_B(shine,0xd0) = 1;
  }
  /* Prevent the original idle callback from starting another local scenario
   * and prevent the common post from dispatching stale damage/contact. Native
   * finalization consumes bit2 in this same task's normal scheduler slot. */
  EQ_W(a,0x68) = (EQ_W(a,0x68) & ~0x40280u) | 2u;
  EQ_W(a,0x60) = 0;
  func_80218DA8_5D4278(a,0,0,0);
}

RECOMP_HOOK("func_080073BC_6C6B0C")
void anchor_world_equipment_hammer(void *actor, void *object) {
  equipment_collected(actor,object,0x3d2,0x62,0x1a6);
}
RECOMP_HOOK("func_080075DC_6C6D2C")
void anchor_world_equipment_fire_ryo(void *actor, void *object) {
  equipment_collected(actor,object,0x3d4,0x6e,0x1a4);
}
RECOMP_HOOK("func_080077C8_6C6F18")
void anchor_world_equipment_bazooka(void *actor, void *object) {
  equipment_collected(actor,object,0x3d5,0x81,0x1a5);
}
