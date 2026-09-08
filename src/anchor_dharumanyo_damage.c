#include "anchor_dharumanyo_damage.h"
#include "anchor_player_models.h"
#include "anchor_remote_collision.h"
#include "anchor_remote_model_pool.h"
#include "utils/array_utils.h"

#ifndef ANCHOR_DHARUMANYO_DAMAGE_HOST_TEST
#include "modding.h"
#else
#define RECOMP_HOOK(name)
#endif

typedef void (*DharumanyoDamageCallback)(void *, void *);

extern unsigned short D_800C7AB2;
extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern void *D_8016DAB4_16E6B4;
extern void func_80218350_5D3820(void *actor);
extern void func_08003F84_6CC194(void *task, void *object);

#define DHARUMANYO_ROOM 0x49u
#define DHARUMANYO_ACTOR 0xCCu
#define DHARUMANYO_ENTITY 0xCCu
#define CAN_RECEIVE_DAMAGE 0x00200000u
#define DEFLECT_DAMAGE 0x00400000u
#define RECOVERY 0x00000001u
#define REMOVE_PENDING 0x00000002u
#define CONTACT 0x00000080u
#define SYNTHETIC_DAMAGE 0x00040000u
#define CALLBACK_DISABLED 0x00800000ul
#define HIT_QUEUE_SIZE 8u
#define HIT_RECOVERY_TICKS 90u

#define BYTE(p, off) (*(volatile unsigned char *)((unsigned char *)(p) + (off)))
#define HALF(p, off) (*(volatile unsigned short *)((unsigned char *)(p) + (off)))
#define WORD(p, off) (*(volatile unsigned int *)((unsigned char *)(p) + (off)))
#define FLOAT(p, off) (*(volatile float *)((unsigned char *)(p) + (off)))
#ifndef DHARUMANYO_DAMAGE_READ_POINTER
#define DHARUMANYO_DAMAGE_READ_POINTER(p, off) \
    (*(void *volatile *)((unsigned char *)(p) + (off)))
#define DHARUMANYO_DAMAGE_WRITE_POINTER(p, off, value) \
    (*(void *volatile *)((unsigned char *)(p) + (off)) = (value))
#endif
#ifndef DHARUMANYO_DAMAGE_AI
#define DHARUMANYO_DAMAGE_AI(p) \
    (*(DharumanyoDamageCallback volatile *)((unsigned char *)(p) + 0x0c))
#define DHARUMANYO_DAMAGE_POST(p) \
    (*(DharumanyoDamageCallback volatile *)((unsigned char *)(p) + 0x10))
#endif
#ifndef DHARUMANYO_DAMAGE_CALLBACKS_DISABLED
#define DHARUMANYO_DAMAGE_CALLBACKS_DISABLED(p) \
    ((((unsigned long)DHARUMANYO_DAMAGE_AI(p) | \
       (unsigned long)DHARUMANYO_DAMAGE_POST(p)) & CALLBACK_DISABLED) != 0)
#endif

typedef struct DharumanyoDamageIdentity
{
    void *task;
    void *object;
    void *backlink;
    unsigned short actor;
    unsigned char generation;
} DharumanyoDamageIdentity;

typedef struct DharumanyoAttackEpisode
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
} DharumanyoAttackEpisode;

static DharumanyoDamageIdentity s_root;
static DharumanyoDamageIdentity s_carrier;
static int s_active;
static int s_owner;
static int s_paused;
static unsigned int s_encounter;
static unsigned int s_frame;
static int s_player_epoch;
static int s_next_sequence;
static int s_capture_cooldown;
static int s_applying;
static DharumanyoAttackEpisode *s_episodes;
static int s_episode_capacity;
static AnchorDharumanyoHit s_hits[HIT_QUEUE_SIZE];
static unsigned int s_hit_read;
static unsigned int s_hit_count;

static int callback_is(DharumanyoDamageCallback a,
                       DharumanyoDamageCallback b)
{
    return ((unsigned long)a & ~CALLBACK_DISABLED) ==
           ((unsigned long)b & ~CALLBACK_DISABLED);
}

static int valid_pointer(const void *pointer)
{
#ifdef ANCHOR_DHARUMANYO_DAMAGE_HOST_TEST
    return pointer != 0 && anchor_remote_model_pool_contains(pointer);
#else
    unsigned int physical = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return (physical >= 0x1000u && physical < 0x800000u) ||
           anchor_remote_model_pool_contains(pointer);
#endif
}

static int linked_task(const void *task)
{
    void *backlink;
    if (!valid_pointer(task))
        return 0;
    backlink = DHARUMANYO_DAMAGE_READ_POINTER(task, 0x04);
    return valid_pointer(backlink) &&
           DHARUMANYO_DAMAGE_READ_POINTER(backlink, 0) == task;
}

static void clear_identity(DharumanyoDamageIdentity *identity)
{
    identity->task = 0;
    identity->object = 0;
    identity->backlink = 0;
    identity->actor = 0;
    identity->generation = 0;
}

static void set_identity(DharumanyoDamageIdentity *identity, void *task)
{
    identity->task = task;
    identity->object = DHARUMANYO_DAMAGE_READ_POINTER(task, 0x18);
    identity->backlink = DHARUMANYO_DAMAGE_READ_POINTER(task, 0x04);
    identity->actor = HALF(task, 0x5c);
    identity->generation = BYTE(task, 0x74);
}

static int identity_is_live(const DharumanyoDamageIdentity *identity)
{
    return identity->task && linked_task(identity->task) &&
           DHARUMANYO_DAMAGE_READ_POINTER(identity->task, 0x04) ==
               identity->backlink &&
           DHARUMANYO_DAMAGE_READ_POINTER(identity->task, 0x18) ==
               identity->object &&
           HALF(identity->task, 0x5c) == identity->actor &&
           BYTE(identity->task, 0x74) == identity->generation &&
           valid_pointer(identity->object) &&
           !(WORD(identity->task, 0x68) & REMOVE_PENDING);
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

void anchor_dharumanyo_damage_reset(void)
{
    clear_contacts();
    clear_identity(&s_root);
    clear_identity(&s_carrier);
    s_active = 0;
    s_owner = 0;
    s_paused = 0;
    s_encounter = 0;
    s_frame = 0;
    s_player_epoch = 0;
    s_applying = 0;
    /* Hit sequence numbers remain monotonic across task-slot reuse. */
}

void anchor_dharumanyo_damage_set_context(int active, int owner, int paused,
                                           unsigned int encounter)
{
    if (!!active != s_active || encounter != s_encounter)
        clear_contacts();
    s_active = !!active;
    s_owner = !!owner;
    s_paused = !!paused;
    s_encounter = encounter;
}

int anchor_dharumanyo_damage_is_shared(void)
{
    return s_active && D_800C7AB2 == DHARUMANYO_ROOM;
}

int anchor_dharumanyo_damage_is_owner(void)
{
    return anchor_dharumanyo_damage_is_shared() && s_owner;
}

int anchor_dharumanyo_damage_is_carrier(const void *actor)
{
    return actor && actor == s_carrier.task &&
           D_800C7AB2 == DHARUMANYO_ROOM &&
           identity_is_live(&s_root) && identity_is_live(&s_carrier) &&
           HALF(s_root.task, 0x5c) == DHARUMANYO_ACTOR &&
           HALF(s_carrier.task, 0x5c) == DHARUMANYO_ACTOR &&
           HALF(s_carrier.task, 0x5e) == DHARUMANYO_ENTITY &&
           DHARUMANYO_DAMAGE_READ_POINTER(s_root.task, 0xdc) ==
               s_carrier.task &&
           DHARUMANYO_DAMAGE_READ_POINTER(s_carrier.task, 0xdc) ==
               s_root.task &&
           callback_is(DHARUMANYO_DAMAGE_POST(s_carrier.task),
                       func_08003F84_6CC194);
}

void anchor_dharumanyo_damage_bind(void *root, void *carrier)
{
    int same;
    if (!root || !carrier)
    {
        clear_identity(&s_root);
        clear_identity(&s_carrier);
        clear_contacts();
        return;
    }
    if (D_800C7AB2 != DHARUMANYO_ROOM ||
        !linked_task(root) || !linked_task(carrier) ||
        !valid_pointer(DHARUMANYO_DAMAGE_READ_POINTER(root, 0x18)) ||
        !valid_pointer(DHARUMANYO_DAMAGE_READ_POINTER(carrier, 0x18)) ||
        HALF(root, 0x5c) != DHARUMANYO_ACTOR ||
        HALF(carrier, 0x5c) != DHARUMANYO_ACTOR ||
        HALF(carrier, 0x5e) != DHARUMANYO_ENTITY ||
        DHARUMANYO_DAMAGE_READ_POINTER(root, 0xdc) != carrier ||
        DHARUMANYO_DAMAGE_READ_POINTER(carrier, 0xdc) != root ||
        !callback_is(DHARUMANYO_DAMAGE_POST(carrier),
                     func_08003F84_6CC194))
        return;
    same = s_root.task == root && s_carrier.task == carrier &&
           identity_is_live(&s_root) && identity_is_live(&s_carrier);
    if (!same)
        clear_contacts();
    set_identity(&s_root, root);
    set_identity(&s_carrier, carrier);
}

static int can_take_damage(void *actor)
{
    unsigned int capability;
    unsigned int status;
    if (!anchor_dharumanyo_damage_is_carrier(actor))
        return 0;
    capability = WORD(actor, 0x60);
    status = WORD(actor, 0x68);
    return BYTE(actor, 0xd1) != 0 && HALF(actor, 0xd6) == 0 &&
           BYTE(actor, 0x8d) != 0 &&
           !(status & (RECOVERY | REMOVE_PENDING | SYNTHETIC_DAMAGE)) &&
           (capability & CAN_RECEIVE_DAMAGE) &&
           !(capability & DEFLECT_DAMAGE);
}

static int local_attacker(void *task, void *object)
{
    void *player = D_801FC604_5B8514;
    if (!linked_task(player) || !linked_task(task) ||
        !valid_pointer(object) || BYTE(object, 4) != 2 ||
        DHARUMANYO_DAMAGE_READ_POINTER(task, 0x18) != object ||
        DHARUMANYO_DAMAGE_READ_POINTER(player, 0x18) !=
            D_801FC60C_5B851C)
        return 0;
    return task == player ||
           DHARUMANYO_DAMAGE_READ_POINTER(task, 0x5c) == player;
}

static DharumanyoAttackEpisode *observe_attack(void *task, void *object)
{
    DharumanyoAttackEpisode *episode = 0;
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
              episode->backlink !=
                  DHARUMANYO_DAMAGE_READ_POINTER(task, 0x04) ||
              episode->descriptor != descriptor;
    if (task == D_801FC604_5B8514 &&
        (episode->animation != animation || frame < episode->animation_frame))
        restart = 1;
    if (restart)
    {
        s_next_sequence = s_next_sequence == 0x7fffffff
                              ? 1 : s_next_sequence + 1;
        episode->sequence = s_next_sequence;
        episode->reported = 0;
    }
    episode->task = task;
    episode->object = object;
    episode->backlink = DHARUMANYO_DAMAGE_READ_POINTER(task, 0x04);
    episode->descriptor = descriptor;
    episode->animation = animation;
    episode->animation_frame = frame;
    episode->seen_frame = s_frame;
    return episode;
}

RECOMP_HOOK("func_801F77F4_5B3704")
void anchor_dharumanyo_damage_scene_frame(void)
{
    int i;
    int epoch = anchor_player_models_get_epoch();
    if (!anchor_dharumanyo_damage_is_shared() || epoch != s_player_epoch)
        clear_contacts();
    s_player_epoch = epoch;
    if (s_paused)
        return;
    ++s_frame;
    if (s_capture_cooldown)
        --s_capture_cooldown;
    for (i = 0; i < s_episode_capacity; ++i)
        if (s_episodes[i].task &&
            s_frame - s_episodes[i].seen_frame > 1u)
            s_episodes[i].task = 0;
}

RECOMP_HOOK("func_80033404_34004")
void anchor_dharumanyo_damage_observe_sphere(void *object, void *task,
                                             void *victims)
{
    (void)victims;
    if (anchor_dharumanyo_damage_is_shared() && !s_paused && task &&
        (BYTE(task, 0x30) & 2u))
        observe_attack(task, object);
}

RECOMP_HOOK("func_80218350_5D3820")
void anchor_dharumanyo_damage_before_native(void *actor)
{
    DharumanyoAttackEpisode *episode;
    void *attacker;
    unsigned int index;
    unsigned int status;

    if (!anchor_dharumanyo_damage_is_shared() || s_applying ||
        !anchor_dharumanyo_damage_is_carrier(actor))
        return;
    status = WORD(actor, 0x68);
    /* boss_sync's terminal handoff owns this verified fast-damage input. */
    if (status & SYNTHETIC_DAMAGE)
        return;
    attacker = DHARUMANYO_DAMAGE_READ_POINTER(actor, 0x38);
    if (!s_paused && s_encounter && s_player_epoch > 0 &&
        !anchor_remote_collision_is_scripted() && !s_capture_cooldown &&
        s_hit_count < HIT_QUEUE_SIZE && (status & CONTACT) &&
        can_take_damage(actor) && linked_task(attacker))
    {
        episode = observe_attack(
            attacker, DHARUMANYO_DAMAGE_READ_POINTER(attacker, 0x18));
        if (episode && !episode->reported)
        {
            index = (s_hit_read + s_hit_count) % HIT_QUEUE_SIZE;
            s_hits[index].sequence = episode->sequence;
            s_hits[index].amount = 1;
            ++s_hit_count;
            episode->reported = 1;
            s_capture_cooldown = HIT_RECOVERY_TICKS;
        }
    }
    /* Every shared physical contact is resolved by the elected owner. The
     * attacker-side impact link remains intact for its native lifetime. */
    WORD(actor, 0x68) = status & ~CONTACT;
    if (status & CONTACT)
        DHARUMANYO_DAMAGE_WRITE_POINTER(actor, 0x38, 0);
}

int anchor_dharumanyo_damage_take_local_hit(AnchorDharumanyoHit *out)
{
    if (anchor_player_models_get_epoch() != s_player_epoch)
        clear_contacts();
    if (!out || !anchor_dharumanyo_damage_is_shared() || !s_hit_count)
        return 0;
    *out = s_hits[s_hit_read];
    s_hit_read = (s_hit_read + 1u) % HIT_QUEUE_SIZE;
    --s_hit_count;
    return 1;
}

int anchor_dharumanyo_damage_apply(int amount)
{
    void *actor = s_carrier.task;
    void *previous_task;
    void *previous_attacker;
    unsigned int previous_status;
    unsigned int previous_health;
    int accepted;

    if (amount != 1 || s_applying || s_paused || !s_encounter ||
        !anchor_dharumanyo_damage_is_owner() ||
        !can_take_damage(actor) ||
        DHARUMANYO_DAMAGE_CALLBACKS_DISABLED(actor))
        return 0;
    previous_task = D_8016DAB4_16E6B4;
    previous_attacker = DHARUMANYO_DAMAGE_READ_POINTER(actor, 0x38);
    previous_status = WORD(actor, 0x68);
    previous_health = BYTE(actor, 0x8d);
    s_applying = 1;
    D_8016DAB4_16E6B4 = actor;
    BYTE(actor, 0x8d) = 2;
    WORD(actor, 0x68) = (previous_status & ~CONTACT) | SYNTHETIC_DAMAGE;
    func_80218350_5D3820(actor);
    D_8016DAB4_16E6B4 = previous_task;
    accepted = anchor_dharumanyo_damage_is_carrier(actor) &&
               BYTE(actor, 0x8d) == 1 &&
               (WORD(actor, 0x68) & RECOVERY) != 0;
    if (anchor_dharumanyo_damage_is_carrier(actor))
    {
        DHARUMANYO_DAMAGE_WRITE_POINTER(actor, 0x38, previous_attacker);
        WORD(actor, 0x68) &= ~(CONTACT | SYNTHETIC_DAMAGE);
        if (!accepted)
            BYTE(actor, 0x8d) = (unsigned char)previous_health;
    }
    s_applying = 0;
    return accepted;
}
