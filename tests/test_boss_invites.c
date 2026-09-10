#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "anchor_boss_invites.h"
#include "anchor_boss_invite_world.h"
#include "anchor_dialog.h"

static int connected = 1, disabled, loaded = 1, current_arena, prompt_safe = 1;
static int current_visit = 4, published_arena = -1, published_visit = -1;
static int published_stage = -1, current_stage;
static int current = 1, dialog_available = 1, modal, starts, cancels, dismissals;
static int warps, warp_ready = 1, peeks, frees, destination_arena;
static int stage_warps, warp_stage_value;
static int published_f90, published_f91, warp_f90, warp_f91;
static const char *packet;
static char shown_name[257], shown_arena[64];
static AnchorDialogResult choice = ANCHOR_DIALOG_PENDING;
static const char invite[] = "{\"cid\":2,\"session\":123,\"seq\":7,\"arena\":1,\"name\":\"Ahmad\"}";

int anchor_is_connected(void) { return connected; }
int anchor_is_disabled(void) { return disabled; }
int item_sync_save_is_loaded(void) { return loaded; }
int anchor_update_boss_arena(int arena, int visit, int stage,
                             unsigned int field90, unsigned int field91)
{
    published_arena = arena;
    published_visit = visit;
    published_stage = stage;
    published_f90 = (int)field90;
    published_f91 = (int)field91;
    return connected;
}
int anchor_boss_invite_world_arena(void) { return current_arena; }
unsigned int anchor_boss_invite_world_visit(void) { return (unsigned)current_visit; }
unsigned int anchor_boss_invite_world_stage(void) { return (unsigned)current_stage; }
unsigned int anchor_boss_invite_world_field90(void) { return 0; }
unsigned int anchor_boss_invite_world_field91(void) { return 0; }
int anchor_boss_invite_world_can_prompt(void) { return prompt_safe; }
int anchor_boss_invite_world_warp(int arena)
{
    assert(!modal); /* no transition while native text owns input */
    if (!warp_ready || !prompt_safe)
        return 0;
    ++warps;
    destination_arena = arena;
    current_arena = arena;
    return 1;
}
int anchor_boss_invite_world_warp_stage(unsigned int stage, unsigned int field90,
                                        unsigned int field91)
{
    assert(!modal);
    if (!warp_ready || !prompt_safe)
        return 0;
    ++stage_warps;
    warp_stage_value = (int)stage;
    warp_f90 = (int)field90;
    warp_f91 = (int)field91;
    destination_arena = 5;
    current_arena = 5;
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

    /* Native cancellation also dismisses; malformed and unknown-arena packets
     * cannot open UI or reach the native warp adapter. */
    begin();
    choice = ANCHOR_DIALOG_CANCELLED;
    anchor_boss_invites_update();
    int before = starts;
    packet = "{\"cid\":2,\"session\":123,\"seq\":7,\"arena\":99,\"name\":\"Other\"}";
    anchor_boss_invites_update();
    packet = "{\"cid\":2,\"session\":123,\"seq\":7}";
    anchor_boss_invites_update();
    assert(starts == before && warps == 1 && !modal);

    /* Destination identity survives asynchronous close and warp retries.
     * A player in one boss room can join a different boss's invitation. */
    static const char *arena_names[] = {
        "Congo's Arena", "Dharumanyo's Arena", "Tsurami's Arena",
        "Control Machine's Arena", "Kashiwagi's Arena",
        "Thaisamba's Arena", "Balberra's Arena", "D'Etoile's Arena"
    };
    char arena_packet[160];
    for (int arena = 1; arena <= 8; ++arena) {
        snprintf(arena_packet, sizeof(arena_packet),
            "{\"cid\":2,\"session\":123,\"seq\":7,\"arena\":%d,\"name\":\"Ahmad\"}", arena);
        current_arena = arena == 1 ? 2 : 1;
        current = 1;
        packet = arena_packet;
        choice = ANCHOR_DIALOG_PENDING;
        int prior_starts = starts, prior_warps = warps;
        int prior_stage_warps = stage_warps;
        anchor_boss_invites_update();
        assert(modal && starts == prior_starts + 1);
        assert(strcmp(shown_arena, arena_names[arena - 1]) == 0);
        choice = ANCHOR_DIALOG_NO;
        anchor_boss_invites_update();
        assert(!modal && warps == prior_warps &&
               stage_warps == prior_stage_warps && !packet);

        packet = arena_packet;
        choice = ANCHOR_DIALOG_PENDING;
        anchor_boss_invites_update();
        assert(modal);
        choice = ANCHOR_DIALOG_YES;
        warp_ready = 0;
        anchor_boss_invites_update();
        assert(!modal && warps == prior_warps &&
               stage_warps == prior_stage_warps);
        /* Peeking a new queue head must not redirect the accepted request. */
        packet = invite;
        anchor_boss_invites_update();
        assert(warps == prior_warps && stage_warps == prior_stage_warps);
        warp_ready = 1;
        anchor_boss_invites_update();
        if (arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST)
            assert(stage_warps == prior_stage_warps + 1);
        else
            assert(warps == prior_warps + 1 && destination_arena == arena);
        assert(!packet);
        anchor_boss_invites_update();
        if (arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST)
            assert(stage_warps == prior_stage_warps + 1);
        else
            assert(warps == prior_warps + 1);
    }

    /* An Impact invitation carries the first native stage and the native
     * load-from-start fields so the guest resumes at the cutscene start. */
    static const char impact_invite[] =
        "{\"cid\":2,\"session\":123,\"seq\":7,\"arena\":5,\"stage\":546,"
        "\"f90\":4660,\"f91\":43981,\"name\":\"Ahmad\"}";
    current_arena = 0;
    current = 1;
    packet = impact_invite;
    choice = ANCHOR_DIALOG_PENDING;
    int prior_stage_warps = stage_warps;
    anchor_boss_invites_update();
    assert(modal && strcmp(shown_arena, "Kashiwagi's Arena") == 0);
    choice = ANCHOR_DIALOG_YES;
    anchor_boss_invites_update();
    assert(!modal && stage_warps == prior_stage_warps + 1 &&
           warp_stage_value == 546 && warp_f90 == 4660 && warp_f91 == 43981);
    assert(published_stage == 0); /* The receiver's own stage is unrelated. */
    assert(!packet);

    puts("boss invitation coordinator: deferred UI, Yes/No, cancellation and native warp ordering passed");
    return 0;
}
