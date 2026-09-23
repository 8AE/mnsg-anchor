#ifndef ANCHOR_WORLD_BRIDGE_H
#define ANCHOR_WORLD_BRIDGE_H

/* File51 Super Pass Bridge: one placed root owns both guard routes, gate
 * presentation and blocker lifetime. No native addresses cross this row. */
#define WORLD_BRIDGE 9
#define WORLD_BRIDGE_ENTITY 0x240
#define WORLD_BRIDGE_ROOM 0x15e
#define WB_FLAGS 4
#define WB_FRESH 5
#define WB_GATE_PHASE 6
#define WB_GATE_FRAME 7
#define WB_GATE_ANIMATION 8
#define WB_BLOCKER_REMOVED 9
#define WB_GUARD_0 10
#define WB_GUARD_1 24
#define WB_PAUSED 38
#define WB_VELOCITY_0 39
#define WB_VELOCITY_1 42
#define WB_RESERVED 45
#define WB_INPUT 46
#define WB_AGGREGATE 47

/* Each guard consumes14 words: phase/playback/status400, XYZ hundredths,
 * yaw|(AA<<10), clip, frame hundredths, rate|(animationFlags<<16), route
 * timer, three origin halfwords, route substate, route PC. Guard phase is
 * 1=pre-open,2=route,3=post-open; bit2 playback, bit3 status400. The route ID
 * is local and immutable: guard0=0x44, guard1=0x43. Gate animation additionally
 * carries native playback enable at bit19. Six velocity words use thousandths.
 * All other pointers/contacts/dialogue/blinking and local locks remain local.
 */
void anchor_world_bridge_reset(int room_changed);
void anchor_world_bridge_register(void *actor, unsigned int entity,
                                  unsigned int selector);
unsigned int anchor_world_bridge_local_inputs(void);
int anchor_world_bridge_needs_restore(void);
/* Bridge reconstructions belong to the atomic root, not the generic NPC stream. */
int anchor_world_bridge_owns(void *actor);
int anchor_world_bridge_capture(void *root, int *row);
int anchor_world_bridge_apply(void *root, const int *row);
void anchor_world_bridge_control(void *root, int remote, int paused,
                                 unsigned int inputs);
void anchor_world_bridge_begin(void);
void anchor_world_bridge_end(void);
#endif
