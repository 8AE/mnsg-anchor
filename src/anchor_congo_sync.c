#ifdef ANCHOR_CONGO_SYNC_HOST_TEST
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern char *anchor_congo_update(int, unsigned int, int, const char *);
extern int anchor_send_congo_hit(int, int);
extern void recomp_free(void *);
#else
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#endif

#include "anchor_congo_sync.h"
#include "anchor_congo_native.h"
#include "anchor_congo_damage.h"
#include "anchor_dialog.h"
#include "anchor_player_models.h"
#include "item_sync.h"
#include "utils/anchor_congo_codec.h"

extern unsigned short D_800C7AB2;
extern void *D_8015C5C8_15D1C8;

static AnchorCongoStatus s_status;
static AnchorCongoNativeSnapshot s_capture;
static char s_encoded[ANCHOR_CONGO_STATE_JSON_SIZE];
static unsigned int s_encounter[3], s_term, s_revision, s_context, s_visit;
static unsigned int s_target_ticks;
static int s_active, s_role, s_target, s_can_publish;
static AnchorCongoHit s_pending_hit;
static int s_pending_epoch;

static int local_world_paused(void)
{
    const unsigned char *system = D_8015C5C8_15D1C8;
    return anchor_dialog_world_paused() || anchor_congo_native_world_paused() || !system ||
           (*(const volatile unsigned short *)(system + 0x3ae24) & 1u) ||
           *(const volatile unsigned short *)(system + 0x3ae26) != 0;
}

static void clear_context(void)
{
    s_encounter[0] = s_encounter[1] = s_encounter[2] = 0;
    s_term = s_revision = 0;
    s_role = s_target = s_can_publish = 0;
    s_target_ticks = 0;
    s_pending_hit.sequence = 0;
    s_pending_epoch = 0;
    anchor_congo_native_set_role(0, 0, 0);
    anchor_congo_damage_set_context(0, 0, 0, 0);
}

static int encounter_changed(const AnchorCongoStatus *status)
{
    return status->encounter[0] != s_encounter[0] ||
           status->encounter[1] != s_encounter[1] ||
           status->encounter[2] != s_encounter[2];
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_congo_sync_frame(void)
{
    AnchorBossTarget target;
    char *json;
    const char *state = "null";
    unsigned int visit;
    int active, paused, ready, changed, needs_state, owner, rotate, epoch, i;

    anchor_congo_native_tick();
    active = anchor_is_connected() && !anchor_is_disabled() &&
             item_sync_save_is_loaded() && D_800C7AB2 == 0x16 &&
             anchor_congo_native_root_task() != 0;
    if (!active)
    {
        if (s_active)
        {
            json = anchor_congo_update(0, 0, 0, "null");
            if (json) recomp_free(json);
        }
        clear_context();
        s_visit = 0;
        s_active = 0;
        return;
    }
    s_active = 1;
    paused = local_world_paused();
    ready = anchor_congo_native_ready();
    visit = anchor_congo_native_visit();
    if (visit != s_visit)
    {
        /* A native reload can reuse the same actor address and encounter.
         * Its fresh model still needs the current checkpoint applied. */
        clear_context();
        s_visit = visit;
    }
    epoch = anchor_player_models_get_epoch();
    if (s_pending_hit.sequence && s_pending_epoch != epoch)
        s_pending_hit.sequence = 0;

    /* A failed bridge send retains the one native intent. Python adds the
     * encounter/session identity and retries accepted intents independently. */
    if (!paused && s_role)
    {
        if (!s_pending_hit.sequence)
        {
            if (anchor_congo_damage_take_local_hit(&s_pending_hit))
                s_pending_epoch = epoch;
        }
        if (s_pending_hit.sequence &&
            anchor_send_congo_hit(s_pending_hit.sequence, s_pending_hit.amount))
            s_pending_hit.sequence = 0;
    }
    if (ready && (s_can_publish || (!s_encounter[0] && !s_role)) &&
        anchor_congo_native_capture(&s_capture) &&
        anchor_congo_state_encode(&s_capture, s_encoded, sizeof(s_encoded)))
        state = s_encoded;
    json = anchor_congo_update(ready, visit, paused, state);
    if (!json || !anchor_congo_status_decode(json, &s_status))
    {
        if (json) recomp_free(json);
        s_can_publish = 0;
        anchor_congo_native_set_role(1, 0, 1);
        anchor_congo_damage_set_context(1, 0, 1, s_context);
        return;
    }
    recomp_free(json);
    changed = encounter_changed(&s_status);
    owner = s_status.role == 1;
    needs_state = s_status.has_state &&
                  (changed || s_status.term != s_term || s_role != s_status.role ||
                   (!owner && s_status.revision != s_revision));
    /* A paused recipient retains the newest checkpoint in Python. Apply it
     * after native Start/flute/dialog ownership has released the world. */
    if (needs_state && (paused || !anchor_congo_native_apply(&s_status.state)))
    {
        s_can_publish = 0;
        anchor_congo_native_set_role(1, 0, 1);
        anchor_congo_damage_set_context(1, 0, 1, s_context);
        return;
    }
    if (changed)
    {
        for (i = 0; i < 3; ++i)
            s_encounter[i] = s_status.encounter[i];
        s_context = s_context == 0xffffffffu ? 1u : s_context + 1u;
        s_pending_hit.sequence = 0;
        s_target = 0;
        s_target_ticks = 0;
    }
    s_term = s_status.term;
    s_revision = s_status.revision;
    s_role = s_status.role;
    s_can_publish = owner;
    paused = paused || s_status.paused || !s_role;
    anchor_congo_native_set_role(1, owner, paused);
    anchor_congo_damage_set_context(1, owner, paused, s_context);

    if (!owner || paused)
        return;
    for (i = 0; i < s_status.hit_count; ++i)
        (void)anchor_congo_damage_apply(s_status.hits[i][4]);
    /* Pick a different participant between attacks. Keep tracking that
     * participant throughout windup/breath instead of changing aim mid-shot. */
    rotate = s_target_ticks >= 120u && s_capture.root[CONGO_PHASE] == 1u;
    if (anchor_player_models_get_boss_target(s_target, rotate, &target))
    {
        if (target.cid != s_target || rotate)
            s_target_ticks = 0;
        s_target = target.cid;
        anchor_congo_native_set_target(target.x, target.y, target.z);
        if (s_target_ticks < 120u)
            ++s_target_ticks;
    }
    else
    {
        s_target = 0;
        s_target_ticks = 0;
        anchor_congo_native_clear_target();
    }
}
