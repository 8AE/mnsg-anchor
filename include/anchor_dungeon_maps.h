#ifndef ANCHOR_DUNGEON_MAPS_H
#define ANCHOR_DUNGEON_MAPS_H

/* Reserve native graphics-visible dot pixels after resident resources load,
 * before render scratch consumes the remaining scene arena. */
void anchor_dungeon_maps_load_resources(void);

#endif
