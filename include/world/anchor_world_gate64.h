#ifndef ANCHOR_WORLD_GATE64_H
#define ANCHOR_WORLD_GATE64_H
#define WORLD_GATE64 10
#define WORLD_GATE64_ENTITY 0x325
#define WORLD_GATE64_ROOM 0x14b
#define WG64_TIMER 11
#define WG64_PHASE 12
#define WG64_COMPLETE 25
void anchor_world_gate64_reset(int room_changed);
void anchor_world_gate64_register(void *actor);
int anchor_world_gate64_capture(void *actor, int *row);
int anchor_world_gate64_apply(void *actor, const int *row);
int anchor_world_gate64_needs_restore(void);
void anchor_world_gate64_control(void *actor, int remote, int paused, int confirmed);
void anchor_world_gate64_begin(void);
#endif
