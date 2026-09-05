#include "anchor_projectile_models.h"

/* Native projectile constructors bind these exact model commands. Resource
 * families 0..3 are the character broad files; family 4 is common file 0x80.
 * IDs are table indices, never a received model pointer or asset offset. */
typedef struct ProjectileRecipe
{
    unsigned int model;
    unsigned char family;
} ProjectileRecipe;

static const ProjectileRecipe s_recipes[] = {
    {0x49009498u, 0}, {0x1900065cu, 0}, /* coin and its impact */
    {0x49009020u, 0}, {0x49009238u, 0}, {0x49009348u, 0},
    {0x49007c40u, 1}, {0x190003e0u, 4}, /* Ebisumaru projectile / common effect */
    {0x49007f90u, 2}, {0x19000064u, 2}, {0x49008470u, 2},
    {0x49008220u, 2}, {0x190006c4u, 2}, {0x19000a24u, 2},
    {0x19000120u, 2}, {0x190001e0u, 2}, {0x190002a0u, 2},
    {0x19000360u, 2},
    {0x4900a350u, 3}, {0x4900a430u, 3}, /* Yae projectile and trail */
    {0x19000124u, 3}, {0x1900025cu, 3},
    {0x4900b5e8u, 0}, {0x4900b878u, 0},
    {0x190006b8u, 0}, {0x1900074cu, 0},
    {0x49002410u, 4}, {0x490025d0u, 4}, {0x490024f0u, 4},
    {0x490026b0u, 4}, {0x49002790u, 4},
    {0x1900012cu, 4}, {0x1900025cu, 4}, {0x1900037cu, 4},
    {0x19000544u, 4}, {0x190006a4u, 4},
    {0x190008a0u, 4}, {0x190008e4u, 4},
    {0x19000420u, 3}, /* Yae native rocket impact. */
};

/* FUN_801E8890 selects one of these resource-relative texture frames and
 * writes segment 11 (+0x50). The high byte selects the locally bound base,
 * not an address carried by the network. Native tables: 802047A0..80205068. */
static const unsigned int s_textures[] __attribute__((unused)) = {
    0x09002cf0u, 0x090034f0u, 0x090044f0u, 0x09006cf0u,
    0x0a000980u, 0x0a006980u, 0x0a007180u, 0x0a007980u,
    0x0a008180u, 0x0a008980u, 0x0a009980u, 0x0a00a980u,
    0x0a00b980u, 0x0a00c980u, 0x0a00d980u, 0x0a00e980u,
    0x0a00f980u, 0x0a010980u, 0x0a011980u, 0x0a013980u,
    0x0a014180u, 0x0a014980u, 0x0a015180u, 0x0a015980u,
    0x0a016180u, 0x0a016980u, 0x0a017180u, 0x0a017980u,
    0x0a018180u, 0x0a018980u,
};

/* FUN_801DC554 emits a template call, optional primitive RGB, environment
 * RGBA, and end. Only these verified native templates can be reconstructed. */
static const unsigned int s_material_templates[] = {
    0x802049c0u, 0xe0204c28u, 0xa0204c78u, 0xa0204c90u,
    0x80204ca8u, 0x8006dde8u,
};

#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define MATERIAL_DIRECT_LAST 4
#define MATERIAL_DYNAMIC_FIRST 5
#define MATERIAL_DYNAMIC_END (MATERIAL_DYNAMIC_FIRST + ARRAY_COUNT(s_material_templates) * 8)
#define MATERIAL_BYTES 32u
#define MATERIAL_ARENA_BYTES (ANCHOR_PROJECTILE_REMOTE_MAX * 2u * MATERIAL_BYTES)

static unsigned int physical(unsigned int value)
{
    return value & 0x1fffffffu;
}

/* Kept independent of native memory so the whitelist and material wire
 * contract can be tested on the host. */
int anchor_projectile_recipe_id(unsigned int model, int family)
{
    int i;
    for (i = 0; i < ARRAY_COUNT(s_recipes); ++i)
        if (s_recipes[i].model == model && s_recipes[i].family == family)
            return i + 1;
    return 0;
}

int anchor_projectile_material_decode(unsigned int context,
                                      const unsigned int *commands,
                                      int *material, unsigned int *prim_rgb,
                                      unsigned int *env_rgba)
{
    unsigned int style = (context >> 29) & 3u;
    int i;
    int primitive;
    int offset;
    if (!material || !prim_rgb || !env_rgba)
        return 0;
    *prim_rgb = 0;
    *env_rgba = 0xffffffffu;
    if (!context)
    {
        *material = 0;
        return 1;
    }
    if (physical(context) == 0x0006d920u)
    {
        *material = 1 + (int)style;
        return 1;
    }
    if (!commands || commands[0] != 0x06000000u)
        return 0;
    for (i = 0; i < ARRAY_COUNT(s_material_templates); ++i)
        if (physical(commands[1]) == physical(s_material_templates[i]))
            break;
    if (i == ARRAY_COUNT(s_material_templates))
        return 0;
    primitive = commands[2] == 0xfa000000u;
    offset = primitive ? 4 : 2;
    if ((primitive && (commands[3] & 0xffu)) ||
        commands[offset] != 0xfb000000u ||
        commands[offset + 2] != 0xb8000000u || commands[offset + 3] != 0)
        return 0;
    if (primitive)
        *prim_rgb = commands[3] >> 8;
    *env_rgba = commands[offset + 1];
    *material = MATERIAL_DYNAMIC_FIRST + i * 8 + primitive * 4 + (int)style;
    return 1;
}

int anchor_projectile_material_build(int material, unsigned int prim_rgb,
                                     unsigned int env_rgba,
                                     unsigned int commands[8])
{
    int code;
    int primitive;
    int offset;
    int i;
    if (!commands || material < MATERIAL_DYNAMIC_FIRST || material >= MATERIAL_DYNAMIC_END ||
        prim_rgb > 0xffffffu)
        return 0;
    code = material - MATERIAL_DYNAMIC_FIRST;
    primitive = (code & 4) != 0;
    for (i = 0; i < 8; ++i)
        commands[i] = 0;
    commands[0] = 0x06000000u;
    commands[1] = s_material_templates[code / 8];
    if (primitive)
    {
        commands[2] = 0xfa000000u;
        commands[3] = prim_rgb << 8;
    }
    offset = primitive ? 4 : 2;
    commands[offset] = 0xfb000000u;
    commands[offset + 1] = env_rgba;
    commands[offset + 2] = 0xb8000000u;
    return 1;
}

#ifndef ANCHOR_PROJECTILE_MODELS_HOST_TEST
#include "anchor.h"
#include "anchor_player_models.h"
#include "anchor_projectile_motion.h"

extern void *D_801FC604_5B8514;
extern unsigned short D_800C7AB2;
extern unsigned short D_80204020_5BFF30[];
extern void *func_800141C4_14DC4(unsigned int file_id);
extern void *func_80013B14_14714(unsigned int file_id);
extern void *func_80034E08_35A08(void *parent, void (*update)(void *, void *),
                                unsigned short flags);
extern void *func_8000DBF0_E7F0(void *task, unsigned int model, unsigned int material,
                              float x, float y, float z,
                              short rx, short ry, short rz,
                              float sx, float sy, float sz,
                              short file8, short file9);
extern float func_8001B5AC_1C1AC(void *object);
extern void func_8001DB04_1E704(unsigned short angles[3], const float direction[3],
                               unsigned short rx, unsigned short ry, unsigned short rz);
extern void *func_8002C9D4_2D5D4(void *out, float x, float y, float z,
                               float dx, float dy, float dz, float range);

typedef struct ProjectileSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} ProjectileSceneResource;
extern ProjectileSceneResource D_80167FC0_168BC0[48];

/* Two display records per shot cap this entire renderer at 64 kind-2 objects,
 * including the second kunai/bomb impact layer. No attack actors are created. */
#define PROJECTILE_SLOTS (ANCHOR_PROJECTILE_REMOTE_MAX / 2)
typedef struct ProjectileSlot
{
    void *task;
    void *object[2];
    AnchorProjectileRemote remote;
    AnchorProjectileMotion motion;
    AnchorCollisionVec3 trail_position;
    unsigned short angles[3];
    unsigned short room;
    unsigned char active, flip;
    int trail_age, trail_alpha;
} ProjectileSlot;

typedef struct ProjectileDraw
{
    unsigned int model, prim_rgb, env_rgba, texture;
    int family, mode, material;
    float scale, frame;
    AnchorCollisionVec3 position;
    unsigned short angles[3];
} ProjectileDraw;

typedef struct ProjectileRay
{
    float origin[3], direction[3], delta[3], normal[3];
    unsigned int surface_data;
    unsigned char surface_type, padding_35;
    unsigned short surface_flags, hit, padding_3a;
    float distance_squared;
    unsigned int object, task;
} ProjectileRay;
_Static_assert(sizeof(ProjectileRay) == 0x48, "Native ray ABI");

static ProjectileSlot s_slots[PROJECTILE_SLOTS];
static unsigned char *s_resources[6];
static unsigned int *s_material_arena;
static void *s_owner;
static void projectile_task_update(void *task, void *object);

static int rdram(const void *pointer)
{
    unsigned int address = physical((unsigned int)(unsigned long)pointer);
    return address >= 0x1000u && address < 0x800000u;
}

static int linked(const void *task)
{
    void *backlink;
    if (!rdram(task))
        return 0;
    backlink = *(void *const *)((const unsigned char *)task + 4);
    return rdram(backlink) && *(void *const *)backlink == task;
}

static int owned_task(const ProjectileSlot *slot)
{
    return linked(slot->task) &&
           *(void **)((unsigned char *)slot->task + 0xc) == (void *)projectile_task_update;
}

static int owned_object(const ProjectileSlot *slot, int index)
{
    return owned_task(slot) && rdram(slot->object[index]) &&
           *(void **)((unsigned char *)slot->task + 0x18) == slot->object[0] &&
           (!index || *(void **)slot->object[0] == slot->object[1]);
}

static unsigned char *resident(unsigned int file)
{
    void *pointer = func_800141C4_14DC4(file);
    if (!rdram(pointer))
        return 0;
    return (unsigned char *)(unsigned long)(physical((unsigned int)(unsigned long)pointer) | 0x80000000u);
}

static void hide(ProjectileSlot *slot)
{
    int i;
    for (i = 0; i < 2; ++i)
        if (owned_object(slot, i))
        {
            unsigned char *object = slot->object[i];
            *(unsigned int *)(object + 0x2c) = 0;
            object[0x64] |= 1u;
        }
}

static void clear_slot(ProjectileSlot *slot, int retain)
{
    hide(slot);
    if (!retain || !owned_task(slot))
    {
        slot->task = 0;
        slot->object[0] = slot->object[1] = 0;
    }
    slot->active = 0;
}

void anchor_projectile_models_reset(void)
{
    int i;
    int retain = s_owner == D_801FC604_5B8514 && linked(s_owner);
    for (i = 0; i < PROJECTILE_SLOTS; ++i)
        clear_slot(&s_slots[i], retain);
    if (!retain)
        s_owner = 0;
}

void anchor_projectile_models_load_resources(void)
{
    int i;
    unsigned int start, end;
    anchor_projectile_models_reset();
    s_material_arena = 0;
    for (i = 0; i < 4; ++i)
        s_resources[i] = resident(D_80204020_5BFF30[i]);
    /* This hook runs after broad character staging, never during drawing.
     * An unavailable unrelated character file must not disable every shot. */
    func_80013B14_14714(0x80);
    func_80013B14_14714(0x152);
    s_resources[4] = resident(0x80);
    s_resources[5] = resident(0x152);
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !rdram(D_80167FC0_168BC0[i].data))
    {
        recomp_printf("[projectiles] material arena has no scene storage\n");
        return;
    }
    start = physical((unsigned int)(unsigned long)D_80167FC0_168BC0[i].data);
    start = ((start + 15u) & ~15u) | 0x80000000u;
    end = start + MATERIAL_ARENA_BYTES;
    if (end < start || end > 0x80800000u)
    {
        recomp_printf("[projectiles] material arena exceeds render RDRAM\n");
        return;
    }
    D_80167FC0_168BC0[i].data = (unsigned char *)(unsigned long)end;
    s_material_arena = (unsigned int *)(unsigned long)start;
    recomp_printf("[projectiles] resources G%d E%d S%d Y%d common%d texture%d arena=%x\n",
                   s_resources[0] != 0, s_resources[1] != 0, s_resources[2] != 0,
                   s_resources[3] != 0, s_resources[4] != 0, s_resources[5] != 0, start);
}

static int family_for_kind(int kind)
{
    if (kind <= 2) return 0;
    if (kind == 0x0c || kind == 0x0d) return 1;
    if (kind >= 0x17 && kind <= 0x19) return 3;
    return 2;
}

static int ensure_task(ProjectileSlot *slot)
{
    if (!owned_task(slot))
    {
        slot->task = 0;
        slot->object[0] = slot->object[1] = 0;
    }
    if (!slot->task)
        slot->task = func_80034E08_35A08(s_owner, projectile_task_update, 0);
    if (!slot->task)
        return 0;
    if (!slot->object[0])
        slot->object[0] = func_8000DBF0_E7F0(slot->task, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    /* Keep a valid task even when the display pool is temporarily exhausted. */
    if (!slot->object[0])
        return 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x30) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x34) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x38) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x3c) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x48) = 0;
    *(unsigned int *)((unsigned char *)slot->task + 0x5c) = 0;
    return owned_object(slot, 0);
}

static void initialize_draw(const ProjectileSlot *slot, ProjectileDraw *draw)
{
    int i;
    const AnchorProjectileMotion *m = &slot->motion;
    draw->model = 0;
    draw->family = family_for_kind(m->kind);
    draw->mode = 10;
    draw->material = 3; /* Native 0xc006d920. */
    draw->prim_rgb = 0;
    draw->env_rgba = 0xffffffffu;
    draw->texture = 0;
    draw->scale = m->scale;
    draw->frame = m->frame;
    draw->position = m->position;
    for (i = 0; i < 3; ++i)
        draw->angles[i] = slot->angles[i];
}

static void select_draw(const ProjectileSlot *slot, int layer, ProjectileDraw *draw)
{
    const AnchorProjectileMotion *m = &slot->motion;
    int flight = m->phase == ANCHOR_SHOT_FLIGHT || m->phase == ANCHOR_SHOT_RETURN;
    initialize_draw(slot, draw);
    if (m->kind == 1)
    {
        if (!layer)
        {
            draw->model = flight ? 0x49009498u : 0x1900065cu;
            draw->mode = 2;
            if (flight) draw->texture = 0x0a000980u;
        }
    }
    else if (m->kind == 2)
    {
        draw->model = layer ? 0x49009238u : 0x49009020u;
        if (layer)
            draw->angles[0] = draw->angles[1] = draw->angles[2] = 0x8000;
    }
    else if (m->kind == 0x0c || m->kind == 0x0d)
    {
        if (!layer)
        {
            draw->model = 0x49007c40u;
            draw->mode = 7;
            draw->material = 18; /* A-tagged context, native A0204C28 + primitive RGB. */
            draw->prim_rgb = 0xfff5deu;
            draw->env_rgba = 0xffe88200u | (unsigned int)m->alpha;
        }
    }
    else if (m->kind >= 0x0e && m->kind <= 0x10)
    {
        if (flight)
        {
            if (!layer)
            {
                draw->model = 0x19000064u;
                draw->mode = 7;
            }
        }
        else
        {
            unsigned int gray = (unsigned int)m->gray;
            draw->model = 0x49008470u;
            draw->material = 6; /* A-tagged native 802049C0 environment material. */
            draw->mode = layer ? 5 : 7;
            draw->env_rgba = (gray << 24) | (gray << 16) | (gray << 8) | (unsigned int)m->alpha;
            draw->angles[0] = layer ? 0x200 : 0;
            draw->angles[1] = draw->angles[2] = 0;
        }
    }
    else if (m->kind >= 0x17 && m->kind <= 0x19)
    {
        if (layer)
        {
            static const unsigned int textures[] = {
                0x0a007980u, 0x0a008180u, 0x0a013980u, 0x0a014180u,
                0x0a014980u, 0x0a015180u, 0x0a015980u, 0x0a016180u
            };
            if (slot->trail_alpha > 0 && s_resources[4])
            {
                int texture = slot->trail_age / 3;
                draw->model = 0x490026b0u;
                draw->family = 4;
                draw->mode = 10;
                draw->material = 6;
                draw->env_rgba = 0xe0e0e000u | (unsigned int)slot->trail_alpha;
                draw->position = slot->trail_position;
                draw->scale = 0.03f + 0.006f * (float)slot->trail_age;
                draw->texture = textures[texture < 8 ? texture : 7];
                draw->angles[0] = draw->angles[1] = draw->angles[2] = 0x8000;
            }
        }
        else if (flight)
        {
            draw->model = 0x4900a350u;
            draw->mode = 5;
            draw->material = 1;
        }
        else
        {
            static const unsigned int textures[] = {
                0x0a007980u, 0x0a008180u, 0x0a013980u, 0x0a014180u,
                0x0a014980u, 0x0a015180u, 0x0a015980u, 0x0a016180u
            };
            int texture = m->phase_age / 3;
            draw->model = 0x19000420u;
            draw->material = 8;
            draw->env_rgba = 0xffffff00u | (unsigned int)m->alpha;
            draw->texture = textures[texture < 8 ? texture : 7];
        }
    }
    else if (flight)
    {
        if (!layer)
        {
            draw->model = 0x49008220u;
            draw->mode = 5;
            draw->material = 1;
        }
    }
    else
    {
        draw->model = layer ? 0x19000a24u : 0x190006c4u;
        draw->material = 20;
        draw->prim_rgb = layer ? 0x28d4ffu : 0x0000aau;
        draw->env_rgba = (layer ? 0xffffff00u : 0x0056ff00u) | (unsigned int)m->alpha;
    }
}

static void draw_object(ProjectileSlot *slot, int index, int layer, const ProjectileDraw *draw)
{
    unsigned char *object = slot->object[layer];
    unsigned int material = 0;
    unsigned int *commands;
    float frame = draw->frame;
    if (!owned_object(slot, layer))
        return;
    if (!draw->model || !s_resources[draw->family] ||
        !anchor_projectile_recipe_id(draw->model, draw->family))
    {
        object[0x64] |= 1u;
        *(unsigned int *)(object + 0x2c) = 0;
        return;
    }
    if (draw->material > 0 && draw->material <= MATERIAL_DIRECT_LAST)
        material = 0x8006d920u | ((unsigned int)(draw->material - 1) << 29);
    else if (draw->material >= MATERIAL_DYNAMIC_FIRST)
    {
        commands = s_material_arena + ((index * 2 + layer) * 2 + slot->flip) * 8;
        if (!anchor_projectile_material_build(draw->material, draw->prim_rgb, draw->env_rgba, commands))
            return;
        material = (unsigned int)(unsigned long)commands |
                   ((unsigned int)(draw->material - MATERIAL_DYNAMIC_FIRST) & 3u) << 29;
    }
    *(unsigned int *)(object + 0x2c) = draw->model;
    *(unsigned int *)(object + 0x30) = material;
    *(unsigned short *)(object + 0x34) = 0;
    *(unsigned int *)(object + 0x38) = 0;
    *(unsigned short *)(object + 0x3c) = draw->family == 4 ? 0x80 : D_80204020_5BFF30[draw->family];
    *(void **)(object + 0x40) = s_resources[draw->family];
    *(unsigned short *)(object + 0x44) = 0x152;
    *(void **)(object + 0x48) = s_resources[5];
    *(unsigned short *)(object + 0x4c) = 0;
    *(unsigned int *)(object + 0x50) = draw->texture ?
        (unsigned int)(unsigned long)s_resources[draw->texture >> 24 == 9 ? draw->family : 5] +
            (draw->texture & 0xffffffu) : 0;
    *(float *)(object + 8) = draw->position.x;
    *(float *)(object + 0xc) = draw->position.y;
    *(float *)(object + 0x10) = draw->position.z;
    *(unsigned short *)(object + 0x14) = draw->angles[0];
    *(unsigned short *)(object + 0x16) = draw->angles[1];
    *(unsigned short *)(object + 0x18) = draw->angles[2];
    *(float *)(object + 0x1c) = draw->scale;
    *(float *)(object + 0x20) = draw->scale;
    *(float *)(object + 0x24) = draw->scale;
    object[5] = (unsigned char)draw->mode;
    if ((draw->model >> 24) == 0x19)
    {
        float count = func_8001B5AC_1C1AC(object);
        if (!layer && slot->motion.phase == ANCHOR_SHOT_IMPACT)
            slot->motion.clip_frames = count;
        if (count > 1.0f)
            while (frame >= count - 1.0f)
                frame -= count - 1.0f;
    }
    *(float *)(object + 0x28) = frame;
    object[0x64] &= ~1u;
    object[0x65] = 0;
}

static void render_slot(ProjectileSlot *slot, int index)
{
    int layer;
    ProjectileDraw draw;
    slot->flip ^= 1u;
    for (layer = 0; layer < 2; ++layer)
    {
        select_draw(slot, layer, &draw);
        if (draw.model && !slot->object[layer])
            slot->object[layer] = func_8000DBF0_E7F0(slot->task, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (slot->object[layer])
            draw_object(slot, index, layer, &draw);
    }
}

static void projectile_task_update(void *task, void *object)
{
    int i;
    for (i = 0; i < PROJECTILE_SLOTS; ++i)
    {
        ProjectileSlot *slot = &s_slots[i];
        if (slot->task != task || slot->object[0] != object || !owned_object(slot, 0))
            continue;
        if (!slot->active || !s_material_arena || s_owner != D_801FC604_5B8514 ||
            !linked(s_owner) || slot->room != D_800C7AB2)
            hide(slot);
        else
            render_slot(slot, i);
        return;
    }
}

static int world_impact(const AnchorCollisionVec3 *from, const AnchorCollisionVec3 *target,
                         AnchorCollisionVec3 *impact)
{
    AnchorCollisionVec3 direction;
    float length, travelled = 0;
    ProjectileRay ray;
    direction.x = target->x - from->x;
    direction.y = target->y - from->y;
    direction.z = target->z - from->z;
    length = __builtin_sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (!(length > 0.0001f && length <= 1024.0f))
        return 0;
    direction.x /= length; direction.y /= length; direction.z /= length;
    /* Native dynamic meshes have a sqrt(2000) nearest-hit limit. */
    while (travelled < length)
    {
        float segment = length - travelled;
        if (segment > 32.0f) segment = 32.0f;
        ray.hit = 0;
        func_8002C9D4_2D5D4(&ray, from->x + direction.x * travelled,
                            from->y + direction.y * travelled, from->z + direction.z * travelled,
                            direction.x, direction.y, direction.z, segment);
        if (ray.hit == 0x7fff &&
            ray.delta[0] >= -33.0f && ray.delta[0] <= 33.0f &&
            ray.delta[1] >= -33.0f && ray.delta[1] <= 33.0f &&
            ray.delta[2] >= -33.0f && ray.delta[2] <= 33.0f)
        {
            impact->x = from->x + direction.x * travelled + ray.delta[0];
            impact->y = from->y + direction.y * travelled + ray.delta[1];
            impact->z = from->z + direction.z * travelled + ray.delta[2];
            return 1;
        }
        travelled += segment;
    }
    return 0;
}

static void simulate_slot(ProjectileSlot *slot)
{
    AnchorProjectileMotion *m = &slot->motion;
    AnchorCollisionVec3 target, impact;
    int old_phase = m->phase;
    int hit = 0;
    anchor_projectile_motion_target(m, &target);
    if (m->phase == ANCHOR_SHOT_FLIGHT)
        hit = world_impact(&m->position, &target, &impact);
    anchor_projectile_motion_step(m, hit ? &impact : 0);
    if (m->kind == 1 && m->phase == ANCHOR_SHOT_FLIGHT)
    {
        float direction[3] = {m->velocity.x, m->velocity.y, m->velocity.z};
        if (direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2] > 0.0001f)
            func_8001DB04_1E704(slot->angles, direction, 0, (unsigned short)((m->age + 1) * 0x80 & 0x3ff), 0);
    }
    if (old_phase == ANCHOR_SHOT_FLIGHT && m->phase == ANCHOR_SHOT_IMPACT)
    {
        float reverse[3] = {-m->velocity.x, -m->velocity.y, -m->velocity.z};
        if (reverse[0] * reverse[0] + reverse[1] * reverse[1] + reverse[2] * reverse[2] > 0.0001f)
            func_8001DB04_1E704(slot->angles, reverse, m->kind == 1 ? 0 : 0x300,
                                m->kind == 1 ? (unsigned short)((m->age + 1) * 0x80 & 0x3ff) : 0, 0);
    }
    if (slot->trail_alpha > 0)
    {
        ++slot->trail_age;
        slot->trail_alpha -= 30;
    }
    if (m->kind >= 0x17 && m->kind <= 0x19 && m->phase == ANCHOR_SHOT_FLIGHT && !(m->age & 1))
    {
        slot->trail_position = m->position;
        slot->trail_age = 0;
        slot->trail_alpha = 240;
    }
}

void anchor_projectile_models_tick(void *owner)
{
    int i;
    if (!linked(owner) || owner != D_801FC604_5B8514 || !s_material_arena)
    {
        anchor_projectile_models_reset();
        return;
    }
    if (s_owner != owner)
    {
        for (i = 0; i < PROJECTILE_SLOTS; ++i)
            clear_slot(&s_slots[i], 0);
        s_owner = owner;
    }
    for (i = 0; i < PROJECTILE_SLOTS; ++i)
    {
        ProjectileSlot *slot = &s_slots[i];
        if (!slot->active) continue;
        if (!owned_object(slot, 0) || slot->room != D_800C7AB2 ||
            !anchor_player_models_peer_is_current(slot->remote.cid, slot->remote.session, slot->remote.epoch))
        {
            clear_slot(slot, 1);
            continue;
        }
        simulate_slot(slot);
        if (!slot->motion.alive)
            clear_slot(slot, 1);
    }
}

int anchor_projectile_models_spawn(const AnchorProjectileRemote *remote, void *owner)
{
    ProjectileSlot *slot = 0;
    int i, index = 0;
    if (!remote || !anchor_projectile_spawn_valid(&remote->spawn) ||
        !anchor_projectile_kind_supported(remote->spawn.kind) || remote->cid <= 0 ||
        remote->session <= 0 || remote->epoch <= 0 || remote->age_ms < 0 || remote->age_ms > 750)
        return 1;
    if (!linked(owner) || owner != D_801FC604_5B8514 || s_owner != owner || !s_material_arena ||
        !s_resources[family_for_kind(remote->spawn.kind)] || !s_resources[5] ||
        !anchor_player_models_peer_is_current(remote->cid, remote->session, remote->epoch))
        return 0;
    for (i = 0; i < PROJECTILE_SLOTS; ++i)
    {
        ProjectileSlot *candidate = &s_slots[i];
        if (candidate->active && candidate->remote.cid == remote->cid &&
            candidate->remote.session == remote->session && candidate->remote.epoch == remote->epoch &&
            candidate->remote.spawn.id == remote->spawn.id)
            return 1;
        if (!candidate->active && !slot)
        {
            slot = candidate;
            index = i;
        }
    }
    if (!slot || !ensure_task(slot))
        return 0;
    slot->remote = *remote;
    if (!anchor_projectile_motion_init(&slot->motion, &remote->spawn))
        return 1;
    slot->room = D_800C7AB2;
    slot->angles[0] = (unsigned short)remote->spawn.rx;
    slot->angles[1] = (unsigned short)remote->spawn.ry;
    slot->angles[2] = (unsigned short)remote->spawn.rz;
    slot->trail_age = slot->trail_alpha = 0;
    slot->active = 1;
    /* Receiver-local queue age compensates time spent waiting for peer bind.
     * Network transit time is unknown and is not fabricated from sender clocks. */
    for (i = 0; i < remote->age_ms * 30 / 1000 && slot->motion.alive; ++i)
        simulate_slot(slot);
    if (!slot->motion.alive)
        clear_slot(slot, 1);
    else
        render_slot(slot, index);
#if DEBUG_BUTTON_ENABLED
    recomp_printf("[projectiles] spawned cid=%d id=%d kind=%d\n", remote->cid, remote->spawn.id, remote->spawn.kind);
#endif
    return 1;
}
#endif
