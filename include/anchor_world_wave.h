#ifndef ANCHOR_WORLD_WAVE_H
#define ANCHOR_WORLD_WAVE_H
#include "anchor_world_dynamic.h"

/* Kind-specific union over the optional NPC continuation, laid out exactly
 * like the Slicer and RNG unions. Instance and receipt are local bridge words;
 * established and present are bounded wire presence. */
enum { WW_ROLE=74, WW_TARGET_X, WW_TARGET_Y, WW_TARGET_Z, WW_STOP,
       WW_INSTANCE, WW_RECEIPT, WW_ESTABLISHED, WW_PRESENT };
/* The invisible producer's two phases and the two child state machines. */
enum { WW_WAIT=30, WW_PRODUCER, WW_FLIGHT, WW_PURSUE, WW_COAST, WW_PURSUE2 };
/* Role 0 is the producer; roles 1 and 2 are its 0x12D and 0xFA children. */
enum { WW_ROOT=0, WW_MODEL_12D=1, WW_MODEL_FA=2 };

/* Typed checkpoint origins pair a replica with its own copy without trusting a
 * pointer or a packet-local address. The producer is a unique placed-body
 * child; its wave children are the only hazard this family births, and no wave
 * ever drops loot. */
#define WW_ROOT_ORIGIN 0x7ffffff1
#define WW_CHILD_ORIGIN 0x7ffffff0

/* Placed File_46 Koryuta body identity. The production roster holds exactly one
 * 0x1B0 entry in room 0x155: normal-list index 4, so the one-based placed
 * parent is 5. The invisible wave producer is that body's own scheduled child
 * and never a roster entry of its own. */
#define WW_ROOM 0x155u
#define WW_ROOT_PARENT 5
#define WW_ENTITY 0x1b0
#define WW_12D_MODEL 0x12d
#define WW_FA_MODEL 0xfa
/* func_080037B0 spawns every wave with resource 0x2E, and the non-faulting
 * probe func_800141C4_14DC4(0x2E) is File_46's own residency code. */
#define WW_FILE_CODE 46
/* func_08001A28 spawns the producer with func_802171A8(body, 370C, 0) and
 * never copies the body's object position into it, so its object stays at the
 * allocator default. The shared anchor is therefore pinned rather than read. */
#define WW_ROOT_CATEGORY 0
#define WW_CHILD_CATEGORY 6 /* the native child spawner's third argument */

/* Exact child continuation words. Both models take the same flag word, the same
 * native no-loot bit and the same clip-0 animation bind; only the 0x12D also
 * binds texture slot 0, which sets the native texture-loop bit. The common
 * +0x60 mask clears that bit, so the family restores it explicitly. */
#define WW_CHILD_FLAGS_LO 0x07e3
#define WW_CHILD_FLAGS_HI 0x0220
#define WW_TEXTURE_LOOP 4
#define WW_CHILD_AUX_LO 0x8000
#define WW_CHILD_AUX_HI 0
#define WW_CHILD_RATE 128 /* func_8021664C clip 0 at 0.5, stored as 0.5 * 256 */
#define WW_CHILD_ANIM_FLAGS 1
#define WW_CHILD_MASK 0x21
#define WW_SPHERE 17 /* func_80218DA8 stores 0xFFFFFFFF at +0x48 */
#define WW_CHILD_RADIUS 300
#define WW_CHILD_HEIGHT 200
#define WW_CHILD_OFFSET -40
#define WW_CHILD_ATTACK 1
#define WW_CHILD_ATTACK_RADIUS 1 /* func_80218DA8(actor, 10, 10, 0) */
#define WW_CHILD_ATTACK_HEIGHT 10
#define WW_CHILD_ATTACK_OFFSET 0
#define WW_12D_TIMER 100
#define WW_FA_PURSUE_TIMER 80
#define WW_FA_COAST_TIMER 32
#define WW_FA_PURSUE2_TIMER 64
#define WW_WAIT_TICKS 512 /* func_0800376C counts +0x8A down from 0x200 */
#define WW_FLIGHT_VZ (-2.2f) /* DAT_08004CD8, the flight phase's Z step */
/* Pursuit destination quantization: hundredths of a world unit. */
#define WW_TARGET_SCALE 100
#define WW_TARGET_LIMIT 3200000

static inline int anchor_world_wave_scope(const int *r, unsigned int room) {
  if (room != WW_ROOM || r[WD_PARENT] != WW_ROOT_PARENT ||
      r[WD_ENTITY] != WW_ENTITY)
    return 0;
  if (!r[WW_ROLE])
    /* The producer inherits the body's entity number. Its descriptor selector
     * is the same 0x1B0 on a normal load and stays zero if the allocator did
     * not inherit one; both describe the same invisible task. */
    return r[WD_MODEL] == WW_ENTITY || !r[WD_MODEL];
  if (r[WW_ROLE] == WW_MODEL_12D)
    return r[WD_MODEL] == WW_12D_MODEL && r[WD_PHASE] == WW_FLIGHT;
  if (r[WW_ROLE] == WW_MODEL_FA)
    return r[WD_MODEL] == WW_FA_MODEL && r[WD_PHASE] >= WW_PURSUE &&
           r[WD_PHASE] <= WW_PURSUE2;
  return 0;
}

static inline int anchor_world_wave_valid(const int *r) {
  int role = r[WW_ROLE];
  unsigned int j;
  if (role < WW_ROOT || role > WW_MODEL_FA ||
      !anchor_world_wave_scope(r, WW_ROOM) || r[WD_ROUTE] != 163 ||
      r[WW_INSTANCE] < 0 || r[WW_RECEIPT] < 0 ||
      r[WW_ESTABLISHED] < 0 || r[WW_ESTABLISHED] > 1 ||
      r[WW_PRESENT] < 0 || r[WW_PRESENT] > 1 ||
      r[WW_STOP] < 0 || r[WW_STOP] > 1 || r[WD_CLIP] || r[WD_BUSY] ||
      r[WD_BOUNCE] < 0 || r[WD_BOUNCE] > 255)
    return 0;
  /* Neither role is a path follower, a talkable NPC or a scripted scene. */
  for (j = WD_PATH_TIMER; j <= WD_DIALOG; ++j)
    if (r[j])
      return 0;
  for (j = WW_TARGET_X; j <= WW_TARGET_Z; ++j)
    if (r[j] < -WW_TARGET_LIMIT || r[j] > WW_TARGET_LIMIT)
      return 0;
  if (!role)
    /* The invisible producer carries no motion, animation, collision, target
     * or claim, and it rests on the pinned shared anchor. Its only shared state
     * is the phase, the encounter stop latch and the birth ordinal. */
    return !r[WD_FLAGS_LO] && !r[WD_FLAGS_HI] && !r[WD_AUX_LO] &&
           !r[WD_AUX_HI] && !r[WD_ANIMATED] && !r[WD_FRAME] && !r[WD_RATE] &&
           !r[WD_ANIM_FLAGS] && !r[WD_VX] && !r[WD_VY] && !r[WD_VZ] &&
           !r[WD_SPHERE] && !r[WD_X] && !r[WD_Y] && !r[WD_Z] &&
           !r[WW_TARGET_X] && !r[WW_TARGET_Y] && !r[WW_TARGET_Z] &&
           r[WD_ORDINAL] >= 0 && r[WD_LIFE] != WD_CLAIM &&
           (r[WD_PHASE] == WW_WAIT || r[WD_PHASE] == WW_PRODUCER) &&
           r[WD_TIMER] >= -1 && r[WD_TIMER] <= WW_WAIT_TICKS;
  /* Both children are one appearance each and share the flag, no-loot, body
   * and attack words the File_46 initializers write. */
  if ((r[WD_FLAGS_LO] & ~WW_TEXTURE_LOOP) != WW_CHILD_FLAGS_LO ||
      r[WD_FLAGS_HI] != WW_CHILD_FLAGS_HI || r[WD_AUX_LO] != WW_CHILD_AUX_LO ||
      r[WD_AUX_HI] != WW_CHILD_AUX_HI || !r[WD_ANIMATED] ||
      r[WD_RATE] != WW_CHILD_RATE ||
      r[WD_ANIM_FLAGS] != WW_CHILD_ANIM_FLAGS || r[WD_SPHERE] != WW_SPHERE ||
      r[WD_FRAME] < 0 || r[WD_ORDINAL] < 1 || r[WD_ATTACK] != WW_CHILD_ATTACK ||
      r[WD_ATTACK_RADIUS] != WW_CHILD_ATTACK_RADIUS ||
      r[WD_ATTACK_HEIGHT] != WW_CHILD_ATTACK_HEIGHT ||
      r[WD_ATTACK_OFFSET] != WW_CHILD_ATTACK_OFFSET ||
      r[WD_MASK] != WW_CHILD_MASK || r[WD_RADIUS] != WW_CHILD_RADIUS ||
      r[WD_HEIGHT] != WW_CHILD_HEIGHT || r[WD_OFFSET] != WW_CHILD_OFFSET ||
      r[WW_STOP] || r[WD_TIMER] < (r[WD_LIFE] == WD_REMOVED ? -1 : 0) ||
      r[WD_TIMER] >
          (role == WW_MODEL_12D ? WW_12D_TIMER : WW_FA_PURSUE_TIMER))
    return 0;
  return 1;
}
#endif
