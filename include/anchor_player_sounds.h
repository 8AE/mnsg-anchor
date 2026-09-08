#ifndef ANCHOR_PLAYER_SOUNDS_H
#define ANCHOR_PLAYER_SOUNDS_H

/* Flush local-player cues and spatially replay fresh remote-player cues.
 * Call once after the remote model roster has been updated for the frame. */
void anchor_player_sounds_update(void);

#endif
