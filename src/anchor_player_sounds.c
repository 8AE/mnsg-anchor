/**
 * @file anchor_player_sounds.c
 * @brief Capture and spatially replay remote-player one-shot sound effects.
 *
 * The game's final sound-command function is shared by music, UI, actors and
 * player actions. Capture is therefore restricted to commands queued while
 * the scheduler is executing the real local-player task or one of its
 * directly owned native effect tasks. Socket/Python work is deferred to the
 * frame-end update instead of running inside the hook.
 */

#ifndef ANCHOR_PLAYER_SOUNDS_HOST_TEST
#include "modding.h"
#include "anchor.h"
#include "anchor_player_models.h"
#endif
#include "anchor_player_sounds.h"

#define PLAYER_SOUND_BATCH_MAX 8
#define REMOTE_SOUND_DRAIN_MAX 4
#define REMOTE_SOUND_DEFER_MAX 64
#define REMOTE_SOUND_MAX_REMAINING_MS 500u
/* libultra osGetTime ticks at OS_CLOCK_RATE * 3 / 4 = 46.875 MHz. */
#define N64_COUNTER_CYCLES_PER_MS 46875ull
#define REMOTE_SOUND_RADIUS 400.0f
#define LOOPING_PLAYER_SOUND_ID 0x026Du

typedef struct PlayerSoundObject
{
    unsigned char header[8];
    float x;
    float y;
    float z;
} PlayerSoundObject;

typedef struct DeferredRemoteSound
{
    int sender_cid;
    int sender_session;
    int sender_epoch;
    unsigned int sound_id;
    unsigned long long deadline_cycles;
} DeferredRemoteSound;

/* Scheduler-current task, real playable task, and its model object. */
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned short D_800C7AB2;
/* Native func_80038C30 queue state: count plus eight packed commands. */
extern volatile unsigned char D_801C09FD_1C15FD;
extern volatile unsigned int D_801C0A00_1C1600[];
extern unsigned long long osGetTime(void);

/* Gameplay camera/listener state used by the native spatial sound helper. */
extern unsigned char D_8020CBF0_5C8B00[];
extern void func_8000F420_10020(unsigned short sound_id,
                                void *listener_state,
                                void *object, float radius);

static unsigned short s_pending_sounds[PLAYER_SOUND_BATCH_MAX];
static int s_pending_sound_count;
static void *s_pending_player_task;
static void *s_pending_player_object;
static unsigned short s_pending_player_room;
static int s_pending_player_session;
static int s_pending_player_epoch;
static int s_pending_player_scripted;
static int s_pending_allow_alive_rebase;
static int s_cached_interaction_session;
static int s_character_switch_sound_scope;
static int s_opening_voice_sound_scope;
static int s_replaying_remote_sound;
static DeferredRemoteSound s_deferred_remote_sounds[REMOTE_SOUND_DEFER_MAX];
static int s_deferred_remote_sound_count;
static void *s_deferred_player_task;
static void *s_deferred_player_object;
static unsigned short s_deferred_player_room;
static int s_deferred_player_session;
static int s_deferred_player_epoch;

static int player_sound_is_safe_one_shot(unsigned int raw_sound_id);

static void clear_pending_player_sounds(void)
{
    s_pending_sound_count = 0;
    s_pending_player_task = 0;
    s_pending_player_object = 0;
    s_pending_player_room = 0;
    s_pending_player_session = 0;
    s_pending_player_epoch = 0;
    s_pending_player_scripted = 0;
    s_pending_allow_alive_rebase = 0;
}

static int player_epoch_is_next(int previous, int current)
{
    return current == (previous == 0x7fffffff ? 1 : previous + 1);
}

static int pending_player_context_is_current(int session)
{
    int current_epoch = anchor_player_models_peek_epoch();

    /* Only cues observed inside the two verified native character-switch
     * call scopes may cross the single alive-state epoch edge those paths
     * create. Every ordinary cue remains strict across death/respawn. */
    return s_pending_player_task == D_801FC604_5B8514 &&
           s_pending_player_object == D_801FC60C_5B851C &&
           s_pending_player_room == D_800C7AB2 &&
           s_pending_player_session == session &&
           (s_pending_player_epoch == current_epoch ||
            (s_pending_allow_alive_rebase &&
             player_epoch_is_next(s_pending_player_epoch, current_epoch) &&
             s_pending_player_scripted ==
                 anchor_player_models_peek_scripted()));
}

static void clear_deferred_remote_sounds(void)
{
    s_deferred_remote_sound_count = 0;
    s_deferred_player_task = 0;
    s_deferred_player_object = 0;
    s_deferred_player_room = 0;
    s_deferred_player_session = 0;
    s_deferred_player_epoch = 0;
}

static int deferred_player_context_is_current(int session)
{
    return s_deferred_player_task == D_801FC604_5B8514 &&
           s_deferred_player_object == D_801FC60C_5B851C &&
           s_deferred_player_room == D_800C7AB2 &&
           s_deferred_player_session == session &&
           s_deferred_player_epoch == anchor_player_models_peek_epoch();
}

static int snapshot_native_sound_queue(unsigned short *reserved)
{
    int count = (int)D_801C09FD_1C15FD;
    int i;

    if (count > PLAYER_SOUND_BATCH_MAX)
        count = PLAYER_SOUND_BATCH_MAX;
    for (i = 0; i < count; ++i)
        reserved[i] = (unsigned short)D_801C0A00_1C1600[i];
    return count;
}

static int native_sound_queue_will_accept(unsigned int raw_sound_id)
{
    unsigned int sound_id = raw_sound_id & 0xffffu;
    int count = (int)D_801C09FD_1C15FD;
    int i;

    if (sound_id == 0 || count >= PLAYER_SOUND_BATCH_MAX)
        return 0;
    for (i = 0; i < count; ++i)
    {
        if ((D_801C0A00_1C1600[i] & 0xffffu) == sound_id)
            return 0;
    }
    return 1;
}

static int sound_id_is_reserved(const unsigned short *reserved,
                                int reserved_count,
                                unsigned int sound_id)
{
    int i;
    for (i = 0; i < reserved_count; ++i)
    {
        if (reserved[i] == (unsigned short)sound_id)
            return 1;
    }
    return 0;
}

static int resolve_remote_sound_position(const DeferredRemoteSound *sound,
                                         PlayerSoundObject *source)
{
    return anchor_player_models_get_sound_position(
        sound->sender_cid, sound->sender_session, sound->sender_epoch,
        &source->x, &source->y, &source->z);
}

static void play_remote_sound(const DeferredRemoteSound *sound,
                              const PlayerSoundObject *source)
{
    /* F420 reads xyz synchronously from offsets +8/+C/+10 and delegates to
     * the game's camera-relative pan and distance attenuation path. */
    s_replaying_remote_sound++;
    func_8000F420_10020((unsigned short)sound->sound_id,
                        D_8020CBF0_5C8B00, (void *)source,
                        REMOTE_SOUND_RADIUS);
    s_replaying_remote_sound--;
}

static void defer_remote_sound(DeferredRemoteSound sound,
                               unsigned long long now_cycles)
{
    if (sound.deadline_cycles > now_cycles &&
        s_deferred_remote_sound_count < REMOTE_SOUND_DEFER_MAX)
    {
        if (s_deferred_remote_sound_count == 0)
        {
            s_deferred_player_task = D_801FC604_5B8514;
            s_deferred_player_object = D_801FC60C_5B851C;
            s_deferred_player_room = D_800C7AB2;
            s_deferred_player_session = s_cached_interaction_session;
            s_deferred_player_epoch = anchor_player_models_peek_epoch();
        }
        s_deferred_remote_sounds[s_deferred_remote_sound_count++] = sound;
    }
}

static int player_sound_is_safe_one_shot(unsigned int raw_sound_id)
{
    unsigned int sound_id = raw_sound_id & 0xffffu;

    /* The high bit is a global stop/control command. IDs below 0x100 are
     * sequences/music. 0x026D is a player loop with a matching 0x826D stop;
     * replaying only its start would leave a remote loop running forever. */
    return !(sound_id & 0x8000u) && sound_id >= 0x100u &&
           sound_id != LOOPING_PLAYER_SOUND_ID;
}

static void capture_player_sound(unsigned int raw_sound_id)
{
    unsigned short sound_id;
    int allow_alive_rebase;
    int i;

    if (s_replaying_remote_sound || !D_801FC604_5B8514 ||
        !D_801FC60C_5B851C ||
        !anchor_player_models_is_local_sound_task(D_8016DAB4_16E6B4) ||
        !player_sound_is_safe_one_shot(raw_sound_id))
        return;

    sound_id = (unsigned short)raw_sound_id;
    allow_alive_rebase =
        (s_character_switch_sound_scope && sound_id == 0x021Au) ||
        s_opening_voice_sound_scope;

    /* A room/player transition can happen after a cue is queued but before
     * the frame-end network flush. Never stamp an old context's cue with the
     * replacement player's room and epoch. If the new player itself queues a
     * cue later in the same frame, begin a fresh batch for that context. */
    if (s_pending_sound_count > 0 &&
        !pending_player_context_is_current(s_cached_interaction_session))
        clear_pending_player_sounds();

    /* A marked switch cue may be emitted after native changes the alive bit
     * but before the frame publisher observes it. Keep that cue isolated:
     * arbitrary earlier/later sounds must not inherit its rebase privilege. */
    if (allow_alive_rebase && s_pending_sound_count > 0 &&
        !s_pending_allow_alive_rebase)
        clear_pending_player_sounds();
    else if (!allow_alive_rebase && s_pending_sound_count > 0 &&
             s_pending_allow_alive_rebase)
        return;

    if (s_pending_sound_count == 0)
    {
        s_pending_player_task = D_801FC604_5B8514;
        s_pending_player_object = D_801FC60C_5B851C;
        s_pending_player_room = D_800C7AB2;
        s_pending_player_session = s_cached_interaction_session;
        /* Never enter Python from the final audio-queue hook. The native
         * movement/frame publisher refreshes this cached epoch, and frame-end
         * performs one authoritative refresh before deciding to send. */
        s_pending_player_epoch = anchor_player_models_peek_epoch();
        s_pending_player_scripted =
            anchor_player_models_peek_scripted();
        s_pending_allow_alive_rebase = allow_alive_rebase;
    }

    for (i = 0; i < s_pending_sound_count; ++i)
    {
        /* The native eight-command queue also suppresses a cue that is
         * already pending, so mirror that behavior on the wire. */
        if (s_pending_sounds[i] == sound_id)
            return;
    }
    if (s_pending_sound_count < PLAYER_SOUND_BATCH_MAX)
        s_pending_sounds[s_pending_sound_count++] = sound_id;
}

#ifndef ANCHOR_PLAYER_SOUNDS_HOST_TEST
RECOMP_HOOK("func_801DCE10_598D20")
#endif
void anchor_player_sound_character_switch_begin(void *task)
{
    s_character_switch_sound_scope =
        task && task == D_801FC604_5B8514 &&
        D_8016DAB4_16E6B4 == task &&
        anchor_player_models_is_local_sound_task(task);
}

#ifndef ANCHOR_PLAYER_SOUNDS_HOST_TEST
RECOMP_HOOK_RETURN("func_801DCE10_598D20")
#endif
void anchor_player_sound_character_switch_end(void)
{
    s_character_switch_sound_scope = 0;
}

#ifndef ANCHOR_PLAYER_SOUNDS_HOST_TEST
RECOMP_HOOK("func_801DD830_599740")
#endif
void anchor_player_sound_opening_voice_begin(void *task, int group)
{
    s_opening_voice_sound_scope =
        group == 3 && task && task == D_801FC604_5B8514 &&
        D_8016DAB4_16E6B4 == task &&
        anchor_player_models_is_local_sound_task(task);
}

#ifndef ANCHOR_PLAYER_SOUNDS_HOST_TEST
RECOMP_HOOK_RETURN("func_801DD830_599740")
#endif
void anchor_player_sound_opening_voice_end(void)
{
    s_opening_voice_sound_scope = 0;
}

#ifndef ANCHOR_PLAYER_SOUNDS_HOST_TEST
RECOMP_HOOK("func_80038C30_39830")
#endif
void anchor_capture_local_player_sound(unsigned short sound_id,
                                       unsigned char pan,
                                       unsigned char volume)
{
    (void)pan;
    (void)volume;
    /* This entry hook runs before C30's own duplicate/full checks. Publish
     * only cues the local native queue is about to accept. */
    if (native_sound_queue_will_accept(sound_id))
        capture_player_sound(sound_id);
}

void anchor_player_sounds_update(void)
{
    unsigned short reserved_sound_ids[
        PLAYER_SOUND_BATCH_MAX + REMOTE_SOUND_DRAIN_MAX];
    int reserved_sound_count = 0;
    int remote_play_budget;
    int remote_play_count = 0;
    int old_deferred_count;
    int interaction_session;
    int send_attempted = 0;
    unsigned long long now_cycles;
    int i;

    /* This accessor returns zero while disconnected and a fresh generation
     * after every reconnect. It closes the gap where connected could become
     * false and true again between two frame-end callbacks. */
    interaction_session = anchor_get_projectile_session();
    if (interaction_session <= 0 || !D_801FC604_5B8514 ||
        !D_801FC60C_5B851C)
    {
        clear_pending_player_sounds();
        clear_deferred_remote_sounds();
        s_cached_interaction_session = 0;
        s_character_switch_sound_scope = 0;
        s_opening_voice_sound_scope = 0;
        return;
    }
    if (s_cached_interaction_session != interaction_session)
    {
        clear_pending_player_sounds();
        clear_deferred_remote_sounds();
        s_cached_interaction_session = interaction_session;
        s_character_switch_sound_scope = 0;
        s_opening_voice_sound_scope = 0;
    }
    (void)anchor_player_models_get_epoch();
    if (s_deferred_remote_sound_count > 0 &&
        !deferred_player_context_is_current(interaction_session))
        clear_deferred_remote_sounds();
    if (s_pending_sound_count > 0)
    {
        /* Sound events are transient. Whether send succeeds or fails, never
         * carry this frame's cues into a later player action or room. */
        if (pending_player_context_is_current(interaction_session))
        {
            send_attempted = 1;
            anchor_send_player_sounds(interaction_session,
                                      anchor_player_models_peek_epoch(),
                                      s_pending_sounds,
                                      s_pending_sound_count);
        }
        clear_pending_player_sounds();
    }
    s_character_switch_sound_scope = 0;
    s_opening_voice_sound_scope = 0;

    if (send_attempted)
    {
        int refreshed_session = anchor_get_projectile_session();
        if (refreshed_session != interaction_session)
        {
            clear_pending_player_sounds();
            clear_deferred_remote_sounds();
            s_cached_interaction_session =
                refreshed_session > 0 ? refreshed_session : 0;
            return;
        }
    }

    /* The send bridge above is synchronous and can take measurable time.
     * Sample freshness only after it returns, then refresh it at each actual
     * replay decision so a stalled bridge cannot revive an expired cue. */
    now_cycles = osGetTime();

    /* Inspect the real global queue after the synchronous send bridge. This
     * accounts for UI, enemies, ambient actors, music/control commands, and
     * player cues alike; C30 suppresses duplicates by the low 16-bit cue ID. */
    reserved_sound_count = snapshot_native_sound_queue(reserved_sound_ids);
    remote_play_budget = PLAYER_SOUND_BATCH_MAX - reserved_sound_count;
    if (remote_play_budget > REMOTE_SOUND_DRAIN_MAX)
        remote_play_budget = REMOTE_SOUND_DRAIN_MAX;

    old_deferred_count = s_deferred_remote_sound_count;
    s_deferred_remote_sound_count = 0;
    for (i = 0; i < old_deferred_count; ++i)
    {
        PlayerSoundObject source = {{0}, 0.0f, 0.0f, 0.0f};
        DeferredRemoteSound sound = s_deferred_remote_sounds[i];

        now_cycles = osGetTime();
        if (sound.deadline_cycles <= now_cycles ||
            !player_sound_is_safe_one_shot(sound.sound_id))
            continue;
        if (!resolve_remote_sound_position(&sound, &source))
        {
            /* Python can accept movement and its following sound after the
             * frame's native model snapshot. Retry against the next snapshot
             * without extending the packet's original deadline. */
            defer_remote_sound(sound, now_cycles);
            continue;
        }
        if (remote_play_count >= remote_play_budget ||
            sound_id_is_reserved(reserved_sound_ids, reserved_sound_count,
                                 sound.sound_id) ||
            !native_sound_queue_will_accept(sound.sound_id))
        {
            defer_remote_sound(sound, now_cycles);
            continue;
        }

        play_remote_sound(&sound, &source);
        reserved_sound_ids[reserved_sound_count++] =
            (unsigned short)sound.sound_id;
        remote_play_count++;
    }

    for (i = 0;
         i < REMOTE_SOUND_DRAIN_MAX &&
             remote_play_count < remote_play_budget &&
             s_deferred_remote_sound_count < REMOTE_SOUND_DEFER_MAX;
         ++i)
    {
        PlayerSoundObject source = {{0}, 0.0f, 0.0f, 0.0f};
        DeferredRemoteSound sound = {0, 0, 0, 0, 0};
        unsigned int remaining_ms;
        unsigned long long remaining_cycles;
        unsigned long long poll_start_cycles = osGetTime();

        if (!anchor_poll_player_sound(&sound.sender_cid, &sound.sender_session,
                                      &sound.sender_epoch, &sound.sound_id,
                                      &remaining_ms))
            break;
        if (remaining_ms == 0)
            continue;
        if (remaining_ms > REMOTE_SOUND_MAX_REMAINING_MS)
            remaining_ms = REMOTE_SOUND_MAX_REMAINING_MS;
        remaining_cycles =
            (unsigned long long)remaining_ms * N64_COUNTER_CYCLES_PER_MS;
        /* Python computes remaining_ms during the synchronous poll. Anchor it
         * to the conservative pre-poll time so bridge latency cannot extend
         * the event's original receive deadline. */
        sound.deadline_cycles = poll_start_cycles + remaining_cycles;
        if (sound.deadline_cycles < poll_start_cycles)
            sound.deadline_cycles = ~0ull;
        if (!player_sound_is_safe_one_shot(sound.sound_id))
            continue;
        now_cycles = osGetTime();
        if (sound.deadline_cycles <= now_cycles)
            continue;
        if (!resolve_remote_sound_position(&sound, &source))
        {
            defer_remote_sound(sound, now_cycles);
            continue;
        }
        if (sound_id_is_reserved(reserved_sound_ids, reserved_sound_count,
                                 sound.sound_id) ||
            !native_sound_queue_will_accept(sound.sound_id))
        {
            defer_remote_sound(sound, now_cycles);
            continue;
        }

        play_remote_sound(&sound, &source);
        reserved_sound_ids[reserved_sound_count++] =
            (unsigned short)sound.sound_id;
        remote_play_count++;
    }
}
