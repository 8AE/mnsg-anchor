#ifdef ANCHOR_IMPACT_SYNC_HOST_TEST
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern char *anchor_impact_update(int, unsigned int, unsigned int, unsigned int,
                                  int, const char *);
extern int anchor_send_impact_hit(int, int);
extern void recomp_free(void *);
#else
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#endif

#include "anchor_impact_sync.h"
#include "anchor_impact_native.h"
#include "anchor_impact_damage.h"
#include "anchor_dialog.h"
#include "anchor_player_models.h"
#include "item_sync.h"
#include "utils/anchor_impact_codec.h"

extern unsigned short D_800C7AB2;
extern unsigned char *D_8015C5C8_15D1C8;

static AnchorImpactStatus s_status;
static AnchorImpactNativeSnapshot s_capture;
static char s_encoded[ANCHOR_IMPACT_STATE_JSON_SIZE];
static unsigned int s_encounter[3], s_term, s_revision, s_context, s_visit;
static int s_active, s_role, s_can_publish;
static AnchorImpactHit s_pending_hit;
static int s_pending_epoch;
static int s_logged_role = -1;
static int s_logged_active = -1;

static int local_world_paused(void)
{
    const unsigned char *system = D_8015C5C8_15D1C8;
    return anchor_dialog_world_paused() ||
           anchor_impact_native_world_paused() || !system ||
           (*(const volatile unsigned short *)(system + 0x3ae24) & 1u) ||
           *(const volatile unsigned short *)(system + 0x3ae26) != 0;
}

static void clear_context(void)
{
    s_encounter[0] = s_encounter[1] = s_encounter[2] = 0;
    s_term = s_revision = 0;
    s_role = s_can_publish = 0;
    s_pending_hit.sequence = 0;
    s_pending_epoch = 0;
    anchor_impact_native_set_role(0, 0, 0);
    anchor_impact_damage_set_context(0, 0, 0, 0);
}

static int encounter_changed(const AnchorImpactStatus *status)
{
    return status->encounter[0] != s_encounter[0] ||
           status->encounter[1] != s_encounter[1] ||
           status->encounter[2] != s_encounter[2];
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_impact_sync_frame(void)
{
    char *json;
    const char *state = "null";
    unsigned int stage, encounter, visit;
    int active, paused, ready, changed, needs_state, owner, epoch, i;

    anchor_impact_native_tick();
    encounter = anchor_impact_native_encounter();
    stage = anchor_impact_native_stage();
    /* The Impact battle can be entered from the title-menu boss rush without a
     * loaded save, so this encounter does not require one. */
    active = anchor_is_connected() && !anchor_is_disabled() &&
             stage != 0 && encounter != 0 &&
             anchor_impact_native_snapshot_ready();
#ifndef ANCHOR_IMPACT_SYNC_HOST_TEST
    if (active != s_logged_active)
    {
        s_logged_active = active;
        recomp_printf("[Impact] sync active=%d encounter=%u stage=%u visit=%u\n",
                      active, encounter, stage,
                      (unsigned int)anchor_impact_native_visit());
    }
#endif
    if (!active)
    {
        if (s_active)
        {
            json = anchor_impact_update(0, stage, encounter, 0, 0, "null");
            if (json)
                recomp_free(json);
        }
        clear_context();
        s_visit = 0;
        s_active = 0;
        return;
    }
    s_active = 1;
    paused = local_world_paused();
    visit = anchor_impact_native_visit();
    if (visit != s_visit)
    {
        clear_context();
        s_visit = visit;
    }
    epoch = anchor_player_models_get_epoch();
    if (s_pending_hit.sequence && s_pending_epoch != epoch)
        s_pending_hit.sequence = 0;

    if (!paused && s_role)
    {
        if (!s_pending_hit.sequence)
        {
            if (anchor_impact_damage_take_local_hit(&s_pending_hit))
                s_pending_epoch = epoch;
        }
        if (s_pending_hit.sequence &&
            anchor_send_impact_hit(s_pending_hit.sequence, s_pending_hit.amount))
            s_pending_hit.sequence = 0;
    }
    {
        int captured = anchor_impact_native_capture(&s_capture);
        int encoded = captured
            ? anchor_impact_state_encode(&s_capture, s_encoded,
                                         (unsigned int)sizeof(s_encoded))
            : 0;
        if (captured && encoded)
            state = s_encoded;
#ifndef ANCHOR_IMPACT_SYNC_HOST_TEST
        {
            static unsigned int s_caplog;
            if ((++s_caplog % 60u) == 0u)
                recomp_printf("[Impact] cap=%d enc=%d hp=%d mhp=%d ammo=%d state=%u\n",
                              captured, encoded,
                              (int)s_capture.root[IMP_BOSS_HP],
                              (int)s_capture.root[IMP_MECH_HP],
                              (int)s_capture.root[IMP_AMMO],
                              state == s_encoded ? 1u : 0u);
        }
#endif
    }
    json = anchor_impact_update(1, stage, encounter, visit, paused, state);
    if (!json || !anchor_impact_status_decode(json, &s_status))
    {
        if (json)
            recomp_free(json);
        s_can_publish = 0;
        anchor_impact_native_set_role(1, 0, 1);
        anchor_impact_damage_set_context(1, 0, 1, s_context);
        return;
    }
    recomp_free(json);
#ifndef ANCHOR_IMPACT_SYNC_HOST_TEST
    if (s_status.role == 0)
    {
        static unsigned int s_dbg;
        if ((++s_dbg % 60u) == 0u)
        {
            char *dbg = anchor_impact_debug();
            if (dbg)
            {
                recomp_printf(dbg);
                recomp_free(dbg);
            }
        }
    }
#endif
    changed = encounter_changed(&s_status);
    owner = s_status.role == 1;
    needs_state = s_status.has_state &&
                  (changed || s_status.term != s_term ||
                   s_role != s_status.role ||
                   (!owner && s_status.revision != s_revision));
    if (needs_state &&
        (paused || !anchor_impact_native_apply(&s_status.state)))
    {
        s_can_publish = 0;
        anchor_impact_native_set_role(1, 0, 1);
        anchor_impact_damage_set_context(1, 0, 1, s_context);
        return;
    }
    if (changed)
    {
        for (i = 0; i < 3; ++i)
            s_encounter[i] = s_status.encounter[i];
        s_context = s_context == 0xffffffffu ? 1u : s_context + 1u;
        s_pending_hit.sequence = 0;
    }
    s_term = s_status.term;
    s_revision = s_status.revision;
#ifndef ANCHOR_IMPACT_SYNC_HOST_TEST
    if (s_status.role != s_logged_role)
    {
        s_logged_role = s_status.role;
        recomp_printf("[Impact] role=%d term=%u e=[%u,%u,%u]\n",
                      s_status.role, s_status.term,
                      s_status.encounter[0], s_status.encounter[1],
                      s_status.encounter[2]);
    }
#endif
    s_role = s_status.role;
    s_can_publish = owner;
    paused = paused || s_status.paused || !s_role;
    anchor_impact_native_set_role(1, owner, paused);
    anchor_impact_damage_set_context(1, owner, paused, s_context);

    if (!owner || paused)
        return;
    for (i = 0; i < s_status.hit_count; ++i)
        (void)anchor_impact_damage_apply(s_status.hits[i][4]);
}
