#ifndef ANCHOR_WORLD_FISH_H
#define ANCHOR_WORLD_FISH_H

#include "world/anchor_world_dynamic.h"

/* File68's 0x338 fish use a native private animation object. Network rows
 * identify a placed fish and arbitrate its touch; they never reconstruct it. */
#define WFISH_ENTITY 0x338
#define WFISH_ROLE 74
#define WFISH_FLAG 75
#define WFISH_VARIANT 76
#define WFISH_MODE 77
#define WFISH_RESERVED 78
#define WFISH_INSTANCE 79
#define WFISH_RECEIPT 80
#define WFISH_ESTABLISHED 81
#define WFISH_PRESENT 82

static inline int anchor_world_fish_placement(unsigned int room,
                                               unsigned int parent,
                                               unsigned int *flag,
                                               unsigned int *variant) {
  static const unsigned char a_flags[] = {0xb8,0xb9,0xba,0xbb,0xbc};
  static const unsigned char a_variants[] = {0,1,2,2,0};
  static const unsigned char b_flags[] = {0xab,0xac,0xad,0xae,0xaf};
  static const unsigned char b_variants[] = {0,0,1,1,2};
  static const unsigned char c_flags[] = {0xa7,0xa8,0xa9,0xaa};
  static const unsigned char c_variants[] = {0,0,0,1};
  static const unsigned char d_flags[] = {0xb0,0xb1,0xb2,0xb3};
  static const unsigned char d_variants[] = {0,2,2,0};
  static const unsigned char e_flags[] = {0xbd,0xbe,0xbf,0xc0,0xc1};
  static const unsigned char e_variants[] = {0,1,1,1,2};
  const unsigned char *flags=0,*variants=0;
  unsigned int first=0,count=0,index;
  switch (room) {
    case 0x168: flags=a_flags;variants=a_variants;first=10;count=5;break;
    case 0x16b: flags=b_flags;variants=b_variants;first=6;count=5;break;
    case 0x16e: flags=c_flags;variants=c_variants;first=11;count=4;break;
    case 0x171: flags=d_flags;variants=d_variants;first=12;count=4;break;
    case 0x172: flags=e_flags;variants=e_variants;first=8;count=5;break;
    case 0x17e: flags=a_flags;variants=a_variants;first=3;count=5;break;
    case 0x180: flags=b_flags;variants=b_variants;first=2;count=5;break;
    default: return 0;
  }
  if (parent<first || parent>=first+count) return 0;
  index=parent-first;
  if (flag) *flag=flags[index];
  if (variant) *variant=variants[index];
  return 1;
}

static inline int anchor_world_fish_valid(const int *r) {
  unsigned int room,flag,variant,i;
  if (!r || r[WD_KIND]!=WD_FISH || r[WD_ENTITY]!=WFISH_ENTITY ||
      r[WD_MODEL]!=WFISH_ENTITY || r[WD_ROUTE]!=163 ||
      r[WD_ORDINAL] || r[WD_PHASE] || r[WD_CLIP] || r[WD_ANIMATED] ||
      r[WD_TALKABLE] || r[WD_DIALOG] || r[WD_BUSY] ||
      r[WD_BIRTH_X] || r[WD_BIRTH_Y] || r[WD_BIRTH_Z] ||
      r[WFISH_ROLE] || r[WFISH_MODE] || r[WFISH_RESERVED] ||
      r[WFISH_INSTANCE]<0 || r[WFISH_RECEIPT]<0 ||
      r[WFISH_ESTABLISHED]<0 || r[WFISH_ESTABLISHED]>1 ||
      r[WFISH_PRESENT]<0 || r[WFISH_PRESENT]>1 ||
      (r[WD_LIFE]==WD_CLAIM && !r[WD_LANDED])) return 0;
  /* This is an identity/claim row, not a portable copy of File68's private
   * animation state. Reject accidental pose or collision serialization. */
  for (i=WD_CLIP;i<WFISH_ROLE;++i)
    if (i!=WD_ROUTE && i!=WD_LANDED && i!=WD_COMMITTER && r[i])
      return 0;
  for (room=0x168;room<=0x180;++room)
    if (anchor_world_fish_placement(room,(unsigned int)r[WD_PARENT],
                                     &flag,&variant) &&
        r[WFISH_FLAG]==(int)flag && r[WFISH_VARIANT]==(int)variant)
      return 1;
  return 0;
}

#endif
