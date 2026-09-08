#ifdef ANCHOR_DHARUMANYO_SYNC_HOST_TEST
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern char *anchor_dharumanyo_update(int, unsigned int, int, const char *);
extern int anchor_send_dharumanyo_hit(int);
extern void recomp_free(void *);
#else
#include "modding.h"
#include "recomputils.h"
#include "anchor.h"
#endif

#include "anchor_dharumanyo_sync.h"
#include "anchor_dharumanyo_native.h"
#include "anchor_dharumanyo_damage.h"
#include "anchor_dialog.h"
#include "anchor_player_models.h"
#include "item_sync.h"
#include "utils/anchor_dharumanyo_codec.h"

extern unsigned short D_800C7AB2;
extern void *D_8015C5C8_15D1C8;

#define DHARUMANYO_ROOM 0x49u
#define DHARUMANYO_NEUTRAL_PHASE 1u

static AnchorDharumanyoStatus s_status;
static AnchorDharumanyoNativeSnapshot s_capture;
static char s_encoded[ANCHOR_DHARUMANYO_STATE_JSON_SIZE];
static unsigned int s_encounter[3], s_term, s_revision, s_context, s_visit;
static unsigned int s_target_ticks;
static int s_active, s_role, s_target, s_can_publish;
static AnchorDharumanyoHit s_pending_hit;
static int s_pending_epoch;

static int local_world_paused(void)
{
    const unsigned char *system = D_8015C5C8_15D1C8;
    return anchor_dialog_world_paused() ||
           anchor_dharumanyo_native_world_paused() || !system ||
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
    anchor_dharumanyo_native_set_role(0, 0, 0);
    anchor_dharumanyo_damage_set_context(0, 0, 0, 0);
}

static int encounter_changed(const AnchorDharumanyoStatus *status)
{
    return status->encounter[0] != s_encounter[0] ||
           status->encounter[1] != s_encounter[1] ||
           status->encounter[2] != s_encounter[2];
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_dharumanyo_sync_frame(void)
{
    AnchorBossTarget target;
    char *json;
    const char *state = "null";
    unsigned int visit;
    int active, paused, ready, changed, needs_state, owner, rotate, epoch, i;

    anchor_dharumanyo_native_tick();
    active = anchor_is_connected() && !anchor_is_disabled() &&
             item_sync_save_is_loaded() && D_800C7AB2 == DHARUMANYO_ROOM &&
             anchor_dharumanyo_native_snapshot_ready();
    if (!active)
    {
        if (s_active)
        {
            json = anchor_dharumanyo_update(0, 0, 0, "null");
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
    ready = anchor_dharumanyo_native_snapshot_ready();
    visit = anchor_dharumanyo_native_visit();
    if (visit != s_visit)
    {
        /* A fresh native actor can reuse the same task address. Force the
         * current checkpoint through the new room-visit identity. */
        clear_context();
        s_visit = visit;
    }
    epoch = anchor_player_models_get_epoch();
    if (s_pending_hit.sequence && s_pending_epoch != epoch)
        s_pending_hit.sequence = 0;

    /* Python attaches encounter/session identity and retries this one local
     * physical hit until it is acknowledged by the elected simulator. */
    if (!paused && s_role)
    {
        if (!s_pending_hit.sequence)
        {
            if (anchor_dharumanyo_damage_take_local_hit(&s_pending_hit))
                s_pending_epoch = epoch;
        }
        if (s_pending_hit.sequence &&
            anchor_send_dharumanyo_hit(s_pending_hit.sequence))
            s_pending_hit.sequence = 0;
    }
    if (ready && (s_can_publish || (!s_encounter[0] && !s_role)) &&
        anchor_dharumanyo_native_capture(&s_capture) &&
        anchor_dharumanyo_state_encode(&s_capture, s_encoded,
                                       (unsigned int)sizeof(s_encoded)))
        state = s_encoded;
    json = anchor_dharumanyo_update(ready, visit, paused, state);
    if (!json || !anchor_dharumanyo_status_decode(json, &s_status))
    {
        if (json)
            recomp_free(json);
        s_can_publish = 0;
        anchor_dharumanyo_native_set_role(1, 0, 1);
        anchor_dharumanyo_damage_set_context(1, 0, 1, s_context);
        return;
    }
    recomp_free(json);
    changed = encounter_changed(&s_status);
    owner = s_status.role == 1;
    needs_state = s_status.has_state &&
                  (changed || s_status.term != s_term ||
                   s_role != s_status.role ||
                   (!owner && s_status.revision != s_revision));
    /* A paused client retains the newest Python checkpoint and adopts it
     * only after Start, flute, invitation or native script ownership ends. */
    if (needs_state &&
        (paused || !anchor_dharumanyo_native_apply(&s_status.state)))
    {
        s_can_publish = 0;
        anchor_dharumanyo_native_set_role(1, 0, 1);
        anchor_dharumanyo_damage_set_context(1, 0, 1, s_context);
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
    anchor_dharumanyo_native_set_role(1, owner, paused);
    anchor_dharumanyo_damage_set_context(1, owner, paused, s_context);

    if (!owner || paused)
        return;
    for (i = 0; i < s_status.hit_count; ++i)
        (void)anchor_dharumanyo_damage_apply(s_status.hits[i][4]);

    /* Keep one participant selected throughout an attack, then allow the
     * owner to rotate between the native neutral callbacks. */
    rotate = s_target_ticks >= 120u &&
             s_capture.root[DHAR_PHASE] == DHARUMANYO_NEUTRAL_PHASE;
    if (anchor_player_models_get_boss_target(s_target, rotate, &target))
    {
        if (target.cid != s_target || rotate)
            s_target_ticks = 0;
        s_target = target.cid;
        anchor_dharumanyo_native_set_target(target.x, target.y, target.z);
        if (s_target_ticks < 120u)
            ++s_target_ticks;
    }
    else
    {
        s_target = 0;
        s_target_ticks = 0;
        anchor_dharumanyo_native_clear_target();
    }
}
