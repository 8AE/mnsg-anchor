#ifndef ANCHOR_WORLD_DYNAMIC_H
#define ANCHOR_WORLD_DYNAMIC_H

#define WORLD_DYNAMIC_MAX 128
#define WORLD_DYNAMIC_WORDS 83
#define WORLD_DYNAMIC_JSON 131072
/* Portable scalar checkpoints. No task, resource, script or callback address
 * is accepted from the network. The callback field is a recipe-local enum. */
enum {
  WD_CID,
  WD_SESSION,
  WD_VISIT,
  WD_SERIAL,
  WD_OWNER,
  WD_LIFE,
  WD_KIND,
  WD_PARENT,
  WD_ENTITY,
  WD_MODEL,
  WD_CLIP,
  WD_ANIMATED,
  WD_X,
  WD_Y,
  WD_Z,
  WD_PITCH,
  WD_YAW,
  WD_ROLL,
  WD_FRAME,
  WD_RATE,
  WD_ANIM_FLAGS,
  WD_VX,
  WD_VY,
  WD_VZ,
  WD_SX,
  WD_SY,
  WD_SZ,
  WD_FLAGS_LO,
  WD_FLAGS_HI,
  WD_AUX_LO,
  WD_AUX_HI,
  WD_TIMER,
  WD_ROUTE,
  WD_PATH_TIMER,
  WD_PATH_STATE,
  WD_PATH_PC,
  WD_ORIGIN_X,
  WD_ORIGIN_Y,
  WD_ORIGIN_Z,
  WD_FACING,
  WD_TALKABLE,
  WD_DIALOG,
  WD_PHASE,
  WD_BOUNCE,
  WD_LANDED,
  WD_BASE_Y,
  WD_ANGLE,
  WD_RADIUS,
  WD_HEIGHT,
  WD_OFFSET,
  WD_ATTACK_RADIUS,
  WD_ATTACK_HEIGHT,
  WD_ATTACK_OFFSET,
  WD_ATTACK,
  WD_MASK,
  WD_DIM0,
  WD_DIM1,
  WD_DIM2,
  WD_DIM3,
  WD_OBJECT_RADIUS,
  WD_OBJECT_HEIGHT,
  WD_BIRTH_X,
  WD_BIRTH_Y,
  WD_BIRTH_Z,
  WD_ORDINAL,
  WD_BUSY,
  WD_PAUSED,
  WD_SPHERE,
  WD_SHADOW_SCALE,
  WD_SHADOW_OFFSET,
  WD_MASK94,
  WD_BODY_OFFSET2,
  WD_COMMITTER,
  WD_GRAVITY,
  WD_NPC_CHECKPOINT
};
enum { WD_NPC = 1, WD_COIN, WD_HEALTH, WD_FOOD, WD_HAZARD, WD_SHUTTER_ENEMY, WD_DOLL, WD_SLICER, WD_RANDOM, WD_BOMB, WD_WAVE, WD_FRAGILE, WD_BOULDER, WD_FISH };
enum { WD_LIVE, WD_CLAIM, WD_REMOVED };
int anchor_world_dynamic_row_valid(const int *row);
int anchor_world_dynamic_encode(const int rows[][WORLD_DYNAMIC_WORDS],
                                unsigned int count, char *out,
                                unsigned int size);
int anchor_world_dynamic_decode(const char *json,
                                int rows[][WORLD_DYNAMIC_WORDS],
                                unsigned int *count, unsigned int *leader);
void anchor_world_dynamic_frame(unsigned int room, unsigned int signature,
                                unsigned int visit, int active);
void anchor_world_dynamic_room(void);
/* File62's once-per-room Silver Doll, with a scheduled File26 initializer. */
int anchor_world_dynamic_doll_spawn(void *root, unsigned int parent);
/* -1: untracked, 0: another client owns it, 1: this client owns it. */
int anchor_world_actor_authority(void *actor, unsigned int *placed_index);
int anchor_world_actor_placed(void *actor);
/* A typed producer may supply a stable ordinal for a native loot child. */
int anchor_world_loot_ordinal(void *parent, unsigned int *ordinal);
int anchor_world_is_paused(void);

#endif
