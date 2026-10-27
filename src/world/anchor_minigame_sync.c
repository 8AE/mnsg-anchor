#ifdef ANCHOR_MINIGAME_SYNC_HOST_TEST
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
extern int anchor_is_connected(void);
extern int anchor_is_disabled(void);
extern char *anchor_update_minigame(int game, int visit, const char *state);
extern void recomp_free(void *memory);
#else
#include "platform/modding.h"
#include "platform/recomputils.h"
#include "core/anchor.h"
#endif
#include "world/anchor_minigame_sync.h"
#include "progression/item_sync.h"

extern unsigned short D_800C7AB2;

static const AnchorMinigameAdapter *s_adapter;
static AnchorMinigameState s_capture;
static AnchorMinigameStatus s_status;
static char s_json[ANCHOR_MINIGAME_JSON_SIZE];
static unsigned int s_visit, s_token[4], s_generation, s_tick;
static unsigned short s_room;
static int s_loaded, s_game, s_active, s_owner, s_ready, s_result;
static int s_input_ack[ANCHOR_MINIGAME_MAX_INPUTS][4];
static unsigned int s_input_ack_count;
static unsigned int s_input_ack_next;
static unsigned int s_local_ack, s_participant_count;
static int s_participants[ANCHOR_MINIGAME_MAX_PARTICIPANTS];

#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
#ifdef ANCHOR_MINIGAME_SYNC_HOST_TEST
extern int recomp_printf(const char *format, ...);
#endif
enum {
    MS_UNBOUND, MS_OFFLINE, MS_NO_REPLY, MS_BAD_REPLY, MS_NO_OWNER,
    MS_WRONG_GAME, MS_WRONG_TOKEN, MS_OLD_GENERATION, MS_OLD_TICK,
    MS_APPLY_FAILED, MS_OWNER_GRAPH, MS_WAITING, MS_OWNER_READY, MS_FOLLOWER_READY
};
static const char *const s_debug_reasons[] = {
    "unbound", "offline", "no-reply", "bad-reply", "no-owner",
    "wrong-game", "wrong-token", "old-generation", "old-tick",
    "apply-failed", "owner-graph", "reply-waiting", "owner-ready", "follower-ready"
};
static int s_debug_previous[24], s_debug_have_previous;
static unsigned int s_debug_wait_frames;
static void debug_sync_trace(int is_bound,int is_online,int captured,
                             int adapter_valid,int state_valid,int encoded,
                             int capture_ready,int received,int decoded,
                             int reason,int blocked)
{
    int values[24], changed, i;
    if(D_800C7AB2!=0x1e1u) {
        s_debug_have_previous=0;s_debug_wait_frames=0;return;
    }
    values[0]=is_bound;values[1]=is_online;values[2]=(int)s_visit;
    values[3]=s_game;values[4]=s_adapter ? s_adapter->game:0;
    values[5]=s_loaded;values[6]=s_room;values[7]=captured;
    values[8]=adapter_valid;values[9]=state_valid;values[10]=encoded;
    values[11]=capture_ready;values[12]=received;values[13]=decoded;
    values[14]=decoded ? (int)s_status.owner:-1;
    values[15]=decoded ? s_status.is_owner:-1;
    values[16]=decoded ? s_status.ready:-1;
    for(i=0;i<4;++i) values[17+i]=decoded ? (int)s_status.token[i]:-1;
    values[21]=decoded ? (int)s_status.generation:-1;
    values[22]=reason;values[23]=blocked;
    changed=!s_debug_have_previous;
    for(i=0;i<24;++i) if(values[i]!=s_debug_previous[i]) changed=1;
    /* Tick and native clocks are printed only, never change signatures. */
    if(!changed && (!blocked || ++s_debug_wait_frames<120u)) return;
    for(i=0;i<24;++i) s_debug_previous[i]=values[i];
    s_debug_have_previous=1;s_debug_wait_frames=0;
    recomp_printf("[MiniEbiSync] bound=%d online=%d visit=%u game=%d adapter=%d loaded=%d room=%X capture=%d adapterValid=%d stateValid=%d encoded=%d captureReady=%d reply=%d decoded=%d owner=%d isOwner=%d replyReady=%d token=%d/%d/%d/%d generation=%d tick=%d role=%d/%d/%d reason=%s\n",
        values[0],values[1],s_visit,values[3],values[4],values[5],values[6],
        values[7],values[8],values[9],values[10],values[11],values[12],values[13],
        values[14],values[15],values[16],values[17],values[18],values[19],values[20],
        values[21],decoded ? (int)s_status.tick:-1,s_active,s_owner,s_ready,
        s_debug_reasons[reason]);
}
#define MS_TRACE(reason,blocked) debug_sync_trace(debug_bound,debug_online,captured, \
    adapter_valid,state_valid,encoded,debug_capture_ready,debug_received,valid,reason,blocked)
#else
#define MS_TRACE(reason,blocked) ((void)0)
#endif

static int game_for_room(unsigned short room)
{
    if (room == 0x1e0u) return 1;
    if (room == 0x1e1u) return 2;
    if (room == 0x1e2u) return 3;
    /* Kegon is only a candidate: no sync exists until its native challenge
     * adapter binds after an explicit launch or the real sign interaction. */
    if (room == 0x154u) return 4;
    return 0;
}
static int online(void)
{
    return anchor_is_connected() && !anchor_is_disabled() && item_sync_save_is_loaded();
}
static int bound(void)
{
    return s_loaded && s_room == D_800C7AB2 && s_game && s_adapter &&
           s_adapter->game == s_game;
}
static void role(int active, int owner, int ready)
{
    s_active = active; s_owner = owner; s_ready = ready;
    if (!active || !ready) s_participant_count = 0;
    if (s_adapter) s_adapter->role(active, owner, ready);
}
int anchor_minigame_sync_bind(const AnchorMinigameAdapter *adapter)
{
    if (!adapter || adapter->game != game_for_room(D_800C7AB2) ||
        !adapter->reset || !adapter->capture || !adapter->validate ||
        !adapter->apply || !adapter->role || !adapter->input ||
        !adapter->return_pending || !adapter->result) return 0;
    if (s_adapter && s_adapter != adapter) return 0;
    s_adapter = adapter;
    role(online(), !online(), !online());
    return 1;
}
void anchor_minigame_sync_begin_load(void)
{
    role(0, 0, 0);
    if (s_adapter) s_adapter->reset();
    s_adapter = 0;
    s_loaded = s_game = s_result = 0;
    s_generation = s_tick = 0;
    s_input_ack_count = s_input_ack_next = 0;
    s_local_ack = s_participant_count = 0;
    anchor_minigame_zero(s_token, sizeof(s_token));
}
void anchor_minigame_sync_finish_load(void)
{
    s_room = D_800C7AB2;
    s_game = game_for_room(s_room);
    s_visit = s_visit >= 0x7fffffffu ? 1u : s_visit + 1u;
    s_loaded = 1;
}
unsigned int anchor_minigame_sync_visit(void) { return s_visit; }
int anchor_minigame_sync_game(void) { return bound() ? s_game : 0; }
int anchor_minigame_sync_native_allowed(int game)
{
    if (!online()) return 1;
    return bound() && game == s_game && s_active && s_owner && s_ready;
}
int anchor_minigame_sync_waiting(int game)
{
    return online() && game_for_room(D_800C7AB2) == game &&
           (!bound() || !s_active || !s_ready);
}
int anchor_minigame_sync_returning(void)
{
    return bound() && s_adapter->return_pending();
}
int anchor_minigame_sync_result(void) { return bound() ? s_result : 0; }
unsigned int anchor_minigame_sync_local_ack(int game)
{
    return bound() && game == s_game && s_active && s_ready &&
           s_token[0] ? s_local_ack : 0;
}
int anchor_minigame_sync_participant_current(int game, int cid)
{
    unsigned int i;
    if (!bound() || game != s_game || !s_active || !s_ready ||
        !s_token[0] || cid <= 0) return 0;
    for (i = 0; i < s_participant_count; ++i)
        if (s_participants[i] == cid) return 1;
    return 0;
}
unsigned int anchor_minigame_sync_participants(int game, int *out,
                                              unsigned int capacity)
{
    unsigned int i, count;
    if (!out || !capacity || !bound() || game != s_game || !s_active ||
        !s_ready || !s_token[0]) return 0;
    count = s_participant_count < capacity ? s_participant_count : capacity;
    for (i = 0; i < count; ++i) out[i] = s_participants[i];
    return count;
}

static int same_token(const unsigned int *a, const unsigned int *b)
{
    unsigned int i;
    for (i = 0; i < 4; ++i) if (a[i] != b[i]) return 0;
    return 1;
}
static int input_slot(const int *input)
{
    unsigned int i;
    for (i = 0; i < s_input_ack_count; ++i)
        if (s_input_ack[i][0] == input[0] && s_input_ack[i][1] == input[1] &&
            s_input_ack[i][2] == input[2]) return (int)i;
    if (i == ANCHOR_MINIGAME_MAX_INPUTS) return (int)s_input_ack_next;
    return (int)i;
}
static void order_inputs(AnchorMinigameState *state)
{
    unsigned int i,j,count;
    int row[ANCHOR_MINIGAME_INPUT_WORDS];
    /* Transport uses cumulative sequence receipts. Native adapters can gather
     * distinct event queues in a different order from their creation order. */
    for(i=1;i<state->input_count;++i) {
        count=state->input_words[i];
        __builtin_memcpy(row,state->inputs[i],sizeof(row));
        for(j=i;j && state->inputs[j-1][0]>row[0];--j) {
            __builtin_memcpy(state->inputs[j],state->inputs[j-1],sizeof(row));
            state->input_words[j]=state->input_words[j-1];
        }
        __builtin_memcpy(state->inputs[j],row,sizeof(row));
        state->input_words[j]=count;
    }
}
void anchor_minigame_sync_frame(void)
{
    char *reply;
    int captured=0, adapter_valid=0, state_valid=0, encoded=0;
    int applied = 0, needs_apply, valid=0, slot, new_ready;
    unsigned int i;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    int debug_bound=bound(),debug_online=online();
    int debug_capture_ready=-1,debug_received=0;
#endif
    if (!bound() || !online()) {
        role(0, 1, 1);
        if (!online()) {
            /* The bridge creates a fresh interaction session after reconnect.
             * An old session token must not pin an otherwise valid live root. */
            anchor_minigame_zero(s_token, sizeof(s_token));
            s_generation = s_tick = 0;
            s_input_ack_count = s_input_ack_next = 0;
            s_local_ack = s_participant_count = 0;
        }
        MS_TRACE(debug_bound ? MS_OFFLINE:MS_UNBOUND,1);
        return;
    }
    anchor_minigame_zero(&s_capture, sizeof(s_capture));
    captured = s_adapter->capture(&s_capture);
    s_capture.adopted_generation = s_generation;
    s_capture.input_ack_count = s_input_ack_count;
    for (i = 0; i < s_input_ack_count; ++i)
        __builtin_memcpy(s_capture.input_ack[i], s_input_ack[i], sizeof(s_input_ack[i]));
    adapter_valid = captured && s_adapter->validate(&s_capture);
    state_valid = adapter_valid && anchor_minigame_state_valid(&s_capture);
    encoded = state_valid;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    debug_capture_ready=s_capture.ready;
#endif
    if(encoded) {
        order_inputs(&s_capture);
        encoded=anchor_minigame_state_encode(&s_capture,s_json,sizeof(s_json));
    }
    reply = anchor_update_minigame(s_game, (int)s_visit, encoded ? s_json : "null");
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    debug_received=reply!=0;
#endif
    valid = reply && anchor_minigame_status_decode(reply, &s_status);
    if (reply) recomp_free(reply);
    if (!valid || !s_status.owner || s_status.token[0] != (unsigned int)s_game ||
        (s_token[0] && !same_token(s_token, s_status.token)) ||
        s_status.generation < s_generation ||
        (s_status.generation == s_generation && s_status.tick < s_tick)) {
        role(1, 0, 0);
        MS_TRACE(!debug_received ? MS_NO_REPLY:!valid ? MS_BAD_REPLY:
            !s_status.owner ? MS_NO_OWNER:s_status.token[0]!=(unsigned int)s_game ? MS_WRONG_GAME:
            (s_token[0] && !same_token(s_token,s_status.token)) ? MS_WRONG_TOKEN:
            s_status.generation<s_generation ? MS_OLD_GENERATION:MS_OLD_TICK,1);
        return;
    }
    needs_apply = (!s_status.is_owner || s_status.takeover) &&
                  (s_status.generation != s_generation || s_status.tick != s_tick || !s_ready ||
                   (!s_status.is_owner && s_status.has_state &&
                    s_status.local_ack > s_local_ack));
    s_status.state.adopted_generation = s_status.generation;
    s_status.state.source_tick = s_status.tick;
    if (needs_apply && s_status.has_state && s_adapter->validate(&s_status.state)) {
        applied = s_adapter->apply(&s_status.state);
        if (applied) {
            anchor_minigame_zero(&s_capture, sizeof(s_capture));
            applied = s_adapter->capture(&s_capture) && s_capture.ready &&
                      s_adapter->validate(&s_capture);
        }
    }
    if (needs_apply && !applied) {
        role(1, 0, 0);
        MS_TRACE(MS_APPLY_FAILED,1);
        return;
    }
    /* A follower's graph may exist before native reconstruction advertises
     * ready. Applying it is allowed; advancing callbacks still waits for the
     * next bridge call to acknowledge native readiness. */
    if (s_status.is_owner && !s_status.takeover && (!encoded || !s_capture.ready)) {
        role(1, 0, 0);
        MS_TRACE(MS_OWNER_GRAPH,1);
        return;
    }
    if (!same_token(s_token, s_status.token)) {
        s_local_ack = 0;
        s_participant_count = 0;
    }
    for (i = 0; i < 4; ++i) s_token[i] = s_status.token[i];
    s_generation = s_status.generation;
    s_tick = s_status.tick;
    new_ready = s_status.ready && (!needs_apply || applied);
    if (new_ready) {
        if (s_status.local_ack > s_local_ack) s_local_ack = s_status.local_ack;
        s_participant_count = s_status.participant_count;
        for (i = 0; i < s_participant_count; ++i)
            s_participants[i] = s_status.participants[i];
    }
    role(1, s_status.is_owner, new_ready);
    if (s_status.ready && s_status.is_owner)
        for (i = 0; i < s_status.input_count; ++i) {
            slot = input_slot(s_status.inputs[i]);
            if ((unsigned int)slot < s_input_ack_count &&
                s_input_ack[slot][0] == s_status.inputs[i][0] &&
                s_input_ack[slot][1] == s_status.inputs[i][1] &&
                s_input_ack[slot][2] == s_status.inputs[i][2] &&
                s_status.inputs[i][3] <= s_input_ack[slot][3]) continue;
            if (!s_adapter->input(s_status.inputs[i], s_status.input_words[i])) break;
            if ((unsigned int)slot == s_input_ack_count) {
                ++s_input_ack_count;
            } else if (s_input_ack[slot][0] != s_status.inputs[i][0] ||
                       s_input_ack[slot][1] != s_status.inputs[i][1] ||
                       s_input_ack[slot][2] != s_status.inputs[i][2]) {
                /* This ready owner reply follows submission of the previous
                 * ACK set. Transport has already consumed those receipts;
                 * finite native history must not block later participants. */
                s_input_ack_next = ((unsigned int)slot + 1u) % ANCHOR_MINIGAME_MAX_INPUTS;
            }
            s_input_ack[slot][0] = s_status.inputs[i][0];
            s_input_ack[slot][1] = s_status.inputs[i][1];
            s_input_ack[slot][2] = s_status.inputs[i][2];
            s_input_ack[slot][3] = s_status.inputs[i][3];
        }
    if (s_status.result && s_status.result != s_result && s_status.ready) {
        s_result = s_status.result;
        s_adapter->result(s_result);
    }
    MS_TRACE(!s_ready ? MS_WAITING:s_owner ? MS_OWNER_READY:MS_FOLLOWER_READY,!s_ready);
}

RECOMP_HOOK("func_8020D6BC_5C8B8C")
void anchor_minigame_sync_load_begin_hook(void) { anchor_minigame_sync_begin_load(); }
RECOMP_HOOK_RETURN("func_8020D6BC_5C8B8C")
void anchor_minigame_sync_load_finish_hook(void) { anchor_minigame_sync_finish_load(); }
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_minigame_sync_frame_hook(void) { anchor_minigame_sync_frame(); }
