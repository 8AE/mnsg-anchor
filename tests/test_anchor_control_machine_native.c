#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*TestCallback)(void *, void *);
static union { void *align; unsigned char bytes[0x200]; } child_store;
static union { void *align; unsigned char bytes[0x200]; } root_store;
static union { void *align; unsigned char bytes[0x200]; } object_store;
static union { void *align; unsigned char bytes[0x200]; } root_object_store;
static union { void *align; unsigned char bytes[0x200]; } shot_store;
static union { void *align; unsigned char bytes[0x200]; } shot_object_store;
static TestCallback child_ai;
static int bound = 1, quest_ready = 1, terminal_events, releases;
static int projectile_apply_ok = 1;
static int projectile_context_owner, projectile_context_paused;
static unsigned int native_shot_births, native_shot_targets;

#define CM_NATIVE_AI(p) (child_ai)
#define ANCHOR_CONTROL_MACHINE_NATIVE_HOST_TEST
#include "../src/bosses/control_machine/anchor_control_machine_native.c"
#include "../src/bosses/control_machine/anchor_control_machine_codec.c"

unsigned short D_800C7AB2 = 0x155u;
unsigned short D_8015CDB6;
unsigned short D_8015CC30_15D830;
unsigned short D_8015CDBC = 5u;
unsigned char *D_8015C5C8_15D1C8;

void anchor_control_machine_projectiles_set_context(void *child,
    unsigned int visit, int active, int owner, int paused)
{ (void)child; (void)visit; (void)active;
  projectile_context_owner = owner; projectile_context_paused = paused; }
int anchor_control_machine_projectiles_capture(AnchorControlMachineSnapshot *s)
{ s->projectile_count = 0; return 1; }
int anchor_control_machine_projectiles_apply(const AnchorControlMachineSnapshot *s)
{ (void)s; return projectile_apply_ok; }
void anchor_control_machine_projectiles_reset(void) {}

void *anchor_control_machine_bound_task(void)
{ return bound ? child_store.bytes : 0; }
void *anchor_control_machine_bound_root(void)
{ return bound ? root_store.bytes : 0; }
unsigned int anchor_boss_invite_world_visit(void) { return 1u; }
int anchor_world_quest_koryuta_local_ready(void) { return quest_ready; }
int anchor_world_quest_koryuta_release_terminal(void)
{ ++releases; quest_ready = 0; return 1; }
void func_80023DF0_249F0(unsigned int event)
{ assert(event == 0u); ++terminal_events; }
void func_8022026C_5DB73C(void *task, unsigned int r,
                           unsigned int g, unsigned int b)
{ (void)task; (void)r; (void)g; (void)b; }
void *func_80036158_36D58(void *task, void *node, unsigned int release)
{ (void)task; (void)node; (void)release; return 0; }
void *func_802171A8_5D2678(void *task, TestCallback callback,
                             unsigned char group)
{
    (void)task;
    if (callback == func_0800413C_70523C && group == 10u) {
        ++native_shot_births;
        CM_NATIVE_PTR(shot_store.bytes, 0x18) = shot_object_store.bytes;
        return shot_store.bytes;
    }
    return 0;
}
float func_80003E10_4A10(unsigned int angle)
{ (void)angle; return 0.5f; }
float func_80003EA0_4AA0(unsigned int angle)
{ (void)angle; return 1.0f; }
void func_8001E4A4_1F0A4(float matrix[16], short pitch,
                         short yaw, short roll)
{ (void)matrix; (void)pitch; (void)yaw; (void)roll; }
void func_8021A858_5D5D28(float matrix[16], float vector[3])
{ (void)matrix; (void)vector; }
unsigned int func_800141C4_14DC4(unsigned int resource)
{ assert(resource == 0x2eu); return 0x12345678u; }
unsigned int func_8021B988_5D6E58(void *task, float x, float y,
                                   float z, float speed)
{ assert(task == shot_store.bytes && speed == 2.0f);
  assert(x == -45.f && y == 50.f && z == 200.f);
  ++native_shot_targets; return 1u; }
void func_0800413C_70523C(void *task, void *object)
{ (void)task; (void)object; }

#define EMPTY_CALLBACK(name) \
    void name(void *task, void *object) { (void)task; (void)object; }
EMPTY_CALLBACK(func_08002F58_704058)
EMPTY_CALLBACK(func_08002F8C_70408C)
EMPTY_CALLBACK(func_08003054_704154)
EMPTY_CALLBACK(func_080030B0_7041B0)
EMPTY_CALLBACK(func_0800316C_70426C)
EMPTY_CALLBACK(func_080031D8_7042D8)
EMPTY_CALLBACK(func_08003634_704734)
EMPTY_CALLBACK(func_08003678_704778)
EMPTY_CALLBACK(func_080036CC_7047CC)
EMPTY_CALLBACK(func_802112EC_5CC7BC)

static AnchorControlMachineSnapshot state(unsigned int phase,
                                          unsigned int hp,
                                          unsigned int status)
{
    AnchorControlMachineSnapshot s;
    memset(&s, 0, sizeof(s));
    s.root[CM_PHASE] = phase;
    s.root[CM_TIMER] = phase >= 6u ? 0x80u : 0x50u;
    s.root[CM_HP] = hp;
    s.root[CM_SCALE_X] = 0x3f800000u;
    s.root[CM_SCALE_Y] = 0x3f800000u;
    s.root[CM_SCALE_Z] = 0x3f800000u;
    s.root[CM_STATUS] = status;
    s.root[CM_STATUS_HP] = hp <= 5u ? hp : 0u;
    s.root[CM_FLASH_ALPHA] = 256u;
    s.root[CM_COLOUR] = 0xff000000u;
    s.root[CM_COMMAND] = 1u;
    assert(anchor_control_machine_snapshot_valid(&s));
    return s;
}

static void codec_boundaries(void)
{
    AnchorControlMachineSnapshot live = state(5u, 0u, 1u);
    AnchorControlMachineSnapshot dead = state(6u, 255u, 2u);
    AnchorControlMachineStatus decoded;
    char encoded[ANCHOR_CONTROL_MACHINE_STATE_JSON_SIZE];
    char response[ANCHOR_CONTROL_MACHINE_STATUS_JSON_SIZE];
    char oversized[ANCHOR_CONTROL_MACHINE_STATUS_JSON_SIZE + 1];
    char *root_words;
    char *fight_version;
    unsigned int length;

    live.projectile_count = 1u;
    live.projectile[0][CM_PROJECTILE_ID] = 7u;
    live.projectile[0][CM_PROJECTILE_BORN] = 123u;
    live.projectile[0][CM_PROJECTILE_TIMER] = 80u;
    live.projectile[0][CM_PROJECTILE_X] = 0x3f800000u;
    assert(anchor_control_machine_state_encode(&live, encoded,
                                               sizeof(encoded)));
    length = (unsigned int)strlen(encoded);
    assert(!anchor_control_machine_state_encode(&live, encoded, length));
    assert(anchor_control_machine_state_encode(&live, encoded, length + 1u));
    assert(snprintf(response, sizeof(response),
        "{\"state\":%s,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[[17,23,29,31,1]],\"role\":1,\"owner\":17,"
        "\"term\":2,\"revision\":3,\"paused\":0}", encoded) > 0);
    assert(anchor_control_machine_status_decode(response, &decoded));
    assert(decoded.has_state && !decoded.has_preview &&
           decoded.state.root[CM_HP] == 0u &&
           decoded.state.root[CM_STATUS] == 1u &&
           decoded.state.projectile_count == 1u &&
           decoded.state.projectile[0][CM_PROJECTILE_ID] == 7u &&
           decoded.state.projectile[0][CM_PROJECTILE_X] == 0x3f800000u &&
           decoded.hit_count == 1 && decoded.hits[0][0] == 17 &&
           decoded.hits[0][1] == 23 && decoded.hits[0][2] == 29 &&
           decoded.hits[0][3] == 31 && decoded.hits[0][4] == 1);
    fight_version = strstr(response, "\"fight\":2");
    assert(fight_version);
    fight_version[8] = '1';
    assert(!anchor_control_machine_status_decode(response, &decoded));
    fight_version[8] = '2';

    /* Exactly 24 root words and the five-word hit tuple are required. */
    root_words = strstr(response, "\"r\":[");
    assert(root_words);
    root_words = strchr(root_words + 5, ',');
    assert(root_words);
    *root_words = ']';
    assert(!anchor_control_machine_status_decode(response, &decoded));
    *root_words = ',';
    assert(!anchor_control_machine_status_decode(
        "{\"state\":null,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[[17,23,29,31]],\"role\":1,\"owner\":17,"
        "\"term\":2,\"revision\":3,\"paused\":0}", &decoded));
    assert(!anchor_control_machine_status_decode(
        "{\"state\":null,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[[17,23,29,31,2]],\"role\":1,\"owner\":17,"
        "\"term\":2,\"revision\":3,\"paused\":0}", &decoded));
    assert(!anchor_control_machine_status_decode(
        "{\"state\":null,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[[17,23,29,4294967296,1]],\"role\":1,\"owner\":17,"
        "\"term\":2,\"revision\":3,\"paused\":0}", &decoded));
    memset(oversized, ' ', sizeof(oversized) - 1u);
    oversized[sizeof(oversized) - 1u] = 0;
    assert(!anchor_control_machine_status_decode(oversized, &decoded));

    live.root[CM_COMMAND] = 0u;
    assert(!anchor_control_machine_state_encode(&live, encoded,
                                                sizeof(encoded)));
    live.root[CM_COMMAND] = 1u;

    assert(anchor_control_machine_state_encode(&dead, encoded,
                                               sizeof(encoded)));
    assert(snprintf(response, sizeof(response),
        "{\"state\":null,\"preview\":%s,\"encounter\":[1,2,3],"
        "\"hits\":[],\"role\":0,\"owner\":17,\"term\":2,"
        "\"revision\":3,\"paused\":1}", encoded) > 0);
    assert(anchor_control_machine_status_decode(response, &decoded));
    assert(!decoded.has_state && decoded.has_preview &&
           decoded.preview.root[CM_PHASE] == 6u &&
           decoded.preview.root[CM_HP] == 255u &&
           decoded.preview.root[CM_STATUS] == 2u);
    dead.root[CM_PHASE] = 5u;
    assert(!anchor_control_machine_state_encode(&dead, encoded,
                                                sizeof(encoded)));
    live.projectile_count = ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES + 1u;
    assert(!anchor_control_machine_state_encode(&live, encoded,
                                                sizeof(encoded)));

    /* The real File46 Control Machine child starts with status zero; the
     * unrelated entity-1BE graph is what initializes status ten. */
    live = state(2u, 5u, 0u);
    assert(anchor_control_machine_state_encode(&live, encoded,
                                               sizeof(encoded)));
    assert(snprintf(response, sizeof(response),
        "{\"state\":%s,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[],\"role\":1,\"owner\":17,\"term\":1,"
        "\"revision\":1,\"paused\":0}", encoded) > 0);
    assert(anchor_control_machine_status_decode(response, &decoded));
    assert(decoded.has_state && decoded.state.root[CM_STATUS] == 0u);
    live.root[CM_STATUS] = 10u;
    assert(!anchor_control_machine_snapshot_valid(&live));
    assert(!anchor_control_machine_state_encode(&live, encoded,
                                                sizeof(encoded)));
}

static void simulate_native_wait_hit(void *child, void *object)
{
    unsigned char old_hp;
    anchor_control_machine_native_hit_begin(child, object);
    if (U32(child, 0x68) & 1u) {
        /* File46 02540 clears the contact bit even when D4 rejects damage. */
        U32(child, 0x68) &= ~1u;
        if (!U16(child, 0xd4) && !U16(child, 0xda)) {
            old_hp = U8(child, 0x8d);
            U8(child, 0x8d) = (unsigned char)(old_hp - 1u);
            if (old_hp == 0u) {
                D_8015CDB6 = 2u;
                child_ai = func_080031D8_7042D8;
            } else {
                D_8015CDB6 = 1u;
                D_8015CDBC = U8(child, 0x8d);
                U16(child, 0xd4) = 0x50u;
            }
        }
    }
    anchor_control_machine_native_hit_end();
}

static void hit_routing(void *child, void *object)
{
    AnchorControlMachineSnapshot snapshot;
    int sequence;
    const int hurt_hit[5] = {11, 11, 11, 11, 1};
    const int remote_hit[5] = {11, 11, 11, 12, 1};
    const int first_batch[5] = {11, 11, 11, 13, 1};
    const int second_batch[5] = {11, 11, 11, 14, 1};
    anchor_control_machine_native_clear_hits();
    s_terminal_event_visit = s_terminal_release_visit = 0;
    U8(child, 0x8d) = D_8015CDBC = 5u;
    U16(child, 0xd4) = U16(child, 0xda) = 0u;
    D_8015CDB6 = 0u;
    child_ai = func_0800316C_70426C;
    anchor_control_machine_native_set_role(1, 1, 0);

    /* Owner contact stays in the native helper; it cannot become a delayed
     * network self-hit. The next capture contains the accepted HP change. */
    U32(child, 0x68) |= 1u;
    anchor_control_machine_native_tick();
    assert(U32(child, 0x68) & 1u);
    assert(!anchor_control_machine_native_hit_pending());
    simulate_native_wait_hit(child, object);
    assert(U8(child, 0x8d) == 4u && U16(child, 0xd4) == 0x50u);
    assert(!anchor_control_machine_native_take_local_hit(&sequence));
    assert(anchor_control_machine_native_capture(&snapshot));
    assert(snapshot.root[CM_HP] == 4u && snapshot.root[CM_STATUS] == 1u);

    /* Repeated overlap during native hurt flash must never queue a future
     * hit, whether it originated locally or arrived from a remote player. */
    for (sequence = 0; sequence < 3; ++sequence) {
        U32(child, 0x68) |= 1u;
        simulate_native_wait_hit(child, object);
        assert(U8(child, 0x8d) == 4u);
    }
    assert(anchor_control_machine_native_queue_hit(hurt_hit));
    assert(!anchor_control_machine_native_hit_pending());
    U16(child, 0xd4) = 0u;
    assert(anchor_control_machine_native_queue_hit(hurt_hit));
    assert(!anchor_control_machine_native_hit_pending());

    assert(anchor_control_machine_native_queue_hit(remote_hit));
    assert(anchor_control_machine_native_hit_pending());
    simulate_native_wait_hit(child, object);
    assert(U8(child, 0x8d) == 3u && U16(child, 0xd4) == 0x50u);
    assert(!anchor_control_machine_native_hit_pending());
    assert(anchor_control_machine_native_queue_hit(remote_hit));
    assert(!anchor_control_machine_native_hit_pending());

    U16(child, 0xd4) = 0u;
    assert(anchor_control_machine_native_queue_hit(first_batch));
    assert(anchor_control_machine_native_queue_hit(second_batch));
    simulate_native_wait_hit(child, object);
    assert(U8(child, 0x8d) == 2u && U16(child, 0xd4) == 0x50u);
    assert(!anchor_control_machine_native_hit_pending());
    U16(child, 0xd4) = 0u;
    assert(anchor_control_machine_native_queue_hit(second_batch));
    assert(!anchor_control_machine_native_hit_pending());

    /* A follower may report only a fresh contact in the owner's vulnerable
     * wait; it clears the local bit so native cannot also damage local HP. */
    anchor_control_machine_native_set_role(1, 0, 0);
    U16(child, 0xd4) = 0x50u;
    U32(child, 0x68) |= 1u;
    anchor_control_machine_native_tick();
    assert(!(U32(child, 0x68) & 1u));
    assert(!anchor_control_machine_native_take_local_hit(&sequence));
    U16(child, 0xd4) = 0u;
    U32(child, 0x68) |= 1u;
    anchor_control_machine_native_tick();
    assert(!(U32(child, 0x68) & 1u));
    assert(anchor_control_machine_native_take_local_hit(&sequence));
    assert(sequence > 0);
    assert(!anchor_control_machine_native_take_local_hit(&sequence));
    anchor_control_machine_native_clear_hits();
}

static void command_playback(void *child, void *object)
{
    AnchorControlMachineSnapshot first = state(2u, 5u, 0u);
    AnchorControlMachineSnapshot next = state(3u, 5u, 0u);
    AnchorControlMachineSnapshot later = state(4u, 4u, 1u);
    AnchorControlMachineSnapshot duplicate, promoted, captured;
    unsigned int old_visit = anchor_control_machine_native_visit();
    assert(command_newer(1u, 0xffffffffu));
    assert(!command_newer(0xffffffffu, 1u));
    next.root[CM_X] = float_bits(100.f);
    next.root[CM_TIMER] = 0x80u;
    next.root[CM_COMMAND] = 2u;
    later.root[CM_X] = float_bits(200.f);
    later.root[CM_COMMAND] = 3u;
    duplicate = later;
    duplicate.root[CM_X] = float_bits(210.f);
    duplicate.root[CM_TIMER] = 70u;
    promoted = duplicate;
    promoted.root[CM_X] = float_bits(300.f);
    promoted.root[CM_TIMER] = 55u;

    anchor_control_machine_native_discard_pending();
    assert(anchor_control_machine_native_apply(&first, 1));
    anchor_control_machine_native_set_role(1, 0, 0);
    assert(s_held && s_pending_kind == CM_PENDING_FULL);
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_held && !s_pending_valid && phase_now() == 2u);
    assert(U16(child, 0x8a) == first.root[CM_TIMER]);
    assert(U8(child, 0x8d) == 5u && s_applied_command == 1u);

    /* Native AI must remain installed and free to advance between packets.
     * An unchanged command may reconcile shots but cannot rewind its timer,
     * health, or locally derived pose. */
    U16(child, 0x8a) = 39u;
    F32(object, 8) = 33.f;
    assert(anchor_control_machine_native_apply(&first, 0));
    assert(s_pending_kind == CM_PENDING_HAZARDS);
    anchor_control_machine_native_set_role(1, 0, 0);
    assert(!s_held);
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_pending_valid && !s_held && U16(child, 0x8a) == 39u);
    assert(F32(object, 8) == 33.f && U8(child, 0x8d) == 5u);

    assert(anchor_control_machine_native_apply(&next, 0));
    anchor_control_machine_native_set_role(1, 0, 0);
    assert(s_pending_kind == CM_PENDING_FULL && s_held);
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_held && phase_now() == 3u);
    assert(U16(child, 0x8a) == 0x80u && F32(object, 8) == 100.f);
    assert(s_applied_command == 2u);
    assert(anchor_control_machine_native_apply(&first, 0));
    assert(!s_pending_valid && s_applied_command == 2u);

    /* A duplicate of a not-yet-adopted command stays a full seed. A failed
     * projectile preflight keeps the native AI held until retry succeeds. */
    assert(anchor_control_machine_native_apply(&later, 0));
    assert(anchor_control_machine_native_apply(&duplicate, 0));
    assert(s_pending_kind == CM_PENDING_FULL);
    anchor_control_machine_native_set_role(1, 0, 0);
    assert(s_held);
    projectile_apply_ok = 0;
    anchor_control_machine_native_adopt_before_pre(child);
    assert(s_pending_valid && s_held && s_applied_command == 2u);
    projectile_apply_ok = 1;
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_pending_valid && !s_held && s_applied_command == 3u);
    assert(phase_now() == 4u && U16(child, 0x8a) == 70u);
    assert(U8(child, 0x8d) == 4u && F32(object, 8) == 210.f);

    /* A pause/resume with the same command takes one fresh full seed. */
    anchor_control_machine_native_set_role(1, 0, 1);
    assert(s_held);
    U16(child, 0x8a) = 30u;
    assert(anchor_control_machine_native_apply(&promoted, 1));
    anchor_control_machine_native_adopt_before_pre(child);
    assert(s_pending_valid && U16(child, 0x8a) == 30u);
    anchor_control_machine_native_set_role(1, 0, 0);
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_held && U16(child, 0x8a) == 55u);

    /* Owner promotion keeps the command number but adopts exact state. */
    promoted.root[CM_X] = float_bits(310.f);
    assert(anchor_control_machine_native_apply(&promoted, 1));
    anchor_control_machine_native_set_role(1, 1, 0);
    assert(s_held);
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_held && F32(object, 8) == 310.f);
    assert(s_applied_command == 3u);
    assert(anchor_control_machine_native_capture(&captured));
    assert(captured.root[CM_COMMAND] == 3u);
    child_ai = func_0800316C_70426C;
    assert(anchor_control_machine_native_capture(&captured));
    assert(captured.root[CM_COMMAND] == 4u);
    U16(child, 0x8a) = 12u;
    F32(object, 8) = 320.f;
    F32(object, 0x28) = 1.f;
    assert(anchor_control_machine_native_capture(&captured));
    assert(captured.root[CM_COMMAND] == 4u);
    U8(child, 0x8d) = 3u;
    D_8015CDBC = 3u;
    assert(anchor_control_machine_native_capture(&captured));
    assert(captured.root[CM_COMMAND] == 5u);
    D_8015CDB6 = 0u;
    assert(anchor_control_machine_native_capture(&captured));
    assert(captured.root[CM_COMMAND] == 6u);

    /* A new transport identity may start its own command sequence. */
    anchor_control_machine_native_discard_pending();
    assert(anchor_control_machine_native_apply(&first, 1));
    anchor_control_machine_native_set_role(1, 0, 0);
    anchor_control_machine_native_adopt_before_pre(child);
    assert(s_applied_command == 1u && phase_now() == 2u && !s_held);

    /* A recycled child at the same address cannot inherit old commands. */
    ++U8(child, 0x74);
    ++U8(root_store.bytes, 0x74);
    anchor_control_machine_native_tick();
    assert(anchor_control_machine_native_visit() != old_visit);
    assert(!s_applied_command && !s_command_serial && !s_pending_valid);
}

int main(void)
{
    AnchorControlMachineSnapshot live, terminal, departure;
    AnchorControlMachineStatus decoded;
    char encoded[ANCHOR_CONTROL_MACHINE_STATE_JSON_SIZE];
    char status_json[ANCHOR_CONTROL_MACHINE_STATUS_JSON_SIZE];
    int hit1[5] = {1, 1, 1, 1, 1};
    int hit2[5] = {1, 1, 1, 2, 1};
    int hit3[5] = {1, 1, 1, 3, 1};
    int hit4[5] = {1, 1, 1, 4, 1};
    void *child = child_store.bytes, *object = object_store.bytes;
    memset(&child_store, 0, sizeof(child_store));
    memset(&root_store, 0, sizeof(root_store));
    memset(&object_store, 0, sizeof(object_store));
    memset(&root_object_store, 0, sizeof(root_object_store));
    codec_boundaries();
    CM_NATIVE_PTR(child, 0x18) = object;
    U8(child, 0x74) = U8(root_store.bytes, 0x74) = 7u;
    U32(object, 0x30) = 0x40000000u;
    U8(child, 0x8d) = 5u;
    U16(child, 0xdc) = 1u;
    F32(object, 0x1c) = F32(object, 0x20) =
        F32(object, 0x24) = 0.2f;
    D_8015CDB6 = 0u;
    D_8015CDBC = 5u;
    child_ai = func_08002F8C_70408C;
    anchor_control_machine_native_tick();
    assert(anchor_control_machine_native_ready());
    assert(anchor_control_machine_native_capture(&live));
    assert(live.root[CM_HP] == 5u && live.root[CM_STATUS] == 0u &&
           live.root[CM_STATUS_HP] == 5u &&
           live.root[CM_COLOUR] == U32(object, 0x8c));
    assert(live.root[CM_PHASE] == 2u);
    CM_NATIVE_PTR(child, 0xd0) = root_store.bytes;
    CM_NATIVE_PTR(root_store.bytes, 0x18) = root_object_store.bytes;
    U16(child, 0xda) = 64u;
    U16(object, 0x16) = 0x180u;
    D_8015CC30_15D830 = 3u;
    func_080029CC_703ACC(child);
    assert(native_shot_births == 1u && native_shot_targets == 1u);
    assert(F32(object, 8) == 5.f && F32(object, 0xc) == 40.f &&
           F32(object, 0x10) == 50.f);
    assert(U16(shot_store.bytes, 0x28) == 0x2eu &&
           U32(shot_store.bytes, 0x2c) == 0x12345678u);
    anchor_control_machine_native_set_role(1, 0, 0);
    U16(object, 0x16) = 0x180u;
    func_080029CC_703ACC(child);
    assert(native_shot_births == 1u && native_shot_targets == 1u);
    assert(F32(object, 8) == 5.f && U16(child, 0xd8) == 32u);
    U16(child, 0xda) = 0u;

    live = state(2u, 5u, 0u);
    assert(anchor_control_machine_state_encode(&live, encoded,
                                               sizeof(encoded)));
    assert(snprintf(status_json, sizeof(status_json),
        "{\"state\":%s,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[[1,1,1,1,1]],\"role\":1,\"owner\":1,"
        "\"term\":1,\"revision\":1,\"paused\":0}", encoded) > 0);
    assert(anchor_control_machine_status_decode(status_json, &decoded));
    assert(decoded.has_state && decoded.state.root[CM_STATUS] == 0u &&
           decoded.hit_count == 1);
    assert(!anchor_control_machine_status_decode(
        "{\"state\":null,\"preview\":null,\"encounter\":[1,2,3],"
        "\"hits\":[],\"role\":1,\"owner\":1,\"term\":1,"
        "\"revision\":1,\"paused\":0,\"paused\":0}", &decoded));
    U32(child, 0x60) = 0x4006e0u;
    assert(anchor_control_machine_native_apply(&live, 1) && s_pending_valid);
    /* Same task binding across a bridge reconnect cannot retain a staged
     * checkpoint from the old transport encounter. */
    anchor_control_machine_native_discard_pending();
    assert(!s_pending_valid);
    assert(apply_now(&live));
    projectile_apply_ok = 0;
    assert(anchor_control_machine_native_apply(&live, 1));
    anchor_control_machine_native_set_role(1, 1, 0);
    assert(s_pending_valid && s_held);
    assert(!projectile_context_owner && projectile_context_paused);
    assert(!anchor_control_machine_native_capture(&terminal));
    anchor_control_machine_native_adopt_before_pre(child);
    assert(s_pending_valid && s_held);
    assert(!projectile_context_owner && projectile_context_paused);
    projectile_apply_ok = 1;
    anchor_control_machine_native_adopt_before_pre(child);
    assert(!s_pending_valid && !s_held);
    assert(projectile_context_owner && !projectile_context_paused);
    anchor_control_machine_native_set_role(1, 0, 0);
    assert(U32(child, 0x60) == 0x4006e0u);
    assert(terminal_events == 0 && releases == 0 && quest_ready);
    assert(anchor_control_machine_native_ready());

    command_playback(child, object);
    hit_routing(child, object);

    terminal = state(6u, 255u, 2u);
    anchor_control_machine_native_set_role(1, 1, 0);
    assert(anchor_control_machine_native_queue_hit(hit3));
    anchor_control_machine_native_set_role(1, 0, 0);
    assert(apply_now(&terminal));
    assert(!anchor_control_machine_native_hit_pending());
    s_owner = 1; /* promoted onto already terminal checkpoint */
    assert(anchor_control_machine_native_queue_hit(hit4));
    assert(!anchor_control_machine_native_hit_pending());
    s_owner = 0;
    assert(terminal_events == 1 && releases == 0);
    assert(U32(child, 0x60) == 0x4006e0u);
    departure = state(7u, 255u, 3u);
    assert(apply_now(&departure));
    assert(terminal_events == 1 && releases == 1 && !quest_ready);
    assert(U32(child, 0x60) == 0u);
    assert(anchor_control_machine_native_ready());
    assert(!anchor_control_machine_native_apply(&live, 1));
    departure.root[CM_STATUS] = 4u;
    assert(apply_now(&departure));
    assert(terminal_events == 1 && releases == 1);

    /* A native final hit is old HP zero -> wrapped 255/status2. Any later
     * batch member must be retired because 0316C will not run again. */
    quest_ready = 1;
    s_terminal_release_visit = s_terminal_event_visit = 0;
    U8(child, 0x8d) = 0;
    U16(child, 0xda) = 0;
    D_8015CDB6 = 1u;
    child_ai = func_0800316C_70426C;
    s_held = 0;
    anchor_control_machine_native_set_role(1, 1, 0);
    assert(anchor_control_machine_native_queue_hit(hit1));
    assert(anchor_control_machine_native_queue_hit(hit2));
    anchor_control_machine_native_hit_begin(child, object);
    assert(U32(child, 0x68) & 1u);
    U32(child, 0x68) &= ~1u;
    U8(child, 0x8d) = 255u;
    D_8015CDB6 = 2u;
    child_ai = func_080031D8_7042D8;
    anchor_control_machine_native_hit_end();
    assert(!anchor_control_machine_native_hit_pending());
    assert(anchor_control_machine_native_queue_hit(hit2));
    assert(!anchor_control_machine_native_hit_pending());
    /* A durable remote completion with no incumbent uses one scheduled
     * native final hit after local readiness, without grinding five HP. */
    U8(child, 0x8d) = 5u;
    D_8015CDB6 = 0u;
    child_ai = func_0800316C_70426C;
    assert(anchor_control_machine_native_request_terminal());
    assert(anchor_control_machine_native_terminal_fallback());
    anchor_control_machine_native_hit_begin(child, object);
    assert(U8(child, 0x8d) == 0u && (U32(child, 0x68) & 1u));
    U8(child, 0x8d) = 255u;
    D_8015CDB6 = 2u;
    child_ai = func_080031D8_7042D8;
    anchor_control_machine_native_hit_end();
    assert(!s_force_terminal_hit && s_terminal_event_visit == s_visit);
    puts("Control Machine child terminal and hit lifecycle: PASS");
    return 0;
}
