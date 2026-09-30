#ifdef ANCHOR_MINIGAME_INVITES_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern int anchor_update_minigame_active(int game, int visit);
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

extern unsigned short D_800C7AB2;
extern unsigned int D_8015C5D8_15D1D8[];
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern void func_801DD5C0_5994D0(void *task, unsigned char character);

typedef struct {
    int cid;
    int session;
    int sequence;
    int game;
    int joining;
    unsigned short local_room;
    unsigned int local_visit;
} MinigameInvitation;

static MinigameInvitation s_invitation;
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

int anchor_minigame_invites_active(void)
{
    return s_invitation.cid != 0;
}

static int game_for_room(unsigned short room)
{
    switch (room) {
    case 0x01e0: return ANCHOR_MINIGAME_GOEMON;
    case 0x01e1: return ANCHOR_MINIGAME_EBISUMARU;
    case 0x01e2: return ANCHOR_MINIGAME_SASUKE;
    default: return ANCHOR_MINIGAME_NONE;
    }
}

static int character_for_game(int game)
{
    switch (game) {
    case ANCHOR_MINIGAME_GOEMON: return 0;
    case ANCHOR_MINIGAME_EBISUMARU: return 1;
    case ANCHOR_MINIGAME_SASUKE: return 2;
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
    default: return 0;
    }
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
    default:
        return 0;
    }
}

static void clear_invitation(void)
{
    s_invitation.cid = 0;
    s_invitation.session = 0;
    s_invitation.sequence = 0;
    s_invitation.game = 0;
    s_invitation.joining = 0;
    s_invitation.local_room = 0;
    s_invitation.local_visit = 0;
}

static void dismiss_invitation(void)
{
    anchor_dismiss_minigame_invitation(s_invitation.cid, s_invitation.session,
                                       s_invitation.sequence);
    clear_invitation();
}

RECOMP_HOOK("func_8020D6BC_5C8B8C")
void anchor_minigame_invites_begin_load(void)
{
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
    s_loaded_room = D_800C7AB2;
    s_visit = s_visit == 0x7fffffffu ? 1u : s_visit + 1u;
    s_room_loaded = 1;
    s_announced_visit = 0;
    s_attempted = 0;
    if (game_for_room(s_loaded_room))
    {
        if (s_saved_character < 0)
            s_saved_character = s_restore_character >= 0
                ? s_restore_character : s_preload_character >= 0
                ? s_preload_character : (int)(D_8015C5D8_15D1D8[1] & 0xffu);
        s_restore_character = -1;
        s_restore_pending = 0;
        s_restore_metadata_pending = 0;
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
    s_preload_character = -1;
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_minigame_invites_update(void)
{
    char *json;
    char name[257];
    const char *label;
    int cid, session, sequence, game, local_game;

    update_local_character();
    local_game = ANCHOR_MINIGAME_NONE;
    if (s_room_loaded && s_loaded_room == D_800C7AB2 &&
        game_for_room(s_loaded_room) &&
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
        if (transfer_to_game(s_invitation.game))
            dismiss_invitation();
        return;
    }

    if (!anchor_boss_invite_world_can_prompt() || anchor_dialog_busy() ||
        anchor_boss_invites_active() || anchor_castle_return_sign_pending())
        return;
    json = anchor_get_minigame_invitation_json();
    if (!json)
        return;
    if (mnsg_json_get_s32(json, "cid", &cid) && cid > 0 &&
        mnsg_json_get_s32(json, "session", &session) && session > 0 &&
        mnsg_json_get_s32(json, "seq", &sequence) && sequence > 0 &&
        mnsg_json_get_s32(json, "game", &game) &&
        (label = game_name(game)) != 0 &&
        mnsg_json_get_string(json, "name", name, sizeof(name)) &&
        anchor_dialog_begin_minigame(name, label))
    {
        s_invitation.cid = cid;
        s_invitation.session = session;
        s_invitation.sequence = sequence;
        s_invitation.game = game;
        s_invitation.joining = 0;
        s_invitation.local_room = D_800C7AB2;
        s_invitation.local_visit = s_visit;
    }
    recomp_free(json);
}
