#ifndef ANCHOR_PLAYER_SKIN_H
#define ANCHOR_PLAYER_SKIN_H

/* Local "alternative Ebisumaru" skin toggle and its shared opening-cutscene asset.
 *
 * File 0x4D9 only holds the nine opening clips, so the toggle borrows its body
 * mesh and texture atlas while retaining the playable action skeleton. The
 * local display object binds a private action slice at +0x38 and a broad/mesh
 * buffer at +0x40. See include/anchor_player_models.h and
 * docs/alternative-ebisumaru-skin.md for the full recipe. */

/* Stage (or re-look-up) file 0x4D9 in the scene registry. Call from the
 * stage-load return hook right after anchor_player_models_load_resources(). */
void anchor_player_skin_load_resources(void);
/* Drop the cached alternative base and the local restore capture. */
void anchor_player_skin_reset(void);
/* Clear the toggle and restore a still-bound Ebisumaru display before a
 * character swap changes the selected character. */
void anchor_player_skin_clear_for_character_change(void);
/* Read the one-frame L button edge and update the toggle. Call once per frame
 * before publishing local state so the same packet carries the new bit. */
void anchor_player_skin_update(void);
/* 1 when the toggle is on, the local character is Ebisumaru, and the game is
 * in normal gameplay. Scripted/cutscene/dialog/Impact states suspend the
 * override while the latch is retained. Character changes clear the latch. */
int anchor_player_skin_active(void);
/* ANCHOR_APPEARANCE_ALTERNATIVE_EBISUMARU when active, otherwise 0. */
int anchor_player_skin_appearance_bit(void);
/* Re-assert or restore the local primary display object for this frame. Call
 * from the late-player-update return hook before any early return. */
void anchor_player_skin_apply_local(void);
int anchor_player_skin_alternative_ready(void);
unsigned int anchor_player_skin_alternative_base(void);
/* Resident size of file 0x4D9, or 0 when its full decoded layout is absent. */
unsigned int anchor_player_skin_alternative_size(void);

#ifdef ANCHOR_PLAYER_SKIN_HOST_TEST
/* Pure logic seams for the host test. */
int anchor_player_skin_toggle_active(int toggle_on, int char_index);
int anchor_player_skin_edge_toggle(int toggle_on, int pressed, int gate);
void anchor_player_skin_test_set_character(int char_index);
void anchor_player_skin_test_set_player(void *task, void *object);
#endif

#endif
