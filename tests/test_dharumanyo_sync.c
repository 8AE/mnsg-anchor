#include "anchor_dharumanyo_sync.h"
#include "anchor_dharumanyo_native.h"
#include "anchor_dharumanyo_damage.h"
#include "anchor_player_models.h"
#include "utils/anchor_dharumanyo_codec.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned short D_800C7AB2 = 0x49;
static unsigned short system_words[0x3af00 / 2];
void *D_8015C5C8_15D1C8 = system_words;
static int connected = 1, loaded = 1, ready = 1, root = 1, local_pause;
static int native_active, native_owner, native_paused;
static int damage_active, damage_owner, damage_paused;
static int apply_ok = 1, applied, hits, target_cleared, supplied_state, updates, exits;
static int send_ok = 1, sends, epoch = 1, native_script_pause;
static unsigned int visit = 11;
static int defer_apply, pending, allow_adoption = 1;
static AnchorDharumanyoNativeSnapshot pending_state;
static AnchorDharumanyoHit queued;
static AnchorDharumanyoNativeSnapshot local, incoming;
static char response[ANCHOR_DHARUMANYO_STATUS_JSON_SIZE];
static char wire_state[ANCHOR_DHARUMANYO_STATE_JSON_SIZE];

int anchor_is_connected(void) { return connected; }
int anchor_is_disabled(void) { return 0; }
int item_sync_save_is_loaded(void) { return loaded; }
int anchor_dialog_world_paused(void) { return local_pause; }
int anchor_dharumanyo_native_world_paused(void) { return native_script_pause; }
void recomp_free(void *pointer) { free(pointer); }
void anchor_dharumanyo_native_tick(void)
{
    if (pending && allow_adoption)
    {
        local = pending_state;
        pending = 0;
        ++applied;
    }
}
int anchor_dharumanyo_native_ready(void) { return ready; }
int anchor_dharumanyo_native_snapshot_ready(void) { return ready; }
unsigned int anchor_dharumanyo_native_visit(void) { return visit; }
int anchor_player_models_get_epoch(void) { return epoch; }
void *anchor_dharumanyo_native_root_task(void) { return root ? &root : 0; }
void anchor_dharumanyo_native_set_role(int active, int owner, int paused)
{
    native_active = active;
    native_owner = owner;
    native_paused = paused;
}
int anchor_dharumanyo_native_capture(AnchorDharumanyoNativeSnapshot *out)
{
    *out = local;
    return ready && !pending;
}
int anchor_dharumanyo_native_apply(const AnchorDharumanyoNativeSnapshot *state)
{
    if (!apply_ok)
        return 0;
    if (defer_apply)
    {
        pending_state = *state;
        pending = 1;
        return 1;
    }
    local = *state;
    ++applied;
    return 1;
}
void anchor_dharumanyo_damage_set_context(int active, int owner, int paused,
                                           unsigned int encounter)
{
    (void)encounter;
    damage_active = active;
    damage_owner = owner;
    damage_paused = paused;
}
int anchor_dharumanyo_damage_take_local_hit(AnchorDharumanyoHit *out)
{
    if (!queued.sequence)
        return 0;
    *out = queued;
    queued.sequence = 0;
    return 1;
}
int anchor_dharumanyo_damage_apply(int amount)
{
    assert(damage_active && damage_owner && !damage_paused && amount == 1);
    ++hits;
    --local.carrier[DHAR_CARRIER_LIVES];
    return 1;
}
int anchor_player_models_get_boss_target(int current, int rotate,
                                          AnchorBossTarget *out)
{
    (void)current;
    (void)rotate;
    (void)out;
    return 0;
}
void anchor_dharumanyo_native_set_target(float x, float y, float z)
{
    (void)x;
    (void)y;
    (void)z;
}
void anchor_dharumanyo_native_clear_target(void) { ++target_cleared; }
int anchor_send_dharumanyo_hit(int sequence)
{
    assert(sequence > 0);
    ++sends;
    return send_ok;
}
char *anchor_dharumanyo_update(int is_ready, unsigned int room_visit,
                               int paused, const char *state)
{
    char *copy = malloc(strlen(response) + 1);
    (void)paused;
    ++updates;
    supplied_state = strcmp(state, "null") != 0;
    if (!is_ready && !room_visit)
        ++exits;
    strcpy(copy, response);
    return copy;
}

static void status(int role, int owner, unsigned int term,
                   unsigned int revision, int paused, int encounter_visit,
                   int with_state, const char *hit_json)
{
    assert(anchor_dharumanyo_state_encode(&incoming, wire_state,
                                           sizeof(wire_state)));
    snprintf(response, sizeof(response),
        "{\"role\":%d,\"owner\":%d,\"term\":%u,\"revision\":%u,"
        "\"paused\":%d,\"encounter\":[10,20,%d],\"state\":%s,\"hits\":%s}",
        role, owner, term, revision, paused, encounter_visit,
        with_state ? wire_state : "null", hit_json);
}

int main(void)
{
    local.root[DHAR_PHASE] = 1;
    local.carrier[DHAR_CARRIER_LIVES] = 12;
    incoming = local;
    strcpy(response,
        "{\"role\":0,\"owner\":0,\"term\":0,\"revision\":0,\"paused\":0,"
        "\"encounter\":[0,0,0],\"state\":null,\"hits\":[]}");
    anchor_dharumanyo_sync_frame();
    assert(native_active && !native_owner && native_paused && supplied_state);
    status(1, 10, 1, 1, 0, 11, 1, "[]");
    anchor_dharumanyo_sync_frame();
    assert(applied == 1 && native_owner && !native_paused && target_cleared);
    local.carrier[DHAR_CARRIER_LIVES] = 10;
    status(1, 10, 1, 2, 0, 11, 1, "[[22,33,44,55,1]]");
    anchor_dharumanyo_sync_frame();
    assert(applied == 1 && local.carrier[DHAR_CARRIER_LIVES] == 9 &&
           hits == 1 && supplied_state);
    queued = (AnchorDharumanyoHit){101, 1};
    send_ok = 0;
    status(1, 10, 1, 3, 0, 11, 0, "[]");
    anchor_dharumanyo_sync_frame();
    assert(sends == 1);
    send_ok = 1;
    anchor_dharumanyo_sync_frame();
    assert(sends == 2);
    anchor_dharumanyo_sync_frame();
    assert(sends == 2);
    incoming.carrier[DHAR_CARRIER_LIVES] = 7;
    status(2, 9, 2, 4, 0, 11, 1, "[]");
    anchor_dharumanyo_sync_frame();
    assert(applied == 2 && !native_owner && !native_paused &&
           local.carrier[DHAR_CARRIER_LIVES] == 7);
    anchor_dharumanyo_sync_frame();
    assert(applied == 2 && !supplied_state);
    local_pause = 1;
    incoming.carrier[DHAR_CARRIER_LIVES] = 5;
    status(2, 9, 2, 5, 0, 11, 1, "[]");
    anchor_dharumanyo_sync_frame();
    assert(applied == 2 && native_paused);
    local_pause = 0;
    anchor_dharumanyo_sync_frame();
    assert(applied == 3 && local.carrier[DHAR_CARRIER_LIVES] == 5 &&
           !native_paused);
    system_words[0x3ae26 / 2] = 1;
    status(1, 10, 3, 6, 1, 11, 0, "[]");
    anchor_dharumanyo_sync_frame();
    assert(native_paused && damage_paused);
    system_words[0x3ae26 / 2] = 0;
    native_script_pause = 1;
    anchor_dharumanyo_sync_frame();
    assert(native_paused);
    native_script_pause = 0;
    status(2, 8, 4, 7, 0, 11, 1, "[]");
    apply_ok = 0;
    anchor_dharumanyo_sync_frame();
    assert(native_paused && !native_owner && applied == 3);
    apply_ok = 1;
    anchor_dharumanyo_sync_frame();
    assert(applied == 4 && !supplied_state);
    strcpy(response, "{\"role\":1}");
    anchor_dharumanyo_sync_frame();
    assert(native_paused && applied == 4);
    connected = 0;
    anchor_dharumanyo_sync_frame();
    assert(!native_active && !damage_active && exits == 1);
    anchor_dharumanyo_sync_frame();
    assert(exits == 1);
    connected = 1;
    incoming.carrier[DHAR_CARRIER_LIVES] = 12;
    status(1, 10, 1, 1, 0, 12, 1, "[]");
    anchor_dharumanyo_sync_frame();
    assert(applied == 5 && native_owner);
    queued = (AnchorDharumanyoHit){102, 1};
    send_ok = 0;
    anchor_dharumanyo_sync_frame();
    assert(sends == 3);
    ++epoch;
    send_ok = 1;
    anchor_dharumanyo_sync_frame();
    assert(sends == 3);
    defer_apply = 1;
    allow_adoption = 0;
    incoming.carrier[DHAR_CARRIER_LIVES] = 4;
    status(2, 9, 2, 2, 0, 12, 1, "[]");
    anchor_dharumanyo_sync_frame();
    assert(pending && applied == 5);
    anchor_dharumanyo_sync_frame();
    assert(pending && !supplied_state);
    allow_adoption = 1;
    anchor_dharumanyo_sync_frame();
    assert(!pending && applied == 6);
    D_800C7AB2 = 0x1a;
    anchor_dharumanyo_sync_frame();
    assert(exits == 2 && !native_active);
    puts("Dharumanyo coordinator tests passed");
}
