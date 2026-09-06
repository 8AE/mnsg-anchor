#ifdef ANCHOR_BOSS_INVITES_HOST_TEST
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern int anchor_update_boss_arena(int arena, int visit);
extern char *anchor_get_boss_invitation_json(void);
extern int anchor_boss_invitation_is_current(int cid, int session, int seq);
extern void anchor_dismiss_boss_invitation(int cid, int session, int seq);
extern void recomp_free(void *memory);
#else
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#endif

#include "anchor_boss_invites.h"
#include "anchor_boss_invite_world.h"
#include "anchor_dialog.h"
#include "item_sync.h"
#include "utils/json_utils.h"

typedef struct
{
    int cid;
    int session;
    int sequence;
    int joining;
} ArenaInvitation;

static ArenaInvitation s_invitation;

static void clear_invitation(void)
{
    s_invitation.cid = 0;
    s_invitation.session = 0;
    s_invitation.sequence = 0;
    s_invitation.joining = 0;
}

static void dismiss_invitation(void)
{
    anchor_dismiss_boss_invitation(s_invitation.cid, s_invitation.session,
                                   s_invitation.sequence);
    clear_invitation();
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_boss_invites_update(void)
{
    char *json;
    char name[257];
    int cid, session, sequence, arena;

    if (!anchor_is_connected() || anchor_is_disabled() ||
        !item_sync_save_is_loaded())
    {
        if (s_invitation.cid)
        {
            anchor_dialog_cancel();
            dismiss_invitation();
        }
        /* A loaded client that returns to file select emits an exit, even
         * when its cached movement room has not changed yet. */
        anchor_update_boss_arena(0, 0);
        return;
    }

    anchor_update_boss_arena(anchor_boss_invite_world_arena(),
                            (int)anchor_boss_invite_world_visit());

    if (s_invitation.cid)
    {
        if (!anchor_boss_invitation_is_current(s_invitation.cid,
                                               s_invitation.session,
                                               s_invitation.sequence))
        {
            anchor_dialog_cancel();
            dismiss_invitation();
            return;
        }

        if (!s_invitation.joining)
        {
            AnchorDialogResult result = anchor_dialog_poll();
            if (result == ANCHOR_DIALOG_PENDING)
                return;
            if (result != ANCHOR_DIALOG_YES)
            {
                dismiss_invitation();
                return;
            }
            s_invitation.joining = 1;
        }

        /* The native dialog must finish releasing its world pause before
         * changing engine steps. If a pause intervenes, retain the accepted
         * request and recheck the sender until normal gameplay resumes. */
        if (anchor_boss_invite_world_warp())
            dismiss_invitation();
        return;
    }

    if (!anchor_boss_invite_world_can_prompt() || anchor_dialog_busy())
        return;
    json = anchor_get_boss_invitation_json();
    if (!json)
        return;
    if (mnsg_json_get_s32(json, "cid", &cid) && cid > 0 &&
        mnsg_json_get_s32(json, "session", &session) && session > 0 &&
        mnsg_json_get_s32(json, "seq", &sequence) && sequence > 0 &&
        mnsg_json_get_s32(json, "arena", &arena) &&
        arena == ANCHOR_BOSS_ARENA_CONGO &&
        mnsg_json_get_string(json, "name", name, sizeof(name)) &&
        anchor_dialog_begin(name, "Congo's Arena"))
    {
        s_invitation.cid = cid;
        s_invitation.session = session;
        s_invitation.sequence = sequence;
        s_invitation.joining = 0;
    }
    recomp_free(json);
}
