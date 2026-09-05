#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "anchor_boss_invites.h"
#include "anchor_boss_invite_world.h"
#include "anchor_dialog.h"

static int connected = 1, disabled, loaded = 1, current_arena, prompt_safe = 1;
static int current_visit = 4, published_arena = -1, published_visit = -1;
static int current = 1, dialog_available = 1, modal, starts, cancels, dismissals;
static int warps, warp_ready = 1, peeks, frees;
static const char *packet;
static char shown_name[257], shown_arena[64];
static AnchorDialogResult choice = ANCHOR_DIALOG_PENDING;
static const char invite[] = "{\"cid\":2,\"session\":123,\"seq\":7,\"arena\":1,\"name\":\"Ahmad\"}";

int anchor_is_connected(void) { return connected; }
int anchor_is_disabled(void) { return disabled; }
int item_sync_save_is_loaded(void) { return loaded; }
int anchor_update_boss_arena(int arena, int visit)
{
    published_arena = arena;
    published_visit = visit;
    return connected;
}
int anchor_boss_invite_world_arena(void) { return current_arena; }
unsigned int anchor_boss_invite_world_visit(void) { return (unsigned)current_visit; }
int anchor_boss_invite_world_can_prompt(void) { return prompt_safe; }
int anchor_boss_invite_world_warp(void)
{
    assert(!modal); /* no transition while native text owns input */
    if (!warp_ready || !prompt_safe)
        return 0;
    ++warps;
    current_arena = 1;
    return 1;
}
char *anchor_get_boss_invitation_json(void)
{
    char *out;
    ++peeks;
    if (!packet)
        return NULL;
    out = malloc(strlen(packet) + 1);
    assert(out);
    strcpy(out, packet);
    return out;
}
void recomp_free(void *memory) { ++frees; free(memory); }
int anchor_boss_invitation_is_current(int cid, int session, int sequence)
{
    assert(cid == 2 && session == 123 && sequence == 7);
    return current;
}
void anchor_dismiss_boss_invitation(int cid, int session, int sequence)
{
    assert(cid == 2 && session == 123 && sequence == 7);
    ++dismissals;
    packet = NULL;
}
int anchor_dialog_begin(const char *name, const char *arena)
{
    assert(!modal);
    if (!dialog_available)
        return 0;
    ++starts;
    strcpy(shown_name, name);
    strcpy(shown_arena, arena);
    modal = 1;
    return 1;
}
AnchorDialogResult anchor_dialog_poll(void)
{
    assert(modal);
    if (choice != ANCHOR_DIALOG_PENDING)
        modal = 0;
    return choice;
}
void anchor_dialog_cancel(void) { ++cancels; modal = 0; }
int anchor_dialog_busy(void) { return modal; }

static void begin(void)
{
    current = 1;
    packet = invite;
    choice = ANCHOR_DIALOG_PENDING;
    anchor_boss_invites_update();
    assert(modal);
}

int main(void)
{
    /* Cutscenes and existing native windows defer without consuming a packet. */
    packet = invite;
    prompt_safe = 0;
    anchor_boss_invites_update();
    assert(starts == 0 && peeks == 0 && dismissals == 0);
    assert(published_arena == 0 && published_visit == 4);
    prompt_safe = 1;
    dialog_available = 0;
    anchor_boss_invites_update();
    assert(starts == 0 && packet && frees == 1);
    dialog_available = 1;
    begin();
    assert(starts == 1 && strcmp(shown_name, "Ahmad") == 0);
    assert(strcmp(shown_arena, "Congo's Arena") == 0);
    for (int i = 0; i < 100; ++i)
        anchor_boss_invites_update();
    assert(starts == 1 && warps == 0 && dismissals == 0);

    /* No dismisses exactly once and never changes rooms. */
    choice = ANCHOR_DIALOG_NO;
    anchor_boss_invites_update();
    anchor_boss_invites_update();
    assert(!modal && dismissals == 1 && warps == 0);

    /* Yes closes the native window first. A temporary native load/pause gate
     * retries the accepted request, not the popup, and warps once. */
    begin();
    choice = ANCHOR_DIALOG_YES;
    warp_ready = 0;
    anchor_boss_invites_update();
    assert(!modal && warps == 0 && dismissals == 1);
    for (int i = 0; i < 3; ++i)
        anchor_boss_invites_update();
    assert(starts == 2 && warps == 0);
    warp_ready = 1;
    anchor_boss_invites_update();
    anchor_boss_invites_update();
    assert(warps == 1 && dismissals == 2 && published_arena == 1);

    /* Sender departure revokes an open prompt and an accepted deferred join. */
    current_arena = 0;
    begin();
    current = 0;
    anchor_boss_invites_update();
    assert(cancels == 1 && !modal && warps == 1);
    begin();
    choice = ANCHOR_DIALOG_YES;
    warp_ready = 0;
    anchor_boss_invites_update();
    current = 0;
    warp_ready = 1;
    anchor_boss_invites_update();
    assert(warps == 1 && dismissals == 4);

    /* Disconnect/save unload cancels without depending on a game UI tick. */
    begin();
    connected = 0;
    anchor_boss_invites_update();
    assert(!modal && published_arena == 0);
    connected = 1;
    begin();
    loaded = 0;
    anchor_boss_invites_update();
    assert(!modal && warps == 1 && dismissals == 6);
    loaded = 1;

    /* Native cancellation also dismisses; malformed and other-boss packets
     * cannot open UI or reach the native warp adapter. */
    begin();
    choice = ANCHOR_DIALOG_CANCELLED;
    anchor_boss_invites_update();
    int before = starts;
    packet = "{\"cid\":2,\"session\":123,\"seq\":7,\"arena\":2,\"name\":\"Other\"}";
    anchor_boss_invites_update();
    packet = "{\"cid\":2,\"session\":123,\"seq\":7}";
    anchor_boss_invites_update();
    assert(starts == before && warps == 1 && !modal);
    puts("boss invitation coordinator: deferred UI, Yes/No, cancellation and native warp ordering passed");
    return 0;
}
