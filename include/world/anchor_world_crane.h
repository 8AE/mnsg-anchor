#ifndef ANCHOR_WORLD_CRANE_H
#define ANCHOR_WORLD_CRANE_H
#define WORLD_CRANE 7
#define WORLD_CRANE_ENTITY 0x1b9
#define WORLD_CRANE_INPUT 46
#define WORLD_CRANE_AGGREGATE 47
void anchor_world_crane_reset(int room_changed);
void anchor_world_crane_register(void *actor, unsigned int entity);
unsigned int anchor_world_crane_local_inputs(void);
int anchor_world_crane_needs_restore(void);
int anchor_world_crane_capture(void *root, int *row);
int anchor_world_crane_apply(void *root, const int *row);
void anchor_world_crane_control(void *root, int remote, int paused,
                                unsigned int inputs);
void anchor_world_crane_begin(void);
void anchor_world_crane_end(void);
#endif
