#ifndef ANCHOR_MINIGAME_INVITES_H
#define ANCHOR_MINIGAME_INVITES_H

enum AnchorMinigame {
    ANCHOR_MINIGAME_NONE = 0,
    ANCHOR_MINIGAME_GOEMON = 1,
    ANCHOR_MINIGAME_EBISUMARU = 2,
    ANCHOR_MINIGAME_SASUKE = 3,
    ANCHOR_MINIGAME_YAE = 4
};

/* Native room-load observation and frame coordinator. */
void anchor_minigame_invites_begin_load(void);
void anchor_minigame_invites_finish_load(void);
void anchor_minigame_invites_update(void);
/* Includes an accepted Yes while its guarded room transfer is pending. */
int anchor_minigame_invites_active(void);

/* -2 holds metadata while a native rebind is incomplete; -1 uses the native
 * selected character; 0..3 names the forced or restored character. */
int anchor_minigame_invites_reporting_character(unsigned short room);
void anchor_minigame_invites_character_published(unsigned short room);
int anchor_minigame_invites_character_motion_ready(unsigned short room);
/* Loaded minigame room scope for the mod's character cycler. */
int anchor_minigame_invites_required_character(void);
int anchor_minigame_invites_is_room(unsigned short room);
/* Kegon remains an ordinary world room. Only these explicit requests or a
 * verified native sign start enter its Mermaid challenge. The adapter calls
 * started before binding its graph, and finished after native result cleanup. */
enum AnchorYaeLaunchOrigin {
    ANCHOR_YAE_LAUNCH_NONE = 0,
    ANCHOR_YAE_LAUNCH_DEBUG = 1,
    ANCHOR_YAE_LAUNCH_JOIN = 2
};
/* Only this DEBUG_BUTTON_ENABLED entry may bypass native story eligibility. */
int anchor_minigame_invites_debug_start_yae(void);
/* JOIN is a pinned existing host visit, not a local story start. */
int anchor_minigame_invites_yae_validate_join(void);
int anchor_minigame_invites_yae_direct_pending(void);
int anchor_minigame_invites_yae_black_scope(void);
/* Native presentation bridge for accepted JOIN arrivals; debug keeps its native tutorial. */
void anchor_minigame_yae_direct_load_begin(void);
void anchor_minigame_yae_direct_load_cancel(void);
int anchor_minigame_invites_yae_request_origin(void);
/* Retire the one-shot request without disturbing a native sign dialogue.
 * finished performs deferred restoration only at a verified idle boundary. */
void anchor_minigame_invites_yae_cancel_request(void);
int anchor_minigame_invites_yae_cancel_pending(void);
int anchor_minigame_invites_transfer_to_yae_client(int cid);
int anchor_minigame_invites_yae_requested(void);
void anchor_minigame_invites_yae_started(void);
void anchor_minigame_invites_yae_finished(void);
/* Use canonical starts for the three challenge rooms; other rooms retain the
 * caller's exact coordinates. Returns the guarded native transfer result. */
int anchor_minigame_invites_transfer_to_room(unsigned short room,
                                             short x, short y, short z);
/* Player-list joins identify their chosen host; a zero visit asks the bridge
 * to resolve that host's current loaded challenge. */
int anchor_minigame_invites_transfer_to_client(unsigned short room,
                                               short x, short y, short z,
                                               int cid);
void anchor_minigame_invites_cancel_player_list_join(int cid);
/* Guarded world transport calls this after native readiness checks. */
int anchor_minigame_invites_prepare_transfer(unsigned short destination_room);
/* Called by the guarded live-world transfer before a minigame warp. Keeps the
 * first ordinary-room source through minigame reloads and chained visits. */
void anchor_minigame_invites_note_transfer(unsigned short destination_room);
/* Returns the exact native player-object XYZ saved for a later result return. */
int anchor_minigame_invites_source_waypoint(unsigned short *room, float xyz[3]);
/* Native helper-return and constructor-return hooks. These preserve native
 * result cleanup; source XYZ is applied once after the matching room spawn. */
void anchor_minigame_invites_return_saved(void);
void anchor_minigame_invites_return_ebisumaru(void);
void anchor_minigame_invites_restore_spawn(void);
void anchor_minigame_invites_restore_in_place(void);

#endif
