#ifndef ANCHOR_WORLD_SLICER_H
#define ANCHOR_WORLD_SLICER_H
#include "world/anchor_world_dynamic.h"

/* Kind-specific union over the optional NPC continuation. Instance/receipt
 * are local bridge words; established/present are bounded wire presence. */
enum { WS_ROLE=74, WS_SUBTYPE, WS_SPEED, WS_REPEAT, WS_INITIAL,
       WS_INSTANCE, WS_RECEIPT, WS_ESTABLISHED, WS_PRESENT };
enum { WS_INITIAL_WAIT=18, WS_BIRTH, WS_REPEAT_WAIT, WS_FLIGHT };
#define WS_ROOT_ORIGIN 0x7ffffff9
#define WS_BLADE_ORIGIN 0x7ffffff8
#define WS_LOOT_ORIGIN 0x7ffffff7

static inline int anchor_world_slicer_params(unsigned int room, unsigned int parent,
                                            int *out) {
  static const unsigned char params[6][4] = {
    {0,0,120,120}, {1,1,120,180}, {2,2,60,30},
    {2,1,50,30}, {2,0,40,40}, {2,1,50,20}};
  unsigned int index;
  if (room==0xab && parent>=6 && parent<=7) index=parent-6;
  else if (room==0xac && parent>=6 && parent<=9) index=parent-4;
  else return 0;
  for (unsigned int j=0;j<4;++j) out[j]=params[index][j];
  return 1;
}

static inline int anchor_world_slicer_valid(const int *r) {
  int role=r[WS_ROLE], phase=r[WD_PHASE];
  if (role<0 || role>1 || r[WS_SUBTYPE]<0 || r[WS_SUBTYPE]>2 ||
      r[WS_SPEED]<0 || r[WS_SPEED]>2 || r[WS_REPEAT]<1 || r[WS_REPEAT]>255 ||
      r[WS_INITIAL]<2 || r[WS_INITIAL]>254 || (r[WS_INITIAL]&1) ||
      r[WS_INSTANCE]<0 || r[WS_RECEIPT]<0 ||
      r[WS_ESTABLISHED]<0 || r[WS_ESTABLISHED]>1 ||
      r[WS_PRESENT]<0 || r[WS_PRESENT]>1 ||
      r[WD_ENTITY]!=0x19d || r[WD_MODEL]!=0x19d || r[WD_CLIP] ||
      r[WD_ANIMATED]!=role || r[WD_PARENT]<1 || r[WD_PARENT]>256 ||
      r[WD_ROUTE]!=163 || r[WD_FLAGS_LO]!=0x6e1 || r[WD_FLAGS_HI]!=0x20 ||
      r[WD_ANIM_FLAGS] || r[WD_RATE]!=(role ? 256 : 0) ||
      r[WD_FRAME]>(role ? 25500 : 0) || r[WD_SPHERE]!=(role ? 1 : 17) ||
      r[WD_BOUNCE]<0 || r[WD_BOUNCE]>255) return 0;
  for (unsigned int j=WD_SX;j<=WD_SZ;++j) if (r[j]!=100) return 0;
  for (unsigned int j=WD_PATH_TIMER;j<=WD_DIALOG;++j) if (r[j]) return 0;
  if (r[WD_BUSY]) return 0;
  if (role) return r[WD_ORDINAL]>=1 && phase==WS_FLIGHT &&
                   r[WD_TIMER]>=-1 && r[WD_TIMER]<=120;
  if (r[WD_VX] || r[WD_VY] || r[WD_VZ]) return 0;
  return (phase==WS_INITIAL_WAIT && r[WD_TIMER]>=0 && r[WD_TIMER]<=r[WS_INITIAL] &&
          !(r[WD_TIMER]&1)) ||
         (phase==WS_BIRTH && r[WD_TIMER]>=-1 && r[WD_TIMER]<=0) ||
         (phase==WS_REPEAT_WAIT && r[WD_TIMER]>=0 && r[WD_TIMER]<=r[WS_REPEAT]);
}
static inline int anchor_world_slicer_scope(const int *r, unsigned int room) {
  int params[4];
  if (!anchor_world_slicer_params(room,(unsigned int)r[WD_PARENT],params)) return 0;
  for (unsigned int j=0;j<4;++j) if (r[WS_SUBTYPE+j]!=params[j]) return 0;
  return 1;
}
#endif
