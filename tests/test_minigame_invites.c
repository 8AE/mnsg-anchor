#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world/anchor_minigame_invites.h"
#include "core/anchor_dialog.h"

unsigned short D_800C7AB2;
unsigned int D_8015C5D8_15D1D8[12];
static unsigned char player_task[0xe0], player_object[0x98], player_work[0x90];
void *D_801FC604_5B8514 = player_task;
void *D_801FC60C_5B851C = player_object;
static int connected = 1, disabled, loaded = 1, live_world = 1;
static int swaps, skin_clears;
static int prompt_safe = 1, boss_active, sign_pending;
static int published_game = -1, published_visit = -1, publishes;
static int current = 1, peeks, frees, dismissals, starts, cancels;
static int warps, warp_ready = 1, warp_room, warp_x, warp_y, warp_z;
static int modal;
static AnchorDialogResult choice = ANCHOR_DIALOG_PENDING;
static AnchorDialogOwner owner = ANCHOR_DIALOG_OWNER_NONE;
static const char *packet;
static char shown_name[257], shown_game[64];

int anchor_is_connected(void) { return connected; }
int anchor_is_disabled(void) { return disabled; }
int item_sync_save_is_loaded(void) { return loaded; }
int anchor_boss_invite_world_loaded_player_active(void)
{
    return live_world && player_work[0x69] == 0;
}
int anchor_boss_invite_world_loaded_player_present(void)
{
    return live_world && (player_work[0x69] == 0 ||
                          (player_work[0x69] == 1 && player_task[0xcc] == 0xba));
}
void anchor_player_skin_clear_for_character_change(void) { ++skin_clears; }
void func_801DD5C0_5994D0(void *task, unsigned char character)
{
    assert(task == player_task && character < 4);
    ++swaps;
    player_task[0x60] = character;
    D_8015C5D8_15D1D8[1] = character;
    player_work[0x69] = 1;
    player_task[0xcc] = 0xba;
}
int anchor_boss_invite_world_can_prompt(void) { return prompt_safe; }
int anchor_boss_invites_active(void) { return boss_active; }
int anchor_castle_return_sign_pending(void) { return sign_pending; }
int anchor_update_minigame_active(int game, int visit)
{
    ++publishes;
    published_game = game;
    published_visit = visit;
    return connected;
}
char *anchor_get_minigame_invitation_json(void)
{
    char *copy;
    ++peeks;
    if (!packet) return 0;
    copy = malloc(strlen(packet) + 1);
    assert(copy);
    strcpy(copy, packet);
    return copy;
}
int anchor_minigame_invitation_is_current(int cid, int session, int seq)
{
    assert(cid == 2 && session == 202 && seq == 7);
    return current;
}
void anchor_dismiss_minigame_invitation(int cid, int session, int seq)
{
    assert(cid == 2 && session == 202 && seq == 7);
    ++dismissals;
    packet = 0;
}
void recomp_free(void *memory) { ++frees; free(memory); }
int anchor_boss_invite_world_transfer_to(unsigned short room,
                                         short x, short y, short z)
{
    assert(!modal);
    if (!warp_ready || !prompt_safe) return 0;
    ++warps;
    warp_room = room;
    warp_x = x;
    warp_y = y;
    warp_z = z;
    return 1;
}
int anchor_dialog_begin_minigame(const char *name, const char *game)
{
    assert(!modal);
    ++starts;
    strcpy(shown_name, name);
    strcpy(shown_game, game);
    modal = 1;
    owner = ANCHOR_DIALOG_OWNER_MINIGAME_INVITE;
    return 1;
}
AnchorDialogResult anchor_dialog_poll_for(AnchorDialogOwner requested)
{
    if (owner != requested) return ANCHOR_DIALOG_IDLE;
    if (choice != ANCHOR_DIALOG_PENDING) {
        modal = 0;
        owner = ANCHOR_DIALOG_OWNER_NONE;
    }
    return choice;
}
void anchor_dialog_cancel_for(AnchorDialogOwner requested)
{
    if (owner != requested) return;
    ++cancels;
    modal = 0;
    choice = ANCHOR_DIALOG_CANCELLED;
}
int anchor_dialog_busy(void) { return modal; }

static void load_room(unsigned short room)
{
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = room;
    anchor_minigame_invites_finish_load();
    anchor_minigame_invites_update();
    if (player_work[0x69] == 1) {
        assert(published_game == 0); /* Rebind is present, not entry-ready. */
        player_work[0x69] = 0;
        anchor_minigame_invites_update();
    }
}

static void begin(int game)
{
    static char invite[100];
    snprintf(invite, sizeof(invite),
             "{\"cid\":2,\"session\":202,\"seq\":7,\"game\":%d,\"name\":\"Ahmad\"}",
             game);
    packet = invite;
    current = 1;
    choice = ANCHOR_DIALOG_PENDING;
    anchor_minigame_invites_update();
}

int main(void)
{
    static const unsigned short rooms[] = {0x01e0, 0x01e1, 0x01e2};
    static const short coords[][3] = {{0, 25, 0}, {0, 0, 0}, {-213, 0, -357}};
    static const char *labels[] = {
        "Sudden Impact Training", "Mini Ebisumaru", "Sasuke High Jump"
    };
    int before;

    D_8015C5D8_15D1D8[1] = 3;
    player_task[0x60] = 3;

    /* Player-list transfers share the invite's fixed minigame starts. A
     * busy native gate leaves the request retryable; ordinary rooms retain
     * the remote's exact signed coordinates. */
    for (int i = 0; i < 3; ++i) {
        assert(anchor_minigame_invites_is_room(rooms[i]));
        before = warps;
        warp_ready = 0;
        assert(!anchor_minigame_invites_transfer_to_room(
            rooms[i], 111, -222, 333));
        assert(warps == before);
        warp_ready = 1;
        assert(anchor_minigame_invites_transfer_to_room(
            rooms[i], 111, -222, 333));
        assert(warps == before + 1 && warp_room == rooms[i]);
        assert(warp_x == coords[i][0] && warp_y == coords[i][1] &&
               warp_z == coords[i][2]);
    }
    assert(!anchor_minigame_invites_is_room(0x0010));
    before = warps;
    warp_ready = 0;
    assert(!anchor_minigame_invites_transfer_to_room(0x0010,
                                                     -123, 456, -789));
    assert(warps == before);
    warp_ready = 1;
    assert(anchor_minigame_invites_transfer_to_room(0x0010,
                                                    -123, 456, -789));
    assert(warps == before + 1 && warp_room == 0x0010 &&
           warp_x == -123 && warp_y == 456 && warp_z == -789);

    anchor_minigame_invites_update();
    assert(published_game == 0);
    for (int i = 0; i < 3; ++i) {
        load_room(rooms[i]);
        assert(published_game == i + 1 && published_visit == i + 1);
        player_work[0x69] = 1;
        player_task[0xcc] = 0xba;
        anchor_minigame_invites_update();
        assert(published_game == i + 1); /* No false exit during rebind. */
        player_work[0x69] = 0;
        before = starts;
        prompt_safe = 0;
        begin(i + 1);
        assert(starts == before && packet);
        prompt_safe = 1;
        anchor_minigame_invites_update();
        assert(starts == before + 1 && !strcmp(shown_name, "Ahmad"));
        assert(!strcmp(shown_game, labels[i]));
        assert(owner == ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
        before = warps;
        choice = ANCHOR_DIALOG_YES;
        warp_ready = 0;
        anchor_minigame_invites_update();
        assert(warps == before && !modal && anchor_minigame_invites_active());
        warp_ready = 1;
        anchor_minigame_invites_update();
        assert(warps == before + 1 && warp_room == rooms[i]);
        assert(warp_x == coords[i][0] && warp_y == coords[i][1] &&
               warp_z == coords[i][2] && !packet &&
               !anchor_minigame_invites_active());
        anchor_minigame_invites_begin_load();
        anchor_minigame_invites_update();
        assert(published_game == 0);
    }

    load_room(rooms[0]);
    begin(1);
    choice = ANCHOR_DIALOG_NO;
    before = warps;
    anchor_minigame_invites_update();
    assert(warps == before && !modal && !packet);

    begin(1);
    live_world = 0; /* Death or world teardown revokes the loaded challenge. */
    current = 0;
    anchor_minigame_invites_update();
    assert(published_game == 0 && cancels > 0 && !modal);
    live_world = 1;
    load_room(rooms[1]);
    begin(2);
    loaded = 0;
    anchor_minigame_invites_update();
    assert(published_game == 0 && !modal && !packet);
    loaded = 1;

    /* A same-room reload invalidates an accepted retry from the old visit. */
    load_room(rooms[1]);
    begin(2);
    choice = ANCHOR_DIALOG_YES;
    warp_ready = 0;
    anchor_minigame_invites_update();
    assert(anchor_minigame_invites_active());
    before = warps;
    anchor_minigame_invites_begin_load();
    anchor_minigame_invites_finish_load();
    warp_ready = 1;
    anchor_minigame_invites_update();
    assert(warps == before && !anchor_minigame_invites_active() && !packet);

    load_room(rooms[0]);
    begin(3);
    choice = ANCHOR_DIALOG_YES;
    warp_ready = 0;
    anchor_minigame_invites_update();
    assert(anchor_minigame_invites_active());
    before = warps;
    D_800C7AB2 = 0x000a; /* Another room transition wins before retry. */
    warp_ready = 1;
    anchor_minigame_invites_update();
    assert(warps == before && !anchor_minigame_invites_active() && !packet);

    packet = "{\"cid\":2,\"session\":202,\"seq\":7,\"game\":99,\"name\":\"Ahmad\"}";
    before = starts;
    anchor_minigame_invites_update();
    assert(starts == before && frees > 0);
    packet = 0;

    /* A castle or boss modal cannot be interpreted as this invite's Yes. */
    modal = 1;
    owner = ANCHOR_DIALOG_OWNER_CASTLE_RETURN;
    choice = ANCHOR_DIALOG_YES;
    begin(3);
    assert(starts == before && owner == ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
    modal = 0;
    owner = ANCHOR_DIALOG_OWNER_NONE;
    packet = 0;
    boss_active = 1;
    begin(3);
    assert(starts == before);
    boss_active = 0;
    sign_pending = 1;
    begin(3);
    assert(starts == before);
    sign_pending = 0;
    packet = 0;

    assert(publishes > 0 && peeks > 0 && dismissals >= 5);
    assert(swaps >= 3 && skin_clears == swaps);

    /* The new room may expose an old-but-live task before its own player is
     * initialized. Publish restored metadata, but withhold position until the
     * native rebind settles. */
    load_room(rooms[0]);
    before = swaps;
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = 0x0010;
    assert(anchor_minigame_invites_reporting_character(0x0010) == -2);
    assert(!anchor_minigame_invites_character_motion_ready(0x0010));
    anchor_minigame_invites_finish_load();
    assert(D_8015C5D8_15D1D8[1] == 3 && player_task[0x60] == 0);
    assert(anchor_minigame_invites_reporting_character(0x0010) == 3);
    assert(!anchor_minigame_invites_character_motion_ready(0x0010));
    anchor_minigame_invites_update();
    assert(swaps == before + 1 && player_work[0x69] == 1);
    assert(anchor_minigame_invites_reporting_character(0x0010) == 3);
    assert(!anchor_minigame_invites_character_motion_ready(0x0010));
    player_work[0x69] = 0;
    anchor_minigame_invites_update();
    assert(anchor_minigame_invites_reporting_character(0x0010) == 3);
    anchor_minigame_invites_character_published(0x0010);
    assert(anchor_minigame_invites_reporting_character(0x0010) == -1);

    /* If the new task has not spawned, the runtime ID is preseeded for its
     * constructor; no native call is made against the previous task. */
    load_room(rooms[1]);
    before = swaps;
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = 0x0011;
    anchor_minigame_invites_finish_load();
    live_world = 0;
    anchor_minigame_invites_update();
    assert(swaps == before && D_8015C5D8_15D1D8[1] == 3);
    player_task[0x60] = 3; /* New constructor copies the preseeded ID. */
    live_world = 1;
    anchor_minigame_invites_update();
    assert(swaps == before && anchor_minigame_invites_reporting_character(0x0011) == 3);
    anchor_minigame_invites_character_published(0x0011);

    /* Capture the previous selection before the loader or a room controller
     * can replace the runtime global with the new minigame character. */
    D_8015C5D8_15D1D8[1] = 3;
    player_task[0x60] = 3;
    anchor_minigame_invites_begin_load();
    D_8015C5D8_15D1D8[1] = 1;
    D_800C7AB2 = rooms[1];
    anchor_minigame_invites_finish_load();
    anchor_minigame_invites_update();
    if (player_work[0x69] == 1) {
        player_work[0x69] = 0;
        anchor_minigame_invites_update();
    }
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = 0x0012;
    anchor_minigame_invites_finish_load();
    assert(D_8015C5D8_15D1D8[1] == 3);
    assert(anchor_minigame_invites_reporting_character(0x0012) == 3);

    /* Unloading a save can change the room before another loader return. */
    load_room(rooms[0]);
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = 0x0001;
    loaded = 0;
    anchor_minigame_invites_update();
    assert(D_8015C5D8_15D1D8[1] == 3);
    puts("minigame invitation coordinator: room edges, Yes/No, owner isolation and fixed destinations passed");
    return 0;
}
