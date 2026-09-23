#ifndef ANCHOR_WORLD_BOMB_H
#define ANCHOR_WORLD_BOMB_H
#include "world/anchor_world_dynamic.h"

/* File_40 entity 0x1A9 "Falling Bomb Block": seven placed roots (room 0x65
 * index 8..10, room 0x66 index 16..19). A placed root arms on local player
 * proximity, falls, ramps its fuse tint and then commits exactly one explosion
 * that births twelve transient children: four spinning rings (variants
 * 1/2/4/8, of which 4 and 8 are registered attackers) and eight particles.
 * The root's native proximity AI is not replayed; the shared row carries the
 * phase, the fuse counter and the committed kill so both clients retire the
 * same block at the same place. Nothing here awards loot: the root ORs +0x64
 * bit 0x00008000 and no child touches a reward path. */
#define WORLD_BOMB 10
#define WORLD_BOMB_ENTITY 0x1a9
#define WORLD_BOMB_CHILD_MODEL 0x7e

/* Kind-specific union over the optional NPC continuation (74..82), the same
 * slot the Slicer uses. Instance/receipt are local bridge words; established
 * and present are bounded wire presence. */
enum { WB_ROLE=74, WB_ALPHA, WB_CAUSE, WB_VARIANT, WB_UNUSED,
       WB_INSTANCE, WB_RECEIPT, WB_ESTABLISHED, WB_PRESENT };
enum { WB_IDLE=24, WB_FALL, WB_FUSE, WB_EXPLODE, WB_RING, WB_PARTICLE };
enum { WB_ROLE_ROOT=0, WB_ROLE_V1, WB_ROLE_V4, WB_ROLE_V2, WB_ROLE_V8,
       WB_ROLE_PARTICLE=5, WB_ROLE_LAST=12 };
enum { WB_CAUSE_NONE=0, WB_CAUSE_HIT, WB_CAUSE_PROXIMITY, WB_CAUSE_FUSE };
#define WB_ORIGIN_ROOT 0x7ffffff3
#define WB_ORIGIN_CHILD 0x7ffffff2
#define WB_PARTICLES 8
#define WB_FUSE_STEP 4
#define WB_FUSE_CAP 176
#define WB_ATTACK_TYPE 6
#define WB_ATTACK_RADIUS 90
#define WB_PROXIMITY_ARM 80.0f
#define WB_PROXIMITY_FUSE 10.0f
/* Root health is two hits, and every accepted hit explodes: a one-point hit
 * survives and reaches the custom +0x90 continuation, while a lethal hit
 * reaches the same continuation because the bomb's +0x60 has no fragment bit
 * and func_80217F1C therefore reports no generic death effect. */
#define WB_ROOT_HEALTH 2
#define WB_ROOT_RADIUS 30
#define WB_ROOT_HEIGHT 60

/* Verified placements: room plus one-based parent index to initial XYZ in
 * hundredths. The live pose may fall, but the birth coordinates never move, so
 * a decoded row can only name a real block. */
static inline int anchor_world_bomb_placed(unsigned int room, unsigned int parent,
                                           int *xyz) {
  static const int roots[7][3] = {
    {4000, 12000, 10000}, {4000, 12000, 2000}, {-3000, 12000, -8000},
    {9000, 12000, 27000}, {14000, 12000, 8000}, {10000, 12000, -2500},
    {14000, 12000, -17000}};
  unsigned int index;
  if (room == 0x65 && parent >= 8 && parent <= 10)
    index = parent - 8;
  else if (room == 0x66 && parent >= 16 && parent <= 19)
    index = parent - 13;
  else
    return 0;
  for (unsigned int j = 0; j < 3; ++j)
    xyz[j] = roots[index][j];
  return 1;
}

static inline int anchor_world_bomb_scope(const int *r, unsigned int room) {
  int xyz[3];
  if (!r || !anchor_world_bomb_placed(room, (unsigned int)r[WD_PARENT], xyz))
    return 0;
  for (unsigned int j = 0; j < 3; ++j)
    if (r[WD_BIRTH_X + j] != xyz[j])
      return 0;
  return 1;
}

/* Full 83-word typed contract. Identity, role/ordinal pairing, phase, attack
 * geometry and the shared collision scalars are pinned to the verified native
 * initializers. The native adapter still validates the placement
 * (anchor_world_bomb_scope) and the resident resource table before it binds
 * anything, and it reads every pose word from the live task rather than from
 * this recipe. */
static inline int anchor_world_bomb_valid(const int *r) {
  /* Role -> caller-assigned +0xE8 variant bit, in the order of the six native
   * func_802171A8 calls inside func_080006D4. */
  static const int role_variant[5] = {0, 1, 4, 2, 8};
  int role, phase, variant;
  unsigned int j;
  if (!r)
    return 0;
  role = r[WB_ROLE];
  phase = r[WD_PHASE];
  variant = r[WB_VARIANT];
  if (r[WD_KIND] != WORLD_BOMB || r[WD_ENTITY] != WORLD_BOMB_ENTITY ||
      role < WB_ROLE_ROOT || role > WB_ROLE_LAST ||
      r[WB_ALPHA] < 0 || r[WB_ALPHA] > 255 ||
      r[WB_CAUSE] < WB_CAUSE_NONE || r[WB_CAUSE] > WB_CAUSE_FUSE ||
      r[WB_UNUSED] || r[WB_INSTANCE] < 0 || r[WB_RECEIPT] < 0 ||
      r[WB_ESTABLISHED] < 0 || r[WB_ESTABLISHED] > 1 ||
      r[WB_PRESENT] < 0 || r[WB_PRESENT] > 1 ||
      r[WD_PARENT] < 1 || r[WD_PARENT] > 19 || r[WD_ROUTE] != 163 ||
      r[WD_ANIMATED] || r[WD_TALKABLE] || r[WD_DIALOG] || r[WD_BUSY] ||
      r[WD_PAUSED] < 0 || r[WD_PAUSED] > 1 ||
      r[WD_BOUNCE] < 0 || r[WD_BOUNCE] > 255 ||
      (r[WD_LANDED] != 0 && r[WD_LANDED] != 1) ||
      (r[WD_BASE_Y] != 0 && r[WD_BASE_Y] != 1) ||
      r[WD_ORDINAL] < 0 || r[WD_ORDINAL] > WB_ROLE_LAST ||
      r[WD_RADIUS] != WB_ROOT_RADIUS || r[WD_HEIGHT] != WB_ROOT_HEIGHT ||
      r[WD_OFFSET] != 0 || r[WD_ATTACK_HEIGHT] != 0 ||
      r[WD_ATTACK_OFFSET] != 0 ||
      (r[WD_ATTACK_RADIUS] != 0 && r[WD_ATTACK_RADIUS] != WB_ATTACK_RADIUS))
    return 0;
  for (j = WD_PATH_TIMER; j <= WD_PATH_PC; ++j)
    if (r[j])
      return 0;
  if (role == WB_ROLE_ROOT)
    return r[WD_ORDINAL] == 0 && r[WD_MODEL] == WORLD_BOMB_ENTITY &&
           r[WD_CLIP] == 0 && variant == 0 && r[WD_ATTACK] == 0 &&
           r[WD_ATTACK_RADIUS] == 0 && r[WD_SPHERE] == 0 &&
           (phase == WB_IDLE || phase == WB_FALL || phase == WB_FUSE ||
            phase == WB_EXPLODE);
  if (r[WD_MODEL] != WORLD_BOMB_CHILD_MODEL || r[WD_ORDINAL] != role ||
      r[WD_BASE_Y] != 0)
    return 0;
  if (role <= WB_ROLE_V8) {
    int attack = role == WB_ROLE_V4 || role == WB_ROLE_V8;
    return phase == WB_RING && variant == role_variant[role] &&
           r[WD_CLIP] == 8 && r[WD_ATTACK] == (attack ? WB_ATTACK_TYPE : 0) &&
           r[WD_ATTACK_RADIUS] == (attack ? WB_ATTACK_RADIUS : 0) &&
           r[WD_SPHERE] == (attack ? 17 : 0);
  }
  return phase == WB_PARTICLE && variant == 0 && r[WD_CLIP] == 0 &&
         r[WD_ATTACK] == 0 && r[WD_ATTACK_RADIUS] == 0 && r[WD_SPHERE] == 0;
}
#endif
