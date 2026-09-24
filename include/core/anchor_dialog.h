#ifndef ANCHOR_DIALOG_H
#define ANCHOR_DIALOG_H

typedef enum AnchorDialogResult {
    ANCHOR_DIALOG_IDLE = 0,
    ANCHOR_DIALOG_PENDING,
    ANCHOR_DIALOG_YES,
    ANCHOR_DIALOG_NO,
    ANCHOR_DIALOG_CANCELLED
} AnchorDialogResult;

typedef enum AnchorDialogOwner {
    ANCHOR_DIALOG_OWNER_NONE = 0,
    ANCHOR_DIALOG_OWNER_BOSS_INVITE,
    ANCHOR_DIALOG_OWNER_CASTLE_RETURN
} AnchorDialogOwner;

/* Begin only while the caller's world/room is safe for a prompt. Returns zero
 * while another native scenario/window owns the UI or resources are absent. */
int anchor_dialog_begin(const char *player_name, const char *arena_name);
/* Offer the local player a return trip to Ugo Stone Circle from the castle
 * sign. Uses the same single native scenario/window owner and result API. */
int anchor_dialog_begin_castle_return(void);
/* A completed result is returned once, after the native window has closed and
 * this dialog's world pause and input filtering have ended. */
AnchorDialogResult anchor_dialog_poll_for(AnchorDialogOwner owner);
void anchor_dialog_cancel_for(AnchorDialogOwner owner);
/* Existing boss-invitation entry points remain owner-scoped. */
AnchorDialogResult anchor_dialog_poll(void);
void anchor_dialog_cancel(void);
int anchor_dialog_busy(void);
/* True only while our modal still owns the scenario in the active world.
 * Frame hooks that mutate gameplay must defer work; networking/UI may tick. */
int anchor_dialog_world_paused(void);

#endif
