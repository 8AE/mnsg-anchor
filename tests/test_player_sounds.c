#define ANCHOR_PLAYER_SOUNDS_HOST_TEST
#include <assert.h>
#include <string.h>

void *D_8016DAB4_16E6B4;
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
unsigned short D_800C7AB2;
unsigned char D_8020CBF0_5C8B00[24];
volatile unsigned char D_801C09FD_1C15FD;
volatile unsigned int D_801C0A00_1C1600[8];

int anchor_is_connected(void);
int anchor_get_projectile_session(void);
int anchor_send_player_sounds(int interaction_session, int player_epoch,
                              const unsigned short *sound_ids,
                              int sound_count);
int anchor_poll_player_sound(int *sender_cid, int *sender_session,
                             int *sender_epoch, unsigned int *sound_id,
                             unsigned int *remaining_ms);
int anchor_player_models_get_sound_position(int cid, int session, int epoch,
                                            float *x, float *y, float *z);
int anchor_player_models_get_epoch(void);
int anchor_player_models_peek_epoch(void);
int anchor_player_models_peek_scripted(void);
int anchor_player_models_is_local_sound_task(const void *task);

#include "../src/anchor_player_sounds.c"

typedef struct MockSound
{
    int cid;
    int session;
    int epoch;
    unsigned int sound_id;
    unsigned int remaining_ms;
} MockSound;

static int connected;
static int local_interaction_session;
static int send_calls;
static int sent_count;
static unsigned short sent_sounds[PLAYER_SOUND_BATCH_MAX];
static MockSound queued[16];
static int queue_count;
static int queue_cursor;
static int peer_current = 1;
static int position_current = 1;
static int play_calls;
static unsigned int played_sound;
static float played_x;
static float played_y;
static float played_z;
static unsigned long long fake_time_cycles;
static unsigned long long send_advance_cycles;
static unsigned long long poll_advance_cycles;
static int local_player_epoch;
static int local_player_scripted;
static int epoch_get_calls;
static int sent_epoch;
static int sent_session;
static int send_changes_session_to;
static void *local_owned_sound_task;

int anchor_is_connected(void)
{
    return connected;
}

int anchor_get_projectile_session(void)
{
    return connected ? local_interaction_session : 0;
}

int anchor_send_player_sounds(int interaction_session, int player_epoch,
                              const unsigned short *sound_ids,
                              int sound_count)
{
    int i;
    send_calls++;
    sent_count = sound_count;
    sent_session = interaction_session;
    sent_epoch = player_epoch;
    for (i = 0; i < sound_count; ++i)
        sent_sounds[i] = sound_ids[i];
    fake_time_cycles += send_advance_cycles;
    if (send_changes_session_to > 0)
        local_interaction_session = send_changes_session_to;
    return 1;
}

int anchor_poll_player_sound(int *sender_cid, int *sender_session,
                             int *sender_epoch, unsigned int *sound_id,
                             unsigned int *remaining_ms)
{
    MockSound *sound;
    if (queue_cursor >= queue_count)
        return 0;
    sound = &queued[queue_cursor++];
    *sender_cid = sound->cid;
    *sender_session = sound->session;
    *sender_epoch = sound->epoch;
    *sound_id = sound->sound_id;
    *remaining_ms = sound->remaining_ms;
    fake_time_cycles += poll_advance_cycles;
    return 1;
}

unsigned long long osGetTime(void)
{
    return fake_time_cycles;
}

int anchor_player_models_get_epoch(void)
{
    epoch_get_calls++;
    return local_player_epoch;
}

int anchor_player_models_peek_epoch(void)
{
    return local_player_epoch;
}

int anchor_player_models_peek_scripted(void)
{
    return local_player_scripted;
}

int anchor_player_models_is_local_sound_task(const void *task)
{
    return task && (task == D_801FC604_5B8514 ||
                    task == local_owned_sound_task);
}

int anchor_player_models_get_sound_position(int cid, int session, int epoch,
                                            float *x, float *y, float *z)
{
    assert(cid == 2 && session == 202 && epoch == 7);
    if (!peer_current || !position_current)
        return 0;
    *x = 10.5f;
    *y = -20.25f;
    *z = 30.75f;
    return 1;
}

static void drain_native_sound_queue(void)
{
    D_801C09FD_1C15FD = 0;
}

static void native_enqueue(void *task, unsigned short sound_id,
                           unsigned char pan, unsigned char volume)
{
    void *previous_task = D_8016DAB4_16E6B4;
    int accepted = native_sound_queue_will_accept(sound_id);

    D_8016DAB4_16E6B4 = task;
    anchor_capture_local_player_sound(sound_id, pan, volume);
    if (accepted)
    {
        int slot = (int)D_801C09FD_1C15FD;
        D_801C0A00_1C1600[slot] =
            (unsigned int)sound_id | ((unsigned int)pan << 16) |
            ((unsigned int)volume << 24);
        D_801C09FD_1C15FD++;
    }
    D_8016DAB4_16E6B4 = previous_task;
}

void func_8000F420_10020(unsigned short sound_id, void *listener_state,
                         void *object, float radius)
{
    PlayerSoundObject *source = (PlayerSoundObject *)object;
    assert(listener_state == D_8020CBF0_5C8B00);
    assert(radius == 400.0f);
    play_calls++;
    played_sound = sound_id;
    played_x = source->x;
    played_y = source->y;
    played_z = source->z;

    /* The spatial helper ultimately reaches the hooked queue function and
     * then its original body appends the accepted packed command. */
    native_enqueue(D_8016DAB4_16E6B4, sound_id, 64, 200);
}

static void reset_test(void)
{
    int i;

    connected = 1;
    local_interaction_session = 101;
    D_801FC604_5B8514 = (void *)0x1000;
    D_801FC60C_5B851C = (void *)0x2000;
    D_8016DAB4_16E6B4 = D_801FC604_5B8514;
    D_800C7AB2 = 10;
    clear_pending_player_sounds();
    s_cached_interaction_session = local_interaction_session;
    s_character_switch_sound_scope = 0;
    s_opening_voice_sound_scope = 0;
    s_replaying_remote_sound = 0;
    clear_deferred_remote_sounds();
    D_801C09FD_1C15FD = 0;
    for (i = 0; i < PLAYER_SOUND_BATCH_MAX; ++i)
        D_801C0A00_1C1600[i] = 0;
    send_calls = sent_count = 0;
    memset(sent_sounds, 0, sizeof(sent_sounds));
    queue_count = queue_cursor = 0;
    peer_current = position_current = 1;
    play_calls = 0;
    played_sound = 0;
    played_x = played_y = played_z = 0.0f;
    fake_time_cycles = 0;
    send_advance_cycles = 0;
    poll_advance_cycles = 0;
    local_player_epoch = 1;
    local_player_scripted = 0;
    epoch_get_calls = 0;
    sent_epoch = 0;
    sent_session = 0;
    send_changes_session_to = 0;
    local_owned_sound_task = 0;
}

static void queue_sound(unsigned int sound_id)
{
    queued[queue_count].cid = 2;
    queued[queue_count].session = 202;
    queued[queue_count].epoch = 7;
    queued[queue_count].sound_id = sound_id;
    queued[queue_count].remaining_ms = REMOTE_SOUND_MAX_REMAINING_MS;
    queue_count++;
}

static void test_capture_filter_and_native_queue_bound(void)
{
    static const unsigned short expected[PLAYER_SOUND_BATCH_MAX] = {
        0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107};
    int i;
    reset_test();

    D_8016DAB4_16E6B4 = (void *)0x3000;
    anchor_capture_local_player_sound(0x212, 0, 0);
    D_8016DAB4_16E6B4 = D_801FC604_5B8514;
    anchor_capture_local_player_sound(0x80, 0, 0);
    anchor_capture_local_player_sound(0x8212, 0, 0);
    anchor_capture_local_player_sound(0x26D, 0, 0);
    assert(s_pending_sound_count == 0);

    for (i = 0; i < PLAYER_SOUND_BATCH_MAX; ++i)
    {
        anchor_capture_local_player_sound(expected[i], 0, 0);
        anchor_capture_local_player_sound(expected[i], 0, 0);
    }
    anchor_capture_local_player_sound(0x108, 0, 0);
    assert(s_pending_sound_count == PLAYER_SOUND_BATCH_MAX);
    assert(epoch_get_calls == 0);

    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_count == PLAYER_SOUND_BATCH_MAX);
    assert(memcmp(sent_sounds, expected, sizeof(expected)) == 0);
    assert(s_pending_sound_count == 0);
}

static void test_native_queue_rejection_is_not_published(void)
{
    int i;

    reset_test();
    native_enqueue((void *)0x3000, 0x212, 0, 0);
    native_enqueue(D_801FC604_5B8514, 0x212, 0, 0);
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);

    reset_test();
    for (i = 0; i < PLAYER_SOUND_BATCH_MAX; ++i)
        native_enqueue((void *)0x3000, (unsigned short)(0x300 + i), 0, 0);
    native_enqueue(D_801FC604_5B8514, 0x400, 0, 0);
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);

    reset_test();
    for (i = 0; i < PLAYER_SOUND_BATCH_MAX - 1; ++i)
        native_enqueue((void *)0x3000, (unsigned short)(0x300 + i), 0, 0);
    native_enqueue(D_801FC604_5B8514, 0x400, 0, 0);
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_count == 1);
    assert(sent_sounds[0] == 0x400);
    assert(sent_session == 101 && sent_epoch == 1);
}

static void test_disconnect_drops_unsent_frame(void)
{
    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    connected = 0;
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);
}

static void test_player_owned_camera_and_bomb_child_sounds(void)
{
    void *child_task = (void *)0x5000;

    reset_test();
    local_owned_sound_task = child_task;
    native_enqueue(child_task, 0x207, 0, 0); /* camera shutter */
    native_enqueue(child_task, 0x104, 0, 0); /* bomb explosion */
    native_enqueue(child_task, 0x207, 0, 0);
    assert(s_pending_sound_count == 2);

    /* The pending batch retains only the authoritative player identity, not
     * the child. Reuse/teardown before frame end cannot be dereferenced. */
    local_owned_sound_task = 0;
    D_8016DAB4_16E6B4 = (void *)0x6000;
    anchor_capture_local_player_sound(0x104, 0, 0);
    anchor_capture_local_player_sound(0x26d, 0, 0);
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_count == 2);
    assert(sent_sounds[0] == 0x207 && sent_sounds[1] == 0x104);
}

static void test_context_transition_drops_or_restarts_batch(void)
{
    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    D_800C7AB2 = 11;
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);

    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    D_801FC604_5B8514 = (void *)0x3000;
    D_801FC60C_5B851C = (void *)0x4000;
    D_8016DAB4_16E6B4 = D_801FC604_5B8514;
    anchor_capture_local_player_sound(0x25B, 0, 0);
    assert(s_pending_sound_count == 1);
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_count == 1 && sent_sounds[0] == 0x25B);
}

static void test_character_switch_alive_edges_rebase_outbound_batch(void)
{
    static const unsigned short opening_voices[PLAYER_SOUND_BATCH_MAX] = {
        0x378, 0x382, /* Goemon */
        0x36a, 0x364, /* Ebisumaru */
        0x392, 0x38a, /* Sasuke */
        0x3a5, 0x39e  /* Yae */
    };
    int i;

    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    local_player_epoch++;
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);

    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    anchor_player_sound_character_switch_begin(D_801FC604_5B8514);
    anchor_capture_local_player_sound(0x21a, 0, 0);
    anchor_player_sound_character_switch_end();
    /* A later ordinary cue cannot inherit the switch cue's privilege. */
    anchor_capture_local_player_sound(0x25b, 0, 0);
    local_player_epoch++;
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_count == 1);
    assert(sent_sounds[0] == 0x21a && sent_epoch == 2);

    reset_test();
    anchor_player_sound_opening_voice_begin(D_801FC604_5B8514, 3);
    for (i = 0; i < PLAYER_SOUND_BATCH_MAX; ++i)
        anchor_capture_local_player_sound(opening_voices[i], 0, 0);
    anchor_player_sound_opening_voice_end();
    local_player_epoch++;
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_count == PLAYER_SOUND_BATCH_MAX);
    assert(memcmp(sent_sounds, opening_voices, sizeof(opening_voices)) == 0);
    assert(sent_epoch == 2);

    reset_test();
    anchor_player_sound_opening_voice_begin(D_801FC604_5B8514, 2);
    anchor_capture_local_player_sound(0x378, 0, 0);
    anchor_player_sound_opening_voice_end();
    local_player_epoch++;
    anchor_player_sounds_update();
    assert(send_calls == 0);

    reset_test();
    anchor_player_sound_character_switch_begin(D_801FC604_5B8514);
    anchor_capture_local_player_sound(0x21a, 0, 0);
    anchor_player_sound_character_switch_end();
    local_player_epoch += 2;
    anchor_player_sounds_update();
    assert(send_calls == 0);

    reset_test();
    anchor_player_sound_character_switch_begin(D_801FC604_5B8514);
    anchor_capture_local_player_sound(0x21a, 0, 0);
    anchor_player_sound_character_switch_end();
    local_player_epoch++;
    local_interaction_session = 202;
    anchor_player_sounds_update();
    assert(send_calls == 0);
}

static void test_scripted_epoch_edge_still_drops_outbound_batch(void)
{
    reset_test();
    anchor_player_sound_character_switch_begin(D_801FC604_5B8514);
    anchor_capture_local_player_sound(0x21a, 0, 0);
    anchor_player_sound_character_switch_end();
    local_player_epoch++;
    local_player_scripted = 1;
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);
}

static void test_spatial_replay_and_feedback_guard(void)
{
    reset_test();
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(play_calls == 1 && played_sound == 0x212);
    assert(played_x == 10.5f && played_y == -20.25f && played_z == 30.75f);
    assert(s_pending_sound_count == 0);

    queue_cursor = queue_count = 0;
    queue_sound(0x8212);
    anchor_player_sounds_update();
    assert(play_calls == 1);

    queue_cursor = queue_count = 0;
    queue_sound(0x25B);
    peer_current = 0;
    anchor_player_sounds_update();
    assert(play_calls == 1);
    assert(s_deferred_remote_sound_count == 1);
    fake_time_cycles =
        (REMOTE_SOUND_MAX_REMAINING_MS + 1ull) * N64_COUNTER_CYCLES_PER_MS;
    anchor_player_sounds_update();
    assert(s_deferred_remote_sound_count == 0);

    reset_test();
    queue_sound(0x25B);
    position_current = 0;
    anchor_player_sounds_update();
    assert(play_calls == 0 && s_deferred_remote_sound_count == 1);
    position_current = 1;
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 0);
}

static void test_remote_drain_reserves_native_queue_capacity(void)
{
    int i;
    reset_test();
    for (i = 0; i < 6; ++i)
        native_enqueue((void *)0x3000, (unsigned short)(0x500 + i), 0, 0);
    for (i = 0; i < 4; ++i)
        queue_sound((unsigned int)(0x300 + i));
    anchor_player_sounds_update();
    assert(queue_cursor == 2 && play_calls == 2);
    assert(D_801C09FD_1C15FD == PLAYER_SOUND_BATCH_MAX);
    drain_native_sound_queue();
    anchor_player_sounds_update();
    assert(queue_cursor == 4 && play_calls == 4);
}

static void test_global_native_queue_traffic_defers_remote_cues(void)
{
    int i;

    reset_test();
    /* This is an already accepted UI/enemy/ambient queue entry, not a
     * publishable player cue. C30 would suppress the equal remote ID. */
    D_801C09FD_1C15FD = 1;
    D_801C0A00_1C1600[0] = 0x212;
    queue_sound(0x212);
    queue_sound(0x213);
    anchor_player_sounds_update();
    assert(queue_cursor == 2 && play_calls == 1 && played_sound == 0x213);
    assert(s_deferred_remote_sound_count == 1);

    D_801C09FD_1C15FD = 0;
    anchor_player_sounds_update();
    assert(play_calls == 2 && played_sound == 0x212);
    assert(s_deferred_remote_sound_count == 0);

    reset_test();
    for (i = 0; i < PLAYER_SOUND_BATCH_MAX; ++i)
        D_801C0A00_1C1600[i] = (unsigned int)(0x300 + i);
    D_801C09FD_1C15FD = PLAYER_SOUND_BATCH_MAX;
    queue_sound(0x400);
    anchor_player_sounds_update();
    assert(queue_cursor == 0 && play_calls == 0);

    D_801C09FD_1C15FD = 0;
    anchor_player_sounds_update();
    assert(queue_cursor == 1 && play_calls == 1);
}

static void test_equal_remote_cues_are_serialized_across_frames(void)
{
    reset_test();
    queue_sound(0x212);
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(queue_cursor == 2 && play_calls == 1);
    assert(s_deferred_remote_sound_count == 1);
    drain_native_sound_queue();
    anchor_player_sounds_update();
    assert(play_calls == 2 && s_deferred_remote_sound_count == 0);
}

static void test_remote_cue_matching_local_cue_is_deferred(void)
{
    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    D_801C09FD_1C15FD = 1;
    D_801C0A00_1C1600[0] = 0x212;
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(send_calls == 1 && queue_cursor == 1 && play_calls == 0);
    assert(s_deferred_remote_sound_count == 1);
    D_801C09FD_1C15FD = 0;
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 0);

    reset_test();
    native_enqueue(D_801FC604_5B8514, 0x212, 0, 0);
    /* If the audio consumer drains before frame-end, the live snapshot makes
     * the slot and ID available again instead of using stale shadow state. */
    drain_native_sound_queue();
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(send_calls == 1 && play_calls == 1);
    assert(s_deferred_remote_sound_count == 0);
}

static void test_deferred_cue_does_not_cross_local_context(void)
{
    reset_test();
    queue_sound(0x212);
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 1);

    D_801FC604_5B8514 = (void *)0x3000;
    D_801FC60C_5B851C = (void *)0x4000;
    D_8016DAB4_16E6B4 = D_801FC604_5B8514;
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 0);

    reset_test();
    queue_sound(0x212);
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 1);
    local_player_epoch++;
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 0);
}

static void test_pending_and_deferred_cues_do_not_cross_reconnect(void)
{
    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    /* Simulate disconnect + reconnect entirely between frame callbacks. */
    local_interaction_session = 202;
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_pending_sound_count == 0);
    assert(s_cached_interaction_session == 202);

    reset_test();
    queue_sound(0x212);
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 1);
    local_interaction_session = 202;
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 0);
    assert(s_cached_interaction_session == 202);
}

static void test_new_session_refresh_and_send_race_are_conservative(void)
{
    reset_test();
    s_cached_interaction_session = 0;
    anchor_capture_local_player_sound(0x212, 0, 0);
    anchor_player_sounds_update();
    assert(send_calls == 0 && s_cached_interaction_session == 101);
    anchor_capture_local_player_sound(0x213, 0, 0);
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_sounds[0] == 0x213);

    reset_test();
    queue_sound(0x212);
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 1);
    drain_native_sound_queue();
    anchor_capture_local_player_sound(0x300, 0, 0);
    queue_sound(0x400);
    send_changes_session_to = 202;
    anchor_player_sounds_update();
    assert(send_calls == 1 && sent_session == 101);
    assert(play_calls == 1 && queue_cursor == 2);
    assert(s_deferred_remote_sound_count == 0);
    assert(s_cached_interaction_session == 202);
}

static void test_deferred_cue_respects_transport_deadline(void)
{
    reset_test();
    anchor_capture_local_player_sound(0x212, 0, 0);
    D_801C09FD_1C15FD = 1;
    D_801C0A00_1C1600[0] = 0x212;
    queue_sound(0x212);
    queued[0].remaining_ms = 100;
    anchor_player_sounds_update();
    assert(play_calls == 0 && s_deferred_remote_sound_count == 1);

    fake_time_cycles = 101ull * N64_COUNTER_CYCLES_PER_MS;
    anchor_player_sounds_update();
    assert(play_calls == 0 && s_deferred_remote_sound_count == 0);
}

static void test_expired_cues_do_not_survive_send_or_zero_budget(void)
{
    reset_test();
    anchor_capture_local_player_sound(0x300, 0, 0);
    queue_sound(0x212);
    queue_sound(0x212);
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 1);

    anchor_capture_local_player_sound(0x301, 0, 0);
    send_advance_cycles =
        (REMOTE_SOUND_MAX_REMAINING_MS + 1ull) * N64_COUNTER_CYCLES_PER_MS;
    anchor_player_sounds_update();
    assert(play_calls == 1 && s_deferred_remote_sound_count == 0);

    reset_test();
    queue_sound(0x212);
    queued[0].remaining_ms = 0;
    anchor_player_sounds_update();
    assert(queue_cursor == 1 && play_calls == 0);
    assert(s_deferred_remote_sound_count == 0);

    reset_test();
    queue_sound(0x212);
    queued[0].remaining_ms = 100;
    poll_advance_cycles = 101ull * N64_COUNTER_CYCLES_PER_MS;
    anchor_player_sounds_update();
    assert(queue_cursor == 1 && play_calls == 0);
    assert(s_deferred_remote_sound_count == 0);
}

int main(void)
{
    test_capture_filter_and_native_queue_bound();
    test_native_queue_rejection_is_not_published();
    test_disconnect_drops_unsent_frame();
    test_player_owned_camera_and_bomb_child_sounds();
    test_context_transition_drops_or_restarts_batch();
    test_character_switch_alive_edges_rebase_outbound_batch();
    test_scripted_epoch_edge_still_drops_outbound_batch();
    test_spatial_replay_and_feedback_guard();
    test_remote_drain_reserves_native_queue_capacity();
    test_global_native_queue_traffic_defers_remote_cues();
    test_equal_remote_cues_are_serialized_across_frames();
    test_remote_cue_matching_local_cue_is_deferred();
    test_deferred_cue_does_not_cross_local_context();
    test_pending_and_deferred_cues_do_not_cross_reconnect();
    test_new_session_refresh_and_send_race_are_conservative();
    test_deferred_cue_respects_transport_deadline();
    test_expired_cues_do_not_survive_send_or_zero_budget();
    return 0;
}
