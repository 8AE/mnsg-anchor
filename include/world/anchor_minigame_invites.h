#ifndef ANCHOR_MINIGAME_INVITES_H
#define ANCHOR_MINIGAME_INVITES_H

enum AnchorMinigame {
    ANCHOR_MINIGAME_NONE = 0,
    ANCHOR_MINIGAME_GOEMON = 1,
    ANCHOR_MINIGAME_EBISUMARU = 2,
    ANCHOR_MINIGAME_SASUKE = 3
};

/* Native room-load observation and frame coordinator. */
void anchor_minigame_invites_begin_load(void);
void anchor_minigame_invites_finish_load(void);
void anchor_minigame_invites_update(void);
/* Includes an accepted Yes while its guarded room transfer is pending. */
int anchor_minigame_invites_active(void);

/* -2 holds metadata while a native rebind is incomplete; -1 uses the native
 * selected character; 0..2 names the forced or restored character. */
int anchor_minigame_invites_reporting_character(unsigned short room);
void anchor_minigame_invites_character_published(unsigned short room);
int anchor_minigame_invites_character_motion_ready(unsigned short room);
/* Loaded minigame room scope for the mod's character cycler. */
int anchor_minigame_invites_required_character(void);
int anchor_minigame_invites_is_room(unsigned short room);
/* Use canonical starts for the three challenge rooms; other rooms retain the
 * caller's exact coordinates. Returns the guarded native transfer result. */
int anchor_minigame_invites_transfer_to_room(unsigned short room,
                                             short x, short y, short z);

#endif
