#include "combat/anchor_player_cube.h"
#include "combat/anchor_player_freeze.h"
#include "combat/anchor_player_freeze_visual.h"
#include "core/anchor.h"
#include "player/anchor_player_models.h"
#include "progression/item_sync.h"
#include "platform/modding.h"

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned short D_800C7AB2;
extern unsigned int D_8015C604;
extern void func_801E54A8_5A13B8(void *player, void *task);
extern void func_801E55A0_5A14B0(void *player);
extern void func_801DACDC_596BEC(void *player, unsigned char action);
extern void func_80033898_34498(unsigned short rx, unsigned short ry,
                                unsigned short rz, float *x, float *y,
                                float *z);
extern void *func_8002C9D4_2D5D4(void *out, float x, float y, float z,
                                   float dx, float dy, float dz, float range);

#define CUBE_MAX_INTERACT_DISTANCE 210.0f
#define CUBE_GRANT_WAIT 30
#define CUBE_LEASE 30
#define CUBE_FLIGHT_LIMIT 180
#define CUBE_FLIGHT_SEGMENT 32.0f
#define CUBE_FLIGHT_SPEED_LIMIT 128.0f
#define CUBE_GRAVITY 0.6666666269302368f
#define CUBE_IMPACT_ATTACK_FRAMES 5
#define CUBE_MAX_RAY_SAMPLES 7
#define CUBE_INTERACT_STACK 8
/* The visual cube extends 100 * scale below its object center. Native carry
 * and throw poses put that center near the hand, leaving the cube in ground. */
#define CUBE_CARRY_LIFT 180.0f

typedef struct CubeQuery
{
    float origin[3], direction[3], delta[3], normal[3];
    unsigned int surface_data;
    unsigned char surface_type, padding_35;
    unsigned short surface_flags, hit, padding_3a;
    float distance_squared;
    unsigned int object, task;
} CubeQuery;
_Static_assert(sizeof(CubeQuery) == 0x48, "native cube query layout");

typedef struct CubeVictim
{
    int carrier_cid, carrier_session, carrier_epoch;
    int target_epoch, carry_id, control_seq, lease;
    float x, y, z;
    unsigned short room;
    unsigned char moving, flight, pose_ready;
} CubeVictim;

typedef struct CubeCarrier
{
    int target_cid, target_session, target_epoch, carry_id;
    int local_epoch, wait, pose_timer, flight_frames;
    int impact_attack_frames;
    void *player, *task, *object;
    float scale, vx, vy, vz;
    unsigned short room;
    unsigned char phase; /* 1 request, 2 held, 3 flight, 4 impact settle */
    unsigned char throw_sent, impact_sent;
} CubeCarrier;

typedef struct CubeInteract
{
    unsigned int selector;
    void *player;
    unsigned char action, candidate, scoped;
} CubeInteract;

static CubeVictim s_victim;
static CubeCarrier s_carrier;
static CubeInteract s_interact[CUBE_INTERACT_STACK];
static unsigned int s_interact_depth;
static int s_next_carry_id;

static int coordinate(float v)
{
    return v >= -1000000.0f && v <= 1000000.0f;
}
static int valid_pose(float x, float y, float z)
{
    return coordinate(x) && coordinate(y) && coordinate(z);
}
static float read_float(const void *p, unsigned int offset)
{
    return *(const volatile float *)((const unsigned char *)p + offset);
}
static unsigned char read_byte(const void *p, unsigned int offset)
{
    return *(const volatile unsigned char *)((const unsigned char *)p + offset);
}
static int held_task_is_ours(void)
{
    void *work;
    if (!s_carrier.task || !s_carrier.player ||
        s_carrier.player != D_801FC604_5B8514 ||
        !anchor_player_freeze_visual_owns_task(s_carrier.task))
        return 0;
    work = *(void **)((unsigned char *)s_carrier.player + 0x5c);
    return work && *(void **)((unsigned char *)work + 0x8c) == s_carrier.task;
}
static void clear_impact_attack(void)
{
    unsigned char *task = s_carrier.task;
    if (!task || !anchor_player_freeze_visual_owns_task(task))
        return;
    task[0x30] &= (unsigned char)~2u;
    *(unsigned int *)(task + 0x48) = 0;
    *(unsigned short *)(task + 0x4c) = 0;
    *(unsigned short *)(task + 0x4e) = 0;
    *(void **)(task + 0x34) = 0;
    *(void **)(task + 0x38) = 0;
}
static void arm_impact_attack(void)
{
    unsigned char *task = s_carrier.task;
    if (!task || !anchor_player_freeze_visual_owns_task(task))
        return;
    /* The native bomb uses one fixed sphere at impact. This cube's object
     * scale is 2 * freeze scale, so radius 75 covers 150 * freeze scale:
     * 50 percent beyond the cube's 100 * freeze scale half-width. The normal
     * native intake handles enemy and breakable-object contact. */
    task[0x30] |= 2u;
    *(unsigned int *)(task + 0x48) = 0xffffffffu;
    *(unsigned short *)(task + 0x4c) = 0x1cu;
    *(unsigned short *)(task + 0x4e) = 75u;
    *(void **)(task + 0x34) = 0;
    *(void **)(task + 0x38) = 0;
    *(void **)(task + 0x5c) = 0;
}
static void detach_carrier(void)
{
    if (held_task_is_ours())
    {
        func_801E55A0_5A14B0(s_carrier.player);
        func_801DACDC_596BEC(s_carrier.player, 0);
    }
}
static int send_control(int op, int cid, int epoch, int carry_id,
                        float x, float y, float z,
                        float vx, float vy, float vz, int rx, int ry, int rz)
{
    if (!valid_pose(x, y, z) || !coordinate(vx) || !coordinate(vy) ||
        !coordinate(vz))
        return 0;
    return anchor_send_player_cube_control(op, cid, epoch, carry_id,
        (int)(x * 100.0f), (int)(y * 100.0f), (int)(z * 100.0f),
        (int)(vx * 100.0f), (int)(vy * 100.0f), (int)(vz * 100.0f),
        rx, ry, rz, anchor_player_models_get_epoch());
}
static void cancel_carrier(void)
{
    if (s_carrier.phase >= 1 && s_carrier.phase <= 3 &&
        s_carrier.target_cid > 0 &&
        anchor_is_connected())
        (void)send_control(ANCHOR_CUBE_CANCEL, s_carrier.target_cid,
            s_carrier.target_epoch, s_carrier.carry_id,
            0, 0, 0, 0, 0, 0, 0, 0, 0);
    clear_impact_attack();
    detach_carrier();
    s_carrier.phase = 0;
    s_carrier.impact_attack_frames = 0;
    s_carrier.task = s_carrier.object = 0;
}
void anchor_player_cube_reset(void)
{
    cancel_carrier();
    s_victim.moving = s_victim.flight = s_victim.pose_ready = 0;
    s_interact_depth = 0;
}
void anchor_player_cube_victim_thaw(void)
{
    s_victim.moving = s_victim.flight = s_victim.pose_ready = 0;
}
static void cancel_victim(void)
{
    if (s_victim.moving && anchor_is_connected() &&
        s_victim.carrier_cid > 0)
        (void)send_control(ANCHOR_CUBE_CANCEL, s_victim.carrier_cid,
            s_victim.carrier_epoch, s_victim.carry_id,
            0, 0, 0, 0, 0, 0, 0, 0, 0);
    anchor_player_cube_victim_thaw();
}
int anchor_player_cube_victim_moving(void)
{
    return s_victim.moving != 0;
}
int anchor_player_cube_victim_pose(float *x, float *y, float *z)
{
    if (!s_victim.moving || !s_victim.pose_ready || !x || !y || !z)
        return 0;
    *x = s_victim.x;
    *y = s_victim.y;
    *z = s_victim.z;
    return 1;
}
int anchor_player_cube_visual_owned(int cid, int session, int epoch)
{
    return s_carrier.phase >= 2 && s_carrier.target_cid == cid &&
           s_carrier.target_session == session &&
           s_carrier.target_epoch == epoch &&
           anchor_player_freeze_visual_owns_task(s_carrier.task);
}
int anchor_player_cube_visual_native_pose(int cid, int session, int epoch)
{
    return anchor_player_cube_visual_owned(cid, session, epoch);
}

static int newer_sequence(int incoming, int previous)
{
    unsigned int delta = ((unsigned int)incoming - (unsigned int)previous) &
                         0x7fffffffu;
    return delta > 0 && delta < 0x40000000u;
}

/* The native interaction caller invokes this only on the fresh 0x4000 button
 * edge. Suppress its attack selector temporarily, let its ordinary actor
 * pickup search run first, and restore the selector on every return. */
RECOMP_HOOK("func_801E4624_5A0534")
void anchor_player_cube_interact_begin(void *player, unsigned int mode)
{
    unsigned int depth = s_interact_depth++;
    CubeInteract *scope;
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    float x, y, z;
    int i, count, near_cube = 0;
    void *work;
    if (depth >= CUBE_INTERACT_STACK)
        return;
    scope = &s_interact[depth];
    scope->scoped = scope->candidate = 0;
    scope->player = player;
    if (player != D_801FC604_5B8514 || (mode & 0xffu) != 0 ||
        s_carrier.phase || anchor_player_freeze_active() ||
        !anchor_is_connected() || !item_sync_save_is_loaded())
        return;
    work = *(void **)((unsigned char *)player + 0x5c);
    if (!work || *(void **)((unsigned char *)work + 0x8c) ||
        !D_801FC60C_5B851C)
        return;
    x = read_float(D_801FC60C_5B851C, 8);
    y = read_float(D_801FC60C_5B851C, 0xc);
    z = read_float(D_801FC60C_5B851C, 0x10);
    count = anchor_player_freeze_visual_get_cubes(cubes,
                                                   ANCHOR_FREEZE_VISUAL_MAX);
    for (i = 0; i < count; ++i)
    {
        float dx, dy, dz;
        if (cubes[i].cid <= 0 || cubes[i].session <= 0 ||
            cubes[i].epoch <= 0)
            continue;
        dx = x - (cubes[i].cube.min.x + cubes[i].cube.max.x) * 0.5f;
        dy = y - cubes[i].cube.min.y;
        dz = z - (cubes[i].cube.min.z + cubes[i].cube.max.z) * 0.5f;
        if (dx * dx + dy * dy + dz * dz <
            CUBE_MAX_INTERACT_DISTANCE * CUBE_MAX_INTERACT_DISTANCE)
            near_cube = 1;
    }
    if (!near_cube)
        return;
    scope->selector = D_8015C604;
    scope->action = read_byte(player, 0xcc);
    scope->candidate = 1;
    D_8015C604 = 0;
    scope->scoped = 1;
}

RECOMP_HOOK_RETURN("func_801E4624_5A0534")
void anchor_player_cube_interact_end(void)
{
    CubeInteract *scope;
    AnchorFreezeCubeCollision cubes[ANCHOR_FREEZE_VISUAL_MAX];
    float x, y, z, best = CUBE_MAX_INTERACT_DISTANCE * CUBE_MAX_INTERACT_DISTANCE;
    int i, count, chosen = -1;
    void *work;
    if (!s_interact_depth)
        return;
    --s_interact_depth;
    if (s_interact_depth >= CUBE_INTERACT_STACK)
        return;
    scope = &s_interact[s_interact_depth];
    if (!scope->scoped)
        return;
    D_8015C604 = scope->selector;
    if (!scope->candidate || scope->player != D_801FC604_5B8514 ||
        !D_801FC60C_5B851C || s_carrier.phase ||
        read_byte(scope->player, 0xcc) != scope->action)
        return;
    work = *(void **)((unsigned char *)scope->player + 0x5c);
    if (!work || *(void **)((unsigned char *)work + 0x8c))
        return; /* Native carried object has priority. */
    x = read_float(D_801FC60C_5B851C, 8);
    y = read_float(D_801FC60C_5B851C, 0xc);
    z = read_float(D_801FC60C_5B851C, 0x10);
    count = anchor_player_freeze_visual_get_cubes(cubes,
                                                   ANCHOR_FREEZE_VISUAL_MAX);
    for (i = 0; i < count; ++i)
    {
        float cx, cy, cz, dx, dy, dz, distance;
        if (cubes[i].cid <= 0 || cubes[i].session <= 0 ||
            cubes[i].epoch <= 0)
            continue;
        cx = (cubes[i].cube.min.x + cubes[i].cube.max.x) * 0.5f;
        cy = cubes[i].cube.min.y;
        cz = (cubes[i].cube.min.z + cubes[i].cube.max.z) * 0.5f;
        dx = x - cx; dy = y - cy; dz = z - cz;
        distance = dx * dx + dy * dy + dz * dz;
        if (distance < best)
        {
            best = distance;
            chosen = i;
        }
    }
    if (chosen < 0)
        return;
    s_next_carry_id = s_next_carry_id == 0x7fffffff ? 1 : s_next_carry_id + 1;
    if (!send_control(ANCHOR_CUBE_REQUEST, cubes[chosen].cid,
                      cubes[chosen].epoch, s_next_carry_id,
                      x, y, z, 0, 0, 0, 0, 0, 0))
        return;
    s_carrier.target_cid = cubes[chosen].cid;
    s_carrier.target_session = cubes[chosen].session;
    s_carrier.target_epoch = cubes[chosen].epoch;
    s_carrier.carry_id = s_next_carry_id;
    s_carrier.local_epoch = anchor_player_models_get_epoch();
    s_carrier.player = scope->player;
    s_carrier.room = D_800C7AB2;
    s_carrier.wait = CUBE_GRANT_WAIT;
    s_carrier.phase = 1;
}

static void accept_request(const AnchorPlayerCubeControl *c)
{
    int self = (int)anchor_get_client_id();
    int epoch = anchor_player_models_get_epoch();
    float px, py, pz, cx, cy, cz;
    float victim_x, victim_y, victim_z;
    if (c->target_cid != self || c->target_epoch != epoch ||
        c->sender_cid <= 0 || c->source_epoch <= 0 ||
        c->source_session <= 0 || c->carry_id <= 0 ||
        !anchor_player_freeze_active() || s_victim.moving ||
        !D_801FC60C_5B851C ||
        !anchor_player_models_peer_is_current(c->sender_cid,
                                              c->source_session,
                                              c->source_epoch))
        return;
    cx = (float)c->x100 / 100.0f;
    cy = (float)c->y100 / 100.0f;
    cz = (float)c->z100 / 100.0f;
    px = read_float(D_801FC60C_5B851C, 8);
    py = read_float(D_801FC60C_5B851C, 0xc);
    pz = read_float(D_801FC60C_5B851C, 0x10);
    victim_x = px; victim_y = py; victim_z = pz;
    if (!valid_pose(cx, cy, cz) || !valid_pose(px, py, pz) ||
        (cx - px) * (cx - px) + (cy - py) * (cy - py) +
            (cz - pz) * (cz - pz) >
                CUBE_MAX_INTERACT_DISTANCE * CUBE_MAX_INTERACT_DISTANCE ||
        !anchor_player_models_get_sound_position(c->sender_cid,
            c->source_session, c->source_epoch, &px, &py, &pz) ||
        (cx - px) * (cx - px) + (cy - py) * (cy - py) +
            (cz - pz) * (cz - pz) > 100.0f * 100.0f)
        return;
    if (!send_control(ANCHOR_CUBE_GRANT, c->sender_cid,
                      c->source_epoch, c->carry_id,
                      read_float(D_801FC60C_5B851C, 8),
                      read_float(D_801FC60C_5B851C, 0xc),
                      read_float(D_801FC60C_5B851C, 0x10),
                      0, 0, 0, 0, 0, 0))
        return;
    s_victim.carrier_cid = c->sender_cid;
    s_victim.carrier_session = c->source_session;
    s_victim.carrier_epoch = c->source_epoch;
    s_victim.target_epoch = epoch;
    s_victim.carry_id = c->carry_id;
    s_victim.control_seq = c->control_seq;
    s_victim.room = D_800C7AB2;
    s_victim.lease = CUBE_LEASE;
    s_victim.moving = 1;
    s_victim.flight = 0;
    s_victim.x = victim_x;
    s_victim.y = victim_y;
    s_victim.z = victim_z;
    s_victim.pose_ready = 1;
}

static void accept_grant(const AnchorPlayerCubeControl *c)
{
    void *task = 0, *object = 0;
    void *work;
    float scale = 0;
    if (s_carrier.phase != 1 || c->target_cid != (int)anchor_get_client_id() ||
        c->target_epoch != s_carrier.local_epoch ||
        c->sender_cid != s_carrier.target_cid ||
        c->source_session != s_carrier.target_session ||
        c->source_epoch != s_carrier.target_epoch ||
        c->carry_id != s_carrier.carry_id ||
        !anchor_player_freeze_visual_get_native(c->sender_cid,
            c->source_session, c->source_epoch, &task, &object, &scale) ||
        !task || !object || scale <= 0.0f || scale > 10.0f ||
        s_carrier.player != D_801FC604_5B8514)
        return;
    work = *(void **)((unsigned char *)s_carrier.player + 0x5c);
    if (!work || *(void **)((unsigned char *)work + 0x8c) ||
        anchor_player_freeze_active())
        return;
    func_801E54A8_5A13B8(s_carrier.player, task);
    if (*(void **)((unsigned char *)work + 0x8c) != task)
        return;
    s_carrier.task = task;
    s_carrier.object = object;
    s_carrier.scale = scale;
    s_carrier.phase = 2;
    s_carrier.pose_timer = 0;
    func_801DACDC_596BEC(s_carrier.player, 0x4e);
}

static void accept_victim_control(const AnchorPlayerCubeControl *c)
{
    float x, y, z;
    if (!s_victim.moving || c->target_cid != (int)anchor_get_client_id() ||
        c->target_epoch != s_victim.target_epoch ||
        c->sender_cid != s_victim.carrier_cid ||
        c->source_session != s_victim.carrier_session ||
        c->source_epoch != s_victim.carrier_epoch ||
        c->carry_id != s_victim.carry_id ||
        !newer_sequence(c->control_seq, s_victim.control_seq))
        return;
    s_victim.control_seq = c->control_seq;
    if (c->op == ANCHOR_CUBE_CANCEL)
    {
        anchor_player_cube_victim_thaw();
        return;
    }
    if (c->op == ANCHOR_CUBE_IMPACT)
    {
        if (!s_victim.flight)
            return;
        x = (float)c->x100 / 100.0f;
        y = (float)c->y100 / 100.0f;
        z = (float)c->z100 / 100.0f;
        if (!valid_pose(x, y, z) ||
            !s_victim.pose_ready ||
            (x - s_victim.x) * (x - s_victim.x) +
            (y - s_victim.y) * (y - s_victim.y) +
            (z - s_victim.z) * (z - s_victim.z) >= 500.0f * 500.0f)
            return;
        s_victim.x = x; s_victim.y = y; s_victim.z = z;
        anchor_player_freeze_thaw_on_cube_impact();
        return;
    }
    if (c->op != ANCHOR_CUBE_POSE && c->op != ANCHOR_CUBE_THROW)
        return;
    if (c->op == ANCHOR_CUBE_THROW && s_victim.flight)
        return;
    x = (float)c->x100 / 100.0f;
    y = (float)c->y100 / 100.0f;
    z = (float)c->z100 / 100.0f;
    if (!valid_pose(x, y, z) ||
        !coordinate((float)c->vx100 / 100.0f) ||
        !coordinate((float)c->vy100 / 100.0f) ||
        !coordinate((float)c->vz100 / 100.0f))
        return;
    if (s_victim.pose_ready &&
        (x - s_victim.x) * (x - s_victim.x) +
        (y - s_victim.y) * (y - s_victim.y) +
        (z - s_victim.z) * (z - s_victim.z) > 500.0f * 500.0f)
        return;
    if (!s_victim.flight && c->op == ANCHOR_CUBE_POSE)
    {
        float px, py, pz;
        if (!anchor_player_models_get_sound_position(c->sender_cid,
                c->source_session, c->source_epoch, &px, &py, &pz) ||
            (x - px) * (x - px) + (y - py) * (y - py) +
            (z - pz) * (z - pz) > 350.0f * 350.0f)
            return;
    }
    s_victim.x = x;
    s_victim.y = y;
    s_victim.z = z;
    s_victim.pose_ready = 1;
    s_victim.lease = CUBE_LEASE;
    if (c->op == ANCHOR_CUBE_THROW)
        s_victim.flight = 1;
}

static void accept_carrier_cancel(const AnchorPlayerCubeControl *c)
{
    if (s_carrier.phase < 1 || s_carrier.phase > 3 ||
        c->target_cid != (int)anchor_get_client_id() ||
        c->target_epoch != s_carrier.local_epoch ||
        c->sender_cid != s_carrier.target_cid ||
        c->source_session != s_carrier.target_session ||
        c->source_epoch != s_carrier.target_epoch ||
        c->carry_id != s_carrier.carry_id)
        return;
    detach_carrier();
    clear_impact_attack();
    s_carrier.phase = 0;
    s_carrier.impact_attack_frames = 0;
    s_carrier.task = s_carrier.object = 0;
}

/* Native carry placement writes the held object's pose each frame. Apply the
 * lift only after that write, so repeated frames never accumulate height. */
static void *s_placement_task;
static float s_placement_x, s_placement_y, s_placement_z;
RECOMP_HOOK("func_801E54E0_5A13F0")
void anchor_player_cube_placement_begin(void *player, void *hand_marker,
                                        unsigned int place_object)
{
    s_placement_task = hand_marker && place_object &&
                       player == s_carrier.player && s_carrier.phase == 2 &&
                       s_carrier.object &&
                       held_task_is_ours() ? s_carrier.task : 0;
    if (s_placement_task)
    {
        s_placement_x = read_float(s_carrier.object, 8);
        s_placement_y = read_float(s_carrier.object, 0xc);
        s_placement_z = read_float(s_carrier.object, 0x10);
    }
}
RECOMP_HOOK_RETURN("func_801E54E0_5A13F0")
void anchor_player_cube_placement_end(void)
{
    if (s_placement_task && s_placement_task == s_carrier.task &&
        s_carrier.phase == 2 && s_carrier.object &&
        anchor_player_freeze_visual_owns_task(s_placement_task) &&
        (read_float(s_carrier.object, 8) != s_placement_x ||
         read_float(s_carrier.object, 0xc) != s_placement_y ||
         read_float(s_carrier.object, 0x10) != s_placement_z))
        *(float *)((unsigned char *)s_carrier.object + 0xc) +=
            CUBE_CARRY_LIFT * s_carrier.scale;
    s_placement_task = 0;
}

/* Native throw detaches the visual task. Sasuke's bomb computes launch velocity
 * from player task +0x68..+0x70, adds (0, 5, 3), then rotates it by the
 * player object's angles. Reuse that vector without running bomb task logic. */
static void *s_throw_task;
RECOMP_HOOK("func_801E55D4_5A14E4")
void anchor_player_cube_throw_begin(void *player)
{
    s_throw_task = player == s_carrier.player && s_carrier.phase == 2 &&
                   held_task_is_ours() ? s_carrier.task : 0;
}
RECOMP_HOOK_RETURN("func_801E55D4_5A14E4")
void anchor_player_cube_throw_end(void)
{
    if (!s_throw_task || s_throw_task != s_carrier.task ||
        !anchor_player_freeze_visual_owns_task(s_throw_task) ||
        !s_carrier.object)
    {
        s_throw_task = 0;
        return;
    }
    if (!s_carrier.player || !D_801FC60C_5B851C)
    {
        cancel_carrier();
        s_throw_task = 0;
        return;
    }
    s_carrier.vx = read_float(s_carrier.player, 0x68);
    s_carrier.vy = read_float(s_carrier.player, 0x6c) + 5.0f;
    s_carrier.vz = read_float(s_carrier.player, 0x70) + 3.0f;
    func_80033898_34498(
        *(unsigned short *)((unsigned char *)D_801FC60C_5B851C + 0x14),
        *(unsigned short *)((unsigned char *)D_801FC60C_5B851C + 0x16),
        *(unsigned short *)((unsigned char *)D_801FC60C_5B851C + 0x18),
        &s_carrier.vx, &s_carrier.vy, &s_carrier.vz);
    if (!coordinate(s_carrier.vx) || !coordinate(s_carrier.vy) ||
        !coordinate(s_carrier.vz) ||
        s_carrier.vx * s_carrier.vx + s_carrier.vy * s_carrier.vy +
            s_carrier.vz * s_carrier.vz >
                CUBE_FLIGHT_SPEED_LIMIT * CUBE_FLIGHT_SPEED_LIMIT)
    {
        cancel_carrier();
        s_throw_task = 0;
        return;
    }
    /* Throw setup writes a fresh hand-height pose after the final held frame.
     * Raise this launch origin by the same amount as the carried cube. */
    *(float *)((unsigned char *)s_carrier.object + 0xc) +=
        CUBE_CARRY_LIFT * s_carrier.scale;
    s_carrier.phase = 3;
    s_carrier.flight_frames = 0;
    s_carrier.pose_timer = 0;
    s_carrier.throw_sent = 0;
    s_carrier.impact_attack_frames = 0;
    s_carrier.wait = CUBE_GRANT_WAIT;
    s_throw_task = 0;
}

static int send_carrier_pose(int op)
{
    float x, y, z;
    unsigned char *object = s_carrier.object;
    if (!object || !anchor_player_freeze_visual_owns_task(s_carrier.task))
        return 0;
    x = read_float(object, 8);
    y = read_float(object, 0xc) - 100.0f * s_carrier.scale;
    z = read_float(object, 0x10);
    return send_control(op, s_carrier.target_cid, s_carrier.target_epoch,
                       s_carrier.carry_id, x, y, z,
                       s_carrier.vx, s_carrier.vy, s_carrier.vz,
                       0, 0, 0);
}

static int ray_hit(float x, float y, float z,
                   float dx, float dy, float dz, float distance)
{
    CubeQuery query;
    float length = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    float travelled = 0.0f;
    int n;
    if (!(length > 0.0001f) || length > CUBE_FLIGHT_SPEED_LIMIT)
        return 0;
    dx /= length; dy /= length; dz /= length;
    for (n = 0; n < CUBE_MAX_RAY_SAMPLES && travelled < distance; ++n)
    {
        float segment = distance - travelled;
        float along, approach;
        if (segment > CUBE_FLIGHT_SEGMENT)
            segment = CUBE_FLIGHT_SEGMENT;
        query.hit = 0;
        func_8002C9D4_2D5D4(&query, x + dx * travelled,
            y + dy * travelled, z + dz * travelled,
            dx, dy, dz, segment);
        if (query.hit == 0x7fff &&
            coordinate(query.delta[0]) && coordinate(query.delta[1]) &&
            coordinate(query.delta[2]) &&
            query.normal[0] >= -1.01f && query.normal[0] <= 1.01f &&
            query.normal[1] >= -1.01f && query.normal[1] <= 1.01f &&
            query.normal[2] >= -1.01f && query.normal[2] <= 1.01f)
        {
            along = query.delta[0] * dx + query.delta[1] * dy +
                    query.delta[2] * dz;
            approach = query.normal[0] * dx + query.normal[1] * dy +
                       query.normal[2] * dz;
            if (along >= -0.01f && along <= segment + 0.01f &&
                approach < -0.0001f)
                return 1;
        }
        travelled += segment;
    }
    return 0;
}

static int sweep_cube(float x, float y, float z, float dx, float dy, float dz,
                      float half)
{
    float ox = dx > 0 ? half : -half;
    float oy = dy > 0 ? half : -half;
    float oz = dz > 0 ? half : -half;
    float length = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    float ax = dx < 0 ? -dx : dx;
    float ay = dy < 0 ? -dy : dy;
    float az = dz < 0 ? -dz : dz;
    int i;
    if (length > CUBE_FLIGHT_SPEED_LIMIT || !coordinate(length))
        return 1;
    /* Cover the leading face with center, corners, and edge midpoints.
     * At the 128-unit speed cap each of these nine rays uses at most four
     * 32-unit scene-query segments. */
    for (i = 0; i < 9; ++i)
    {
        float sx = (float)(i / 3 - 1) * half;
        float sy = (float)(i % 3 - 1) * half;
        float cx, cy, cz;
        if (ax >= ay && ax >= az)
        {
            cx = x + ox;
            cy = y + sx;
            cz = z + sy;
        }
        else if (ay >= az)
        {
            cx = x + sx;
            cy = y + oy;
            cz = z + sy;
        }
        else
        {
            cx = x + sx;
            cy = y + sy;
            cz = z + oz;
        }
        if (ray_hit(cx, cy, cz, dx, dy, dz, length))
            return 1;
    }
    return 0;
}

static void carrier_frame(void)
{
    unsigned char *object = s_carrier.object;
    float x, y, z, vx, vy, vz, half;
    if (s_carrier.phase == 1)
    {
        if (--s_carrier.wait <= 0)
            cancel_carrier();
        return;
    }
    if (!anchor_player_freeze_visual_owns_task(s_carrier.task) || !object)
    {
        cancel_carrier();
        return;
    }
    if (s_carrier.phase == 2)
    {
        if (!held_task_is_ours())
        {
            cancel_carrier();
            return;
        }
        if (++s_carrier.pose_timer >= 3)
        {
            send_carrier_pose(ANCHOR_CUBE_POSE);
            s_carrier.pose_timer = 0;
        }
        return;
    }
    if (s_carrier.phase == 4)
    {
        /* This tick follows native intake. Re-arm for a bounded number of
         * further scans, clearing the native first-contact guard each time. */
        if (s_carrier.impact_attack_frames > 0)
        {
            --s_carrier.impact_attack_frames;
            if (s_carrier.impact_attack_frames)
                arm_impact_attack();
            else
                clear_impact_attack();
        }
        if (!s_carrier.impact_sent)
        {
            s_carrier.impact_sent = send_carrier_pose(ANCHOR_CUBE_IMPACT);
            if (s_carrier.impact_sent)
                s_carrier.wait = CUBE_GRANT_WAIT;
            else if (--s_carrier.wait > 0)
                return;
        }
        else if (--s_carrier.wait > 0)
            return;
        if (s_carrier.wait <= 0)
        {
            clear_impact_attack();
            s_carrier.phase = 0;
            s_carrier.task = s_carrier.object = 0;
        }
        return;
    }
    if (s_carrier.phase != 3)
        return;
    if (!s_carrier.throw_sent)
    {
        s_carrier.throw_sent = send_carrier_pose(ANCHOR_CUBE_THROW);
        if (!s_carrier.throw_sent && --s_carrier.wait <= 0)
            cancel_carrier();
        if (!s_carrier.throw_sent)
            return;
    }
    if (s_carrier.flight_frames++ > CUBE_FLIGHT_LIMIT)
    {
        cancel_carrier();
        return;
    }
    x = read_float(object, 8);
    y = read_float(object, 0xc);
    z = read_float(object, 0x10);
    vx = s_carrier.vx;
    vy = s_carrier.vy - CUBE_GRAVITY;
    vz = s_carrier.vz;
    half = s_carrier.scale * 100.0f;
    if (vx * vx + vy * vy + vz * vz >
        CUBE_FLIGHT_SPEED_LIMIT * CUBE_FLIGHT_SPEED_LIMIT)
    {
        cancel_carrier();
        return;
    }
    if (!valid_pose(x, y, z))
    {
        cancel_carrier();
        return;
    }
    if (sweep_cube(x, y, z, vx, vy, vz, half))
    {
        s_carrier.phase = 4;
        s_carrier.wait = CUBE_GRANT_WAIT;
        s_carrier.impact_sent = 0;
        s_carrier.impact_attack_frames = CUBE_IMPACT_ATTACK_FRAMES;
        (void)anchor_player_freeze_visual_shatter(s_carrier.target_cid,
            s_carrier.target_session, s_carrier.target_epoch);
        arm_impact_attack();
        return;
    }
    x += vx; y += vy; z += vz;
    if (!valid_pose(x, y, z))
    {
        cancel_carrier();
        return;
    }
    *(float *)(object + 8) = x;
    *(float *)(object + 0xc) = y;
    *(float *)(object + 0x10) = z;
    s_carrier.vy = vy;
    if (++s_carrier.pose_timer >= 3)
    {
        send_carrier_pose(ANCHOR_CUBE_POSE);
        s_carrier.pose_timer = 0;
    }
}

void anchor_player_cube_tick(void)
{
    AnchorPlayerCubeControl c;
    int i, epoch = anchor_player_models_get_epoch();
    if (!anchor_is_connected() || !item_sync_save_is_loaded())
    {
        anchor_player_cube_reset();
        return;
    }
    if (s_carrier.phase &&
        (s_carrier.player != D_801FC604_5B8514 ||
         s_carrier.room != D_800C7AB2 ||
         s_carrier.local_epoch != epoch ||
         !anchor_player_models_peer_is_current(s_carrier.target_cid,
                s_carrier.target_session, s_carrier.target_epoch) ||
         (s_carrier.phase <= 3 &&
          !anchor_player_freeze_visual_has_cube(s_carrier.target_cid,
                s_carrier.target_session, s_carrier.target_epoch))))
        cancel_carrier();
    if (s_victim.moving &&
        (s_victim.room != D_800C7AB2 ||
         s_victim.target_epoch != epoch ||
         !anchor_player_freeze_active() ||
         !anchor_player_models_peer_is_current(s_victim.carrier_cid,
                s_victim.carrier_session, s_victim.carrier_epoch) ||
         --s_victim.lease <= 0))
        cancel_victim();
    for (i = 0; i < 16 && anchor_poll_player_cube_control(&c); ++i)
    {
        if (c.room_id != (int)D_800C7AB2)
            continue;
        if (c.op == ANCHOR_CUBE_REQUEST)
            accept_request(&c);
        else if (c.op == ANCHOR_CUBE_GRANT)
            accept_grant(&c);
        else if (c.op == ANCHOR_CUBE_CANCEL &&
                 c.target_cid == (int)anchor_get_client_id() &&
                 c.sender_cid == s_carrier.target_cid)
            accept_carrier_cancel(&c);
        else
            accept_victim_control(&c);
    }
    if (s_carrier.phase)
        carrier_frame();
}
