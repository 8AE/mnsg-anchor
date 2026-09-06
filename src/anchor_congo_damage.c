#include "anchor_congo_damage.h"
#include "anchor_player_models.h"
#include "anchor_remote_collision.h"
#include "anchor_remote_model_pool.h"
#include "utils/array_utils.h"

#ifndef ANCHOR_CONGO_DAMAGE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#endif

extern unsigned short D_800C7AB2;
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern void *D_8016DAB4_16E6B4;
extern void func_80218350_5D3820(void *actor);

#define CONGO_ROOM 0x16u
#define CONGO_ENTITY 0x323u
#define ROOT_HEALTH_CONTROLLER 0x04000000u
#define CAN_RECEIVE_DAMAGE 0x00200000u
#define DEFLECT_DAMAGE 0x00400000u
#define RECOVERY 0x00000001u
#define REMOVE_PENDING 0x00000002u
#define CONTACT 0x00000080u
#define SYNTHETIC_DAMAGE 0x00040000u
#define CALLBACK_DISABLED 0x00800000u
#define NO_GENERIC_DROPS 0x00008000u
#define GENERIC_HIT_EFFECTS 0x0017f800u
#define HIT_QUEUE_SIZE 8u

#define BYTE(p, off) (*(unsigned char *)((unsigned char *)(p) + (off)))
#define HALF(p, off) (*(unsigned short *)((unsigned char *)(p) + (off)))
#define WORD(p, off) (*(unsigned int *)((unsigned char *)(p) + (off)))
#define FLOAT(p, off) (*(float *)((unsigned char *)(p) + (off)))
#ifndef CONGO_READ_POINTER
#define CONGO_READ_POINTER(p, off) (*(void **)((unsigned char *)(p) + (off)))
#define CONGO_WRITE_POINTER(p, off, value) \
    (*(void **)((unsigned char *)(p) + (off)) = (value))
#endif

typedef struct CongoAttackEpisode
{
    void *task;
    void *object;
    void *backlink;
    unsigned int descriptor;
    unsigned int animation;
    unsigned int seen_frame;
    float animation_frame;
    int sequence;
    int reported;
} CongoAttackEpisode;

static void *s_root;
static void *s_root_object;
static void *s_root_backlink;
static unsigned short s_root_actor;
static unsigned char s_root_generation;
static int s_active;
static int s_owner;
static int s_paused;
static unsigned int s_encounter;
static unsigned int s_frame;
static int s_player_epoch;
static int s_next_sequence;
static int s_capture_cooldown;
static int s_applying;
static CongoAttackEpisode *s_episodes;
static int s_episode_capacity;
static AnchorCongoHit s_hits[HIT_QUEUE_SIZE];
static unsigned int s_hit_read;
static unsigned int s_hit_count;

static int valid_pointer(const void *pointer)
{
    unsigned int physical = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return (physical >= 0x1000u && physical < 0x800000u) ||
           anchor_remote_model_pool_contains(pointer);
}

static int linked_task(const void *task)
{
    void *backlink;
    if (!valid_pointer(task))
        return 0;
    backlink = CONGO_READ_POINTER(task, 0x04);
    return valid_pointer(backlink) && CONGO_READ_POINTER(backlink, 0) == task;
}

static void clear_contacts(void)
{
    int i;
    s_hit_read = 0;
    s_hit_count = 0;
    s_capture_cooldown = 0;
    for (i = 0; i < s_episode_capacity; ++i)
        s_episodes[i].task = 0;
}

void anchor_congo_damage_reset(void)
{
    clear_contacts();
    s_root = s_root_object = s_root_backlink = 0;
    s_root_actor = s_root_generation = 0;
    s_active = s_owner = s_paused = 0;
    s_encounter = s_frame = 0;
    s_player_epoch = 0;
    /* Keep sequences monotonic across reconnects and player task recycling. */
}

void anchor_congo_damage_set_context(int active, int owner, int paused,
                                     unsigned int encounter)
{
    if (!!active != s_active || encounter != s_encounter)
        clear_contacts();
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
    s_encounter = encounter;
}

int anchor_congo_damage_is_shared(void)
{
    return s_active && D_800C7AB2 == CONGO_ROOM;
}

int anchor_congo_damage_is_owner(void)
{
    return anchor_congo_damage_is_shared() && s_owner;
}

int anchor_congo_damage_is_root(const void *actor)
{
    /* The native follower temporarily masks E8's health-controller bit while
     * A228 runs. Identity must remain valid through that scoped mask. */
    return actor && actor == s_root && D_800C7AB2 == CONGO_ROOM &&
           linked_task(actor) &&
           CONGO_READ_POINTER(actor, 0x04) == s_root_backlink &&
           CONGO_READ_POINTER(actor, 0x18) == s_root_object &&
           HALF(actor, 0x5c) == s_root_actor &&
           BYTE(actor, 0x74) == s_root_generation &&
           valid_pointer(s_root_object) && HALF(actor, 0x5e) == CONGO_ENTITY;
}

void anchor_congo_damage_bind_root(void *actor)
{
    if (!actor)
    {
        s_root = s_root_object = s_root_backlink = 0;
        clear_contacts();
        return;
    }
    if (D_800C7AB2 != CONGO_ROOM || !linked_task(actor) ||
        !valid_pointer(CONGO_READ_POINTER(actor, 0x18)) ||
        HALF(actor, 0x5e) != CONGO_ENTITY ||
        !(WORD(actor, 0xe8) & ROOT_HEALTH_CONTROLLER))
        return;
    if (!anchor_congo_damage_is_root(actor))
        clear_contacts();
    s_root = actor;
    s_root_object = CONGO_READ_POINTER(actor, 0x18);
    s_root_backlink = CONGO_READ_POINTER(actor, 0x04);
    s_root_actor = HALF(actor, 0x5c);
    s_root_generation = BYTE(actor, 0x74);
}

static int can_take_damage(void *actor)
{
    unsigned int capability = WORD(actor, 0x60);
    return BYTE(actor, 0x8d) != 0 &&
           !(WORD(actor, 0x68) & (RECOVERY | REMOVE_PENDING)) &&
           (capability & CAN_RECEIVE_DAMAGE) &&
           !(capability & DEFLECT_DAMAGE);
}

static int attack_amount(unsigned int type)
{
    if (type == 0x1b) return 3;
    if (type == 0x16 || type == 0x23) return 2;
    if (type == 0x17 || type == 0x24) return 4;
    if (type == 0x22) return 8;
    return 1;
}

static int attack_type(int amount)
{
    if (amount == 1) return 0x15;
    if (amount == 2) return 0x16;
    if (amount == 3) return 0x1b;
    if (amount == 4) return 0x17;
    if (amount == 8) return 0x22;
    return 0;
}

static int local_attacker(void *task, void *object)
{
    void *player = D_801FC604_5B8514;
    if (!linked_task(player) || !linked_task(task) ||
        !valid_pointer(object) || BYTE(object, 4) != 2 ||
        CONGO_READ_POINTER(task, 0x18) != object ||
        CONGO_READ_POINTER(player, 0x18) != D_801FC60C_5B851C)
        return 0;
    /* Render-only remote projectiles never own the real playable task. */
    return task == player || CONGO_READ_POINTER(task, 0x5c) == player;
}

static CongoAttackEpisode *observe_attack(void *task, void *object)
{
    CongoAttackEpisode *episode = 0;
    unsigned int descriptor;
    unsigned int animation;
    float frame;
    int i;
    int restart;

    if (!local_attacker(task, object))
        return 0;
    descriptor = WORD(task, 0x48);
    animation = WORD(object, 0x2c);
    frame = FLOAT(object, 0x28);
    if (!descriptor || !(frame >= -10000000.0f && frame <= 10000000.0f))
        return 0;
    for (i = 0; i < s_episode_capacity; ++i)
    {
        if (s_episodes[i].task == task)
        {
            episode = &s_episodes[i];
            break;
        }
        if (!s_episodes[i].task && !episode)
            episode = &s_episodes[i];
    }
    if (!episode)
    {
        int index = s_episode_capacity;
        if (index == 0x7fffffff ||
            !mnsg_array_reserve((void **)&s_episodes, &s_episode_capacity,
                                index + 1, sizeof(*s_episodes)))
            return 0;
        episode = &s_episodes[index];
    }
    restart = !episode->task || episode->object != object ||
              episode->backlink != CONGO_READ_POINTER(task, 0x04) ||
              episode->descriptor != descriptor;
    if (task == D_801FC604_5B8514 &&
        (episode->animation != animation || frame < episode->animation_frame))
        restart = 1;
    if (restart)
    {
        s_next_sequence = s_next_sequence == 0x7fffffff ? 1 : s_next_sequence + 1;
        episode->sequence = s_next_sequence;
        episode->reported = 0;
    }
    episode->task = task;
    episode->object = object;
    episode->backlink = CONGO_READ_POINTER(task, 0x04);
    episode->descriptor = descriptor;
    episode->animation = animation;
    episode->animation_frame = frame;
    episode->seen_frame = s_frame;
    return episode;
}

RECOMP_HOOK("func_801F77F4_5B3704")
void anchor_congo_damage_scene_frame(void)
{
    int i;
    int epoch = anchor_player_models_get_epoch();
    if (!anchor_congo_damage_is_shared() || epoch != s_player_epoch)
        clear_contacts();
    s_player_epoch = epoch;
    if (s_paused)
        return;
    ++s_frame;
    if (s_capture_cooldown)
        --s_capture_cooldown;
    for (i = 0; i < s_episode_capacity; ++i)
    {
        if (s_episodes[i].task &&
            s_frame - s_episodes[i].seen_frame > 1u)
            s_episodes[i].task = 0;
    }
}

RECOMP_HOOK("func_80033404_34004")
void anchor_congo_damage_observe_sphere(void *object, void *task, void *victims)
{
    (void)victims;
    if (anchor_congo_damage_is_shared() && !s_paused &&
        task && (BYTE(task, 0x30) & 2u))
        observe_attack(task, object);
}

RECOMP_HOOK("func_80218350_5D3820")
void anchor_congo_damage_before_native(void *actor)
{
    void *attacker;
    CongoAttackEpisode *episode;
    unsigned int status;
    unsigned int index;

    if (!anchor_congo_damage_is_shared() || s_applying ||
        !anchor_congo_damage_is_root(actor))
        return;
    status = WORD(actor, 0x68);
    attacker = CONGO_READ_POINTER(actor, 0x38);
    if (!s_paused && s_encounter && s_player_epoch > 0 &&
        !anchor_remote_collision_is_scripted() && !s_capture_cooldown &&
        s_hit_count < HIT_QUEUE_SIZE && (status & CONTACT) &&
        !(status & SYNTHETIC_DAMAGE) && can_take_damage(actor) &&
        linked_task(attacker))
    {
        episode = observe_attack(attacker, CONGO_READ_POINTER(attacker, 0x18));
        if (episode && !episode->reported)
        {
            index = (s_hit_read + s_hit_count) % HIT_QUEUE_SIZE;
            s_hits[index].sequence = episode->sequence;
            s_hits[index].amount = attack_amount(BYTE(attacker, 0x4c));
            ++s_hit_count;
            episode->reported = 1;
            /* A local pending hit has the same 60-tick recovery window as
             * Congo's ordinary +90==NULL intake, without changing shared HP
             * or the authoritative recovery byte. */
            s_capture_cooldown = 60;
        }
    }
    /* Consume the input, not the HP result. Restoring these bits would let a
     * stale contact fire after a role change; restoring HP would be too late
     * because 18350 can invoke hit/death callbacks synchronously. Congo's
     * private post skips its +38 cleanup when recovery bit1 is clear, so
     * consume this victim reference as well. The attacker's +34 remains
     * untouched for its native impact/projectile lifetime callback. */
    WORD(actor, 0x68) = status & ~(CONTACT | SYNTHETIC_DAMAGE);
    if (status & (CONTACT | SYNTHETIC_DAMAGE))
        CONGO_WRITE_POINTER(actor, 0x38, 0);
}

int anchor_congo_damage_take_local_hit(AnchorCongoHit *out)
{
    if (anchor_player_models_get_epoch() != s_player_epoch)
        clear_contacts();
    if (!out || !anchor_congo_damage_is_shared() || !s_hit_count)
        return 0;
    *out = s_hits[s_hit_read];
    s_hit_read = (s_hit_read + 1u) % HIT_QUEUE_SIZE;
    --s_hit_count;
    return 1;
}

int anchor_congo_damage_apply(int amount)
{
    union { void *alignment; unsigned char bytes[0xa0]; } source;
    void *actor = s_root;
    void *previous_task;
    void *previous_attacker;
    unsigned int previous_status;
    unsigned int previous_health;
    unsigned int i;
    int type = attack_type(amount);
    int accepted;

    if (!type || s_applying || s_paused || !s_encounter ||
        !anchor_congo_damage_is_owner() ||
        !anchor_congo_damage_is_root(actor) || !can_take_damage(actor) ||
        ((WORD(actor, 0x0c) | WORD(actor, 0x10)) & CALLBACK_DISABLED) ||
        CONGO_READ_POINTER(actor, 0x90) ||
        !(WORD(actor, 0x64) & NO_GENERIC_DROPS) ||
        (WORD(actor, 0x60) & GENERIC_HIT_EFFECTS))
        return 0;
    /* Guard the verified ordinary Congo path: no +90 callback, generic drop,
     * or generic death effect. It reads attacker+4C only and never activates
     * an actor resource file, so frame-end delivery needs no TLB remapping.
     * No attacker pointer is retained by those routines. */
    for (i = 0; i < sizeof(source.bytes); ++i)
        ((volatile unsigned char *)source.bytes)[i] = 0;
    source.bytes[0x4c] = (unsigned char)type;
    previous_task = D_8016DAB4_16E6B4;
    previous_attacker = CONGO_READ_POINTER(actor, 0x38);
    previous_status = WORD(actor, 0x68);
    previous_health = BYTE(actor, 0x8d);
    s_applying = 1;
    D_8016DAB4_16E6B4 = actor;
    CONGO_WRITE_POINTER(actor, 0x38, source.bytes);
    WORD(actor, 0x68) = (previous_status & ~SYNTHETIC_DAMAGE) | CONTACT;
    func_80218350_5D3820(actor);
    D_8016DAB4_16E6B4 = previous_task;
    accepted = anchor_congo_damage_is_root(actor) &&
               BYTE(actor, 0x8d) < previous_health;
    if (anchor_congo_damage_is_root(actor))
    {
        CONGO_WRITE_POINTER(actor, 0x38, previous_attacker);
        /* Keep new native recovery/death flags. Only remove our own input. */
        WORD(actor, 0x68) = (WORD(actor, 0x68) & ~(CONTACT | SYNTHETIC_DAMAGE)) |
                           (previous_status & (CONTACT | SYNTHETIC_DAMAGE));
    }
    s_applying = 0;
    return accepted;
}
