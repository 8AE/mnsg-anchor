#ifndef ANCHOR_WORLD_BOULDER_H
#define ANCHOR_WORLD_BOULDER_H
#include "world/anchor_world_dynamic.h"

/* File34's placed Kompira boulders. Race mode may make extra native copies of
 * one placement, each with a distinct, bounded ordinal. No native task or
 * callback address is serialized. */
#define WBO_ENTITY 0x3da
#define WBO_FILE 34
#define WBO_ACTIVE_MODEL 0x191
enum { WBO_ROLE=74, WBO_RESERVED0, WBO_RESERVED1, WBO_RESERVED2,
       WBO_RESERVED3, WBO_INSTANCE, WBO_RECEIPT, WBO_ESTABLISHED, WBO_PRESENT };
#define WBO_ROOT_ORIGIN 0x7fffffec

static inline int anchor_world_boulder_placement(unsigned int room,
                                                 unsigned int parent,
                                                 short out[3]) {
  static const short a[8][3]={{9,-65,161},{47,-95,222},{21,-25,80},
      {59,134,-237},{94,134,-237},{130,134,-237},{89,94,-155},
      {68,34,-34}};
  static const short b[12][3]={{84,-213,465},{62,-233,509},
      {-23,-116,-192},{11,-96,-152},{117,-183,403},
      {39,66,-94},{5,36,-35},{51,86,-136},{15,76,-112},
      {-60,236,-626},{-100,236,-626},{-142,236,-626}};
  const short *p;
  if (room==0x13d && parent>=10 && parent<=17) p=a[parent-10];
  else if (room==0x13f && parent>=8 && parent<=12) p=b[parent-8];
  else if (room==0x13f && parent>=18 && parent<=24) p=b[parent-13];
  else return 0;
  if (out) for (unsigned int j=0;j<3;++j) out[j]=p[j];
  return 1;
}

static inline int anchor_world_boulder_parent(unsigned int room,
                                               unsigned int parent) {
  return (room==0x13d && parent>=10 && parent<=17) ||
         (room==0x13f && ((parent>=8 && parent<=12) ||
                          (parent>=18 && parent<=24)));
}
static inline int anchor_world_boulder_scope(const int *r,unsigned int room) {
  short xyz[3];
  if (!anchor_world_boulder_placement(room,(unsigned int)r[WD_PARENT],xyz) ||
      r[WD_ENTITY]!=WBO_ENTITY || r[WD_ORDINAL]<0 ||
      r[WD_ORDINAL]>65535) return 0;
  for (unsigned int j=0;j<3;++j)
    if (r[WD_BIRTH_X+j]!=(int)xyz[j]) return 0;
  return 1;
}
static inline int anchor_world_boulder_valid(const int *r) {
  unsigned int j;
  if (!anchor_world_boulder_scope(r,0x13d) &&
      !anchor_world_boulder_scope(r,0x13f)) return 0;
  if (r[WBO_ROLE] || r[WBO_RESERVED0] || r[WBO_RESERVED1] ||
      r[WBO_RESERVED2] || r[WBO_RESERVED3] || r[WBO_INSTANCE]<0 ||
      r[WBO_RECEIPT]<0 || r[WBO_ESTABLISHED]<0 ||
      r[WBO_ESTABLISHED]>1 || r[WBO_PRESENT]<0 || r[WBO_PRESENT]>1 ||
      r[WD_PHASE]<0 || r[WD_PHASE]>4 || r[WD_TIMER]<0 ||
      r[WD_TIMER]>150 || r[WD_ROUTE]!=163 || r[WD_TALKABLE] ||
      r[WD_DIALOG] || r[WD_BUSY] || r[WD_LIFE]==WD_CLAIM ||
      r[WD_CLIP] || (r[WD_PHASE]==4 && !r[WD_ANIMATED]) ||
      r[WD_MODEL] != (r[WD_PHASE]>=4 ? WBO_ACTIVE_MODEL : WBO_ENTITY))
    return 0;
  for (j=WD_PATH_TIMER;j<=WD_FACING;++j) if (r[j]) return 0;
  return 1;
}
#endif
