#ifndef ANCHOR_WORLD_H
#define ANCHOR_WORLD_H
#define ANCHOR_WORLD_MAX 256
#define ANCHOR_WORLD_WORDS 39
#define ANCHOR_WORLD_JSON 131072
#define WORLD_NPC 1
#define WORLD_PLATFORM 2
#define WORLD_PICKUP 3
#define WORLD_EMITTER 4
/* Called by the existing, resource-guarded actor-data enumeration. */
void anchor_world_roster_begin(unsigned int room);
void anchor_world_roster_add(unsigned int index, void *source,
                             const void *definition);
void anchor_world_roster_end(unsigned int count);
void anchor_world_reset(void);
int anchor_world_row_valid(const int *row);
int anchor_world_encode(const int rows[][ANCHOR_WORLD_WORDS],
                        unsigned int count, const unsigned char *dead,
                        char *out, unsigned int capacity);
int anchor_world_decode(const char *json, int rows[][ANCHOR_WORLD_WORDS + 2],
                        unsigned int *count, unsigned char *dead);
#endif
