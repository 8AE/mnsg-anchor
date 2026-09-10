#ifdef ANCHOR_BOSS_INVITES_HOST_TEST
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern int anchor_update_boss_arena(int arena, int visit, int stage,
                                    unsigned int field90, unsigned int field91);
extern char *anchor_get_boss_invitation_json(void);
extern int anchor_boss_invitation_is_current(int cid, int session, int seq);
extern void anchor_dismiss_boss_invitation(int cid, int session, int seq);
extern void recomp_free(void *memory);
#else
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
extern unsigned short D_800C7AB2;
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
    int arena;
    unsigned int stage;
    unsigned int field90;
    unsigned int field91;
    int joining;
} ArenaInvitation;

static ArenaInvitation s_invitation;

static void clear_invitation(void)
{
    s_invitation.cid = 0;
    s_invitation.session = 0;
    s_invitation.sequence = 0;
    s_invitation.arena = 0;
    s_invitation.stage = 0;
    s_invitation.field90 = 0;
    s_invitation.field91 = 0;
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
    const char *arena_name;
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
        anchor_update_boss_arena(0, 0, 0, 0, 0);
        return;
    }

    {
        int arena = anchor_boss_invite_world_arena();
        unsigned int stage = anchor_boss_invite_world_stage();
        unsigned int field90 = anchor_boss_invite_world_field90();
        unsigned int field91 = anchor_boss_invite_world_field91();
#ifndef ANCHOR_BOSS_INVITES_HOST_TEST
        static int s_debug_arena = -1;
        static unsigned int s_debug_stage = 0xffffffffu;
        static unsigned int s_debug_f90 = 0xffffffffu;
        static unsigned int s_debug_f91 = 0xffffffffu;
        static unsigned short s_debug_room = 0xffffu;
        unsigned short room = D_800C7AB2;
        if (arena != s_debug_arena || stage != s_debug_stage ||
            field90 != s_debug_f90 || field91 != s_debug_f91 ||
            room != s_debug_room)
        {
            s_debug_arena = arena;
            s_debug_stage = stage;
            s_debug_f90 = field90;
            s_debug_f91 = field91;
            s_debug_room = room;
            recomp_printf("[BossInvite] room=0x%X arena=%d stage=0x%X f90=0x%X f91=0x%X\n",
                          (unsigned int)room, arena, stage, field90, field91);
        }
#endif
        anchor_update_boss_arena(arena, (int)anchor_boss_invite_world_visit(),
                                 (int)stage, field90, field91);
    }

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
        if (s_invitation.arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST &&
            s_invitation.arena <= ANCHOR_BOSS_ARENA_IMPACT_LAST &&
            s_invitation.stage)
        {
            if (anchor_boss_invite_world_warp_stage(s_invitation.stage,
                                                    s_invitation.field90,
                                                    s_invitation.field91))
                dismiss_invitation();
        }
        else if (anchor_boss_invite_world_warp(s_invitation.arena))
        {
            dismiss_invitation();
        }
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
        (arena_name = anchor_boss_arena_name(arena)) != 0 &&
        mnsg_json_get_string(json, "name", name, sizeof(name)) &&
        anchor_dialog_begin(name, arena_name))
    {
        s_invitation.cid = cid;
        s_invitation.session = session;
        s_invitation.sequence = sequence;
        s_invitation.arena = arena;
        s_invitation.stage = 0;
        s_invitation.field90 = 0;
        s_invitation.field91 = 0;
        s_invitation.joining = 0;
        if (arena >= ANCHOR_BOSS_ARENA_IMPACT_FIRST &&
            arena <= ANCHOR_BOSS_ARENA_IMPACT_LAST)
        {
            int room = anchor_boss_arena_room(arena);
            int stage, field;
            /* Default to the arena's boss stage when a legacy peer omits it. */
            s_invitation.stage = room > 0 ? (unsigned int)room : 0;
            if (mnsg_json_get_s32(json, "stage", &stage) &&
                ANCHOR_BOSS_IMPACT_STAGE_VALID(stage))
                s_invitation.stage = (unsigned int)stage;
            if (mnsg_json_get_s32(json, "f90", &field) && field >= 0)
                s_invitation.field90 = (unsigned int)field & 0xffffu;
            if (mnsg_json_get_s32(json, "f91", &field) && field >= 0)
                s_invitation.field91 = (unsigned int)field;
        }
    }
    recomp_free(json);
}
