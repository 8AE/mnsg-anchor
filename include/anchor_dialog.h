#ifndef ANCHOR_DIALOG_H
#define ANCHOR_DIALOG_H

typedef enum AnchorDialogResult {
    ANCHOR_DIALOG_IDLE = 0,
    ANCHOR_DIALOG_PENDING,
    ANCHOR_DIALOG_YES,
    ANCHOR_DIALOG_NO,
    ANCHOR_DIALOG_CANCELLED
} AnchorDialogResult;

/* Begin only while the caller's world/room is safe for a prompt. Returns zero
 * while another native scenario/window owns the UI or resources are absent. */
int anchor_dialog_begin(const char *player_name, const char *arena_name);
/* A completed result is returned once, after the native window has closed and
 * this dialog's input filtering has ended. */
AnchorDialogResult anchor_dialog_poll(void);
void anchor_dialog_cancel(void);
int anchor_dialog_busy(void);

#endif
