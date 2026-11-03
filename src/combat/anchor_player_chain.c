#include "combat/anchor_player_chain.h"
#include "combat/anchor_player_attack.h"
#include "player/anchor_player_models.h"
#include "player/anchor_remote_model_pool.h"
#include "progression/item_sync.h"
#include "platform/modding.h"
#include "utils/array_utils.h"
#if DEBUG_BUTTON_ENABLED
#include "platform/recomputils.h"
#endif

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned short D_800C7AB2;
extern float D_800C7DBC;
extern int anchor_is_connected(void);
extern void func_801EA0D8_5A5FE8(void *task, void *object);
extern void func_801EA4E8_5A63F8(void *task, void *object);
extern void func_801EA5E4_5A64F4(void *chain, void *player, void *target);
extern void func_801DACDC_596BEC(void *player, unsigned char action);
extern void func_801F4C20_5B0B30(void *player, unsigned char release_action);
extern unsigned short func_801CF51C_58B42C(void *player, float x, float y,
                                         float z);
extern void func_80038BC8_397C8(unsigned short cue);

/* Pointer fields are four bytes wide in native records. The host fixture
 * substitutes pointer slots so adjacent native byte fields remain distinct. */
#ifndef CHAIN_PTR
#define CHAIN_PTR(record, offset) \
    (*(void **)((unsigned char *)(record) + (offset)))
#endif
#ifndef CHAIN_UPDATE
#define CHAIN_UPDATE(record) \
    (*(void (**)(void *, void *))((unsigned char *)(record) + 0x0c))
#endif

typedef struct ChainCandidate
{
    void *attacker;
    void *head;
    void *head_object;
    void *player;
    void *player_object;
    void *work;
    int local_epoch;
    unsigned short room;
    AnchorPlayerHitTarget target;
    float distance_squared;
} ChainCandidate;

static ChainCandidate s_candidate;
static ChainCandidate s_active;
static void *s_sphere_attacker;
static void *s_blocked_head;
static unsigned int s_pull_ticks;
static float s_latched_y;
static void *s_visual_head;
static void *s_visual_owner;
static void *s_visual_object;
static int s_visual_epoch;
static unsigned short s_visual_room;

#define CHAIN_PULL_TICK_LIMIT 180u

static void copy_bytes(void *destination, const void *source, unsigned int count)
{
    volatile unsigned char *out = destination;
    const volatile unsigned char *in = source;
    unsigned int i;
    for (i = 0; i < count; ++i)
        out[i] = in[i];
}

static int coordinate(float value)
{
    union { float value; unsigned int bits; } number;
    number.value = value;
    return (number.bits & 0x7f800000u) != 0x7f800000u &&
           value >= -10000000.0f && value <= 10000000.0f;
}

static int chain_action(unsigned char action)
{
    return action >= 0x70 && action <= 0x7b;
}

static int pull_action(unsigned char action)
{
    return action == 0x73 || action == 0x7a;
}

static int rdram(const void *pointer)
{
    unsigned int physical = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return (physical >= 0x1000u && physical < 0x800000u) ||
           anchor_remote_model_pool_contains(pointer);
}

static int linked(void *task)
{
    void *backlink;
    if (!rdram(task))
        return 0;
    backlink = CHAIN_PTR(task, 4);
    return rdram(backlink) && CHAIN_PTR(backlink, 0) == task;
}

static void *chain_head(const void *pointer)
{
    unsigned char *task = (unsigned char *)pointer;
    unsigned char *player = D_801FC604_5B8514;
    void *head;
    if (!linked(task) || !linked(player) ||
        CHAIN_PTR(task, 0x5c) != player)
        return 0;
    if (CHAIN_UPDATE(task) == func_801EA0D8_5A5FE8)
        return task;
    if (CHAIN_UPDATE(task) != func_801EA4E8_5A63F8)
        return 0;
    head = CHAIN_PTR(task, 0x9c);
    if (!linked(head) || CHAIN_UPDATE(head) != func_801EA0D8_5A5FE8 ||
        CHAIN_PTR(head, 0x5c) != player)
        return 0;
    return head;
}

int anchor_player_chain_is_native(const void *task)
{
    return chain_head(task) != 0;
}

static int same_lifetime(const ChainCandidate *state)
{
    unsigned char *player = state->player;
    return state->head && player && player == D_801FC604_5B8514 && linked(player) &&
           state->room == D_800C7AB2 &&
           state->player_object == D_801FC60C_5B851C &&
           rdram(state->player_object) &&
           CHAIN_PTR(player, 0x18) == state->player_object &&
           CHAIN_PTR(player, 0x5c) == state->work && rdram(state->work) &&
           chain_head(state->head) == state->head &&
           CHAIN_PTR(state->head, 0x18) == state->head_object &&
           rdram(state->head_object);
}

static int current_owner(const ChainCandidate *state)
{
    return same_lifetime(state) &&
           ((unsigned char *)state->player)[0x60] == 0 && anchor_is_connected() &&
           item_sync_save_is_loaded() && !anchor_remote_collision_is_scripted() &&
           anchor_player_models_get_epoch() == state->local_epoch;
}

/* CD310's special-floor neutral-stick branch discards existing velocity even
 * in native forced movement mode. A chain anchored to a player's floor-level
 * position encounters that branch; native elevated block pulls normally do
 * not. Supply only this invocation's existing native forced acceleration as
 * its non-dead input magnitude, leaving native acceleration/caps untouched. */
typedef struct ChainPhysicsScope
{
    unsigned int saved_bits;
    unsigned int raw_bits;
    unsigned int exposed_bits;
    int forced;
    int changed;
} ChainPhysicsScope;

#define CHAIN_PHYSICS_SCOPE_CAPACITY 8u
static ChainPhysicsScope s_physics_scopes[CHAIN_PHYSICS_SCOPE_CAPACITY];
static unsigned int s_physics_depth;
static unsigned int s_physics_overflow_bits;
static int s_physics_overflow_changed;

static unsigned int input_magnitude_bits(void)
{
    unsigned int bits;
    copy_bytes(&bits, &D_800C7DBC, sizeof(bits));
    return bits;
}

static void set_input_magnitude_bits(unsigned int bits)
{
    copy_bytes(&D_800C7DBC, &bits, sizeof(bits));
}

static int forced_input_magnitude(void *task, unsigned int raw_bits,
                                  unsigned int *out_bits)
{
    unsigned char *player;
    unsigned char *head;
    unsigned char *work;
    unsigned short floor;
    union { float value; unsigned int bits; } raw, acceleration;
    if (!s_active.head || task != s_active.player ||
        task != D_801FC604_5B8514 || !current_owner(&s_active))
        return 0;
    player = s_active.player;
    head = s_active.head;
    work = s_active.work;
    floor = *(unsigned short *)(player + 0x98);
    if (!pull_action(player[0xcc]) || player[0x61] || work[0x69] ||
        head[0x60] != 2 || head[0x65] || *(unsigned short *)work != 3 ||
        (floor != 0x95 && floor != 0x9a) ||
        !anchor_player_models_peer_is_current(s_active.target.cid,
                                               s_active.target.session,
                                               s_active.target.epoch))
        return 0;
    raw.bits = raw_bits;
    copy_bytes(&acceleration.bits, work + 4, sizeof(acceleration.bits));
    if ((raw.bits & 0x7f800000u) == 0x7f800000u ||
        (acceleration.bits & 0x7f800000u) == 0x7f800000u ||
        raw.value < 0.0f || raw.value >= 0.1f || acceleration.value < 0.1f)
        return 0;
    *out_bits = acceleration.bits;
    return 1;
}

RECOMP_HOOK("func_801CD310_589220")
void anchor_player_chain_physics_begin(void *task)
{
    unsigned int depth = s_physics_depth++;
    unsigned int current_bits = input_magnitude_bits();
    ChainPhysicsScope *scope;
    if (depth >= CHAIN_PHYSICS_SCOPE_CAPACITY)
    {
        /* Native CD310 is not recursive. If another mod nests it beyond our
         * bounded stack, expose raw input to those calls rather than leaking
         * the outer player's temporary forced magnitude into another task. */
        if (depth == CHAIN_PHYSICS_SCOPE_CAPACITY)
        {
            scope = &s_physics_scopes[depth - 1];
            s_physics_overflow_bits = current_bits;
            s_physics_overflow_changed = scope->forced &&
                current_bits == scope->exposed_bits && current_bits != scope->raw_bits;
            if (s_physics_overflow_changed)
                set_input_magnitude_bits(scope->raw_bits);
        }
        return;
    }
    scope = &s_physics_scopes[depth];
    scope->saved_bits = current_bits;
    scope->raw_bits = current_bits;
    if (depth)
    {
        const ChainPhysicsScope *outer = &s_physics_scopes[depth - 1];
        if (outer->forced && current_bits == outer->exposed_bits)
            scope->raw_bits = outer->raw_bits;
    }
    scope->exposed_bits = scope->raw_bits;
    scope->forced = forced_input_magnitude(task, scope->raw_bits, &scope->exposed_bits);
    scope->changed = scope->exposed_bits != scope->saved_bits;
    if (scope->changed)
        set_input_magnitude_bits(scope->exposed_bits);
}

RECOMP_HOOK_RETURN("func_801CD310_589220")
void anchor_player_chain_physics_end(void)
{
    if (!s_physics_depth)
        return;
    --s_physics_depth;
    if (s_physics_depth >= CHAIN_PHYSICS_SCOPE_CAPACITY)
    {
        if (s_physics_depth == CHAIN_PHYSICS_SCOPE_CAPACITY && s_physics_overflow_changed)
            set_input_magnitude_bits(s_physics_overflow_bits);
        return;
    }
    /* Restore even when player ownership/action/lifetime changed in the
     * native call. No retained player or object is dereferenced on return. */
    if (s_physics_scopes[s_physics_depth].changed)
        set_input_magnitude_bits(s_physics_scopes[s_physics_depth].saved_bits);
}

/* Walk the current collision roster again rather than retaining a remote
 * task. Reconnects may reuse CID and epoch, so session is checked as well. */
static int current_target(ChainCandidate *state)
{
    int capacity = anchor_player_models_capacity();
    int i;
    /* The model API fills an array, but this interaction retains only one
     * target. Reuse its dynamic scratch buffer without imposing a player cap. */
    static AnchorPlayerHitTarget *targets;
    static int target_capacity;
    int count;
    if (!mnsg_array_reserve((void **)&targets, &target_capacity, capacity,
                            sizeof(*targets)))
        return 0;
    count = anchor_player_models_get_hit_targets(targets, target_capacity);
    if (count < 0 || count > target_capacity)
        return 0;
    for (i = 0; i < count; ++i)
    {
        const AnchorPlayerHitTarget *target = &targets[i];
        if (target->cid == state->target.cid &&
            target->session == state->target.session &&
            target->epoch == state->target.epoch)
        {
            copy_bytes(&state->target, target, sizeof(*target));
            return 1;
        }
    }
    return 0;
}

static int target_endpoint(const ChainCandidate *state, float endpoint[3])
{
    endpoint[0] = state->target.body.position.x;
    endpoint[1] = state->target.body.position.y + 3.0f;
    endpoint[2] = state->target.body.position.z;
    return coordinate(endpoint[0]) && coordinate(endpoint[1]) &&
           coordinate(endpoint[2]);
}

/* Native arrival is 8.660254, smaller than two solid player bodies. Finish
 * at body contact while leaving the visible chain endpoint attached to B. */
static int bodies_touch(const ChainCandidate *state)
{
    unsigned char *player = state->player;
    unsigned char *object = state->player_object;
    float dx = state->target.body.position.x - *(float *)(object + 8);
    float dz = state->target.body.position.z - *(float *)(object + 0x10);
    float radius = (float)*(unsigned short *)(player + 0x3c) *
                   *(float *)(object + 0x1c) + state->target.body.radius + 1.0f;
    return coordinate(dx) && coordinate(dz) && coordinate(radius) &&
           radius > 1.0f && dx * dx + dz * dz <= radius * radius;
}

#if DEBUG_BUTTON_ENABLED
typedef struct ChainMovementDiagnostic
{
    ChainCandidate owner;
    unsigned int tick;
    float before[3];
    int pending;
    int peer;
} ChainMovementDiagnostic;

static ChainMovementDiagnostic s_movement_diagnostic;

static void chain_debug_reset(void)
{
    s_movement_diagnostic.owner.head = 0;
    s_movement_diagnostic.tick = 0;
    s_movement_diagnostic.pending = 0;
}

static int diagnostic_scaled(float value, float scale)
{
    union { float value; unsigned int bits; } number;
    number.value = value * scale;
    if ((number.bits & 0x7f800000u) == 0x7f800000u ||
        number.value < -2000000000.0f || number.value > 2000000000.0f)
        return 0x7fffffff;
    return (int)number.value;
}

static int diagnostic_owner(ChainCandidate *out)
{
    unsigned char *player = D_801FC604_5B8514;
    void *head = s_active.head ? s_active.head : s_visual_head;
    if (!player || !head || !linked(player) || player[0x60] != 0 ||
        !pull_action(player[0xcc]) || chain_head(head) != head ||
        ((unsigned char *)head)[0x60] > 3 || ((unsigned char *)head)[0x65])
        return 0;
    if (!s_active.head &&
        (s_visual_owner != player || s_visual_object != D_801FC60C_5B851C ||
         s_visual_room != D_800C7AB2 ||
         s_visual_epoch != anchor_player_models_get_epoch()))
        return 0;
    out->head = head;
    out->head_object = CHAIN_PTR(head, 0x18);
    out->player = player;
    out->player_object = D_801FC60C_5B851C;
    out->work = CHAIN_PTR(player, 0x5c);
    out->room = D_800C7AB2;
    out->local_epoch = anchor_player_models_get_epoch();
    return current_owner(out);
}

static void diagnostic_log(const char *edge, const ChainCandidate *owner,
                            unsigned int tick, int peer, const float *before)
{
    const unsigned char *player = owner->player;
    const unsigned char *head = owner->head;
    const unsigned char *work = owner->work;
    const unsigned char *object = owner->player_object;
    float x = *(const float *)(object + 8);
    float y = *(const float *)(object + 0xc);
    float z = *(const float *)(object + 0x10);
    recomp_printf("[chain_pull] %s %s tick=%u action=%02x phase=%u mode=%u "
                  "state61=%u lock69=%u gravity1000=%d input1000=%d "
                  "acc1000=%d cap1000=%d,%d vel1000=%d,%d,%d "
                  "b0_1000=%d flags=%02x,%02x,%02x ground=%04x "
                  "normal1000=%d,%d,%d xyz100=%d,%d,%d delta100=%d,%d,%d\n",
                  peer ? "peer" : "block", edge, tick, player[0xcc], head[0x60],
                  *(const unsigned short *)work,
                  player[0x61], work[0x69],
                  diagnostic_scaled(*(const float *)(player + 0xa4), 1000.0f),
                  diagnostic_scaled(D_800C7DBC, 1000.0f),
                  diagnostic_scaled(*(const float *)(work + 4), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0xe4), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0xe8), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0x68), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0x6c), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0x70), 1000.0f),
                  diagnostic_scaled(*(const float *)(work + 0xb0), 1000.0f),
                  work[0x18], work[0x19], work[0x59],
                  *(const unsigned short *)(player + 0x98),
                  diagnostic_scaled(*(const float *)(player + 0x84), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0x88), 1000.0f),
                  diagnostic_scaled(*(const float *)(player + 0x8c), 1000.0f),
                  diagnostic_scaled(x, 100.0f), diagnostic_scaled(y, 100.0f),
                  diagnostic_scaled(z, 100.0f),
                  before ? diagnostic_scaled(x - before[0], 100.0f) : 0,
                  before ? diagnostic_scaled(y - before[1], 100.0f) : 0,
                  before ? diagnostic_scaled(z - before[2], 100.0f) : 0);
}

static void chain_debug_commit(void)
{
    ChainCandidate owner;
    chain_debug_reset();
    if (diagnostic_owner(&owner))
    {
        copy_bytes(&s_movement_diagnostic.owner, &owner, sizeof(owner));
        s_movement_diagnostic.peer = 1;
        diagnostic_log("commit", &owner, 0, 1, 0);
    }
}

static void chain_debug_release(void)
{
    if (s_active.head && current_owner(&s_active))
        diagnostic_log("release", &s_active, s_movement_diagnostic.tick, 1, 0);
}

RECOMP_HOOK("func_801CBAF8_587A08")
void anchor_player_chain_movement_diagnostic_begin(void *task)
{
    ChainCandidate owner;
    ChainMovementDiagnostic *diagnostic = &s_movement_diagnostic;
    const unsigned char *object;
    diagnostic->pending = 0;
    if (task != D_801FC604_5B8514)
        return;
    if (!diagnostic_owner(&owner))
    {
        chain_debug_reset();
        return;
    }
    if (diagnostic->owner.head != owner.head ||
        diagnostic->owner.player != owner.player ||
        diagnostic->owner.player_object != owner.player_object ||
        diagnostic->owner.work != owner.work ||
        diagnostic->owner.room != owner.room ||
        diagnostic->owner.local_epoch != owner.local_epoch)
        diagnostic->tick = 0;
    copy_bytes(&diagnostic->owner, &owner, sizeof(owner));
    diagnostic->peer = s_active.head == owner.head;
    ++diagnostic->tick;
    if (diagnostic->tick > 180u ||
        (diagnostic->tick > 12u && diagnostic->tick % 30u != 0))
        return;
    object = owner.player_object;
    diagnostic->before[0] = *(const float *)(object + 8);
    diagnostic->before[1] = *(const float *)(object + 0xc);
    diagnostic->before[2] = *(const float *)(object + 0x10);
    diagnostic->pending = 1;
    diagnostic_log("before", &owner, diagnostic->tick, diagnostic->peer, 0);
}

RECOMP_HOOK_RETURN("func_801CBAF8_587A08")
void anchor_player_chain_movement_diagnostic_end(void)
{
    ChainMovementDiagnostic *diagnostic = &s_movement_diagnostic;
    if (!diagnostic->pending)
        return;
    diagnostic->pending = 0;
    if (!current_owner(&diagnostic->owner))
    {
        chain_debug_reset();
        return;
    }
    diagnostic_log("after", &diagnostic->owner, diagnostic->tick,
                   diagnostic->peer, diagnostic->before);
}
#else
#define chain_debug_reset() ((void)0)
#define chain_debug_commit() ((void)0)
#define chain_debug_release() ((void)0)
#endif

void anchor_player_chain_sphere_begin(void *task)
{
    s_sphere_attacker = chain_head(task) ? task : 0;
}

void anchor_player_chain_sphere_end(void)
{
    void *attacker = s_sphere_attacker;
    void *head;
    s_sphere_attacker = 0;
    if (!attacker || !(head = chain_head(attacker)))
        return;
    if (CHAIN_PTR(attacker, 0x34) || CHAIN_PTR(attacker, 0x38) ||
        CHAIN_PTR(head, 0x34) || CHAIN_PTR(head, 0x38))
        s_blocked_head = head;
}

void anchor_player_chain_observe(const AnchorPlayerAttackSample *sample,
                                  const AnchorPlayerHitTarget *target)
{
    unsigned char *head;
    unsigned char *object;
    float dx, dy, dz, distance_squared;
    if (!sample || !sample->is_chain_pipe || !target || target->cid <= 0 ||
        target->session <= 0 || target->epoch <= 0 ||
        !anchor_is_connected() || anchor_remote_collision_is_scripted())
        return;
    head = chain_head(sample->task);
    if (!head || head[0x60] != 0 || head[0x65] ||
        !rdram(object = CHAIN_PTR(head, 0x18)))
        return;
    dx = sample->center.x - target->body.position.x;
    dy = sample->center.y - target->body.position.y;
    dz = sample->center.z - target->body.position.z;
    distance_squared = dx * dx + dy * dy + dz * dz;
    if (s_candidate.head && distance_squared >= s_candidate.distance_squared)
        return;
    s_candidate.attacker = (void *)sample->task;
    s_candidate.head = head;
    s_candidate.head_object = object;
    s_candidate.player = D_801FC604_5B8514;
    s_candidate.player_object = D_801FC60C_5B851C;
    s_candidate.work = CHAIN_PTR(s_candidate.player, 0x5c);
    s_candidate.local_epoch = anchor_player_models_get_epoch();
    s_candidate.room = D_800C7AB2;
    copy_bytes(&s_candidate.target, target, sizeof(*target));
    s_candidate.distance_squared = distance_squared;
}

static int commit_candidate(void *updating_head)
{
    union { void *alignment; unsigned char bytes[0x40]; } target_task, target_model;
    ChainCandidate candidate;
    unsigned char *head;
    unsigned char *player;
    unsigned char *work;
    unsigned char action;
    float endpoint[3];
    copy_bytes(&candidate, &s_candidate, sizeof(candidate));
    head = candidate.head;
    player = candidate.player;
    s_candidate.head = 0;
    if (!head || head != updating_head || s_blocked_head == head ||
        s_active.head || !current_owner(&candidate) ||
        !linked(candidate.attacker) || head[0x60] != 0 || head[0x65] ||
        chain_head(candidate.attacker) != head || CHAIN_PTR(player, 0x38) ||
        CHAIN_PTR(candidate.attacker, 0x34) ||
        CHAIN_PTR(candidate.attacker, 0x38) || CHAIN_PTR(head, 0x34) ||
        CHAIN_PTR(head, 0x38))
        return 0;
    work = CHAIN_PTR(player, 0x5c);
    action = player[0xcc];
    if ((action != 0x70 && action != 0x71 && action != 0x75 && action != 0x76 &&
         action != 0x77 && action != 0x78) ||
        CHAIN_PTR(work, 0x88) || !current_target(&candidate) ||
        !target_endpoint(&candidate, endpoint))
        return 0;

    /* EA5E4 reads only target task+0x18 and its object's XYZ. These two
     * temporary records never enter native actor/task/collision lists, and
     * the native latch does not retain them. The +3 offset makes its native
     * targetY-3 alignment place A's feet at B's feet. */
    CHAIN_PTR(target_task.bytes, 0x18) = target_model.bytes;
    *(float *)(target_model.bytes + 8) = endpoint[0];
    *(float *)(target_model.bytes + 0xc) = endpoint[1];
    *(float *)(target_model.bytes + 0x10) = endpoint[2];
    *(float *)(player + 0xa4) = 0.0f;
    *(unsigned int *)(work + 0x48) = 0;
    player[0x30] &= (unsigned char)~1u;
    func_801EA5E4_5A64F4(head, player, target_task.bytes);
    if (action == 0x75 || action == 0x76)
    {
        /* F4754's successful alternate-chain branch clears these two
         * halfwords before entering the ordinary ground pull action. */
        *(unsigned short *)(work + 0x34) = 0;
        *(unsigned short *)(player + 0xce) = 0;
    }
    func_801DACDC_596BEC(player, action >= 0x77 ? 0x7a : 0x73);
    /* F4C20 normally latches on counter==0 from work+0x88. Our latch already
     * used the transient descriptor, so skip that first-tick relatch. */
    *(short *)(work + 0x30) = 1;
    func_80038BC8_397C8(0x21c);
    copy_bytes(&s_active, &candidate, sizeof(candidate));
    s_pull_ticks = 0;
    s_latched_y = candidate.target.body.position.y;
    chain_debug_commit();
    return 1;
}

static void finish_pull(void)
{
    unsigned char *player = s_active.player;
    unsigned char *head = s_active.head;
    if (same_lifetime(&s_active) && player[0x60] == 0 && pull_action(player[0xcc]))
    {
        /* Native completion clears movement and starts the matching ground
         * or air release animation. Never run a remote actor callback. */
        head[0x60] = 3;
        func_801F4C20_5B0B30(player, player[0xcc] == 0x7a ? 0x7b : 0x74);
        chain_debug_release();
    }
    s_active.head = 0;
    chain_debug_reset();
}

RECOMP_HOOK("func_801EA0D8_5A5FE8")
void anchor_player_chain_head_update(void *task, void *object)
{
    unsigned char *head = task;
    unsigned char *player = D_801FC604_5B8514;
    float endpoint[3];
    unsigned char *work;
    int latched = 0;
    if (!task || chain_head(task) != task || CHAIN_PTR(task, 0x18) != object)
        return;
    s_visual_head = task;
    s_visual_owner = player;
    s_visual_object = D_801FC60C_5B851C;
    s_visual_epoch = anchor_player_models_get_epoch();
    s_visual_room = D_800C7AB2;
    if (s_candidate.head)
        latched = commit_candidate(task);
    if (s_blocked_head == task)
        s_blocked_head = 0;
    if (latched)
        return;
    if (!s_active.head)
        return;
    if (s_active.head != task || !same_lifetime(&s_active) ||
        !pull_action(player[0xcc]))
    {
        s_active.head = 0;
        return;
    }
    if (!current_owner(&s_active) || head[0x60] >= 3 || head[0x65] ||
        ++s_pull_ticks > CHAIN_PULL_TICK_LIMIT ||
        !current_target(&s_active) || !target_endpoint(&s_active, endpoint) ||
        s_active.target.body.position.y - s_latched_y > 8.660254f ||
        s_latched_y - s_active.target.body.position.y > 8.660254f ||
        bodies_touch(&s_active))
    {
        finish_pull();
        return;
    }
    *(float *)((unsigned char *)object + 8) = endpoint[0];
    *(float *)((unsigned char *)object + 0xc) = endpoint[1];
    *(float *)((unsigned char *)object + 0x10) = endpoint[2];
    work = CHAIN_PTR(player, 0x5c);
    *(unsigned short *)(work + 2) =
        func_801CF51C_58B42C(player, endpoint[0], endpoint[1], endpoint[2]);
    *(unsigned short *)(player + 0x94) = *(unsigned short *)(work + 2);
}

RECOMP_HOOK("func_80034A10_35610")
void anchor_player_chain_task_reinitialized(void *task)
{
#if DEBUG_BUTTON_ENABLED
    if (s_movement_diagnostic.owner.head == task ||
        s_movement_diagnostic.owner.player == task)
        chain_debug_reset();
#endif
    if (s_candidate.head == task || s_candidate.attacker == task ||
        s_candidate.player == task)
        s_candidate.head = 0;
    if (s_sphere_attacker == task)
        s_sphere_attacker = 0;
    if (s_blocked_head == task)
        s_blocked_head = 0;
    if (s_active.head == task || s_active.player == task)
        s_active.head = 0;
    if (s_visual_head == task || s_visual_owner == task)
        s_visual_head = 0;
}

int anchor_player_chain_visual_snapshot(int out[4])
{
    unsigned char *head = s_visual_head;
    unsigned char *player = s_visual_owner;
    unsigned char *object;
    unsigned int i;
    if (!out || !head || !player || player != D_801FC604_5B8514 || !linked(player) ||
        s_visual_object != D_801FC60C_5B851C ||
        s_visual_room != D_800C7AB2 || !anchor_is_connected() ||
        !item_sync_save_is_loaded() || anchor_remote_collision_is_scripted() ||
        s_visual_epoch != anchor_player_models_get_epoch() ||
        player[0x60] != 0 || !chain_action(player[0xcc]) ||
        chain_head(head) != head ||
        head[0x60] > 3 || head[0x65] ||
        !rdram(object = CHAIN_PTR(head, 0x18)))
        return 0;
    out[0] = head[0x60] + 1;
    for (i = 0; i < 3; ++i)
    {
        float value = *(float *)(object + 8 + i * 4);
        if (!coordinate(value))
            return 0;
        out[i + 1] = (int)(value * 100.0f);
    }
    return 1;
}
