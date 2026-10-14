#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world/anchor_minigame_invites.h"
#include "core/anchor_dialog.h"

unsigned short D_800C7AB2;
unsigned int D_8015C5D8_15D1D8[12];
short D_8006B780_6C380[0x226 * 5];
static unsigned char player_task[0xe0], player_object[0x98], player_work[0x90];
void *D_801FC604_5B8514 = player_task;
void *D_801FC60C_5B851C = player_object;
static unsigned int system_words[0xd0000u / 4u];
unsigned char *D_8015C5C8_15D1C8 = (unsigned char *)system_words;
static int sync_game, sync_returning;
static int return_destination_calls, return_step_calls;
static short return_camera, return_player_rot;
void func_8000607C_6C7C(unsigned short room, short x, short y, short z,
                        short camera, short player_rot, short field90, int field91)
{
    short xyz[3]={x,y,z};
    assert(room<=0x225 && field90==0 && field91==0);
    memcpy(D_8015C5C8_15D1C8+0x3afe0,&room,sizeof(room));
    memcpy(D_8015C5C8_15D1C8+0x3afe2,&player_rot,sizeof(player_rot));
    memcpy(D_8015C5C8_15D1C8+0x3afe4,xyz,sizeof(xyz));
    memcpy(D_8015C5C8_15D1C8+0x3afea,&camera,sizeof(camera));
    memcpy(D_8015C5C8_15D1C8+0x3ae3a,&field90,sizeof(field90));
    memcpy(D_8015C5C8_15D1C8+0x3ae3c,&field91,sizeof(field91));
    D_8015C5C8_15D1C8[0x3ae29]=1;
    return_camera=camera;return_player_rot=player_rot;
    ++return_destination_calls;
}
void func_80003728_4328(unsigned char step)
{
    assert(step==12 && return_destination_calls==return_step_calls+1);
    D_8015C5C8_15D1C8[0x3add4]=step;
    ++return_step_calls;
}
int anchor_minigame_sync_game(void) { return sync_game; }
int anchor_minigame_sync_returning(void) { return sync_returning; }
static int connected = 1, disabled, loaded = 1, live_world = 1;
static int swaps, skin_clears;
static int prompt_safe = 1, boss_active, sign_pending;
static int published_game = -1, published_visit = -1, publishes;
static int current = 1, peeks, frees, dismissals, starts, cancels;
static int warps, warp_ready = 1, warp_room, warp_x, warp_y, warp_z;
static int prepared_calls, prepared_game, prepared_cid;
static int prepared_session, prepared_visit, reject_explicit;
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
int anchor_minigame_prepare_join(int game, int cid, int session, int visit)
{
    ++prepared_calls;
    prepared_game = game;
    prepared_cid = cid;
    prepared_session = session;
    prepared_visit = visit;
    return cid == 0 || !reject_explicit;
}
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
    if (!anchor_minigame_invites_prepare_transfer(room)) return 0;
    anchor_minigame_invites_note_transfer(room);
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
             "{\"cid\":2,\"session\":202,\"seq\":7,\"game\":%d,\"gameVisit\":17,\"name\":\"Ahmad\"}",
             game);
    packet = invite;
    current = 1;
    choice = ANCHOR_DIALOG_PENDING;
    anchor_minigame_invites_update();
}

static void set_player_position(float x, float y, float z)
{
    memcpy(player_object + 0x08, &x, sizeof(x));
    memcpy(player_object + 0x0c, &y, sizeof(y));
    memcpy(player_object + 0x10, &z, sizeof(z));
}

static void test_source_waypoint(void)
{
    static const unsigned short games[] = {0x01e0, 0x01e1, 0x01e2};
    unsigned short room;
    float xyz[3];
    int i;

    loaded = 1;
    live_world = 1;
    player_work[0x69] = 0;
    for (i = 0; i < 3; ++i)
    {
        loaded = 0;
        anchor_minigame_invites_update();
        loaded = 1;
        D_800C7AB2 = (unsigned short)(0x30 + i);
        set_player_position(12.25f + (float)i, -8.5f, 32767.0f);
        assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
        anchor_minigame_invites_note_transfer(games[i]);
        assert(anchor_minigame_invites_source_waypoint(&room, xyz));
        assert(room == (unsigned short)(0x30 + i));
        assert(xyz[0] == 12.25f + (float)i && xyz[1] == -8.5f &&
               xyz[2] == 32767.0f);
        set_player_position(99.0f, 100.0f, 101.0f);
        anchor_minigame_invites_note_transfer(games[i]);
        assert(anchor_minigame_invites_source_waypoint(&room, xyz) &&
               xyz[0] == 12.25f + (float)i);
    }

    /* A fresh source in a minigame is never mistaken for the prior ordinary
     * room, and reload/chained minigames keep the original exact float XYZ. */
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = games[0];
    anchor_minigame_invites_finish_load();
    anchor_minigame_invites_note_transfer(games[0]);
    anchor_minigame_invites_begin_load();
    D_800C7AB2 = games[2];
    anchor_minigame_invites_finish_load();
    anchor_minigame_invites_note_transfer(games[1]);
    assert(anchor_minigame_invites_source_waypoint(&room, xyz));
    assert(room == 0x32 && xyz[0] == 14.25f && xyz[1] == -8.5f);

    anchor_minigame_invites_begin_load();
    D_800C7AB2 = 0x40;
    anchor_minigame_invites_finish_load();
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));

    D_800C7AB2 = games[0];
    anchor_minigame_invites_note_transfer(games[1]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    D_800C7AB2 = 0x40;

    /* Invalid source data is ignored; ordinary transfers do not capture. */
    set_player_position(1.0f, 2.0f, 3.0f);
    anchor_minigame_invites_note_transfer(0x41);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    set_player_position(32768.0f, 2.0f, 3.0f);
    anchor_minigame_invites_note_transfer(games[0]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    {
        union { unsigned int bits; float number; } nan_value;
        nan_value.bits = 0x7fc00000u;
        set_player_position(nan_value.number, 2.0f, 3.0f);
    }
    anchor_minigame_invites_note_transfer(games[1]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    set_player_position(1.0f, -32769.0f, 3.0f);
    anchor_minigame_invites_note_transfer(games[1]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    {
        union { unsigned int bits; float number; } infinity;
        infinity.bits = 0x7f800000u;
        set_player_position(1.0f, 2.0f, infinity.number);
    }
    anchor_minigame_invites_note_transfer(games[1]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    {
        union { unsigned int bits; float number; } neg_infinity;
        neg_infinity.bits = 0xff800000u;
        set_player_position(neg_infinity.number, 2.0f, 3.0f);
    }
    anchor_minigame_invites_note_transfer(games[1]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    D_800C7AB2 = 0x0226; /* Outside the native room-start table. */
    set_player_position(1.0f, 2.0f, 3.0f);
    anchor_minigame_invites_note_transfer(games[1]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    D_800C7AB2 = 0x40;
    live_world = 0;
    set_player_position(1.0f, 2.0f, 3.0f);
    anchor_minigame_invites_note_transfer(games[2]);
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
    live_world = 1;
    anchor_minigame_invites_note_transfer(games[2]);
    assert(anchor_minigame_invites_source_waypoint(&room, xyz));
    loaded = 0;
    anchor_minigame_invites_update();
    assert(!anchor_minigame_invites_source_waypoint(&room, xyz));
}

static void test_player_list_source_entry(void)
{
    static const unsigned short rooms[] = {0x1e0,0x1e1,0x1e2};
    static const short starts[][3] = {{0,25,0},{0,0,0},{-213,0,-357}};
    for (int i=0;i<3;++i) {
        loaded=0;anchor_minigame_invites_update();loaded=1;
        connected=live_world=prompt_safe=warp_ready=1;reject_explicit=0;
        player_work[0x69]=0;player_task[0x60]=3;D_8015C5D8_15D1D8[1]=3;
        load_room(0x40);set_player_position(18.25f,30.5f,-44.75f);
        int before=warps;
        assert(anchor_minigame_invites_transfer_to_client(rooms[i],111,-222,333,2));
        assert(warps==before+1 && warp_room==rooms[i]);
        assert(warp_x==starts[i][0] && warp_y==starts[i][1] && warp_z==starts[i][2]);
        assert(prepared_cid==2 && prepared_game==i+1);
        anchor_minigame_invites_begin_load();D_800C7AB2=rooms[i];
        anchor_minigame_invites_finish_load();
        assert(D_8015C5D8_15D1D8[1]==(unsigned int)i); /* Before new native spawn. */
        player_task[0x60]=1;D_8015C5D8_15D1D8[1]=1; /* Native forced-character edge. */
        player_work[0x69]=0;anchor_minigame_invites_update();
        assert(anchor_minigame_invites_required_character()==i);
        assert(player_task[0x60]==i && D_8015C5D8_15D1D8[1]==(unsigned int)i);
        if(i!=1) {
            assert(player_work[0x69]==1);
            assert(anchor_minigame_invites_reporting_character(rooms[i])==-2);
            assert(!anchor_minigame_invites_character_motion_ready(rooms[i]));
        }
        player_work[0x69]=0;anchor_minigame_invites_update();
        assert(anchor_minigame_invites_reporting_character(rooms[i])==i);
        assert(anchor_minigame_invites_character_motion_ready(rooms[i]));
        load_room(0x40);assert(D_8015C5D8_15D1D8[1]==3 && player_task[0x60]==3);
    }
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

    /* A direct debug/offline challenge uses cid=0 without a peer. */
    connected = 0;
    before = warps;
    assert(anchor_minigame_invites_transfer_to_room(rooms[0], 0, 0, 0));
    assert(warps == before + 1 && prepared_game == 1 &&
           prepared_cid == 0 && prepared_session == 0 && prepared_visit == 0);
    connected = 1;

    anchor_minigame_invites_update();
    assert(published_game == 0);
    for (int i = 0; i < 3; ++i) {
        load_room(rooms[i]);
        assert(published_game == i + 1 && published_visit == i + 1);
        /* Player-list host selection survives a failed backend preparation;
         * an intervening default attempt cannot replace that host. */
        before = warps;
        reject_explicit = 1;
        assert(!anchor_minigame_invites_transfer_to_client(
            rooms[i], 111, -222, 333, 2));
        assert(warps == before && prepared_game == i + 1 &&
               prepared_cid == 2 && prepared_session == 0 &&
               prepared_visit == 0);
        assert(!anchor_minigame_invites_transfer_to_room(
            rooms[i], 111, -222, 333));
        assert(warps == before && prepared_cid == 2);
        reject_explicit = 0;
        assert(anchor_minigame_invites_transfer_to_client(
            rooms[i], 111, -222, 333, 2));
        assert(warps == before + 1 && prepared_cid == 2);
        anchor_minigame_invites_cancel_player_list_join(2);
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
        if (i == 1) {
            const char *accepted_packet = packet;
            char accepted_json[100];
            int prepare_before = prepared_calls;
            int starts_before = starts, dismissals_before = dismissals;
            int peeks_before = peeks;

            strcpy(accepted_json, packet);
            reject_explicit = 1;
            /* Yes has released the dialog. A host whose native graph is
             * still pending rejects backend preparation, so the accepted
             * invitation must retry without asking or selecting again. */
            choice = ANCHOR_DIALOG_NO;
            for (int retry = 0; retry < 3; ++retry) {
                anchor_minigame_invites_update();
                assert(prepared_calls == prepare_before + retry + 1);
                assert(prepared_game == 2 && prepared_cid == 2 &&
                       prepared_session == 202 && prepared_visit == 17);
                assert(warps == before && !modal &&
                       owner == ANCHOR_DIALOG_OWNER_NONE &&
                       anchor_minigame_invites_active());
                assert(starts == starts_before && peeks == peeks_before &&
                       dismissals == dismissals_before);
                assert(packet == accepted_packet &&
                       !strcmp(packet, accepted_json));
            }
            reject_explicit = 0;
        }
        anchor_minigame_invites_update();
        assert(warps == before + 1 && warp_room == rooms[i]);
        assert(warp_x == coords[i][0] && warp_y == coords[i][1] &&
               warp_z == coords[i][2] && !packet &&
               !anchor_minigame_invites_active());
        assert(prepared_game == i + 1 && prepared_cid == 2 &&
               prepared_session == 202 && prepared_visit == 17);
        if (i == 1) {
            assert(anchor_minigame_invites_required_character() == 1 &&
                   player_task[0x60] == 1 && D_8015C5D8_15D1D8[1] == 1);
            anchor_minigame_invites_update();
            assert(warps == before + 1 && !packet && !modal &&
                   !anchor_minigame_invites_active());
        }
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

    /* A loader-completed ordinary room and a disconnect both discard a
     * rejected explicit request; later debug starts resolve cid=0. */
    loaded = 1;
    load_room(0x0010);
    reject_explicit = 1;
    assert(!anchor_minigame_invites_transfer_to_client(
        rooms[0], 0, 0, 0, 2));
    assert(prepared_cid == 2);
    load_room(0x0011);
    assert(anchor_minigame_invites_transfer_to_room(rooms[0], 0, 0, 0));
    assert(prepared_cid == 0);
    assert(!anchor_minigame_invites_transfer_to_client(
        rooms[1], 0, 0, 0, 2));
    connected = 0;
    anchor_minigame_invites_update();
    connected = 1;
    assert(anchor_minigame_invites_transfer_to_room(rooms[1], 0, 0, 0));
    assert(prepared_cid == 0);
    reject_explicit = 0;

    /* Older invitation JSON lacks gameVisit; the bridge discovers it from
     * current host metadata when C passes zero. */
    load_room(rooms[0]);
    packet = "{\"cid\":2,\"session\":202,\"seq\":7,\"game\":1,\"name\":\"Ahmad\"}";
    current = 1;
    choice = ANCHOR_DIALOG_PENDING;
    anchor_minigame_invites_update();
    choice = ANCHOR_DIALOG_YES;
    anchor_minigame_invites_update();
    assert(prepared_cid == 2 && prepared_session == 202 &&
           prepared_visit == 0 && !packet);

    test_source_waypoint();
    test_player_list_source_entry();
    puts("minigame invitation coordinator: room edges, fixed destinations and source waypoints passed");
    return 0;
}
