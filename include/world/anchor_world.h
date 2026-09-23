#ifndef ANCHOR_WORLD_H
#define ANCHOR_WORLD_H
#define ANCHOR_WORLD_MAX 256
#define ANCHOR_WORLD_WORDS 50
#define WORLD_INSTANCE 48
#define WORLD_RECEIPT 49
#define WORLD_BOOTSTRAP 0x40000000u
#define WORLD_APPLY_FAILED 0x3fffffffu
#define WORLD_NPC_CHECKPOINT 39
#define ANCHOR_WORLD_JSON 131072
#define WORLD_NPC 1
#define WORLD_PLATFORM 2
#define WORLD_PICKUP 3
#define WORLD_EMITTER 4
#define WORLD_DOOR 5
#define WORLD_SWITCH 6
#define WORLD_SHUTTER 8
#define WORLD_SHUTTER_ENTITY 0x354
/* Stable parent/attempt identity for the shutter's dynamically emitted robot. */
int anchor_world_shutter_birth(void *parent, unsigned int *index,
                               unsigned int *ordinal);
#define WORLD_PHYSICS_FIRST 69
#define WORLD_PHYSICS_REWARD 73
#define WORLD_PHYSICS_DONE 74
#define WORLD_PHYSICS_BROKEN 75
#define WORLD_PHYSICS_ROUND_MAX 8388607
/* File24 spike floors keep one native callback; its private byte supplies
 * wait/open/retract. Marker76 does not alias virtual physics tombstone75. */
#define WORLD_SPIKE_ENTITY 0x3ca
#define WORLD_SPIKE_VARIANT 27
#define WORLD_SPIKE_PHASE 76
#define WORLD_ROPE_ENTITY 0x1aa
#define WORLD_ROPE_VARIANT 28
#define WORLD_ROPE_PHASE 77
#define WORLD_TOP_ENTITY 0x365
#define WORLD_TOP_VARIANT 29
#define WORLD_TOP_PHASE 78
#define WORLD_ROTOR_ENTITY 0x366
#define WORLD_ROTOR_VARIANT 30
#define WORLD_ROTOR_PHASE 79
/* File 30's save-controlled, placed presentation actors. Roster slots below
 * are zero-based; the room definitions name them as placements 8, 20, 15. */
#define WORLD_FILE30_CA_ROOM 0x14
#define WORLD_FILE30_CA_INDEX 7
#define WORLD_FILE30_CA_ENTITY 0xca
#define WORLD_FILE30_CA_SAVE 0x12e
#define WORLD_FILE30_339_ROOM_A 0x14e
#define WORLD_FILE30_339_ROOM_B 0x15c
#define WORLD_FILE30_339_INDEX_A 19
#define WORLD_FILE30_339_INDEX_B 14
#define WORLD_FILE30_339_ENTITY 0x339
#define WORLD_FILE30_339_SAVE 0x32
/* Called by the existing, resource-guarded actor-data enumeration. */
void anchor_world_roster_begin(unsigned int room);
void anchor_world_roster_add(unsigned int index, void *source,
                             const void *definition);
void anchor_world_roster_end(unsigned int count);
/* Resolve the guarded room roster before native placement creates a task. */
int anchor_world_source_index(const void *source, unsigned int *placed_index);
int anchor_world_source_position(unsigned int placed_index, short out[3]);
void anchor_world_reset(void);
int anchor_world_row_valid(const int *row);
int anchor_world_encode(const int rows[][ANCHOR_WORLD_WORDS],
                        unsigned int count, const unsigned char *dead,
                        char *out, unsigned int capacity);
int anchor_world_decode(const char *json, int rows[][ANCHOR_WORLD_WORDS + 2],
                        unsigned int *count, unsigned char *dead);
#endif
