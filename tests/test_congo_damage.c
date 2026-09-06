#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANCHOR_CONGO_DAMAGE_HOST_TEST
static void *read_pointer(const void *record, unsigned int offset)
{
    void *value;
    memcpy(&value, (const unsigned char *)record + offset, sizeof(value));
    return value;
}
static void write_pointer(void *record, unsigned int offset, void *value)
{
    memcpy((unsigned char *)record + offset, &value, sizeof(value));
}
#define CONGO_READ_POINTER(p, off) read_pointer(p, off)
#define CONGO_WRITE_POINTER(p, off, value) write_pointer(p, off, value)
#include "../src/anchor_congo_damage.c"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #test); return 1; \
} } while (0)

unsigned short D_800C7AB2;
void *D_801FC604_5B8514;
void *D_801FC60C_5B851C;
void *D_8016DAB4_16E6B4;
static union { void *alignment; unsigned char bytes[8192]; } s_storage;
static unsigned char *s_boss;
static unsigned char *s_player;
static unsigned char *s_projectile;
static unsigned char *s_other;
static int s_epoch;
static int s_scripted;
static int s_allocation_fails;
static int s_native_calls;
static int s_native_context_correct;
static int s_reentrant_result;
static int s_native_deaths;

int anchor_remote_model_pool_contains(const void *pointer)
{
    unsigned long p = (unsigned long)pointer;
    unsigned long begin = (unsigned long)s_storage.bytes;
    return p >= begin && p < begin + sizeof(s_storage.bytes);
}
int anchor_player_models_get_epoch(void) { return s_epoch; }
int anchor_remote_collision_is_scripted(void) { return s_scripted; }
int mnsg_array_reserve(void **data, int *capacity, int needed, unsigned int size)
{
    void *next;
    if (needed <= *capacity) return 1;
    if (s_allocation_fails) return 0;
    next = realloc(*data, (size_t)needed * size);
    if (!next) return 0;
    memset((char *)next + (size_t)*capacity * size, 0,
           (size_t)(needed - *capacity) * size);
    *data = next;
    *capacity = needed;
    return 1;
}

/* Native 80218350 semantics relevant to Congo: the synthetic branch bypasses
 * gates, normal hits use attacker+4C, and accepted hits enter 60-tick recovery.
 * Calling the real entry hook here also tests that scoped owner delivery is
 * not accidentally captured and suppressed as another local physical hit. */
void func_80218350_5D3820(void *actor)
{
    unsigned int status;
    void *attacker;
    int amount;
    anchor_congo_damage_before_native(actor);
    ++s_native_calls;
    status = WORD(actor, 0x68);
    attacker = CONGO_READ_POINTER(actor, 0x38);
    if (s_applying)
    {
        s_native_context_correct = D_8016DAB4_16E6B4 == actor &&
            attacker && attacker != s_player && !(status & SYNTHETIC_DAMAGE);
        s_reentrant_result = anchor_congo_damage_apply(1);
    }
    if (status & SYNTHETIC_DAMAGE)
        amount = 1;
    else
    {
        if (!(WORD(actor, 0x60) & CAN_RECEIVE_DAMAGE) ||
            !(status & CONTACT) || !attacker)
            return;
        WORD(actor, 0x68) &= ~CONTACT;
        if (WORD(actor, 0x60) & DEFLECT_DAMAGE)
        {
            WORD(actor, 0x68) |= RECOVERY;
            return;
        }
        if (status & RECOVERY) return;
        amount = attack_amount(BYTE(attacker, 0x4c));
    }
    BYTE(actor, 0x8d) = amount >= BYTE(actor, 0x8d) ? 0 :
                       BYTE(actor, 0x8d) - amount;
    WORD(actor, 0x68) |= RECOVERY;
    if (BYTE(actor, 0x8d)) BYTE(actor, 0x8c) = 60;
    else ++s_native_deaths;
}

static void initialize_task(unsigned char *task, unsigned char *object,
                            unsigned char *backlink)
{
    write_pointer(task, 4, backlink);
    write_pointer(backlink, 0, task);
    write_pointer(task, 0x18, object);
    object[4] = 2;
    WORD(task, 0x48) = 1234;
    BYTE(task, 0x30) = 2;
    WORD(object, 0x2c) = 27;
    FLOAT(object, 0x28) = 1.0f;
}

static void reset_case(int owner)
{
    anchor_congo_damage_reset();
    memset(s_storage.bytes, 0, sizeof(s_storage.bytes));
    s_boss = s_storage.bytes;
    s_player = s_storage.bytes + 1024;
    s_projectile = s_storage.bytes + 2048;
    s_other = s_storage.bytes + 3072;
    initialize_task(s_boss, s_boss + 256, s_boss + 512);
    initialize_task(s_player, s_player + 256, s_player + 512);
    initialize_task(s_projectile, s_projectile + 256, s_projectile + 512);
    initialize_task(s_other, s_other + 256, s_other + 512);
    D_800C7AB2 = CONGO_ROOM;
    D_801FC604_5B8514 = s_player;
    D_801FC60C_5B851C = s_player + 256;
    D_8016DAB4_16E6B4 = s_other;
    HALF(s_boss, 0x5e) = CONGO_ENTITY;
    WORD(s_boss, 0xe8) = ROOT_HEALTH_CONTROLLER;
    WORD(s_boss, 0x60) = CAN_RECEIVE_DAMAGE | 0x6e1;
    WORD(s_boss, 0x64) = 0x8000;
    BYTE(s_boss, 0x8d) = 30;
    BYTE(s_player, 0x4c) = 0x15;
    write_pointer(s_projectile, 0x5c, s_player);
    BYTE(s_projectile, 0x4c) = 0x22;
    s_epoch = 7;
    s_scripted = s_allocation_fails = s_native_calls = 0;
    s_native_deaths = s_native_context_correct = 0;
    s_reentrant_result = -1;
    anchor_congo_damage_bind_root(s_boss);
    anchor_congo_damage_set_context(1, owner, 0, 42);
    anchor_congo_damage_scene_frame();
}

static void contact(void *attacker)
{
    write_pointer(attacker, 0x34, s_boss);
    write_pointer(s_boss, 0x38, attacker);
    WORD(s_boss, 0x68) |= CONTACT;
    func_80218350_5D3820(s_boss);
}

static int test_all_physical_hits_use_one_authority_path(void)
{
    AnchorCongoHit hit;
    int owner;
    for (owner = 0; owner <= 1; ++owner)
    {
        reset_case(owner);
        contact(s_player);
        CHECK(BYTE(s_boss, 0x8d) == 30 && s_native_deaths == 0);
        CHECK(!(WORD(s_boss, 0x68) & CONTACT));
        CHECK(read_pointer(s_boss, 0x38) == 0);
        CHECK(read_pointer(s_player, 0x34) == s_boss);
        CHECK(anchor_congo_damage_take_local_hit(&hit));
        CHECK(hit.sequence > 0 && hit.amount == 1);
        CHECK(!anchor_congo_damage_take_local_hit(&hit));
        contact(s_player);
        CHECK(!anchor_congo_damage_take_local_hit(&hit));
    }
    return 0;
}

static int test_native_strength_recovery_and_current_task_scope(void)
{
    int amounts[] = {1, 2, 3, 4, 8};
    unsigned int i;
    for (i = 0; i < sizeof(amounts) / sizeof(*amounts); ++i)
    {
        reset_case(1);
        write_pointer(s_boss, 0x38, s_player);
        WORD(s_boss, 0x68) = 0x2000;
        CHECK(anchor_congo_damage_apply(amounts[i]));
        CHECK(BYTE(s_boss, 0x8d) == 30 - amounts[i]);
        CHECK(BYTE(s_boss, 0x8c) == 60);
        CHECK(WORD(s_boss, 0x68) == (0x2000 | RECOVERY));
        CHECK(read_pointer(s_boss, 0x38) == s_player);
        CHECK(D_8016DAB4_16E6B4 == s_other);
        CHECK(s_native_context_correct && s_reentrant_result == 0);
        CHECK(!anchor_congo_damage_apply(8));
        CHECK(s_native_calls == 1);
    }
    reset_case(1); BYTE(s_boss, 0x8d) = 2;
    CHECK(anchor_congo_damage_apply(3));
    CHECK(BYTE(s_boss, 0x8d) == 0 && s_native_deaths == 1);
    CHECK(!anchor_congo_damage_apply(1) && s_native_deaths == 1);
    return 0;
}

static int test_vulnerability_and_exact_root_guards(void)
{
    AnchorCongoHit hit;
    unsigned int effect;
    reset_case(0);
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); WORD(s_boss, 0x60) |= DEFLECT_DAMAGE;
    contact(s_player);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); WORD(s_boss, 0x60) &= ~CAN_RECEIVE_DAMAGE;
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); WORD(s_boss, 0x68) |= REMOVE_PENDING;
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); write_pointer(s_boss + 512, 0, 0);
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); write_pointer(s_boss, 0x18, s_other + 256);
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); D_800C7AB2 = 0x71;
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); WORD(s_boss, 0x10) |= CALLBACK_DISABLED;
    CHECK(!anchor_congo_damage_apply(1)); /* native flute/cutscene post pause */
    reset_case(1); WORD(s_boss, 0x0c) |= CALLBACK_DISABLED;
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(1); write_pointer(s_boss, 0x90, s_other);
    CHECK(!anchor_congo_damage_apply(1) && !s_native_calls);
    reset_case(1); WORD(s_boss, 0x64) &= ~NO_GENERIC_DROPS;
    CHECK(!anchor_congo_damage_apply(1) && !s_native_calls);
    for (effect = 1; effect <= GENERIC_HIT_EFFECTS; effect <<= 1)
    {
        if (!(effect & GENERIC_HIT_EFFECTS)) continue;
        reset_case(1); WORD(s_boss, 0x60) |= effect;
        CHECK(!anchor_congo_damage_apply(1) && !s_native_calls);
    }
    reset_case(1); D_8016DAB4_16E6B4 = 0;
    CHECK(anchor_congo_damage_apply(1));
    CHECK(!D_8016DAB4_16E6B4 && s_native_context_correct);
    reset_case(1);
    CHECK(!anchor_congo_damage_apply(0) && !anchor_congo_damage_apply(5));
    WORD(s_other, 0x68) = CONTACT | SYNTHETIC_DAMAGE;
    anchor_congo_damage_before_native(s_other);
    CHECK(WORD(s_other, 0x68) == (CONTACT | SYNTHETIC_DAMAGE));
    WORD(s_boss, 0xe8) &= ~ROOT_HEALTH_CONTROLLER;
    CHECK(anchor_congo_damage_is_root(s_boss)); /* scoped follower A228 */
    return 0;
}

static int test_remote_visuals_pause_and_pending_checkpoint_cannot_damage(void)
{
    AnchorCongoHit hit;
    reset_case(0);
    contact(s_other);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    contact(s_projectile);
    CHECK(anchor_congo_damage_take_local_hit(&hit) && hit.amount == 8);
    reset_case(1);
    anchor_congo_damage_set_context(1, 1, 1, 42);
    contact(s_player);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    CHECK(!anchor_congo_damage_apply(1));
    reset_case(0);
    anchor_congo_damage_set_context(1, 0, 1, 0);
    WORD(s_boss, 0x68) = SYNTHETIC_DAMAGE | CONTACT;
    contact(s_player);
    CHECK(BYTE(s_boss, 0x8d) == 30);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    CHECK(!(WORD(s_boss, 0x68) & (CONTACT | SYNTHETIC_DAMAGE)));
    reset_case(1); s_scripted = 1;
    contact(s_player);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    return 0;
}

static int test_attack_generation_and_context_reset(void)
{
    AnchorCongoHit hit;
    int first_sequence;
    int i;
    reset_case(0);
    contact(s_player);
    CHECK(anchor_congo_damage_take_local_hit(&hit));
    first_sequence = hit.sequence;
    for (i = 0; i < 65; ++i)
    {
        anchor_congo_damage_scene_frame();
        FLOAT(s_player + 256, 0x28) += 1.0f;
        anchor_congo_damage_observe_sphere(s_player + 256, s_player, s_boss);
    }
    contact(s_player);
    CHECK(!anchor_congo_damage_take_local_hit(&hit)); /* one long episode */
    FLOAT(s_player + 256, 0x28) = 0.0f;
    anchor_congo_damage_observe_sphere(s_player + 256, s_player, s_boss);
    contact(s_player);
    CHECK(anchor_congo_damage_take_local_hit(&hit));
    CHECK(hit.sequence != first_sequence);
    anchor_congo_damage_set_context(1, 1, 0, 42); /* handoff retains dedupe */
    contact(s_player);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    anchor_congo_damage_set_context(1, 1, 0, 43);
    contact(s_player);
    CHECK(s_hit_count == 1);
    ++s_epoch;
    /* A death after collision but before frame-end transport cannot relabel
     * the queued attack with the next life, even before another scene tick. */
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    anchor_congo_damage_scene_frame();
    contact(s_player);
    CHECK(s_hit_count == 1);
    anchor_congo_damage_bind_root(s_boss); /* harmless refresh */
    CHECK(s_hit_count == 1);
    ++BYTE(s_boss, 0x74); /* same native task/object/list word, new generation */
    CHECK(!anchor_congo_damage_is_root(s_boss));
    anchor_congo_damage_bind_root(s_boss);
    CHECK(anchor_congo_damage_is_root(s_boss) && s_hit_count == 0);
    anchor_congo_damage_bind_root(0);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    return 0;
}

static int test_offline_native_intake_is_unchanged(void)
{
    AnchorCongoHit hit;
    reset_case(1);
    anchor_congo_damage_set_context(0, 0, 0, 0);
    contact(s_player);
    CHECK(BYTE(s_boss, 0x8d) == 29);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    return 0;
}

static int test_local_capture_backpressure_and_allocation_failure(void)
{
    AnchorCongoHit hit;
    unsigned int i;
    reset_case(0);
    free(s_episodes);
    s_episodes = 0;
    s_episode_capacity = 0;
    s_allocation_fails = 1;
    contact(s_player);
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    CHECK(BYTE(s_boss, 0x8d) == 30);
    s_allocation_fails = 0;
    for (i = 0; i < HIT_QUEUE_SIZE + 1u; ++i)
    {
        s_capture_cooldown = 0;
        WORD(s_player + 256, 0x2c) = i + 100;
        contact(s_player);
    }
    CHECK(s_hit_count == HIT_QUEUE_SIZE);
    CHECK(BYTE(s_boss, 0x8d) == 30);
    for (i = 0; i < HIT_QUEUE_SIZE; ++i)
        CHECK(anchor_congo_damage_take_local_hit(&hit));
    CHECK(!anchor_congo_damage_take_local_hit(&hit));
    return 0;
}

int main(void)
{
    int result = test_all_physical_hits_use_one_authority_path() ||
        test_native_strength_recovery_and_current_task_scope() ||
        test_vulnerability_and_exact_root_guards() ||
        test_remote_visuals_pause_and_pending_checkpoint_cannot_damage() ||
        test_attack_generation_and_context_reset() ||
        test_offline_native_intake_is_unchanged() ||
        test_local_capture_backpressure_and_allocation_failure();
    free(s_episodes);
    if (!result) puts("Congo damage authority tests passed");
    return result;
}
