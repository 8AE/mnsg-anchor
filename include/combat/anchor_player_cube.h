#ifndef ANCHOR_PLAYER_CUBE_H
#define ANCHOR_PLAYER_CUBE_H

/* Frozen-player cube carry is a transient, direct exchange. A victim keeps
 * authority for its playable position and freeze timer. */
void anchor_player_cube_tick(void);
void anchor_player_cube_reset(void);
int anchor_player_cube_victim_moving(void);
int anchor_player_cube_victim_pose(float *x, float *y, float *z);
void anchor_player_cube_victim_thaw(void);
/* A side contact sends one bounded push intent to the frozen cube's owner. */
int anchor_player_cube_request_push(int cid, int session, int epoch,
    float source_x, float source_y, float source_z,
    float intent_x, float intent_z);
/* The frozen owner applies this real position after its native update. */
int anchor_player_cube_victim_push_pose(float *x, float *y, float *z);
/* A manual breakout cancels an active carry, including a thrown cube. */
void anchor_player_cube_victim_breakout(void);
int anchor_player_cube_visual_owned(int cid, int session, int epoch);
int anchor_player_cube_visual_native_pose(int cid, int session, int epoch);

#endif
