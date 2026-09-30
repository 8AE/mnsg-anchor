#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ANCHOR_CONTROL_MACHINE_SYNC_HOST_TEST
#include "../src/bosses/control_machine/anchor_control_machine_sync.c"

unsigned short D_800C7AB2 = 0x155u;
static unsigned char system_store[0x3ae28];
void *D_8015C5C8_15D1C8 = system_store;
static AnchorControlMachineStatus reply;
static int connected = 1, bound = 1, ready, paused, pending_flag;
static int capture_ok = 1, decode_ok = 1;
static unsigned int visit = 42u;
static int update_ready, update_paused, update_calls, capture_calls;
static int apply_calls, role_active, role_owner, role_paused, intro_fresh;
static int apply_force_last;
static int last_state_null, completion_calls, terminal_requests;
static int local_window, local_hit_sequence, send_accept = 1;
static int send_calls, last_sent_sequence, cleared_local_hits;
static unsigned int mock_command_serial, mock_phase, mock_observed_phase;
static unsigned int last_captured_command;

int anchor_is_connected(void) { return connected; }
int anchor_is_disabled(void) { return 0; }
int item_sync_save_is_loaded(void) { return 1; }
int anchor_boss_invite_world_arena(void)
{ return ANCHOR_BOSS_ARENA_CONTROL_MACHINE; }
int anchor_player_models_get_epoch(void) { return 1; }
int anchor_dialog_world_paused(void) { return paused; }
char *anchor_control_machine_update(int r, unsigned int v, int p,
                                    const char *state)
{
    ++update_calls;
    update_ready = r;
    update_paused = p;
    last_state_null = !strcmp(state, "null");
    assert(v == (r || v ? visit : 0u) || v == 0u);
    return "ok";
}
int anchor_send_control_machine_hit(int sequence)
{ ++send_calls; last_sent_sequence = sequence; return send_accept; }
void recomp_free(void *memory) { (void)memory; }
int anchor_control_machine_status_decode(const char *json,
                                         AnchorControlMachineStatus *out)
{ assert(!strcmp(json, "ok")); *out = reply; return decode_ok; }
int anchor_control_machine_state_encode(const AnchorControlMachineSnapshot *s,
                                        char *out, unsigned int capacity)
{ (void)s; assert(capacity > 8u); strcpy(out, "captured"); return 1; }
void anchor_control_machine_native_tick(void) {}
unsigned int anchor_control_machine_native_visit(void) { return visit; }
int anchor_control_machine_native_bound(void) { return bound; }
int anchor_control_machine_native_pending(void) { return 0; }
int anchor_control_machine_native_ready(void) { return ready; }
int anchor_control_machine_native_transport_ready(void) { return ready; }
int anchor_control_machine_native_capture(AnchorControlMachineSnapshot *s)
{
    ++capture_calls;
    if (!capture_ok) return 0;
    memset(s, 0, sizeof(*s));
    if (mock_observed_phase && mock_phase != mock_observed_phase)
        ++mock_command_serial;
    mock_observed_phase = mock_phase;
    s->root[CM_COMMAND] = last_captured_command = mock_command_serial;
    return 1;
}
int anchor_control_machine_native_apply(const AnchorControlMachineSnapshot *s,
                                        int force_recovery)
{ (void)s; apply_force_last = force_recovery; ++apply_calls; return 1; }
void anchor_control_machine_native_set_role(int active, int owner, int p)
{ role_active = active; role_owner = owner; role_paused = p; }
int anchor_control_machine_native_take_local_hit(int *sequence)
{ if (!local_hit_sequence) return 0;
  *sequence = local_hit_sequence; local_hit_sequence = 0; return 1; }
int anchor_control_machine_native_local_hit_window(void)
{ return local_window; }
int anchor_control_machine_native_queue_hit(const int identity[5])
{ (void)identity; return 1; }
int anchor_control_machine_native_hit_pending(void) { return 0; }
void anchor_control_machine_native_clear_hits(void) {}
void anchor_control_machine_native_clear_local_hits(void)
{ ++cleared_local_hits; local_hit_sequence = 0; }
void anchor_control_machine_native_discard_pending(void)
{ mock_command_serial = mock_observed_phase = 0; }
int anchor_control_machine_native_request_terminal(void)
{ ++terminal_requests; return 1; }
int anchor_control_machine_native_terminal_fallback(void) { return 0; }
void anchor_control_machine_intro_preview(unsigned int v, int fresh)
{ (void)v; intro_fresh = fresh; }
void anchor_control_machine_intro_owner_hint(unsigned int v, unsigned int owner)
{ (void)v; (void)owner; }
int boss_sync_control_machine_remote_pending(void) { return pending_flag; }
void boss_sync_control_machine_native_complete(void) { ++completion_calls; }

static void reply_role(int role, int state, int preview,
                       unsigned int revision)
{
    memset(&reply, 0, sizeof(reply));
    reply.role = role;
    reply.owner = role ? 7 : 0;
    reply.term = role ? 1u : 0u;
    reply.encounter[0] = role ? 7u : 0u;
    reply.encounter[1] = role ? 1u : 0u;
    reply.encounter[2] = role ? 1u : 0u;
    reply.revision = revision;
    reply.has_state = state;
    reply.has_preview = preview;
    reply.state.root[CM_PHASE] = 2u;
    reply.state.root[CM_COMMAND] = 1u;
}

int main(void)
{
    reply_role(0, 0, 1, 0);
    anchor_control_machine_sync_frame();
    assert(update_ready == 0 && update_paused == 1 && last_state_null);
    assert(intro_fresh && !apply_calls && !capture_calls);

    /* Late arrival asks for the incumbent snapshot before any local offer. */
    ready = 1;
    reply_role(2, 1, 0, 4);
    anchor_control_machine_sync_frame();
    assert(update_ready == 1 && last_state_null && apply_calls == 1);
    assert(apply_force_last);
    assert(!capture_calls && role_active && !role_owner && !role_paused);

    /* A rate-limited follower contact cannot be delivered on a later
     * vulnerable cycle after the original native window has closed. */
    local_window = 1;
    local_hit_sequence = 19;
    send_accept = 0;
    anchor_control_machine_sync_frame();
    assert(send_calls == 1 && last_sent_sequence == 19 &&
           s_pending_local_sequence == 19);
    local_window = 0;
    send_accept = 1;
    anchor_control_machine_sync_frame();
    assert(send_calls == 1 && !s_pending_local_sequence &&
           cleared_local_hits > 0);
    local_window = 1;
    local_hit_sequence = 20;
    anchor_control_machine_sync_frame();
    assert(send_calls == 2 && last_sent_sequence == 20 &&
           !s_pending_local_sequence);
    local_window = 0;

    /* A paused recipient holds a newer checkpoint until gameplay resumes. */
    paused = 1;
    reply.revision = 5;
    anchor_control_machine_sync_frame();
    assert(update_paused && apply_calls == 1 && role_paused);
    paused = 0;
    anchor_control_machine_sync_frame();
    assert(apply_calls == 2 && apply_force_last);
    reply.revision = 6;
    anchor_control_machine_sync_frame();
    assert(apply_calls == 3 && !apply_force_last);

    /* A temporarily invalid first capture must not freeze the local AI.
     * It has to advance natively until a publishable checkpoint exists. */
    visit++;
    reply_role(0, 0, 0, 0);
    capture_ok = 0;
    decode_ok = 0;
    anchor_control_machine_sync_frame();
    assert(capture_calls == 1 && last_state_null && !role_active);
    decode_ok = 1;
    anchor_control_machine_sync_frame();
    assert(capture_calls == 2 && last_state_null && !role_active);
    capture_ok = 1;
    mock_command_serial = 3u;
    mock_phase = 2u;
    anchor_control_machine_sync_frame();
    assert(capture_calls == 3 && last_captured_command == 3u &&
           !last_state_null && !role_active);
    anchor_control_machine_sync_frame();
    assert(capture_calls == 4 && !last_state_null && !role_active);
    reply_role(1, 1, 0, 1);
    reply.encounter[2] = visit;
    anchor_control_machine_sync_frame();
    assert(apply_calls == 3 && role_owner && !role_paused);
    assert(mock_command_serial == 3u);
    anchor_control_machine_sync_frame();
    assert(last_captured_command == 3u);
    mock_phase = 3u;
    anchor_control_machine_sync_frame();
    assert(last_captured_command == 4u);

    /* A higher-term takeover is not a fresh self-creation, even when its
     * creator and visit happen to match the current local binding. */
    visit++;
    reply_role(1, 1, 0, 1);
    reply.term = 2u;
    reply.encounter[2] = visit;
    anchor_control_machine_sync_frame();
    assert(apply_calls == 4 && role_owner && apply_force_last);

    /* Terminal continuation remains a ready transport visit after the
     * dragon follower latch has been released locally. */
    reply.state.root[CM_PHASE] = 7u;
    reply.state.root[CM_STATUS] = 3u;
    reply.revision = 2;
    anchor_control_machine_sync_frame();
    assert(update_ready == 1 && apply_calls == 4); /* owner does not reapply */
    assert(completion_calls >= 1 && !terminal_requests);

    /* Serial one at the first election must survive the self response too. */
    visit++;
    reply_role(0, 0, 0, 0);
    anchor_control_machine_sync_frame();
    mock_command_serial = 1u;
    mock_phase = 2u;
    mock_observed_phase = 0u;
    anchor_control_machine_sync_frame();
    assert(last_captured_command == 1u);
    reply_role(1, 1, 0, 1);
    reply.encounter[2] = visit;
    anchor_control_machine_sync_frame();
    assert(mock_command_serial == 1u);
    mock_phase = 3u;
    anchor_control_machine_sync_frame();
    assert(last_captured_command == 2u);
    puts("Control Machine bridge readiness and adoption: PASS");
    return 0;
}
