#ifndef ANCHOR_PLAYER_CUBE_H
#define ANCHOR_PLAYER_CUBE_H

/* Frozen-player cube carry is a transient, direct exchange. A victim keeps
 * authority for its playable position and freeze timer. */
void anchor_player_cube_tick(void);
void anchor_player_cube_reset(void);
int anchor_player_cube_victim_moving(void);
int anchor_player_cube_victim_pose(float *x, float *y, float *z);
void anchor_player_cube_victim_thaw(void);
int anchor_player_cube_visual_owned(int cid, int session, int epoch);
int anchor_player_cube_visual_native_pose(int cid, int session, int epoch);

#endif
