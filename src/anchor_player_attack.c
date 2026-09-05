#include "anchor_player_attack.h"
#include "anchor_player_models.h"
#include "utils/array_utils.h"

extern int anchor_send_player_hit(int target_cid, int target_epoch,
                                  float hit_x, float hit_y, float hit_z);

typedef struct AttackHit
{
    int cid;
    int epoch;
} AttackHit;

typedef struct AttackEpisode
{
    const void *task;
    const void *object;
    unsigned int descriptor;
    unsigned int animation;
    unsigned int seen_frame;
    float animation_frame;
    int hit_count;
    int hit_capacity;
    AttackHit *hits;
} AttackEpisode;

static AttackEpisode *s_episodes;
static int s_episode_capacity;
static AnchorPlayerHitTarget *s_targets;
static int s_target_capacity;
static unsigned int s_frame;
static int s_enabled;
static int s_player_epoch;

void anchor_player_attack_reset(void)
{
    int i;
    for (i = 0; i < s_episode_capacity; ++i)
        s_episodes[i].task = 0;
    s_enabled = 0;
    s_frame = 0;
    s_player_epoch = 0;
}

void anchor_player_attack_begin_frame(int enabled, int player_epoch)
{
    int i;
    if (!enabled || player_epoch != s_player_epoch)
        anchor_player_attack_reset();
    s_enabled = enabled;
    s_player_epoch = player_epoch;
    ++s_frame;
    for (i = 0; i < s_episode_capacity; ++i)
    {
        if (s_episodes[i].task &&
            s_frame - s_episodes[i].seen_frame > 1u)
            s_episodes[i].task = 0;
    }
}

static int valid_float(float value)
{
    return value >= -10000000.0f && value <= 10000000.0f;
}

static int sphere_hits_body(const AnchorPlayerAttackSample *sample,
                            const AnchorCollisionBody *body)
{
    float dx = sample->center.x - body->position.x;
    float dy = sample->center.y - body->position.y;
    float dz = sample->center.z - body->position.z;
    float radius = sample->radius + body->radius;

    /* FUN_80033BDC uses a cylinder expanded by the attack sphere's radius,
     * with interval overlap at its flat top and bottom. Player body offsets
     * +0x40/+0x42 are zero, and these remote bodies stay upright. */
    return body->radius > 0.0f && body->height > 0.0f &&
           dx * dx + dz * dz <= radius * radius &&
           dy - sample->radius <= body->height &&
           dy + sample->radius >= 0.0f;
}

static AttackEpisode *find_episode(const AnchorPlayerAttackSample *sample)
{
    AttackEpisode *episode = 0;
    int i;
    int restart;

    for (i = 0; i < s_episode_capacity; ++i)
    {
        if (s_episodes[i].task == sample->task)
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
        /* Growing the roster must not evict a live attack's hit history. */
        if (index == 0x7fffffff ||
            !mnsg_array_reserve((void **)&s_episodes, &s_episode_capacity,
                                index + 1, sizeof(*s_episodes)))
            return 0;
        episode = &s_episodes[index];
    }
    restart = !episode->task || episode->object != sample->object ||
              episode->descriptor != sample->descriptor;
    if (sample->is_player &&
        (episode->animation != sample->animation ||
         sample->frame < episode->animation_frame))
        restart = 1;
    if (restart)
        episode->hit_count = 0;
    episode->task = sample->task;
    episode->object = sample->object;
    episode->descriptor = sample->descriptor;
    episode->animation = sample->animation;
    episode->animation_frame = sample->frame;
    episode->seen_frame = s_frame;
    return episode;
}

void anchor_player_attack_observe(const AnchorPlayerAttackSample *sample)
{
    AttackEpisode *episode;
    int count;
    int i;

    if (!s_enabled || !sample || !sample->task || !sample->object ||
        !sample->descriptor || !valid_float(sample->frame))
        return;
    episode = find_episode(sample);
    if (!episode || !(sample->radius > 0.0f && sample->radius <= 10000.0f) ||
        !valid_float(sample->center.x) || !valid_float(sample->center.y) ||
        !valid_float(sample->center.z))
        return;
    count = anchor_player_models_capacity();
    if (!mnsg_array_reserve((void **)&s_targets, &s_target_capacity, count,
                            sizeof(*s_targets)))
        return;
    count = anchor_player_models_get_hit_targets(s_targets, s_target_capacity);
    if (count < 0 || count > s_target_capacity)
        return;
    for (i = 0; i < count; ++i)
    {
        int j;
        if (!sphere_hits_body(sample, &s_targets[i].body))
            continue;
        for (j = 0; j < episode->hit_count; ++j)
        {
            if (episode->hits[j].cid == s_targets[i].cid &&
                episode->hits[j].epoch == s_targets[i].epoch)
                break;
        }
        if (j != episode->hit_count)
            continue;
        /* Reserve dedup state before emitting damage. Allocation failure can
         * retry safely on a later sphere; a sent hit must always be recorded. */
        if (j == 0x7fffffff ||
            !mnsg_array_reserve((void **)&episode->hits, &episode->hit_capacity,
                                j + 1, sizeof(*episode->hits)))
            continue;
        if (anchor_send_player_hit(s_targets[i].cid, s_targets[i].epoch,
                                    sample->center.x, sample->center.y,
                                    sample->center.z))
        {
            episode->hits[j].cid = s_targets[i].cid;
            episode->hits[j].epoch = s_targets[i].epoch;
            ++episode->hit_count;
        }
    }
}

#ifndef ANCHOR_PLAYER_ATTACK_HOST_TEST
#include "anchor.h"
#include "modding.h"

extern void *D_801FC600_5B8510; /* Native player-manager task. */
extern void *D_801FC604_5B8514; /* Native playable task. */
extern float D_80168F80_169B80;
extern float D_80168F84_169B84;
extern float D_80168F88_169B88;
extern float D_80168F8C_169B8C;
extern void func_80033898_34498(unsigned short rx, unsigned short ry,
                               unsigned short rz, float *x, float *y, float *z);

static int s_player_attack_scan;

RECOMP_HOOK("func_801F77F4_5B3704")
void anchor_player_attack_scene_frame(void)
{
    s_player_attack_scan = 0;
    anchor_player_attack_begin_frame(anchor_is_connected() &&
                                    !anchor_remote_collision_is_scripted(),
                                    anchor_player_models_get_epoch());
}

RECOMP_HOOK("func_80033024_33C24")
void anchor_player_attack_scan_begin(void *attackers, void *victims)
{
    (void)victims;
    s_player_attack_scan = s_enabled && D_801FC600_5B8510 &&
                           attackers == D_801FC600_5B8510;
}

RECOMP_HOOK_RETURN("func_80033024_33C24")
void anchor_player_attack_scan_end(void)
{
    s_player_attack_scan = 0;
}

RECOMP_HOOK("func_80033404_34004")
void anchor_player_attack_native_sphere(void *object, void *task, void *victims)
{
    unsigned char *actor = task;
    unsigned char *model = object;
    AnchorPlayerAttackSample sample;
    (void)victims;

    /* The generic collision scanner already decoded the active animation
     * sphere (or a projectile's fixed sphere) into these four float globals.
     * Native constructors store the local owner in projectile task+0x5c.
     * Never register a remote task as a native victim: doing so consumes
     * attacker+0x34 and can redirect native enemy/projectile callbacks. */
    if (!s_player_attack_scan || !actor || !model ||
        !D_801FC604_5B8514 || anchor_remote_collision_is_scripted() ||
        (task != D_801FC604_5B8514 &&
         *(void **)(actor + 0x5c) != D_801FC604_5B8514) ||
        !(actor[0x30] & 2u) || !*(unsigned int *)(actor + 0x48) ||
        *(unsigned int *)(actor + 0x34) ||
        *(unsigned int *)(actor + 0x38) || model[4] != 2)
        return;

    sample.task = task;
    sample.object = object;
    sample.descriptor = *(unsigned int *)(actor + 0x48);
    sample.animation = *(unsigned int *)(model + 0x2c);
    sample.frame = *(float *)(model + 0x28);
    sample.is_player = task == D_801FC604_5B8514;
    sample.center.x = D_80168F80_169B80 * *(float *)(model + 0x1c);
    sample.center.y = D_80168F84_169B84 * *(float *)(model + 0x20);
    sample.center.z = D_80168F88_169B88 * *(float *)(model + 0x24);
    sample.radius = D_80168F8C_169B8C * *(float *)(model + 0x1c);
    func_80033898_34498(*(unsigned short *)(model + 0x14) & 0x3ff,
                       *(unsigned short *)(model + 0x16) & 0x3ff,
                       *(unsigned short *)(model + 0x18) & 0x3ff,
                       &sample.center.x, &sample.center.y, &sample.center.z);
    sample.center.x += *(float *)(model + 8);
    sample.center.y += *(float *)(model + 0xc);
    sample.center.z += *(float *)(model + 0x10);
    anchor_player_attack_observe(&sample);
}
#endif
