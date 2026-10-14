#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define ANCHOR_MINIGAME_SYNC_HOST_TEST
#include "../src/world/anchor_minigame_sync.c"
#include "../src/world/anchor_minigame_codec.c"

unsigned short D_800C7AB2 = 0x1e0;
static int connected = 1, loaded = 1, calls, resets, applies, apply_ok = 1;
static int role_owner, role_ready, capture_ready = 1, native_value, results, input_calls, input_ok = 1;
static int capture_inputs;
static int capture_calls, validate_calls, capture_ok=1, invalid_generic, response_null;
static int expected_game=1;
static unsigned int applied_local_ack, applied_generation, applied_full_tick, applied_source_tick;
static unsigned int validated_source_tick;
static int connection_queries;
static int check_role_cache;
static unsigned int role_expected_ack;
static int role_expected_participant;
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
static int diagnostic_calls;
static char diagnostic_line[2048];
int recomp_printf(const char *format,...)
{
    va_list args;
    va_start(args,format);
    int n=vsnprintf(diagnostic_line,sizeof(diagnostic_line),format,args);
    va_end(args);
    assert(n>0 && (unsigned int)n<sizeof(diagnostic_line));
    ++diagnostic_calls;return n;
}
#endif
static char response[ANCHOR_MINIGAME_JSON_SIZE];
static char submitted[ANCHOR_MINIGAME_JSON_SIZE];
int anchor_is_connected(void) { ++connection_queries; return connected; }
int anchor_is_disabled(void) { ++connection_queries; return 0; }
int item_sync_save_is_loaded(void) { return loaded; }
char *anchor_update_minigame(int game, int visit, const char *state)
{
    assert(game == expected_game && visit > 0);
    ++calls;
    strcpy(submitted, state);
    return response_null ? 0:response;
}
void recomp_free(void *p) { assert(p == response); }
static void reset_native(void) { ++resets; }
static int capture_native(AnchorMinigameState *s)
{
    ++capture_calls;
    s->ready = capture_ready;
    s->running = invalid_generic ? 2:1;
    s->controller_count = 1;
    s->controller[0] = native_value;
    if(capture_inputs) {
        s->input_count=3;
        s->input_words[0]=2;s->inputs[0][0]=20;s->inputs[0][1]=1;
        s->input_words[1]=3;s->inputs[1][0]=17;s->inputs[1][1]=2;s->inputs[1][2]=117;
        s->input_words[2]=2;s->inputs[2][0]=19;s->inputs[2][1]=3;
    }
    return capture_ok;
}
static int validate_native(const AnchorMinigameState *s)
{
    ++validate_calls;
    if (s == &s_status.state) validated_source_tick = s->source_tick;
    return s->controller_count == 1 && s->controller[0] >= 0 && s->controller[0] <= 100;
}
static int apply_native(const AnchorMinigameState *s)
{
    ++applies;
    applied_local_ack = s->local_ack;
    applied_generation = s->adopted_generation;
    applied_full_tick = s->full_tick;
    applied_source_tick = s->source_tick;
    if (!apply_ok) return 0;
    native_value = s->controller[0];
    return 1;
}
static void role_native(int active, int owner, int ready)
{
    role_owner = owner; role_ready = ready;
    if (check_role_cache && active && ready) {
        assert(anchor_minigame_sync_local_ack(expected_game)==role_expected_ack);
        assert(anchor_minigame_sync_participant_current(expected_game,role_expected_participant));
    }
}
static int input_native(const int *values, unsigned int count)
{ assert(count == 6 && values[0] == 8 && values[4] == 1); ++input_calls; return input_ok; }
static int return_native(void) { return results != 0; }
static void result_native(int result) { assert(result == 2); ++results; }
static const AnchorMinigameAdapter adapter = {
    1, reset_native, capture_native, validate_native, apply_native,
    role_native, input_native, return_native, result_native
};
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
static const AnchorMinigameAdapter ebi_adapter = {
    2, reset_native, capture_native, validate_native, apply_native,
    role_native, input_native, return_native, result_native
};
#endif

static void response_state(int owner, int ready, int generation, int tick,
                           int value, int takeover)
{
    snprintf(response, sizeof(response),
        "{\"ready\":%d,\"owner\":7,\"isOwner\":%d,\"token\":[1,7,1,1],"
        "\"generation\":%d,\"tick\":%d,\"result\":0,\"takeover\":%d,"
        "\"running\":1,\"c\":[%d],\"a\":[],\"inputs\":[]}", ready, owner, generation, tick, takeover, value);
}
static void fresh(void)
{
    anchor_minigame_sync_begin_load();
    anchor_minigame_sync_finish_load();
    assert(anchor_minigame_sync_bind(&adapter));
    native_value = 0; results = 0; applies = 0;
    capture_ready = apply_ok = 1;
    capture_inputs=0;
}
static void response_fields(const char *fields)
{
    unsigned int n = (unsigned int)strlen(response);
    assert(n && response[n - 1] == '}');
    assert(strlen(fields) + n + 1 < sizeof(response));
    response[n - 1] = ',';
    strcpy(response + n, fields);
    strcat(response, "}");
}
static void test_bridge_codec(void)
{
    static AnchorMinigameStatus status, previous;
    static AnchorMinigameState state;
    static char encoded[ANCHOR_MINIGAME_JSON_SIZE];
    static const char *bad[] = {
        "\"localAck\":-1", "\"localAck\":2147483648",
        "\"localAck\":1,\"localAck\":2", "\"localAck\":null",
        "\"localAck\":\"1\"", "\"localAck\":1.5",
        "\"fullTick\":-1", "\"fullTick\":2147483648",
        "\"fullTick\":1,\"fullTick\":2", "\"fullTick\":null",
        "\"fullTick\":\"1\"", "\"fullTick\":1.5",
        "\"members\":[1,1]", "\"members\":[2,1]", "\"members\":[0]",
        "\"members\":[-1]", "\"members\":[2147483648]",
        "\"members\":null", "\"members\":[1],\"members\":[2]"
    };
    unsigned int i, n;
    response_state(0,1,1,1,50,0);
    assert(anchor_minigame_status_decode(response,&status));
    assert(status.local_ack == 0 && status.state.local_ack == 0 &&
           status.full_tick == 0 && status.state.full_tick == 0 &&
           status.participant_count == 0); /* Older bridge compatibility. */
    response_fields("\"localAck\":2147483647,\"fullTick\":2147483647,\"members\":[1,8,2147483647]");
    assert(anchor_minigame_status_decode(response,&status));
    assert(status.local_ack == 0x7fffffffu && status.state.local_ack == 0x7fffffffu &&
           status.full_tick == 0x7fffffffu && status.state.full_tick == 0x7fffffffu &&
           status.participant_count == 3 && status.participants[1] == 8);
    previous = status;
    for (i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        response_state(0,1,1,1,50,0);response_fields(bad[i]);
        assert(!anchor_minigame_status_decode(response,&status));
        assert(!memcmp(&status,&previous,sizeof(status)));
    }
    response_state(0,1,1,1,50,0);
    n=(unsigned int)strlen(response)-1;
    n+=(unsigned int)snprintf(response+n,sizeof(response)-n,",\"members\":[");
    for(i=1;i<=64;++i)
        n+=(unsigned int)snprintf(response+n,sizeof(response)-n,"%s%u",i>1?",":"",i);
    strcpy(response+n,"]}");
    assert(anchor_minigame_status_decode(response,&status) && status.participant_count==64);
    strcpy(response+n,",65]}");
    assert(!anchor_minigame_status_decode(response,&status));
    assert(!anchor_minigame_status_decode("{\"ready\":0,\"owner\":0,\"isOwner\":0,"
        "\"token\":[],\"generation\":0,\"localAck\":1}",&status));
    assert(!anchor_minigame_status_decode("{\"ready\":0,\"owner\":0,\"isOwner\":0,"
        "\"token\":[],\"generation\":0,\"members\":[1]}",&status));
    assert(!anchor_minigame_status_decode("{\"ready\":0,\"owner\":0,\"isOwner\":0,"
        "\"token\":[],\"generation\":0,\"fullTick\":1}",&status));

    anchor_minigame_zero(&state,sizeof(state));
    state.controller_count=32;state.actor_count=1;state.actor_words[0]=40;
    state.local_ack=5;state.full_tick=17;
    assert(anchor_minigame_state_encode(&state,encoded,sizeof(encoded)));
    assert(!strstr(encoded,"localAck")); /* Bridge receipt is never outbound. */
    assert(!strstr(encoded,"fullTick")); /* Complete snapshot marker is bridge-only. */
    state.actor_words[0]=41;assert(!anchor_minigame_state_valid(&state));
    state.actor_words[0]=40;state.controller_count=33;
    assert(!anchor_minigame_state_valid(&state));
    state.controller_count=32;state.input_count=1;state.input_words[0]=8;
    state.inputs[0][0]=state.inputs[0][1]=1;
    assert(anchor_minigame_state_valid(&state));
    state.input_words[0]=9;assert(!anchor_minigame_state_valid(&state));
    state.input_count=0;state.local_ack=0x80000000u;
    assert(!anchor_minigame_state_valid(&state));
    state.local_ack=0;state.full_tick=0x80000000u;
    assert(!anchor_minigame_state_valid(&state));
    for (i=32;i<=33;++i) {
        n=(unsigned int)snprintf(response,sizeof(response),"{\"ready\":1,\"owner\":7,"
            "\"isOwner\":0,\"token\":[1,7,1,1],\"generation\":1,\"tick\":1,"
            "\"running\":1,\"a\":[],\"c\":[");
        for(unsigned int j=0;j<i;++j)
            n+=(unsigned int)snprintf(response+n,sizeof(response)-n,"%s0",j?",":"");
        strcpy(response+n,"]}");
        assert(anchor_minigame_status_decode(response,&status)==(i==32));
    }
    response_state(1,1,1,1,50,0);
    /* Received rows retain their distinct 11-word bound. */
    response[strlen(response)-strlen("[]}")]=0;
    strcat(response,"[[8,1,1,1,1,0,0,0,0,0,0]]}");
    assert(anchor_minigame_status_decode(response,&status));
    n=(unsigned int)strlen(response);
    strcpy(response+n-3,",0]]}");
    assert(!anchor_minigame_status_decode(response,&status));
}
static void test_codec(void)
{
    AnchorMinigameStatus status, previous;
    AnchorMinigameState state;
    char encoded[1024], too_small[8];
    memset(&state, 0, sizeof(state));
    state.ready = state.running = 1;
    state.controller_count = 2;
    state.controller[0] = -2147483647 - 1;
    state.controller[1] = 2147483647;
    state.actor_count = 2;
    state.actor_words[0] = state.actor_words[1] = 3;
    state.actors[0][0] = state.actors[1][0] = 1;
    state.actors[1][1] = 1;
    assert(anchor_minigame_state_encode(&state, encoded, sizeof(encoded)));
    assert(strstr(encoded, "[-2147483648,2147483647]"));
    assert(!anchor_minigame_state_encode(&state, too_small, sizeof(too_small)) && !too_small[0]);
    state.actors[1][1] = 0;
    assert(!anchor_minigame_state_valid(&state));
    response_state(0, 1, 1, 1, 99, 0);
    assert(anchor_minigame_status_decode(response, &status));
    previous = status;
    assert(!anchor_minigame_status_decode("{\"ready\":1,\"ready\":1}", &status));
    assert(!memcmp(&status, &previous, sizeof(status)));
    assert(!anchor_minigame_status_decode(
        "{\"ready\":1,\"owner\":7,\"isOwner\":0,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":1,\"running\":1,\"c\":[2147483648],\"a\":[]}", &status));
    assert(!memcmp(&status, &previous, sizeof(status)));
    assert(!anchor_minigame_status_decode(
        "{\"ready\":1,\"owner\":7,\"isOwner\":0,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":1,\"running\":1,\"c\":[1]}", &status));
    assert(!anchor_minigame_status_decode(
        "{\"ready\":1,\"owner\":7,\"isOwner\":0,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":1,\"running\":1,\"c\":[1],\"a\":[[1,2,1],[1,2,1]]}", &status));
    assert(!anchor_minigame_status_decode(
        "{\"ready\":1,\"owner\":7,\"isOwner\":0,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":1,\"c\":[1],\"a\":[]}", &status));
    assert(!anchor_minigame_status_decode(
        "{\"ready\":1,\"owner\":7,\"isOwner\":0,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":1,\"running\":2,\"c\":[1],\"a\":[]}", &status));
    response_state(0, 0, 1, 1, 99, 0);
    assert(anchor_minigame_status_decode(response, &status));
    assert(!status.ready && status.state.ready && status.state.running);
    assert(anchor_minigame_status_decode(
        "{\"ready\":0,\"owner\":0,\"isOwner\":0,\"token\":[],\"generation\":0}", &status));
    state.actors[1][1] = 1;
    state.input_ack_count = 2;
    state.input_ack[0][0] = state.input_ack[1][0] = 8;
    state.input_ack[0][1] = state.input_ack[1][1] = 1;
    state.input_ack[0][2] = state.input_ack[1][2] = 1;
    state.input_ack[0][3] = state.input_ack[1][3] = 2;
    assert(!anchor_minigame_state_valid(&state));
}
static void test_source_tick_codec(void)
{
    static AnchorMinigameState state;
    static AnchorMinigameStatus status, previous;
    char encoded[1024], previous_encoded[1024];
    anchor_minigame_zero(&state,sizeof(state));
    state.controller_count=1;
    assert(anchor_minigame_state_encode(&state,previous_encoded,sizeof(previous_encoded)));
    state.source_tick=0x7fffffffu;
    assert(anchor_minigame_state_valid(&state));
    assert(anchor_minigame_state_encode(&state,encoded,sizeof(encoded)));
    assert(!strcmp(encoded,previous_encoded)); /* The source tick never changes wire JSON. */
    assert(!strstr(encoded,"source_tick") && !strstr(encoded,"sourceTick"));
    state.source_tick=0x80000000u;
    assert(!anchor_minigame_state_valid(&state));
    assert(!anchor_minigame_state_encode(&state,encoded,sizeof(encoded)) && !encoded[0]);
    response_state(0,1,1,2147483647,50,0);response_fields("\"fullTick\":1");
    assert(anchor_minigame_status_decode(response,&status));
    assert(status.tick==0x7fffffffu && status.state.source_tick==status.tick);
    assert(status.state.full_tick==1);
    previous=status;
    response_state(0,1,1,1,50,0);response_fields("\"sourceTick\":2");
    assert(!anchor_minigame_status_decode(response,&status));
    assert(!memcmp(&status,&previous,sizeof(status)));
    response_state(0,1,1,1,50,0);response_fields("\"source_tick\":2");
    assert(!anchor_minigame_status_decode(response,&status));
    assert(!memcmp(&status,&previous,sizeof(status)));
    strcpy(response,"{\"ready\":1,\"owner\":7,\"isOwner\":0,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":2147483648,\"running\":1,\"c\":[50],\"a\":[]}");
    assert(!anchor_minigame_status_decode(response,&status));
    assert(!memcmp(&status,&previous,sizeof(status)));
    response_state(1,1,2,0,50,1);response_fields("\"fullTick\":17");
    assert(anchor_minigame_status_decode(response,&status));
    assert(!status.state.source_tick && status.state.full_tick==17);
    response_state(0,1,2,0,50,0);
    assert(!anchor_minigame_status_decode(response,&status)); /* Zero needs a legitimate takeover. */
}
static void test_source_tick_propagation(void)
{
    int before;
    fresh();response_state(0,0,1,1,50,0);response_fields("\"fullTick\":1");
    anchor_minigame_sync_frame();
    assert(applies==1 && applied_source_tick==1 && validated_source_tick==1 && !role_ready);
    before=applies;response_state(0,1,1,1,50,0);response_fields("\"fullTick\":1");
    anchor_minigame_sync_frame();
    assert(applies==before+1 && applied_source_tick==1 && validated_source_tick==1 && role_ready);
    before=applies;response_fields("\"localAck\":1");anchor_minigame_sync_frame();
    assert(applies==before+1 && applied_source_tick==1 && applied_local_ack==1);
    before=applies;anchor_minigame_sync_frame();assert(applies==before);
    /* An ACTION's controller is fresh even when the actor census is unchanged. */
    response_state(0,1,1,2,49,0);response_fields("\"fullTick\":1,\"localAck\":1");
    anchor_minigame_sync_frame();
    assert(applies==before+1 && applied_source_tick==2 && validated_source_tick==2);
    assert(applied_full_tick==1 && applied_generation==1 && native_value==49);
    response_state(0,1,1,3,48,0);response_fields("\"fullTick\":3,\"localAck\":1");
    anchor_minigame_sync_frame();assert(applied_source_tick==3 && applied_full_tick==3);
    response_state(0,1,1,4,47,0);response_fields("\"fullTick\":3,\"localAck\":2");
    apply_ok=0;anchor_minigame_sync_frame();
    assert(applied_source_tick==4 && validated_source_tick==4 && s_tick==3 && s_local_ack==1);
    assert(native_value==48 && !role_ready);
    apply_ok=1;anchor_minigame_sync_frame();
    assert(applied_source_tick==4 && s_tick==4 && s_local_ack==2 && native_value==47);
    before=applies;response_state(0,1,1,3,48,0);response_fields("\"fullTick\":3");
    anchor_minigame_sync_frame();assert(applies==before && s_tick==4 && !role_ready);
    /* The inherited census may exceed a new owner's zero source tick. */
    response_state(1,1,2,0,47,1);response_fields("\"fullTick\":3,\"localAck\":2");
    anchor_minigame_sync_frame();
    assert(applies==before+1 && !applied_source_tick && !validated_source_tick);
    assert(applied_generation==2 && applied_full_tick==3 && s_generation==2 && !s_tick);
    before=applies;anchor_minigame_sync_frame();assert(applies==before);
}
static void test_sync(void)
{
    int before;
    fresh();
    assert(!anchor_minigame_sync_native_allowed(1));
    response_state(0, 0, 1, 1, 50, 0);
    anchor_minigame_sync_frame();
    assert(applies == 1 && native_value == 50 && !role_ready);
    assert(anchor_minigame_sync_waiting(1));
    response_state(0, 1, 1, 1, 50, 0);
    anchor_minigame_sync_frame();
    assert(role_ready && !role_owner && !anchor_minigame_sync_native_allowed(1));
    before = applies;
    anchor_minigame_sync_frame();
    assert(applies == before);
    response_state(0, 1, 1, 2, 150, 0);
    anchor_minigame_sync_frame();
    assert(native_value == 50 && applies == before && !role_ready);
    response_state(0, 1, 1, 2, 60, 0);
    apply_ok = 0;
    anchor_minigame_sync_frame();
    assert(native_value == 50 && !role_ready);
    apply_ok = 1;
    anchor_minigame_sync_frame();
    assert(native_value == 60 && role_ready);
    response_state(0, 1, 1, 10, 60, 0);
    anchor_minigame_sync_frame();
    assert(s_tick == 10);
    response_state(1, 1, 2, 0, 70, 1);
    apply_ok = 0;
    anchor_minigame_sync_frame();
    assert(s_generation == 1 && s_tick == 10 && !role_ready && native_value == 60);
    assert(strstr(submitted, "\"adoptedGeneration\":1"));
    apply_ok = 1;
    anchor_minigame_sync_frame();
    assert(native_value == 70 && role_owner && role_ready);
    assert(anchor_minigame_sync_native_allowed(1));
    native_value = 71;
    response_state(1, 1, 2, 1, 70, 0);
    anchor_minigame_sync_frame();
    assert(native_value == 71 && role_ready && s_tick == 1);
    assert(strstr(submitted, "\"adoptedGeneration\":2"));
    response_state(1, 1, 1, 100, 20, 0);
    anchor_minigame_sync_frame();
    assert(native_value == 71 && !role_ready);
    strcpy(response, "{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":2,\"tick\":3,\"result\":2,\"takeover\":0,"
        "\"running\":1,\"c\":[71],\"a\":[],\"inputs\":[[8,1,1,1,1,5]]}");
    anchor_minigame_sync_frame();
    assert(results == 1 && input_calls == 1 && anchor_minigame_sync_returning());
    anchor_minigame_sync_frame();
    assert(input_calls == 1 && strstr(submitted, "\"inputAck\":[[8,1,1,1]]"));
    strcpy(response, "invalid");
    anchor_minigame_sync_frame();
    assert(!role_ready);
    connected = 0;
    anchor_minigame_sync_frame();
    assert(anchor_minigame_sync_native_allowed(1));
    connected = 1;
    anchor_minigame_sync_begin_load();
    assert(!anchor_minigame_sync_returning() && !anchor_minigame_sync_game());
    assert(resets > 0 && calls > 0);
}
static void test_initial_owner_and_deferred_input(void)
{
    fresh();
    strcpy(response, "{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":0,\"result\":0,\"takeover\":0,\"inputs\":[]}");
    capture_ready = 0;
    anchor_minigame_sync_frame();
    assert(!anchor_minigame_sync_native_allowed(1));
    capture_ready = 1;
    anchor_minigame_sync_frame();
    assert(anchor_minigame_sync_native_allowed(1));
    strcpy(response, "{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":1,\"result\":0,\"takeover\":0,"
        "\"running\":1,\"c\":[0],\"a\":[],\"inputs\":[[8,1,1,2,1,5]]}");
    input_ok = 0;
    anchor_minigame_sync_frame();
    assert(!s_input_ack_count);
    anchor_minigame_sync_frame();
    assert(strcmp(submitted, "null") != 0);
    assert(strstr(submitted, "\"inputAck\":[]"));
    input_ok = 1;
    anchor_minigame_sync_frame();
    assert(s_input_ack_count == 1 && s_input_ack[0][3] == 2);
    anchor_minigame_sync_frame();
    assert(strstr(submitted, "\"inputAck\":[[8,1,1,2]]"));
    strcpy(response, "{\"ready\":1,\"owner\":9,\"isOwner\":0,\"token\":[1,9,2,1],"
        "\"generation\":1,\"tick\":1,\"running\":1,\"c\":[10],\"a\":[]}");
    anchor_minigame_sync_frame();
    assert(!role_ready && native_value == 0);
}
static void test_full_pool_graph(void)
{
    static AnchorMinigameState state;
    static AnchorMinigameStatus status;
    static char encoded[ANCHOR_MINIGAME_JSON_SIZE];
    static char graph[ANCHOR_MINIGAME_JSON_SIZE];
    unsigned int i, j;
    state.controller_count = ANCHOR_MINIGAME_MAX_WORDS;
    state.ready = state.running = 1;
    state.actor_count = ANCHOR_MINIGAME_MAX_ACTORS;
    for (i = 0; i < state.actor_count; ++i) {
        state.actor_words[i] = ANCHOR_MINIGAME_MAX_ACTOR_WORDS;
        state.actors[i][0] = 1;
        state.actors[i][1] = (int)i;
        state.actors[i][2] = 1;
        for (j = 3; j < ANCHOR_MINIGAME_MAX_ACTOR_WORDS; ++j)
            state.actors[i][j] = -2147483647 - 1;
    }
    assert(anchor_minigame_state_encode(&state, encoded, sizeof(encoded)));
    /* Exercise the decoder's complete graph using the encoded row payload. */
    {
        char *a = strstr(encoded, "\"a\":[");
        char *end = strstr(encoded, ",\"inputs\":");
        int n;
        assert(a && end);
        n = snprintf(graph, sizeof(graph), "{\"ready\":1,\"owner\":7,\"isOwner\":0,"
            "\"token\":[1,7,1,1],\"generation\":1,\"tick\":1,\"running\":1,\"c\":[0],%.*s}",
            (int)(end - a), a);
        assert(n > 0 && (unsigned int)n < sizeof(graph));
    }
    assert(anchor_minigame_status_decode(graph, &status));
    assert(status.state.actor_count == 64 && status.state.actor_words[63] == 40);
    {
        unsigned int n=(unsigned int)strlen(graph);
        strcpy(graph+n-3,",0]]}");
        assert(!anchor_minigame_status_decode(graph,&status)); /* 41 words. */
        strcpy(graph+n-3,"]]}");
    }
    state.actor_count = 65;
    assert(!anchor_minigame_state_valid(&state));
    /* Add a 65th row to a valid 64-row status to prove decode is bounded too. */
    {
        unsigned int n = (unsigned int)strlen(graph);
        assert(n > 2 && graph[n - 2] == ']' && graph[n - 1] == '}');
        strcpy(graph + n - 2, ",[1,64,1]]}");
    }
    assert(!anchor_minigame_status_decode(graph, &status));
}
static void test_committed_bridge_status(void)
{
    int before, queries;
    fresh();
    response_state(0,1,1,1,50,0);response_fields("\"localAck\":5,\"members\":[7,8]");
    check_role_cache=1;role_expected_ack=5;role_expected_participant=8;
    anchor_minigame_sync_frame();
    check_role_cache=0;
    assert(applied_local_ack==5 && applied_generation==1);
    before=calls;queries=connection_queries;
    assert(anchor_minigame_sync_local_ack(1)==5 && anchor_minigame_sync_local_ack(2)==0);
    assert(anchor_minigame_sync_participant_current(1,8));
    assert(!anchor_minigame_sync_participant_current(2,8));
    assert(!anchor_minigame_sync_participant_current(1,0));
    assert(calls==before && connection_queries==queries); /* No Python queries. */
    response_state(0,1,1,2,50,0);response_fields("\"localAck\":3,\"members\":[7]");
    anchor_minigame_sync_frame();
    assert(anchor_minigame_sync_local_ack(1)==5 && !anchor_minigame_sync_participant_current(1,8));
    /* Invalid/stale status cannot advance receipts, and revokes target membership. */
    response_state(0,1,1,1,50,0);response_fields("\"localAck\":900,\"members\":[9]");
    anchor_minigame_sync_frame();
    assert(s_local_ack==5 && !s_participant_count && anchor_minigame_sync_local_ack(1)==0);
    assert(!anchor_minigame_sync_participant_current(1,9));
    response_state(0,1,1,3,50,0);response_fields("\"localAck\":900,\"members\":[9,9]");
    anchor_minigame_sync_frame();assert(s_local_ack==5 && !s_participant_count);
    response_state(0,0,1,3,50,0);response_fields("\"localAck\":900,\"members\":[9]");
    anchor_minigame_sync_frame();
    assert(applied_local_ack==900 && s_local_ack==5 && !s_participant_count);
    assert(!anchor_minigame_sync_participant_current(1,9) && anchor_minigame_sync_local_ack(1)==0);
    response_state(0,1,1,4,50,0);response_fields("\"localAck\":6,\"members\":[7,8]");
    apply_ok=0;anchor_minigame_sync_frame();
    assert(s_local_ack==5 && !s_participant_count && anchor_minigame_sync_local_ack(1)==0);
    apply_ok=1;anchor_minigame_sync_frame();
    assert(anchor_minigame_sync_local_ack(1)==6 && anchor_minigame_sync_participant_current(1,8));
    response_state(1,1,2,0,70,1);response_fields("\"localAck\":6,\"members\":[7,8]");
    apply_ok=0;anchor_minigame_sync_frame();
    assert(applied_generation==2 && s_generation==1 && s_local_ack==6 && !s_participant_count);
    apply_ok=1;anchor_minigame_sync_frame();
    assert(applied_generation==2 && applied_local_ack==6 && s_generation==2);
    assert(anchor_minigame_sync_local_ack(1)==6);
    response_state(1,1,2,1,70,0); /* Old bridge defaults do not rewind accepted ACK. */
    anchor_minigame_sync_frame();
    assert(anchor_minigame_sync_local_ack(1)==6 && !s_participant_count);
    response_state(1,1,2,2,70,0);response_fields("\"localAck\":900,\"members\":[9]");
    capture_ok=0;anchor_minigame_sync_frame();
    assert(s_local_ack==6 && !s_participant_count && anchor_minigame_sync_local_ack(1)==0);
    capture_ok=1;
    response_null=1;anchor_minigame_sync_frame();
    assert(s_local_ack==6 && !s_participant_count);
    response_null=0;
    strcpy(response,"{\"ready\":1,\"owner\":9,\"isOwner\":0,\"token\":[1,9,2,1],"
        "\"generation\":2,\"tick\":2,\"running\":1,\"c\":[50],\"a\":[],"
        "\"localAck\":900,\"members\":[9]}");
    anchor_minigame_sync_frame();assert(s_local_ack==6 && !s_participant_count);
    connected=0;before=calls;
    anchor_minigame_sync_frame();assert(calls==before && s_local_ack==0);
    assert(anchor_minigame_sync_local_ack(1)==0 && !anchor_minigame_sync_participant_current(1,8));
    connected=1;anchor_minigame_sync_frame(); /* Fresh interaction may bind a new token. */
    assert(anchor_minigame_sync_local_ack(1)==900 && anchor_minigame_sync_participant_current(1,9));
    D_800C7AB2=0x40;
    assert(anchor_minigame_sync_local_ack(1)==0 && !anchor_minigame_sync_participant_current(1,9));
    D_800C7AB2=0x1e0;fresh();
    assert(s_local_ack==0 && !s_participant_count);
    response_state(1,1,1,1,0,0);response_fields("\"localAck\":0,\"members\":[7,8]");
    anchor_minigame_sync_frame();
    strcpy(response,"{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":2,\"inputs\":[[8,1,1,20,1,5]],"
        "\"localAck\":0,\"members\":[7,8]}");
    input_ok=0;anchor_minigame_sync_frame();
    assert(!s_input_ack_count && anchor_minigame_sync_local_ack(1)==0);
    input_ok=1;anchor_minigame_sync_frame();
    assert(s_input_ack_count==1 && anchor_minigame_sync_local_ack(1)==0);
    anchor_minigame_sync_begin_load();
    assert(!s_local_ack && !s_participant_count && anchor_minigame_sync_local_ack(1)==0);
    /* Even a same-tick bridge receipt must adopt its canonical state before
     * pending local claims can retire. A failed apply cannot commit the ACK. */
    fresh();response_state(0,1,1,1,50,0);
    response_fields("\"localAck\":1,\"members\":[7,8]");
    anchor_minigame_sync_frame();assert(anchor_minigame_sync_local_ack(1)==1);
    before=applies;response_state(0,1,1,1,51,0);
    response_fields("\"localAck\":2,\"members\":[7,8]");
    apply_ok=0;anchor_minigame_sync_frame();
    assert(applies==before+1 && applied_local_ack==2 && applied_generation==1);
    assert(native_value==50 && s_local_ack==1 && !s_participant_count);
    assert(anchor_minigame_sync_local_ack(1)==0);
    apply_ok=1;anchor_minigame_sync_frame();
    assert(native_value==51 && s_tick==1 && anchor_minigame_sync_local_ack(1)==2);
    before=applies;anchor_minigame_sync_frame();assert(applies==before);
}
static void test_full_checkpoint_marker(void)
{
    int before;
    fresh();response_state(0,1,1,1,50,0);
    response_fields("\"fullTick\":1");anchor_minigame_sync_frame();
    assert(applied_full_tick==1 && applied_generation==1);
    /* ACTION advances k while preserving the prior complete snapshot marker. */
    before=applies;response_state(0,1,1,2,50,0);
    response_fields("\"fullTick\":1");anchor_minigame_sync_frame();
    assert(applies==before+1 && applied_full_tick==1);
    /* Identical scalar rows from a new complete SYNC still carry freshness. */
    before=applies;response_state(0,1,1,3,50,0);
    response_fields("\"fullTick\":3");anchor_minigame_sync_frame();
    assert(applies==before+1 && applied_full_tick==3);
    response_state(1,1,2,0,50,1);response_fields("\"fullTick\":3");
    anchor_minigame_sync_frame();
    assert(applied_generation==2 && applied_full_tick==3 && s_tick==0);
    /* A takeover's inherited fullTick legitimately exceeds the new tick. */
    before=applies;anchor_minigame_sync_frame();assert(applies==before);
}
static void test_ack_history_churn(void)
{
    unsigned int i;
    int before;
    fresh();
    response_state(1,1,1,1,0,0);anchor_minigame_sync_frame();
    s_input_ack_count=ANCHOR_MINIGAME_MAX_INPUTS;
    for(i=0;i<s_input_ack_count;++i) {
        s_input_ack[i][0]=(int)i+100;s_input_ack[i][1]=1;s_input_ack[i][2]=1;s_input_ack[i][3]=10;
    }
    strcpy(response, "{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":2,\"result\":0,\"takeover\":0,"
        "\"inputs\":[[8,1,1,1,1,5]]}");
    before=input_calls;anchor_minigame_sync_frame();
    assert(input_calls==before+1 && s_input_ack_count==32 && s_input_ack[0][0]==8);
    assert(strstr(submitted,"[100,1,1,10]")); /* Old receipt submitted first. */
    anchor_minigame_sync_frame();assert(input_calls==before+1);
    assert(strstr(submitted,"[8,1,1,1]"));
}
static void test_outgoing_sequence_order(void)
{
    fresh();capture_inputs=1;response_state(1,1,1,1,0,0);
    anchor_minigame_sync_frame();
    assert(strstr(submitted,"\"inputs\":[[17,2,117],[19,3],[20,1]]"));
}
static void test_same_session_new_visit(void)
{
    int before;
    fresh();response_state(1,1,1,1,0,0);anchor_minigame_sync_frame();
    strcpy(response,"{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":2,\"inputs\":[[8,1,1,5,1,5]]}");
    before=input_calls;anchor_minigame_sync_frame();assert(input_calls==before+1);
    strcpy(response,"{\"ready\":1,\"owner\":7,\"isOwner\":1,\"token\":[1,7,1,1],"
        "\"generation\":1,\"tick\":3,\"inputs\":[[8,1,2,1,1,5]]}");
    anchor_minigame_sync_frame();assert(input_calls==before+2);
    assert(strstr(submitted,"\"inputAck\":[[8,1,1,5]]"));
    anchor_minigame_sync_frame();assert(input_calls==before+2);
    assert(strstr(submitted,"\"inputAck\":[[8,1,1,5],[8,1,2,1]]"));
}
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
static void ebi_response(int ready,int tick)
{
    snprintf(response,sizeof(response),
        "{\"ready\":%d,\"owner\":7,\"isOwner\":1,\"token\":[2,7,1,1],"
        "\"generation\":1,\"tick\":%d,\"result\":0,\"takeover\":0,\"inputs\":[]}",ready,tick);
}
static void test_ebi_diagnostics(void)
{
    int before, captures, validations, logs;
    anchor_minigame_sync_begin_load();D_800C7AB2=0x1e1;
    anchor_minigame_sync_finish_load();expected_game=2;
    before=calls;logs=diagnostic_calls;
    anchor_minigame_sync_frame();
    assert(calls==before && diagnostic_calls==logs+1);
    assert(strstr(diagnostic_line,"bound=0 online=1") && strstr(diagnostic_line,"reason=unbound"));
    assert(anchor_minigame_sync_bind(&ebi_adapter));
    native_value=0;capture_ready=1;capture_inputs=0;ebi_response(1,0);
    capture_ok=0;before=calls;captures=capture_calls;validations=validate_calls;
    anchor_minigame_sync_frame();
    assert(calls==before+1 && capture_calls==captures+1 && validate_calls==validations);
    assert(!strcmp(submitted,"null") && !anchor_minigame_sync_native_allowed(2));
    assert(strstr(diagnostic_line,"capture=0 adapterValid=0 stateValid=0 encoded=0"));
    assert(strstr(diagnostic_line,"reason=owner-graph"));
    capture_ok=1;native_value=101;captures=capture_calls;validations=validate_calls;
    anchor_minigame_sync_frame();
    assert(capture_calls==captures+1 && validate_calls==validations+1 && !strcmp(submitted,"null"));
    assert(strstr(diagnostic_line,"capture=1 adapterValid=0 stateValid=0 encoded=0"));
    native_value=0;invalid_generic=1;anchor_minigame_sync_frame();
    assert(strstr(diagnostic_line,"capture=1 adapterValid=1 stateValid=0 encoded=0"));
    assert(!strcmp(submitted,"null") && !anchor_minigame_sync_native_allowed(2));
    invalid_generic=0;capture_ready=0;anchor_minigame_sync_frame();
    assert(strstr(diagnostic_line,"capture=1 adapterValid=1 stateValid=1 encoded=1 captureReady=0"));
    assert(!role_ready && strcmp(submitted,"null"));
    logs=diagnostic_calls;before=calls;captures=capture_calls;
    for(int i=1;i<=119;++i) { ebi_response(1,i);anchor_minigame_sync_frame(); }
    assert(diagnostic_calls==logs && calls==before+119 && capture_calls==captures+119);
    ebi_response(1,120);anchor_minigame_sync_frame();
    assert(diagnostic_calls==logs+1 && strstr(diagnostic_line,"tick=120"));
    capture_ready=1;before=calls;captures=capture_calls;validations=validate_calls;
    anchor_minigame_sync_frame();
    assert(calls==before+1 && capture_calls==captures+1 && validate_calls==validations+1);
    assert(anchor_minigame_sync_native_allowed(2) && strstr(diagnostic_line,"reason=owner-ready"));
    logs=diagnostic_calls;
    for(int i=121;i<=260;++i) { ebi_response(1,i);anchor_minigame_sync_frame(); }
    assert(diagnostic_calls==logs); /* Ready clocks alone do not flood logs. */
    strcpy(response,"invalid");anchor_minigame_sync_frame();
    assert(!role_ready && strstr(diagnostic_line,"reply=1 decoded=0 owner=-1"));
    assert(strstr(diagnostic_line,"reason=bad-reply"));
    response_null=1;anchor_minigame_sync_frame();
    assert(strstr(diagnostic_line,"reply=0 decoded=0") && strstr(diagnostic_line,"reason=no-reply"));
    response_null=0;ebi_response(1,261);anchor_minigame_sync_frame();
    assert(role_ready); /* Logging did not suppress the valid retry. */
    connected=0;before=calls;anchor_minigame_sync_frame();
    assert(calls==before && strstr(diagnostic_line,"bound=1 online=0") && strstr(diagnostic_line,"reason=offline"));
    connected=1;anchor_minigame_sync_begin_load();D_800C7AB2=0x1e0;expected_game=1;
    anchor_minigame_sync_finish_load();assert(anchor_minigame_sync_bind(&adapter));
}
#endif
int main(void)
{
    test_codec(); test_bridge_codec(); test_sync(); test_initial_owner_and_deferred_input();
    test_source_tick_codec(); test_source_tick_propagation();
    test_full_pool_graph();
    test_ack_history_churn();
    test_outgoing_sequence_order();
    test_same_session_new_visit();
    test_committed_bridge_status();
    test_full_checkpoint_marker();
#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
    test_ebi_diagnostics();
#endif
    puts("minigame codec/coordinator tests passed");
    return 0;
}
