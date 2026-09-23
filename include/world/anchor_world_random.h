#ifndef ANCHOR_WORLD_RANDOM_H
#define ANCHOR_WORLD_RANDOM_H
#include "world/anchor_world_dynamic.h"

/* Kind-specific union over the optional NPC continuation, laid out exactly
 * like the Slicer union. Instance and receipt are local bridge words;
 * established and present are bounded wire presence. */
enum { WR_ROLE=74, WR_TARGET_X, WR_TARGET_Z, WR_RESERVED0, WR_RESERVED1,
       WR_INSTANCE, WR_RECEIPT, WR_ESTABLISHED, WR_PRESENT };
/* The two native continuations of the File30 0x3EF graph. */
enum { WR_ROOT=22, WR_CHILD=23 };

/* Typed checkpoint origins pair a replica with its own copy without trusting a
 * pointer or a packet-local address. */
#define WR_ROOT_ORIGIN 0x7ffffff6
#define WR_CHILD_ORIGIN 0x7ffffff5
#define WR_LOOT_ORIGIN 0x7ffffff4

/* Placed File30 RNG spawner identity. The production roster holds exactly one
 * 0x3EF entry: room 0x91, normal-list index 35, one-based placed parent 36. */
#define WR_ROOM 0x91u
#define WR_ROOT_PARENT 36
#define WR_ENTITY 0x3ef
#define WR_CHILD_MODEL 0x12f
/* Exact child continuation words: incoming hit + outgoing contact plus the
 * native texture-loop bit, clip slot 0 at rate 0.25 with its loop enabled. */
#define WR_CHILD_FLAGS_LO 0x06e7
#define WR_CHILD_FLAGS_HI 0x0020
#define WR_CHILD_RATE 64
#define WR_CHILD_ANIM_FLAGS 1
#define WR_CHILD_TIMER 90
/* Native turn helper step: 802197D8 forwards 0x10 and the coordinate routine
 * moves at most twice that in the 10-bit yaw space. */
#define WR_STEER_STEP 16
/* func_80218DA8_5D4278 selects the fixed scalar attack-shape representation by
 * storing 0xFFFFFFFF at +0x48: signed radius/10 at +0x4E, height at +0x50 and
 * vertical offset at +0x52. That is the shared "fixed scalar shape" word the
 * generic checkpoint already models as sphere 17, not a disabled collider. */
#define WR_SPHERE 17
/* Target X/Z quantization: hundredths of a world unit. */
#define WR_TARGET_SCALE 100
#define WR_TARGET_LIMIT 3200000

static inline int anchor_world_random_scope(const int *r, unsigned int room) {
  if (room != WR_ROOM || r[WD_PARENT] != WR_ROOT_PARENT ||
      r[WD_ENTITY] != WR_ENTITY)
    return 0;
  return r[WR_ROLE] ? r[WD_MODEL] == WR_CHILD_MODEL : r[WD_MODEL] == WR_ENTITY;
}

static inline int anchor_world_random_valid(const int *r) {
  int role = r[WR_ROLE];
  unsigned int j;
  if (role < 0 || role > 1 || r[WD_PHASE] != (role ? WR_CHILD : WR_ROOT) ||
      !anchor_world_random_scope(r, WR_ROOM) || r[WD_ROUTE] != 163 ||
      r[WR_RESERVED0] || r[WR_RESERVED1] || r[WR_INSTANCE] < 0 ||
      r[WR_RECEIPT] < 0 || r[WR_ESTABLISHED] < 0 || r[WR_ESTABLISHED] > 1 ||
      r[WR_PRESENT] < 0 || r[WR_PRESENT] > 1 || r[WD_CLIP] ||
      r[WD_ANIM_FLAGS] != (role ? WR_CHILD_ANIM_FLAGS : 0) ||
      r[WD_SPHERE] != WR_SPHERE || r[WD_BUSY] || r[WD_BOUNCE] < 0 ||
      r[WD_BOUNCE] > 255)
    return 0;
  /* Neither role is a path follower, a talkable NPC or a scripted scene. */
  for (j = WD_PATH_TIMER; j <= WD_DIALOG; ++j)
    if (r[j]) return 0;
  if (!role)
    /* File30 never writes +0x60/+0x64 on the placed root, and the root carries
     * no motion, animation or target, so the checkpoint holds none of them.
     * The root is an invisible, invulnerable scheduler: it never takes a hit
     * and therefore never carries a claim. */
    return !r[WD_FLAGS_LO] && !r[WD_FLAGS_HI] && !r[WD_AUX_LO] &&
           !r[WD_AUX_HI] && !r[WD_ANIMATED] && !r[WD_VX] && !r[WD_VY] &&
           !r[WD_VZ] && !r[WD_FRAME] && !r[WD_RATE] && !r[WR_TARGET_X] &&
           !r[WR_TARGET_Z] && r[WD_LIFE] != WD_CLAIM && r[WD_ORDINAL] >= 0;
  if (r[WD_FLAGS_LO] != WR_CHILD_FLAGS_LO ||
      r[WD_FLAGS_HI] != WR_CHILD_FLAGS_HI || !r[WD_ANIMATED] ||
      r[WD_RATE] != WR_CHILD_RATE || r[WD_FRAME] < 0 ||
      r[WD_TIMER] < (r[WD_LIFE] == WD_REMOVED ? -1 : 0) ||
      r[WD_TIMER] > WR_CHILD_TIMER || r[WD_ORDINAL] < 1 ||
      r[WR_TARGET_X] < -WR_TARGET_LIMIT || r[WR_TARGET_X] > WR_TARGET_LIMIT ||
      r[WR_TARGET_Z] < -WR_TARGET_LIMIT || r[WR_TARGET_Z] > WR_TARGET_LIMIT)
    return 0;
  return 1;
}
#endif
