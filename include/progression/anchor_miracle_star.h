#ifndef ANCHOR_MIRACLE_STAR_H
#define ANCHOR_MIRACLE_STAR_H

/* Tsurami awards the Star inside a controller-owned scene (file 73). The
 * controller retains its reward child, camera and player-control ownership;
 * receiving inventory is never permission to delete those native tasks. */
void anchor_miracle_star_update_room(unsigned short current_room);
void anchor_miracle_star_reset(void);
int anchor_miracle_star_local_scene_active(void);

#endif
