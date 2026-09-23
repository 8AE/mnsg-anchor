#ifndef ANCHOR_WORLD_DOLL_H
#define ANCHOR_WORLD_DOLL_H

/* File62 entity 0x3D6: the repeatable Silver Doll container in rooms 0x16A
 * and 0x182. The placed 0x3D6 root is a hit trigger whose pitch walks a
 * repeating close cycle; the nested File_26 Doll is born by the dynamic-world
 * API. This row carries the root container cycle only. */
#define WORLD_DOLL_CONTAINER 11
#define WORLD_DOLL_ENTITY 0x3d6
#define WORLD_DOLL_ROOM_A 0x16a
#define WORLD_DOLL_ROOM_B 0x182
/* 3 busy/zero, 4..6 XYZ hundredths, 7 pitch 0..70, 8 yaw, 9 roll, 10 zero,
 * 11 cycle, 12 phase 0=idle 1=opening 2=birth 3=closing, 13 spawned, 14..37
 * zero, 38 paused, 39..47 zero, 48 instance, 49 receipt. */
#define WDC_BUSY 3
#define WDC_X 4
#define WDC_Y 5
#define WDC_Z 6
#define WDC_PITCH 7
#define WDC_YAW 8
#define WDC_ROLL 9
#define WDC_CYCLE 11
#define WDC_PHASE 12
#define WDC_SPAWNED 13
#define WDC_PAUSED 38

void anchor_world_doll_reset(int room_changed);
void anchor_world_doll_register(void *actor, unsigned int index_onebased);
int anchor_world_doll_capture(void *actor, int *row);
int anchor_world_doll_apply(void *actor, const int *row);
int anchor_world_doll_needs_restore(void);
void anchor_world_doll_control(void *actor, int remote, int paused,
                               int confirmed);
void anchor_world_doll_begin(void);

/* The unique placed 0x3D6 root that owns the nested File_26 Doll is named by
 * its one-based placed roster slot: room 0x16A slot 8 or room 0x182 slot 3.
 * The mapping is a roster identity, not a live task, so it survives the root
 * being culled or reused and only clears on a room reset. Both functions
 * return 0 when the room is wrong or no root was registered. */
/* Non-zero when index_onebased is the verified placed slot in this room. */
int anchor_world_doll_parent_valid(unsigned int index_onebased);
/* The known root slot in this room, or 0 when none is registered. */
unsigned int anchor_world_doll_parent(void);
#endif
