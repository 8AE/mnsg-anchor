#ifdef ANCHOR_CONTROL_MACHINE_SYNC_HOST_TEST
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern char *anchor_control_machine_update(int, unsigned int, int, const char *);
extern int anchor_send_control_machine_hit(int);
extern void recomp_free(void *);
#else
#include "platform/modding.h"
#include "platform/recomputils.h"
#include "core/anchor.h"
#endif

#include "bosses/control_machine/anchor_control_machine_sync.h"
#include "bosses/control_machine/anchor_control_machine_native.h"
#include "bosses/control_machine/anchor_control_machine_codec.h"
#include "bosses/control_machine/anchor_control_machine_intro.h"
#include "bosses/anchor_boss_invite_world.h"
#include "bosses/anchor_boss_arenas.h"
#include "bosses/boss_sync.h"
#include "core/anchor_dialog.h"
#include "player/anchor_player_models.h"
#include "progression/item_sync.h"

extern unsigned short D_800C7AB2;
extern void *D_8015C5C8_15D1C8;

static AnchorControlMachineStatus s_status;
static AnchorControlMachineSnapshot s_capture;
static char s_encoded[ANCHOR_CONTROL_MACHINE_STATE_JSON_SIZE];
static unsigned int s_encounter[3], s_term, s_revision, s_visit, s_context;
static int s_active, s_role, s_can_publish, s_had_preview;
static int s_has_checkpoint;
static int s_pause_recovery;
static int s_pending_local_sequence, s_pending_local_epoch;
static int s_player_epoch;
static unsigned int s_remote_pending_frames;

#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED && \
    !defined(ANCHOR_CONTROL_MACHINE_SYNC_HOST_TEST)
typedef struct ControlMachineSyncDebug {
    unsigned int ticks, reported;
    unsigned int captures, capture_failures, encode_failures, blocked;
    unsigned int decode_failures, stage_deferred, stage_failures;
    unsigned int phase, timer, hp, status, x, y, z, shots;
} ControlMachineSyncDebug;
static ControlMachineSyncDebug s_debug_sync;

static void sync_debug_reset(void)
{
    volatile unsigned char *bytes = (volatile unsigned char *)&s_debug_sync;
    unsigned int i;
    for (i = 0; i < sizeof(s_debug_sync); ++i) bytes[i] = 0;
}

static void debug_capture(const AnchorControlMachineSnapshot *snapshot)
{
    const unsigned int *r = snapshot->root;
    ++s_debug_sync.captures;
    s_debug_sync.phase = r[CM_PHASE];
    s_debug_sync.timer = r[CM_TIMER];
    s_debug_sync.hp = r[CM_HP];
    s_debug_sync.status = r[CM_STATUS];
    s_debug_sync.x = r[CM_X];
    s_debug_sync.y = r[CM_Y];
    s_debug_sync.z = r[CM_Z];
    s_debug_sync.shots = snapshot->projectile_count;
}

#define DEBUG_CAPTURE(snapshot) debug_capture(snapshot)
#define DEBUG_COUNT(field) (++s_debug_sync.field)
#define DEBUG_TICK() (++s_debug_sync.ticks)
#define DEBUG_RESET() sync_debug_reset()
static void sync_diagnostic(unsigned int visit, int ready, int paused,
                            int offer, int role, int state, int preview)
{
    AnchorControlMachineNativeDebug native;
    const unsigned int *received;
    unsigned int source_kind, phase, timer, hp, status, x, y, z, shots;
    if (!s_debug_sync.ticks || s_debug_sync.ticks % 120u ||
        s_debug_sync.reported == s_debug_sync.ticks) return;
    s_debug_sync.reported = s_debug_sync.ticks;
    source_kind = role == 2 && state && s_status.has_state;
    received = source_kind ? s_status.state.root : 0;
    phase = received ? received[CM_PHASE] : s_debug_sync.phase;
    timer = received ? received[CM_TIMER] : s_debug_sync.timer;
    hp = received ? received[CM_HP] : s_debug_sync.hp;
    status = received ? received[CM_STATUS] : s_debug_sync.status;
    x = received ? received[CM_X] : s_debug_sync.x;
    y = received ? received[CM_Y] : s_debug_sync.y;
    z = received ? received[CM_Z] : s_debug_sync.z;
    shots = received ? s_status.state.projectile_count : s_debug_sync.shots;
    anchor_control_machine_native_debug_take(&native);
    recomp_printf("[CM] sample v=%u tick=%u ready=%d pause=%d role=%d owner=%d rev=%u state=%d preview=%d offer=%d pending=%d age=%u cap=%u/%u/%u block=%u decode=%u stage=%u/%u applied=%u\n",
                  visit, s_debug_sync.ticks, ready, paused, role,
                  role < 0 ? 0 : s_status.owner, s_status.revision,
                  state, preview, offer,
                  anchor_control_machine_native_pending(), native.pending_age,
                  s_debug_sync.captures, s_debug_sync.capture_failures,
                  s_debug_sync.encode_failures, s_debug_sync.blocked,
                  s_debug_sync.decode_failures, s_debug_sync.stage_deferred,
                  s_debug_sync.stage_failures, native.applied_count);
    recomp_printf("[CM] pose srcKind=%u src=%u/%u/%u/%u xyz=%08x,%08x,%08x shots=%u dst=%u/%u/%u/%u xyz=%08x,%08x,%08x shots=%u fail=ready:%u model:%u valid:%u proj:%u hold:%u release:%u rewind:%u\n",
                  source_kind, phase, timer, hp, status, x, y, z, shots,
                  native.phase, native.timer, native.hp, native.status,
                  native.x, native.y, native.z, native.shots,
                  native.failure[CM_DEBUG_FAIL_READY],
                  native.failure[CM_DEBUG_FAIL_MODEL],
                  native.failure[CM_DEBUG_FAIL_VALID],
                  native.failure[CM_DEBUG_FAIL_PROJECTILES],
                  native.failure[CM_DEBUG_FAIL_HOLD],
                  native.failure[CM_DEBUG_FAIL_RELEASE],
                  native.failure[CM_DEBUG_FAIL_REWIND]);
    s_debug_sync.captures = s_debug_sync.capture_failures =
        s_debug_sync.encode_failures = s_debug_sync.blocked = 0;
    s_debug_sync.decode_failures = s_debug_sync.stage_deferred =
        s_debug_sync.stage_failures = 0;
}
#else
#define DEBUG_CAPTURE(snapshot) ((void)0)
#define DEBUG_COUNT(field) ((void)0)
#define DEBUG_TICK() ((void)0)
#define DEBUG_RESET() ((void)0)
#define sync_diagnostic(visit, ready, paused, offer, role, state, preview) \
    ((void)(offer))
#endif

static int local_world_paused(void)
{
    const unsigned char *system = D_8015C5C8_15D1C8;
    return anchor_dialog_world_paused() || !system ||
           (*(const volatile unsigned short *)(system + 0x3ae24) & 1u) ||
           *(const volatile unsigned short *)(system + 0x3ae26) != 0;
}

static void clear_context(void)
{
    s_encounter[0] = s_encounter[1] = s_encounter[2] = 0;
    s_term = s_revision = s_role = s_can_publish = s_has_checkpoint = 0;
    s_pause_recovery = 0;
    s_pending_local_sequence = s_pending_local_epoch = 0;
    s_player_epoch = 0;
    s_had_preview = 0;
    anchor_control_machine_native_discard_pending();
    anchor_control_machine_native_set_role(0, 0, 0);
    anchor_control_machine_native_clear_hits();
    anchor_control_machine_intro_preview(0, 0);
    anchor_control_machine_intro_owner_hint(0, 0);
}

static int encounter_changed(const AnchorControlMachineStatus *status)
{
    return status->encounter[0] != s_encounter[0] ||
           status->encounter[1] != s_encounter[1] ||
           status->encounter[2] != s_encounter[2];
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_control_machine_sync_frame(void)
{
    char *json;
    const char *state = "null";
    unsigned int visit;
    int active, ready, paused, changed, owner, needs_state, epoch, i;
    int initial_self_publish;
    int resume_recovery, force_recovery;
    int offer = 0;

    anchor_control_machine_native_tick();
    boss_sync_control_machine_native_complete();
    if (anchor_control_machine_native_terminal_fallback()) {
        if (s_active) {
            json = anchor_control_machine_update(0, 0, 0, "null");
            if (json) recomp_free(json);
            clear_context();
            s_active = 0;
            DEBUG_RESET();
        }
        return;
    }
    active = anchor_is_connected() && !anchor_is_disabled() &&
             item_sync_save_is_loaded() && D_800C7AB2 == 0x155u &&
             anchor_boss_invite_world_arena() ==
                 ANCHOR_BOSS_ARENA_CONTROL_MACHINE &&
             anchor_control_machine_native_bound();
    if (!active) {
        if (s_active) {
            json = anchor_control_machine_update(0, 0, 0, "null");
            if (json) recomp_free(json);
        }
        clear_context();
        s_visit = 0;
        s_active = 0;
        DEBUG_RESET();
        if (boss_sync_control_machine_remote_pending() &&
            anchor_control_machine_native_ready() &&
            ++s_remote_pending_frames >= 60u)
            (void)anchor_control_machine_native_request_terminal();
        else if (!boss_sync_control_machine_remote_pending() ||
                 !anchor_control_machine_native_ready())
            s_remote_pending_frames = 0;
        return;
    }
    visit = anchor_control_machine_native_visit();
    if (!s_active || visit != s_visit) {
        clear_context();
        DEBUG_RESET();
        s_visit = visit;
        s_remote_pending_frames = 0;
        ++s_context;
        if (!s_context) ++s_context;
    }
    s_active = 1;
    DEBUG_TICK();
    ready = anchor_control_machine_native_transport_ready();
    paused = local_world_paused();
    epoch = anchor_player_models_get_epoch();
    if (epoch != s_player_epoch) {
        s_pending_local_sequence = 0;
        anchor_control_machine_native_clear_local_hits();
        s_player_epoch = epoch;
    }
    /* A native contact is a one-shot event. If Python has not accepted its
     * sequence before the owner's vulnerable wait ends, it must not land as
     * delayed damage in a later wait cycle. */
    if (!ready || paused ||
        !anchor_control_machine_native_local_hit_window()) {
        s_pending_local_sequence = 0;
        s_pending_local_epoch = 0;
        anchor_control_machine_native_clear_local_hits();
    }

    if (!ready) {
        s_remote_pending_frames = 0;
        /* Actor-bound observers can see an incumbent fight before the local
         * camera/dialogue entrance. Python never elects or accepts their hits. */
        json = anchor_control_machine_update(0, visit, 1, "null");
        if (!json || !anchor_control_machine_status_decode(json, &s_status)) {
            DEBUG_COUNT(decode_failures);
            if (json) recomp_free(json);
            anchor_control_machine_intro_preview(0, 0);
            anchor_control_machine_intro_owner_hint(0, 0);
            anchor_control_machine_native_set_role(0, 0, 0);
            sync_diagnostic(visit, 0, 1, 0, -1, 0, 0);
            return;
        }
        recomp_free(json);
        s_had_preview = s_status.has_preview;
        anchor_control_machine_intro_owner_hint(visit,
                                                  (unsigned int)s_status.owner);
        anchor_control_machine_intro_preview(visit, s_status.has_preview);
        anchor_control_machine_native_set_role(0, 0, 0);
        sync_diagnostic(visit, 0, 1, 0, s_status.role,
                        s_status.has_state, s_status.has_preview);
        return;
    }
    anchor_control_machine_intro_preview(0, 0);
    anchor_control_machine_intro_owner_hint(0, 0);
    if (!paused && s_role &&
        anchor_control_machine_native_local_hit_window()) {
        if (!s_pending_local_sequence &&
            anchor_control_machine_native_take_local_hit(
                &s_pending_local_sequence))
            s_pending_local_epoch = epoch;
        if (s_pending_local_sequence &&
            anchor_send_control_machine_hit(s_pending_local_sequence))
            s_pending_local_sequence = 0;
    }
    /* The first ready call after preview obtains and adopts the incumbent
     * checkpoint before this client offers its own pre-adoption state. */
    if (!s_had_preview && !s_pending_local_sequence &&
        !anchor_control_machine_native_hit_pending() &&
        (s_can_publish || (!s_encounter[0] && !s_role))) {
        offer = 2;
        if (anchor_control_machine_native_capture(&s_capture)) {
            offer = 3;
            if (anchor_control_machine_state_encode(&s_capture, s_encoded,
                                                    sizeof(s_encoded))) {
                state = s_encoded;
                offer = 4;
                DEBUG_CAPTURE(&s_capture);
            } else DEBUG_COUNT(encode_failures);
        } else DEBUG_COUNT(capture_failures);
    } else {
        offer = 1;
        DEBUG_COUNT(blocked);
    }
    json = anchor_control_machine_update(1, visit, paused, state);
    if (!json || !anchor_control_machine_status_decode(json, &s_status)) {
        DEBUG_COUNT(decode_failures);
        if (json) recomp_free(json);
        s_can_publish = 0;
        anchor_control_machine_native_set_role(s_has_checkpoint || s_role,
                                               0, 1);
        sync_diagnostic(visit, 1, paused, offer, -1, 0, 0);
        if (boss_sync_control_machine_remote_pending() &&
            ++s_remote_pending_frames >= 60u)
            (void)anchor_control_machine_native_request_terminal();
        return;
    }
    recomp_free(json);
    sync_diagnostic(visit, 1, paused, offer, s_status.role,
                    s_status.has_state, s_status.has_preview);
    if (!boss_sync_control_machine_remote_pending() ||
        (s_status.has_state && s_status.state.root[CM_PHASE] >= 6u))
        s_remote_pending_frames = 0;
    else if (++s_remote_pending_frames >= 60u &&
             anchor_control_machine_native_request_terminal())
        return;
    s_had_preview = 0;
    changed = encounter_changed(&s_status);
    owner = s_status.role == 1;
    initial_self_publish = owner && changed && s_status.has_state &&
        state == s_encoded && s_status.term == 1u &&
        s_status.encounter[0] == (unsigned int)s_status.owner &&
        s_status.encounter[2] == visit;
    if (paused || s_status.paused) s_pause_recovery = 1;
    resume_recovery = s_pause_recovery && !paused && !s_status.paused &&
                      s_role == 2 && s_status.has_state;
    if (changed) {
        /* This exact first election published our captured native state.
         * Preserve its observed command serial through the self response. */
        if (!initial_self_publish)
            anchor_control_machine_native_discard_pending();
        anchor_control_machine_native_clear_hits();
        s_has_checkpoint = 0;
        for (i = 0; i < 3; ++i) s_encounter[i] = s_status.encounter[i];
        ++s_context;
        if (!s_context) ++s_context;
        s_pending_local_sequence = 0;
    }
    /* The first elected owner already supplied this visit's native state.
     * Restaging that same state can hold its AI waiting for adoption. A
     * promoted owner of another encounter still adopts its checkpoint. */
    needs_state = s_status.has_state &&
                  (changed || s_status.term != s_term ||
                   s_role != s_status.role ||
                   resume_recovery ||
                   (!owner && s_status.revision != s_revision)) &&
                  !initial_self_publish;
    force_recovery = changed || s_status.term != s_term ||
                     s_role != s_status.role || resume_recovery;
    if (needs_state && (paused ||
        !anchor_control_machine_native_apply(&s_status.state,
                                             force_recovery))) {
        if (paused) DEBUG_COUNT(stage_deferred);
        else DEBUG_COUNT(stage_failures);
        s_can_publish = 0;
        anchor_control_machine_native_set_role(1, 0, 1);
        sync_diagnostic(visit, 1, paused, 5, s_status.role,
                        s_status.has_state, s_status.has_preview);
        return;
    }
    if (needs_state) s_has_checkpoint = 1;
    if ((needs_state && !paused && !s_status.paused) ||
        (owner && !paused && !s_status.paused))
        s_pause_recovery = 0;
    s_term = s_status.term;
    s_revision = s_status.revision;
    s_role = s_status.role;
    s_can_publish = owner;
    anchor_control_machine_native_set_role(
        owner || s_has_checkpoint, owner,
        paused || s_status.paused || !s_role);
    if (!owner || paused || s_status.paused) return;
    for (i = 0; i < s_status.hit_count; ++i)
        if (!anchor_control_machine_native_queue_hit(s_status.hits[i])) {
            s_can_publish = 0;
            anchor_control_machine_native_set_role(1, 1, 1);
            return;
        }
}
