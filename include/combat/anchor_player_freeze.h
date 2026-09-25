#ifndef ANCHOR_PLAYER_FREEZE_H
#define ANCHOR_PLAYER_FREEZE_H

/* Apply the ordinary guarded native hit, then start a bounded local ice
 * status when the packet identifies Sasuke's native kunai hit. */
int anchor_player_freeze_apply_hit(float x, float y, float z, int hit_kind);
int anchor_player_freeze_active(void);
/* Frozen visual pose; native gameplay animation continues to advance. */
int anchor_player_freeze_visual_pose(int *action, float *frame);
/* True only while this mod owns the temporary native movement-control bit. */
int anchor_player_freeze_control_scoped(void);

#endif
