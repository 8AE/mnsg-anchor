#ifdef ANCHOR_MINIGAME_INVITES_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern int anchor_update_minigame_active(int game, int visit);
extern char *anchor_update_minigame(int game, int visit, const char *state_json);
extern char *anchor_get_minigame_invitation_json(void);
extern int anchor_minigame_invitation_is_current(int cid, int session, int seq);
extern void anchor_dismiss_minigame_invitation(int cid, int session, int seq);
extern void recomp_free(void *memory);
#else
#include "platform/modding.h"
#include "platform/recomputils.h"
#include "core/anchor.h"
#endif

#include "world/anchor_minigame_invites.h"
#include "bosses/anchor_boss_invites.h"
#include "bosses/anchor_boss_invite_world.h"
#include "core/anchor_dialog.h"
#include "progression/item_sync.h"
#include "player/alternative_ebisumaru/anchor_player_skin.h"
#include "utils/json_utils.h"
#include "world/anchor_castle_return_sign.h"
#include "world/anchor_minigame_sync.h"

extern unsigned short D_800C7AB2;
extern unsigned int D_8015C5D8_15D1D8[];
extern short D_8006B780_6C380[];
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned char *D_8015C5C8_15D1C8;
extern void func_801DD5C0_5994D0(void *task, unsigned char character);
extern void func_8000607C_6C7C(unsigned short room, short x, short y, short z,
                              short camera_rot, short player_rot,
                              short field90, int field91);
extern void func_80003728_4328(unsigned char step);
extern char *anchor_get_transfer_target_json(unsigned int cid);
extern int func_8003F1D8_3FDD8(void);
extern int anchor_minigame_prepare_join(int game, int cid, int session, int visit);

typedef struct {
    int cid;
    int session;
    int sequence;
    int game;
    int game_visit;
    int joining;
    unsigned short local_room;
    unsigned int local_visit;
} MinigameInvitation;

enum JoinRequestOwner {
    JOIN_REQUEST_NONE = 0,
    JOIN_REQUEST_INVITE = 1,
    JOIN_REQUEST_PLAYER_LIST = 2
};

typedef struct {
    int owner;
    int game;
    int cid;
    int session;
    int game_visit;
    unsigned short source_room;
    unsigned int source_visit;
} MinigameJoinRequest;

static MinigameInvitation s_invitation;
static MinigameJoinRequest s_join_request;
static unsigned short s_loaded_room;
static unsigned int s_visit;
static int s_room_loaded;
static int s_saved_character = -1;
static int s_preload_character = -1;
static int s_restore_character = -1;
static int s_restore_pending;
static int s_restore_metadata_pending;
static int s_announced_visit;
static void *s_attempt_task;
static void *s_attempt_object;
static unsigned int s_attempt_visit;
static int s_attempted;
static unsigned short s_source_room;
static float s_source_xyz[3];
static int s_source_valid;
static unsigned int s_source_mini_visit;
static unsigned short s_source_mini_room;
static int s_yae_transfer, s_yae_requested, s_yae_active;
static int s_yae_transfer_origin, s_yae_request_origin, s_yae_cancelled;
static struct { int cid,session,visit; } s_yae_host;
static unsigned int s_yae_request_visit, s_yae_wait_frames;
static void yae_request_trace(const char *edge,int origin)
{
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && !defined(ANCHOR_MINIGAME_INVITES_HOST_TEST)
    if(origin==ANCHOR_YAE_LAUNCH_DEBUG)
        recomp_printf("[YaeDebugRequest] %s room=%X loaded=%d visit=%u pending=%d requested=%d origin=%d requestVisit=%u\n",
            edge,(unsigned int)D_800C7AB2,s_room_loaded,s_visit,s_yae_transfer,
            s_yae_requested,s_yae_request_origin,s_yae_request_visit);
#else
    (void)edge;(void)origin;
#endif
}
/* The result helper still runs natively, including its preceding statistics
 * and reward cleanup. Its request is replaced by a full ordinary-world warp;
 * the new player constructor consumes the signed-short waypoint first. */
static struct {
    int phase; /* 1: requested, 2: matching room loaded, awaiting constructor. */
    unsigned short room;
    unsigned int origin_visit, arrival_visit;
    float xyz[3];
} s_return;

#define MINI_SYS_MARKER 0x3ae29u
#define MINI_SYS_SETUP 0x3ae2cu
#define MINI_SYS_OPTION 0x3ae30u
#define MINI_SYS_DEST_ROOM 0x3afe0u
#define MINI_SYS_DEST_X 0x3afe4u

static void clear_return(void)
{
    __builtin_memset(&s_return, 0, sizeof(s_return));
}

int anchor_minigame_invites_active(void)
{
    return s_invitation.cid != 0;
}

static int dedicated_room_game(unsigned short room)
{
    switch (room) {
    case 0x01e0: return ANCHOR_MINIGAME_GOEMON;
    case 0x01e1: return ANCHOR_MINIGAME_EBISUMARU;
    case 0x01e2: return ANCHOR_MINIGAME_SASUKE;
    default: return ANCHOR_MINIGAME_NONE;
    }
}

static int game_for_room(unsigned short room)
{
    if (room == 0x0154u && (s_yae_active || s_yae_requested))
        return ANCHOR_MINIGAME_YAE;
    return dedicated_room_game(room);
}

static int game_for_transfer(unsigned short room)
{
    if (room == 0x0154u && s_yae_transfer)
        return ANCHOR_MINIGAME_YAE;
    return dedicated_room_game(room);
}

int anchor_minigame_invites_is_room(unsigned short room)
{
    return dedicated_room_game(room) != ANCHOR_MINIGAME_NONE;
}

static int valid_waypoint_coordinate(float value)
{
    union { float number; unsigned int bits; } encoded;
    encoded.number = value;
    return (encoded.bits & 0x7f800000u) != 0x7f800000u &&
           value >= -32768.0f && value <= 32767.0f;
}

void anchor_minigame_invites_note_transfer(unsigned short destination_room)
{
    const unsigned char *object = D_801FC60C_5B851C;
    float xyz[3];
    int i;
    if (!game_for_transfer(destination_room) || s_source_valid ||
        !item_sync_save_is_loaded() ||
        D_800C7AB2 > 0x0225u || game_for_room(D_800C7AB2) ||
        !anchor_boss_invite_world_loaded_player_active() || !object)
        return;
    for (i = 0; i < 3; ++i)
    {
        __builtin_memcpy(&xyz[i], object + 0x08 + i * 4, sizeof(float));
        if (!valid_waypoint_coordinate(xyz[i]))
            return;
    }
    s_source_room = D_800C7AB2;
    for (i = 0; i < 3; ++i)
        s_source_xyz[i] = xyz[i];
    s_source_valid = 1;
    s_source_mini_visit = 0;
    s_source_mini_room = 0;
}

int anchor_minigame_invites_source_waypoint(unsigned short *room, float xyz[3])
{
    int i;
    if (!s_source_valid || !room || !xyz)
        return 0;
    *room = s_source_room;
    for (i = 0; i < 3; ++i)
        xyz[i] = s_source_xyz[i];
    return 1;
}

static int request_saved_source_return(void)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    unsigned int setup = 0;
    unsigned short option = 0;
    short xyz[3];
    const short *start;
    int i;
    if (!system || !s_source_valid || s_return.phase ||
        !item_sync_save_is_loaded() || !s_room_loaded ||
        s_loaded_room != D_800C7AB2 ||
        s_source_mini_room != s_loaded_room || s_source_mini_visit != s_visit ||
        s_source_room > 0x225u || dedicated_room_game(s_source_room))
        return 0;
    for (i = 0; i < 3; ++i)
        if (!valid_waypoint_coordinate(s_source_xyz[i])) return 0;
    s_return.room = s_source_room;
    s_return.origin_visit = s_visit;
    for (i = 0; i < 3; ++i) {
        xyz[i] = (short)s_source_xyz[i];
        s_return.xyz[i] = s_source_xyz[i];
    }
    /* Main's full world loader selects the character's render/model bank in
     * func_8001FA80 before func_8020D6BC returns. The native result has already
     * restored its statistics; seed the preserved source character before
     * that load. The retiring minigame player is not rebound here. */
    if (s_saved_character >= 0 && s_saved_character < 4) {
        D_8015C5D8_15D1D8[1] = (unsigned int)s_saved_character;
        D_8015C5D8_15D1D8[0x2c / 4] = 1;
    }
    s_return.phase = 1;
    /* This frame's main RETURN hooks run before the next warp dispatch. Treat
     * the old visit as unloading so its character reconciliation cannot undo
     * the source-character seed or advertise another minigame participant. */
    s_room_loaded = 0;
    s_announced_visit = 0;
    start = &D_8006B780_6C380[s_return.room * 5u];
    func_8000607C_6C7C(s_return.room, xyz[0], xyz[1], xyz[2],
                       start[3], start[4], 0, 0);
    /* Ebisumaru's extended helper leaves a teacher-room camera override even
     * after marker flag 0x08 is cleared. Ordinary source rooms use their own
     * default rotations, without that setup/option override. */
    __builtin_memcpy(system + MINI_SYS_SETUP, &setup, sizeof(setup));
    __builtin_memcpy(system + MINI_SYS_OPTION, &option, sizeof(option));
    /* The exit callers only finish their native phase after the helper. Main
     * dispatches once per frame, so step 12 preempts File11's selective room
     * chain before it can reuse the old room's resource/initializer cache. */
    func_80003728_4328(12);
    return 1;
}

static void override_native_return(int ebisumaru)
{
    unsigned char *system = D_8015C5C8_15D1C8;
    unsigned short pending_room;
    int game = game_for_room(D_800C7AB2);
    if (!system || !game || game == ANCHOR_MINIGAME_YAE ||
        anchor_minigame_sync_game() != game || !anchor_minigame_sync_returning())
        return;
    __builtin_memcpy(&pending_room, system + MINI_SYS_DEST_ROOM, sizeof(pending_room));
    /* The extended Ebisumaru helper has already ORed flag 0x08 into marker 1
     * at this RETURN boundary, so its native exit marker is 9. */
    if (ebisumaru ? game != ANCHOR_MINIGAME_EBISUMARU ||
                       pending_room != 0x016du || system[MINI_SYS_MARKER] != 9u :
                   game == ANCHOR_MINIGAME_EBISUMARU || system[MINI_SYS_MARKER] != 4u)
        return;
    (void)request_saved_source_return();
}

RECOMP_HOOK_RETURN("func_801FA6E4_5B65F4")
void anchor_minigame_invites_return_saved(void)
{
    override_native_return(0);
}

RECOMP_HOOK_RETURN("func_801FA420_5B6330")
void anchor_minigame_invites_return_ebisumaru(void)
{
    override_native_return(1);
}

static int native_player_pointer(const void *pointer)
{
#ifdef ANCHOR_MINIGAME_INVITES_HOST_TEST
    return pointer != 0 && (unsigned long)pointer != 0x80000000ul;
#else
    unsigned int address = (unsigned int)(unsigned long)pointer;
    return address >= 0x80001000u && address < 0x80800000u;
#endif
}
static void *player_pointer_at(const void *base, unsigned int offset)
{
    void *pointer;
    __builtin_memcpy(&pointer, (const unsigned char *)base + offset, sizeof(pointer));
    return pointer;
}

/* File11 constructor has registered the new real player and its object by
 * return. Its first AI initializes previous-position fields from this object
 * afterward (func_801CC30C), so restore before that first physics callback. */
static void restore_player_coordinates(int retained_player)
{
    void *task = D_801FC604_5B8514, *object = D_801FC60C_5B851C, *backlink;
    void *work = 0;
    int i;
    if (s_return.phase != 2) return;
    if (!item_sync_save_is_loaded() || !s_room_loaded ||
        s_return.arrival_visit != s_visit || s_loaded_room != s_return.room ||
        D_800C7AB2 != s_return.room || game_for_room(D_800C7AB2)) {
        clear_return();return;
    }
    if (!native_player_pointer(task) || !native_player_pointer(object)) {
        clear_return();return;
    }
    backlink = player_pointer_at(task, 4);
    if (!native_player_pointer(backlink) || player_pointer_at(backlink, 0) != task ||
        player_pointer_at(task, 0x18) != object) {
        clear_return();return;
    }
    if (retained_player) {
        work = player_pointer_at(task, 0x5c);
        if (!native_player_pointer(work)) { clear_return();return; }
    }
    for (i = 0; i < 3; ++i)
    {
        __builtin_memcpy((unsigned char *)object + 0x08 + i * 4,
                         &s_return.xyz[i], sizeof(float));
        /* A retained player has already run its first AI. Its previous XYZ
         * uses the same work+5c/60/64 fields initialized by func_801CC30C. */
        if (work) __builtin_memcpy((unsigned char *)work + 0x5c + i * 4,
                                   &s_return.xyz[i], sizeof(float));
    }
    clear_return();
}

RECOMP_HOOK_RETURN("func_801CB5D0_5874E0")
void anchor_minigame_invites_restore_spawn(void)
{
    restore_player_coordinates(0);
}

/* File11's alternate loader retains the player. Its room dispatcher has
 * finished at this return boundary, before the resumed world frame. */
RECOMP_HOOK_RETURN("func_801F7F78_5B3E88")
void anchor_minigame_invites_restore_in_place(void)
{
    restore_player_coordinates(1);
}

static void clear_join_request(void)
{
    s_join_request.owner = JOIN_REQUEST_NONE;
    s_join_request.game = 0;
    s_join_request.cid = 0;
    s_join_request.session = 0;
    s_join_request.game_visit = 0;
    s_join_request.source_room = 0;
    s_join_request.source_visit = 0;
}

static int stage_join_request(int owner, int game, int cid,
                              int session, int game_visit)
{
    if (!game || cid <= 0 || session < 0 || game_visit < 0 ||
        !s_room_loaded || s_loaded_room != D_800C7AB2)
        return 0;
    if (s_join_request.owner == JOIN_REQUEST_INVITE &&
        owner == JOIN_REQUEST_PLAYER_LIST && s_invitation.joining)
        return 0;
    s_join_request.owner = owner;
    s_join_request.game = game;
    s_join_request.cid = cid;
    s_join_request.session = session;
    s_join_request.game_visit = game_visit;
    s_join_request.source_room = D_800C7AB2;
    s_join_request.source_visit = s_visit;
    return 1;
}

int anchor_minigame_invites_prepare_transfer(unsigned short destination_room)
{
    int game = game_for_transfer(destination_room);
    if (!game)
        return 1;
    if (s_join_request.owner &&
        (!s_room_loaded || s_join_request.source_room != D_800C7AB2 ||
         s_join_request.source_visit != s_visit))
        clear_join_request();
    if (s_join_request.owner)
    {
        if (s_join_request.game != game)
            return 0;
        return anchor_minigame_prepare_join(game, s_join_request.cid,
                                            s_join_request.session,
                                            s_join_request.game_visit);
    }
    return anchor_minigame_prepare_join(game, 0, 0, 0);
}

void anchor_minigame_invites_cancel_player_list_join(int cid)
{
    if (s_join_request.owner == JOIN_REQUEST_PLAYER_LIST &&
        s_join_request.cid == cid)
        clear_join_request();
}

static int character_for_game(int game)
{
    switch (game) {
    case ANCHOR_MINIGAME_GOEMON: return 0;
    case ANCHOR_MINIGAME_EBISUMARU: return 1;
    case ANCHOR_MINIGAME_SASUKE: return 2;
    case ANCHOR_MINIGAME_YAE: return 3;
    default: return -1;
    }
}

int anchor_minigame_invites_required_character(void)
{
    if (!s_room_loaded || s_loaded_room != D_800C7AB2)
        return -1;
    return character_for_game(game_for_room(s_loaded_room));
}

static int character_settled(int character)
{
    unsigned char *task = D_801FC604_5B8514;
    return character >= 0 && anchor_boss_invite_world_loaded_player_active() &&
           task && task[0x60] == (unsigned char)character &&
           (D_8015C5D8_15D1D8[1] & 0xffu) == (unsigned int)character;
}

int anchor_minigame_invites_reporting_character(unsigned short room)
{
    int character;
    if (room != D_800C7AB2 || !s_room_loaded || s_loaded_room != room)
        return game_for_room(room) || s_saved_character >= 0 ||
               s_restore_pending ? -2 : -1;
    character = anchor_minigame_invites_required_character();
    if (character >= 0)
        return character_settled(character) ? character : -2;
    if (s_restore_metadata_pending)
        return s_restore_character;
    return -1;
}

int anchor_minigame_invites_character_motion_ready(unsigned short room)
{
    if (!s_room_loaded && (s_saved_character >= 0 || s_restore_pending) &&
        room == D_800C7AB2)
        return 0;
    if (s_room_loaded && s_loaded_room == room && room == D_800C7AB2 &&
        game_for_room(room))
        return character_settled(character_for_game(game_for_room(room)));
    return !(s_restore_pending && room == s_loaded_room &&
             room == D_800C7AB2);
}

void anchor_minigame_invites_character_published(unsigned short room)
{
    if (room == s_loaded_room && !game_for_room(room) &&
        s_restore_metadata_pending && !s_restore_pending)
    {
        s_restore_metadata_pending = 0;
        s_restore_character = -1;
    }
}

static void reset_attempt_for_current_task(void)
{
    if (s_attempt_visit != s_visit ||
        s_attempt_task != D_801FC604_5B8514 ||
        s_attempt_object != D_801FC60C_5B851C)
    {
        s_attempt_visit = s_visit;
        s_attempt_task = D_801FC604_5B8514;
        s_attempt_object = D_801FC60C_5B851C;
        s_attempted = 0;
    }
}

static void update_local_character(void)
{
    int target;
    unsigned char *task;
    if (!item_sync_save_is_loaded())
    {
        s_source_valid = 0;
        s_source_mini_visit = 0;
        s_yae_transfer = s_yae_requested = s_yae_active = 0;
        s_yae_transfer_origin = s_yae_request_origin = s_yae_cancelled = 0;
        s_yae_host.cid=s_yae_host.session=s_yae_host.visit=0;
        anchor_minigame_yae_direct_load_cancel();
        s_yae_request_visit = s_yae_wait_frames = 0;
        clear_return();
        clear_join_request();
        if (s_saved_character >= 0)
        {
            D_8015C5D8_15D1D8[1] = (unsigned int)s_saved_character;
            D_8015C5D8_15D1D8[0x2c / 4] = 1;
            s_saved_character = -1;
        }
        s_preload_character = -1;
        s_restore_character = -1;
        s_restore_pending = 0;
        s_restore_metadata_pending = 0;
        s_announced_visit = 0;
        return;
    }
    if (!s_room_loaded || s_loaded_room != D_800C7AB2)
        return;
    target = anchor_minigame_invites_required_character();
    if (target < 0 && !s_restore_pending)
        return;
    if (target < 0)
        target = s_restore_character;
    reset_attempt_for_current_task();
    if (character_settled(target))
    {
        s_attempted = 0;
        if (s_restore_pending)
            s_restore_pending = 0;
        return;
    }
    if (!anchor_boss_invite_world_loaded_player_active() ||
        anchor_dialog_busy() || s_attempted)
        return;
    task = D_801FC604_5B8514;
    anchor_player_skin_clear_for_character_change();
    D_8015C5D8_15D1D8[1] = (unsigned int)target;
    D_8015C5D8_15D1D8[0x2c / 4] = 1;
    func_801DD5C0_5994D0(task, (unsigned char)target);
    s_attempted = 1;
}

static const char *game_name(int game)
{
    switch (game) {
    case ANCHOR_MINIGAME_GOEMON: return "Sudden Impact Training";
    case ANCHOR_MINIGAME_EBISUMARU: return "Mini Ebisumaru";
    case ANCHOR_MINIGAME_SASUKE: return "Sasuke High Jump";
    case ANCHOR_MINIGAME_YAE: return "Yae Mermaid Training";
    default: return 0;
    }
}

static int transfer_to_yae(int origin)
{
#if !defined(DEBUG_BUTTON_ENABLED) || !DEBUG_BUTTON_ENABLED
    if(origin==ANCHOR_YAE_LAUNCH_DEBUG) return 0;
#endif
    if (origin != ANCHOR_YAE_LAUNCH_DEBUG &&
        (origin != ANCHOR_YAE_LAUNCH_JOIN || s_join_request.game!=ANCHOR_MINIGAME_YAE ||
         s_join_request.cid<=0 || s_join_request.session<=0 || s_join_request.game_visit<=0))
        return 0;
    if (s_yae_transfer)
        return 0;
    s_yae_transfer = 1;
    s_yae_transfer_origin = origin;
    if (anchor_boss_invite_world_transfer_to(0x0154, 29, -320, -544)) {
        /* Preserve the source ID before the next full world loader
         * reconstructs player/render state with the required character.
         * The retiring ordinary-world task must not be rebound. */
        if (s_saved_character < 0)
            s_saved_character = s_restore_character >= 0
                ? s_restore_character : (int)(D_8015C5D8_15D1D8[1] & 0xffu);
        D_8015C5D8_15D1D8[1] = 3;
        D_8015C5D8_15D1D8[0x2c / 4] = 1;
        s_room_loaded = 0;
        s_announced_visit = 0;
        if(origin==ANCHOR_YAE_LAUNCH_JOIN) {
            s_yae_host.cid=s_join_request.cid;s_yae_host.session=s_join_request.session;
            s_yae_host.visit=s_join_request.game_visit;
        } else s_yae_host.cid=s_yae_host.session=s_yae_host.visit=0;
        if(origin==ANCHOR_YAE_LAUNCH_JOIN) anchor_minigame_yae_direct_load_begin();
        else anchor_minigame_yae_direct_load_cancel();
        yae_request_trace("armed",origin);
        return 1;
    }
    s_yae_transfer = 0;
    s_yae_transfer_origin = ANCHOR_YAE_LAUNCH_NONE;
    return 0;
}

static int transfer_to_game(int game)
{
    switch (game) {
    case ANCHOR_MINIGAME_GOEMON:
        return anchor_boss_invite_world_transfer_to(0x01e0, 0, 25, 0);
    case ANCHOR_MINIGAME_EBISUMARU:
        return anchor_boss_invite_world_transfer_to(0x01e1, 0, 0, 0);
    case ANCHOR_MINIGAME_SASUKE:
        return anchor_boss_invite_world_transfer_to(0x01e2, -213, 0, -357);
    case ANCHOR_MINIGAME_YAE:
        return transfer_to_yae(ANCHOR_YAE_LAUNCH_JOIN);
    default:
        return 0;
    }
}

int anchor_minigame_invites_transfer_to_client(unsigned short room,
                                               short x, short y, short z,
                                               int cid)
{
    int game = dedicated_room_game(room);
    if (game)
    {
        if (!stage_join_request(JOIN_REQUEST_PLAYER_LIST, game, cid, 0, 0))
            return 0;
        return transfer_to_game(game);
    }
    return anchor_boss_invite_world_transfer_to(room, x, y, z);
}

int anchor_minigame_invites_transfer_to_room(unsigned short room,
                                             short x, short y, short z)
{
    int game = dedicated_room_game(room);
    if (game)
        return transfer_to_game(game);
    return anchor_boss_invite_world_transfer_to(room, x, y, z);
}

int anchor_minigame_invites_debug_start_yae(void)
{
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    return transfer_to_yae(ANCHOR_YAE_LAUNCH_DEBUG);
#else
    return 0;
#endif
}

int anchor_minigame_invites_transfer_to_yae_client(int cid)
{
    char *json;
    int selected,room,game,session,visit,valid;
    if(cid<=0) return 0;
    json=anchor_get_transfer_target_json((unsigned int)cid);
    if(!json) return 0;
    valid=mnsg_json_get_s32(json,"cid",&selected) && selected==cid &&
        mnsg_json_get_s32(json,"room",&room) && room==0x154 &&
        mnsg_json_get_s32(json,"game",&game) && game==ANCHOR_MINIGAME_YAE &&
        mnsg_json_get_s32(json,"session",&session) && session>0 &&
        mnsg_json_get_s32(json,"gameVisit",&visit) && visit>0;
    recomp_free(json);
    if(!valid || !stage_join_request(JOIN_REQUEST_PLAYER_LIST,ANCHOR_MINIGAME_YAE,
                                     cid,session,visit)) return 0;
    return transfer_to_game(ANCHOR_MINIGAME_YAE);
}

int anchor_minigame_invites_yae_requested(void)
{
    return s_yae_requested && s_room_loaded &&
           s_yae_request_visit == s_visit && s_loaded_room == 0x0154u &&
           D_800C7AB2 == s_loaded_room && item_sync_save_is_loaded();
}

int anchor_minigame_invites_yae_validate_join(void)
{
    return anchor_minigame_invites_yae_requested() &&
        s_yae_request_origin==ANCHOR_YAE_LAUNCH_JOIN && s_yae_host.cid>0 &&
        s_yae_host.session>0 && s_yae_host.visit>0 &&
        anchor_minigame_prepare_join(ANCHOR_MINIGAME_YAE,s_yae_host.cid,
                                    s_yae_host.session,s_yae_host.visit);
}
int anchor_minigame_invites_yae_direct_pending(void)
{
    return s_yae_transfer || s_yae_requested;
}
int anchor_minigame_invites_yae_black_scope(void)
{
    return item_sync_save_is_loaded() && (s_yae_transfer ||
        ((s_yae_requested || s_yae_active) && s_room_loaded &&
         s_loaded_room==0x154u && D_800C7AB2==s_loaded_room));
}

int anchor_minigame_invites_yae_request_origin(void)
{
    return anchor_minigame_invites_yae_requested() ? s_yae_request_origin :
           ANCHOR_YAE_LAUNCH_NONE;
}

void anchor_minigame_invites_yae_cancel_request(void)
{
    if (!s_yae_requested) return;
    yae_request_trace("cancel",s_yae_request_origin);
    /* A manual sign interaction may own native control/VM now. Stop only the
     * deferred launch; preserve return/character data until that owner exits. */
    s_yae_requested = 0;
    s_yae_request_origin = ANCHOR_YAE_LAUNCH_NONE;
    /* A retry may already have staged its next guarded warp. Retiring the
     * old actor's request must not erase that independently armed load. */
    s_yae_request_visit = s_yae_wait_frames = 0;
    s_yae_cancelled = 1;
    if(!s_yae_transfer) {
        s_yae_host.cid=s_yae_host.session=s_yae_host.visit=0;
        anchor_minigame_yae_direct_load_cancel();
    }
    clear_join_request();
}

int anchor_minigame_invites_yae_cancel_pending(void)
{
    return s_yae_cancelled && s_room_loaded && s_loaded_room==0x0154u &&
           D_800C7AB2==s_loaded_room && item_sync_save_is_loaded();
}

void anchor_minigame_invites_yae_started(void)
{
    if (s_yae_active || !s_room_loaded || s_loaded_room != 0x0154u ||
        D_800C7AB2 != s_loaded_room || !item_sync_save_is_loaded())
        return;
    yae_request_trace("started",s_yae_request_origin);
    /* A sign can launch another challenge without reloading Kegon. Give that
     * run its own lifecycle before the adapter registers any native nodes. */
    anchor_minigame_sync_begin_load();
    anchor_minigame_sync_finish_load();
    s_visit = s_visit == 0x7fffffffu ? 1u : s_visit + 1u;
    if (s_saved_character < 0)
        s_saved_character = (int)(D_8015C5D8_15D1D8[1] & 0xffu);
    s_yae_requested = s_yae_transfer = 0;
    s_yae_request_origin = s_yae_transfer_origin = s_yae_cancelled = 0;
    s_yae_request_visit = s_yae_wait_frames = 0;
    s_yae_active = 1;
    s_announced_visit = s_attempted = 0;
    if (s_source_valid) {
        s_source_mini_visit = s_visit;
        s_source_mini_room = s_loaded_room;
    }
}

void anchor_minigame_invites_yae_finished(void)
{
    int returning;
    char *reply;
    if ((!s_yae_active && !s_yae_requested && !s_yae_cancelled) || !s_room_loaded ||
        s_loaded_room != 0x0154u || D_800C7AB2 != s_loaded_room ||
        !item_sync_save_is_loaded())
        return;
    if (s_yae_cancelled &&
        (!native_player_pointer(D_801FC604_5B8514) ||
         ((unsigned char *)D_801FC604_5B8514)[0xcd] != 0 ||
         !anchor_boss_invite_world_can_prompt() || anchor_dialog_busy() ||
         func_8003F1D8_3FDD8()))
        return;
    /* The native Mermaid controller has already released its camera, proxy,
     * timer, dialog and player lock. Its success path stays in Kegon, so it
     * cannot use the dedicated challenge rooms' transition-marker checks. */
    anchor_minigame_yae_direct_load_cancel();
    returning = request_saved_source_return();
    /* Completion may precede the first successful invitation announcement.
     * Retire the actual bridge visit even if no entered edge was committed. */
    reply = anchor_update_minigame(0, 0, "null");
    if (reply) recomp_free(reply);
    anchor_minigame_sync_begin_load();
    s_yae_active = s_yae_requested = s_yae_transfer = 0;
    s_yae_request_origin = s_yae_transfer_origin = s_yae_cancelled = 0;
    s_yae_host.cid=s_yae_host.session=s_yae_host.visit=0;
    s_yae_request_visit = s_yae_wait_frames = 0;
    s_announced_visit = s_attempted = 0;
    if (!returning) {
        s_source_valid = 0;
        s_source_mini_visit = 0;
        if (s_saved_character >= 0) {
            s_restore_character = s_saved_character;
            s_saved_character = -1;
            s_restore_pending = s_restore_metadata_pending = 1;
        }
    }
}

static void clear_invitation(void)
{
    s_invitation.cid = 0;
    s_invitation.session = 0;
    s_invitation.sequence = 0;
    s_invitation.game = 0;
    s_invitation.game_visit = 0;
    s_invitation.joining = 0;
    s_invitation.local_room = 0;
    s_invitation.local_visit = 0;
}

static void dismiss_invitation(void)
{
    if (s_join_request.owner == JOIN_REQUEST_INVITE &&
        s_join_request.cid == s_invitation.cid &&
        s_join_request.session == s_invitation.session)
        clear_join_request();
    anchor_dismiss_minigame_invitation(s_invitation.cid, s_invitation.session,
                                       s_invitation.sequence);
    clear_invitation();
}

RECOMP_HOOK("func_8020D6BC_5C8B8C")
void anchor_minigame_invites_begin_load(void)
{
    yae_request_trace("load-begin",s_yae_transfer_origin ? s_yae_transfer_origin : s_yae_request_origin);
    if (s_return.phase == 2) clear_return();
    if (s_saved_character < 0)
    {
        unsigned int current = D_8015C5D8_15D1D8[1] & 0xffu;
        s_preload_character = current < 4u ? (int)current : -1;
    }
    s_room_loaded = 0;
    s_announced_visit = 0;
    s_attempted = 0;
}

RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")
void anchor_minigame_invites_finish_load(void)
{
    unsigned int previous_visit = s_visit;
    int previous_origin = s_yae_transfer_origin ? s_yae_transfer_origin : s_yae_request_origin;
    clear_join_request();
    s_loaded_room = D_800C7AB2;
    s_visit = s_visit == 0x7fffffffu ? 1u : s_visit + 1u;
    s_room_loaded = 1;
    s_announced_visit = 0;
    s_attempted = 0;
    s_yae_active = 0;
    s_yae_requested = s_loaded_room == 0x0154u && s_yae_transfer;
    s_yae_request_origin = s_yae_requested ? s_yae_transfer_origin : ANCHOR_YAE_LAUNCH_NONE;
    s_yae_transfer = s_yae_transfer_origin = s_yae_cancelled = 0;
    if(!s_yae_requested) {
        s_yae_host.cid=s_yae_host.session=s_yae_host.visit=0;
        anchor_minigame_yae_direct_load_cancel();
    }
    s_yae_request_visit = s_yae_requested ? s_visit : 0;
    s_yae_wait_frames = 0;
    if (s_return.phase == 1) {
        if (s_return.origin_visit == previous_visit &&
            s_return.room == s_loaded_room && !dedicated_room_game(s_loaded_room)) {
            s_return.phase = 2;s_return.arrival_visit = s_visit;
        } else clear_return();
    }
    if (game_for_room(s_loaded_room))
    {
        if (s_saved_character < 0)
            s_saved_character = s_restore_character >= 0
                ? s_restore_character : s_preload_character >= 0
                ? s_preload_character : (int)(D_8015C5D8_15D1D8[1] & 0xffu);
        s_restore_character = -1;
        s_restore_pending = 0;
        s_restore_metadata_pending = 0;
        /* Full loads create the real player after this resource boundary.
         * Seed its native constructor with the challenge character while the
         * preserved original remains available for the ordinary-room return.
         * In-place loads still reconcile the retained task on a safe frame. */
        D_8015C5D8_15D1D8[1] = (unsigned int)character_for_game(game_for_room(s_loaded_room));
        D_8015C5D8_15D1D8[0x2c / 4] = 1;
    }
    else if (s_saved_character >= 0)
    {
        s_restore_character = s_saved_character;
        s_saved_character = -1;
        s_restore_pending = 1;
        s_restore_metadata_pending = 1;
        /* This resource boundary precedes the new room's player spawn. The
         * constructor copies this runtime ID into its new task. */
        D_8015C5D8_15D1D8[1] = (unsigned int)s_restore_character;
        D_8015C5D8_15D1D8[0x2c / 4] = 1;
    }
    if (!game_for_room(s_loaded_room))
        s_source_valid = 0;
    else if (s_source_valid) {
        s_source_mini_visit = s_visit;s_source_mini_room = s_loaded_room;
    }
    s_preload_character = -1;
    yae_request_trace("load-finish",previous_origin);
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_minigame_invites_update(void)
{
    char *json;
    char name[257];
    const char *label;
    int cid, session, sequence, game, game_visit, local_game;

    if (anchor_minigame_invites_yae_requested() &&
        ++s_yae_wait_frames > 600u &&
        anchor_boss_invite_world_can_prompt() && !anchor_dialog_busy()) {
        yae_request_trace("timeout",s_yae_request_origin);
        anchor_minigame_invites_yae_cancel_request();
    }
    update_local_character();
    local_game = ANCHOR_MINIGAME_NONE;
    if (s_room_loaded && s_loaded_room == D_800C7AB2 &&
        game_for_room(s_loaded_room) &&
        (s_loaded_room != 0x0154u || s_yae_active) &&
        anchor_boss_invite_world_loaded_player_present())
    {
        int required = anchor_minigame_invites_required_character();
        if (character_settled(required))
            s_announced_visit = 1;
        if (s_announced_visit)
            local_game = game_for_room(s_loaded_room);
    }
    else
        s_announced_visit = 0;
    if (!anchor_is_connected() || anchor_is_disabled() ||
        !item_sync_save_is_loaded())
    {
        clear_join_request();
        local_game = ANCHOR_MINIGAME_NONE;
        if (s_invitation.cid)
        {
            anchor_dialog_cancel_for(ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
            (void)anchor_dialog_poll_for(ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
            dismiss_invitation();
        }
    }
    /* The Python transport retries an unsent edge and only accepts entries
     * once its local room metadata agrees with this loaded native room. */
    anchor_update_minigame_active(local_game, local_game ? (int)s_visit : 0);
    if (!local_game && (!anchor_is_connected() || anchor_is_disabled() ||
                        !item_sync_save_is_loaded()))
        return;

    if (s_invitation.cid)
    {
        if (!s_room_loaded || s_loaded_room != D_800C7AB2 ||
            s_invitation.local_room != D_800C7AB2 ||
            s_invitation.local_visit != s_visit ||
            !anchor_minigame_invitation_is_current(s_invitation.cid,
                                                    s_invitation.session,
                                                    s_invitation.sequence))
        {
            anchor_dialog_cancel_for(ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
            (void)anchor_dialog_poll_for(ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
            dismiss_invitation();
            return;
        }
        if (!s_invitation.joining)
        {
            AnchorDialogResult result =
                anchor_dialog_poll_for(ANCHOR_DIALOG_OWNER_MINIGAME_INVITE);
            if (result == ANCHOR_DIALOG_PENDING)
                return;
            if (result != ANCHOR_DIALOG_YES)
            {
                dismiss_invitation();
                return;
            }
            s_invitation.joining = 1;
        }
        /* Transfer waits for the native dialog and world control to release.
         * Room and coordinates are local constants, never packet fields. */
        if (stage_join_request(JOIN_REQUEST_INVITE, s_invitation.game,
                               s_invitation.cid, s_invitation.session,
                               s_invitation.game_visit) &&
            transfer_to_game(s_invitation.game))
            dismiss_invitation();
        return;
    }

    if (!anchor_boss_invite_world_can_prompt() || anchor_dialog_busy() ||
        anchor_boss_invites_active() || anchor_castle_return_sign_pending())
        return;
    json = anchor_get_minigame_invitation_json();
    if (!json)
        return;
    game_visit = 0;
    if (mnsg_json_get_s32(json, "cid", &cid) && cid > 0 &&
        mnsg_json_get_s32(json, "session", &session) && session > 0 &&
        mnsg_json_get_s32(json, "seq", &sequence) && sequence > 0 &&
        mnsg_json_get_s32(json, "game", &game) &&
        (label = game_name(game)) != 0 &&
        mnsg_json_get_string(json, "name", name, sizeof(name)))
    {
        if(game==ANCHOR_MINIGAME_YAE &&
           (!mnsg_json_get_s32(json,"gameVisit",&game_visit) || game_visit<=0)) {
            anchor_dismiss_minigame_invitation(cid,session,sequence);
            recomp_free(json);return;
        }
        if (!anchor_dialog_begin_minigame(name, label)) {
            recomp_free(json);
            return;
        }
        s_invitation.cid = cid;
        s_invitation.session = session;
        s_invitation.sequence = sequence;
        s_invitation.game = game;
        if (!mnsg_json_get_s32(json, "gameVisit", &game_visit) ||
            game_visit < 0)
            game_visit = 0;
        s_invitation.game_visit = game_visit;
        s_invitation.joining = 0;
        s_invitation.local_room = D_800C7AB2;
        s_invitation.local_visit = s_visit;
    }
    recomp_free(json);
}
