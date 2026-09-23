#ifndef ANCHOR_WORLD_FRAGILE_H
#define ANCHOR_WORLD_FRAGILE_H
#include "world/anchor_world_dynamic.h"

/* File30's placed one-HP objects, independently dying child, and native loot.
 * The optional continuation union is shared with other typed families. */
enum { WF_ROLE=74, WF_VARIANT, WF_RESERVED0, WF_RESERVED1, WF_RESERVED2,
       WF_INSTANCE, WF_RECEIPT, WF_ESTABLISHED, WF_PRESENT };
#define WF_ROOT_ORIGIN 0x7fffffef
#define WF_CHILD_ORIGIN 0x7fffffee
#define WF_LOOT_ORIGIN 0x7fffffed
#define WF_PHASE 23

static inline int anchor_world_fragile_scope(unsigned int room,
                                              unsigned int entity,
                                              unsigned int parent) {
  static const struct { unsigned short room, entity; unsigned long long slots; } placements[] = {
    {0x5,0x196,0x600ull},{0x7,0x196,0x3c0ull},
    {0x11,0x196,0xe00ull},{0x14,0x196,0xc00ull},
    {0x54,0x196,0x1eull},{0x61,0x196,0x7c00ull},
    {0x6a,0x196,0x180ull},{0x81,0x3ec,0x20ull},
    {0x81,0x330,0x80ull},{0x82,0x3ec,0x200ull},
    {0x82,0x330,0xf8000ull},{0x83,0x330,0x7e00ull},
    {0x84,0x330,0x3ull},{0x85,0x332,0xf800ull},
    {0x85,0x330,0x180000ull},{0x86,0x3ec,0x40000ull},
    {0x86,0x196,0xf000000ull},{0x86,0x330,0x30000000ull},
    {0x87,0x330,0x600000000ull},{0x88,0x330,0x8ull},
    {0x89,0x330,0xc0000ull},{0x8a,0x330,0x2000ull},
    {0x8b,0x330,0x40ull},{0x8c,0x3ec,0x4000ull},
    {0x8c,0x330,0x7c00000ull},{0x8d,0x330,0x7000ull},
    {0x8e,0x330,0x300ull},{0x90,0x3ec,0x100ull},
    {0x90,0x330,0x3000ull},{0x91,0x3ec,0x40000ull},
    {0x91,0x331,0x8000000ull},{0x91,0x330,0x7f0000000ull},
    {0x94,0x330,0x40ull},{0x95,0x196,0x600ull},
    {0x95,0x330,0x7000ull},{0x96,0x330,0x1800ull},
    {0x97,0x196,0x700ull},{0x97,0x330,0x1800ull},
    {0x98,0x330,0xc0ull},{0x99,0x3ec,0x100ull},
    {0x99,0x330,0x38000ull},{0x9d,0x330,0x10ull},
    {0xae,0x196,0x8000ull},{0xba,0x196,0xc000ull},
    {0x142,0x196,0x4dc000ull},{0x14e,0x339,0x80000ull},
    {0x15c,0x339,0x4000ull},{0x164,0x196,0xf00ull}
  };
  unsigned int i;
  if (!parent || parent > 64) return 0;
  for (i=0;i<sizeof(placements)/sizeof(placements[0]);++i)
    if (placements[i].room==room && placements[i].entity==entity)
      return (placements[i].slots & (1ull << (parent-1))) != 0;
  return 0;
}
static inline int anchor_world_fragile_valid(const int *r) {
  int role=r[WF_ROLE], entity=r[WD_ENTITY], model=r[WD_MODEL];
  if (role<0 || role>1 || r[WD_PARENT]<1 || r[WD_PARENT]>256 ||
      r[WD_ORDINAL]!=role || r[WD_PHASE]!=WF_PHASE ||
      r[WF_VARIANT]<0 || r[WF_VARIANT]>255 ||
      r[WF_RESERVED0] || r[WF_RESERVED1] || r[WF_RESERVED2] ||
      r[WF_INSTANCE]<0 || r[WF_RECEIPT]<0 ||
      r[WF_ESTABLISHED]<0 || r[WF_ESTABLISHED]>1 ||
      r[WF_PRESENT]<0 || r[WF_PRESENT]>1 ||
      r[WD_ROUTE]!=163 || r[WD_TALKABLE] || r[WD_DIALOG] || r[WD_BUSY]) return 0;
  if (entity!=0x196 && entity!=0x330 && entity!=0x331 && entity!=0x332 &&
      entity!=0x339 && entity!=0x3ec) return 0;
  if (role && entity!=0x331 && entity!=0x332) return 0;
  if (model!=(entity==0x339 ? 0x24f : entity)) return 0;
  if (entity==0x330 && (r[WF_VARIANT]>2 || r[WD_ANIMATED] ||
      r[WD_CLIP]!=(r[WF_VARIANT]==2 ? 1 : 0))) return 0;
  if (entity==0x332 && r[WF_VARIANT]>4) return 0;
  if ((entity==0x196 || entity==0x3ec) &&
      (r[WD_ANIMATED] || r[WD_CLIP])) return 0;
  if (entity==0x339 && (role || r[WF_VARIANT] || r[WD_ANIMATED] || r[WD_CLIP]!=4)) return 0;
  if (entity==0x331 && (r[WD_ANIMATED] || r[WD_CLIP]!=(role ? 0 : 1))) return 0;
  if (entity==0x332 && (r[WD_ANIMATED]!=role ||
      (role ? r[WD_CLIP]!=1 && r[WD_CLIP]!=3 : r[WD_CLIP]!=0))) return 0;
  if (r[WD_LIFE]==WD_CLAIM && !r[WD_LANDED]) return 0;
  if (r[WD_LIFE]==WD_REMOVED && r[WD_LANDED] && !r[WD_COMMITTER]) return 0;
  return 1;
}
#endif
