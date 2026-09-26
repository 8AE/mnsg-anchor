#ifndef ANCHOR_PLAYER_SOUNDS_H
#define ANCHOR_PLAYER_SOUNDS_H

/* Flush local-player cues and spatially replay fresh remote-player cues.
 * Call once after the remote model roster has been updated for the frame. */
void anchor_player_sounds_update(void);
/* Play the game's two ice-break cues at an exact world position without
 * capturing them as ordinary player sounds for a second network send. */
void anchor_player_sounds_play_ice_break(float x, float y, float z);

#endif
