/**
 * @file anchor_nameplates.c
 * @brief Native world-space names above Anchor remote render objects.
 *
 * Each plate is a camera-facing kind-2 display record on a child of its remote
 * task. The game font is rasterized into a small IA8 texture
 * in original RDRAM. Every plate owns one texture/model/material set per
 * graphics bank, so changing a name never rewrites data still in flight.
 */

#include "ui/anchor_nameplates.h"
#include "anchor_nameplate_bitmap.h"
#include "anchor_nameplate_budget.h"
#include "player/anchor_player_models.h"
#include "core/anchor_dialog.h"
#include "ui/anchor_freeze_prompt.h"
#include "platform/modding.h"
#include "platform/recompconfig.h"
#include "platform/recomputils.h"

#define NAMEPLATE_BANKS 2u
#define NAMEPLATE_MODEL 0x49001040u
#define NAMEPLATE_SCALE 0.33f
#define NAMEPLATE_HALF_HEIGHT 8
#define NAMEPLATE_CLEARANCE 2.0f
#define NAMEPLATE_COMMANDS 20u

#define NATIVE_BANK_BYTES 0x1d6d8u
#define NATIVE_COMMAND_BYTES 0x14c80u
#define NATIVE_MATRIX_END 0x1d380u
#define NATIVE_COMMAND_TAIL (512u * 8u)
#define NATIVE_DIALOG_TAIL (2048u * 8u)
/* The stock case-4 quad is short. This margin includes its object transform,
 * material wrapper, render setup, and a conservative scene-finalization tail. */
#define PLATE_DRAW_COMMAND_ALLOWANCE (256u * 8u)
#define PLATE_MATRIX_RESERVE (16u * 64u)

typedef struct NameplateSceneResource
{
    unsigned short file_id, padding;
    unsigned char *data;
} NameplateSceneResource;

typedef struct NameplateVertex
{
    short x, y, z;
    unsigned short flag;
    short s, t;
    unsigned char r, g, b, a;
} NameplateVertex;

typedef struct NameplateBankData
{
    unsigned char texture[ANCHOR_NAMEPLATE_TEXTURE_BYTES];
    NameplateVertex vertices[4];
    unsigned int display[NAMEPLATE_COMMANDS * 2u];
    unsigned int material_template[20];
    unsigned int material[8];
} NameplateBankData;

typedef struct NameplateSlot
{
    int cid;
    int active;
    int same_team;
    unsigned short room;
    void *owner_task;
    void *plate_task;
    const void *body;
    unsigned char *object;
    float height;
    char name[32];
    unsigned int revision;
    unsigned int uploaded[NAMEPLATE_BANKS];
    unsigned char initialized[NAMEPLATE_BANKS];
} NameplateSlot;

_Static_assert(sizeof(NameplateVertex) == 16, "Native Vtx ABI");
_Static_assert(sizeof(NameplateBankData) == 0x1150,
               "Segment-9 nameplate offsets");

extern NameplateSceneResource D_80167FC0_168BC0[48];
extern const unsigned char D_800629A0_635A0[];
extern const unsigned char D_8005BB10_5C710[];
extern unsigned short D_800C7AB2;
extern short D_800C7A72_C8672;
extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned int *D_8015C5CC_15D1CC;
extern unsigned char *D_80168504_169104;
extern void *func_80034E08_35A08(void *owner,
                                 void (*update)(void *, void *),
                                 unsigned short flags);
extern void func_80034EF8_35AF8(void *task);
extern void *func_8000DBF0_E7F0(void *task, unsigned int model,
                                unsigned int material, float x, float y,
                                float z, short rx, short ry, short rz,
                                float sx, float sy, float sz,
                                short file8, short file9);
/* libultra declaration in mnsg/libultra/include/PR/os.h; exported by the
 * native executable. The original translucent material builder flushes its
 * Gfx commands before they are consumed by the renderer. */
extern void osWritebackDCache(void *address, int bytes);

static NameplateSlot *s_slots;
static int s_capacity;
static NameplateBankData *s_banks;
static unsigned char *s_guard_object;
static unsigned char s_guard_hidden;
#if DEBUG_BUTTON_ENABLED
enum
{
    NAMEPLATE_LOG_SCENE = 1u << 0,
    NAMEPLATE_LOG_ARENA = 1u << 1,
    NAMEPLATE_LOG_ALLOC = 1u << 2,
    NAMEPLATE_LOG_READY = 1u << 3,
    NAMEPLATE_LOG_SYNC_GATE = 1u << 4,
    NAMEPLATE_LOG_SYNC_FULL = 1u << 5,
    NAMEPLATE_LOG_CHILD = 1u << 6,
    NAMEPLATE_LOG_OBJECT = 1u << 7,
    NAMEPLATE_LOG_SYNC_READY = 1u << 8,
    NAMEPLATE_LOG_REFRESH = 1u << 9,
    NAMEPLATE_LOG_REFRESH_BODY = 1u << 10,
    NAMEPLATE_LOG_DRAW_ATTEMPT = 1u << 11,
    NAMEPLATE_LOG_DRAW_GATE = 1u << 12,
    NAMEPLATE_LOG_DRAW_HEADROOM = 1u << 13,
    NAMEPLATE_LOG_DRAW_READY = 1u << 14,
    NAMEPLATE_LOG_REFRESH_READY = 1u << 15,
    NAMEPLATE_LOG_CONSTRUCTOR_GROUP = 1u << 16,
};
/* Process-lifetime flags: a stage transition must not turn these into
 * per-room or per-frame logging. */
static unsigned int s_debug_reported;
#endif

static unsigned int physical(unsigned int address)
{
    return address & 0x1fffffffu;
}

static int low_rdram(const void *pointer)
{
    unsigned int address = physical((unsigned int)(unsigned long)pointer);
    return address >= 0x1000u && address < 0x800000u;
}

static int nameplates_disabled(void)
{
    return recomp_get_config_u32("anchor_show_nameplates") == 1u;
}

static void nameplate_task_update(void *task, void *object)
{
    (void)task;
    (void)object;
}

static int linked_plate_task(const void *pointer)
{
    const unsigned char *task = pointer;
    void *backlink;
    if (!low_rdram(task))
        return 0;
    backlink = *(void *const *)(task + 4);
    return low_rdram(backlink) && *(void *const *)backlink == task &&
           *(void *const *)(task + 0xc) == (void *)nameplate_task_update;
}

static int plate_object_live(const NameplateSlot *slot)
{
    return slot->object && linked_plate_task(slot->plate_task) &&
           *(void *const *)((const unsigned char *)slot->plate_task + 0x18) ==
               slot->object;
}

static void clear_bytes(void *pointer, unsigned int size)
{
    unsigned char *p = pointer;
    while (size--)
        *p++ = 0;
}

static int current_body(const NameplateSlot *slot)
{
    const unsigned char *body = slot->body;
    return slot->active && slot->cid > 0 &&
           slot->room == D_800C7AB2 &&
           anchor_player_models_is_remote_pair(slot->owner_task, body) &&
           plate_object_live(slot) &&
           !(body[0x64] & 1u) && *(const unsigned int *)(body + 0x2c);
}

#if DEBUG_BUTTON_ENABLED
static const char *body_failure_reason(const NameplateSlot *slot)
{
    const unsigned char *body = slot->body;
    if (!slot->active || slot->cid <= 0)
        return "inactive";
    if (slot->room != D_800C7AB2)
        return "room";
    if (!anchor_player_models_is_remote_pair(slot->owner_task, body))
        return "owner pair";
    if (!plate_object_live(slot))
        return "plate link";
    if (body[0x64] & 1u)
        return "body hidden";
    if (!*(const unsigned int *)(body + 0x2c))
        return "body model zero";
    return "unknown";
}
#endif

static void hide_plate(NameplateSlot *slot)
{
    if (plate_object_live(slot) &&
        anchor_player_models_is_remote_pair(slot->owner_task, slot->body))
    {
        slot->object[0x64] |= 1u;
        *(unsigned int *)(slot->object + 0x2c) = 0;
    }
}

static void release_plate(NameplateSlot *slot)
{
    hide_plate(slot);
    if (linked_plate_task(slot->plate_task) &&
        anchor_player_models_is_remote_pair(slot->owner_task, slot->body))
        func_80034EF8_35AF8(slot->plate_task);
    slot->plate_task = 0;
    slot->object = 0;
}

void anchor_nameplates_reset(void)
{
    int i;
    for (i = 0; i < s_capacity; ++i)
    {
        release_plate(&s_slots[i]);
        clear_bytes(&s_slots[i], sizeof(s_slots[i]));
    }
    s_guard_object = 0;
}

void anchor_nameplates_load_resources(void)
{
    unsigned int start, end, capacity;
    int planned;
    int i;
    anchor_nameplates_reset();
    s_banks = 0;
    if (s_slots)
        recomp_free(s_slots);
    s_slots = 0;
    s_capacity = 0;
    for (i = 0; i < 48 && D_80167FC0_168BC0[i].file_id; ++i)
        ;
    if (i == 48 || !low_rdram(D_80167FC0_168BC0[i].data))
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_SCENE))
        {
            s_debug_reported |= NAMEPLATE_LOG_SCENE;
            recomp_printf("[nameplates] load: scene cursor invalid (sentinel=%d cursor=%p)\n",
                          i, i == 48 ? 0 : D_80167FC0_168BC0[i].data);
        }
#endif
        return;
    }
    planned = anchor_nameplate_arena_plan(
            physical((unsigned int)(unsigned long)D_80167FC0_168BC0[i].data),
            sizeof(NameplateBankData) * NAMEPLATE_BANKS,
            &start, &capacity, &end);
    if (!planned || capacity > 0x7fffffffu / sizeof(NameplateSlot))
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_ARENA))
        {
            s_debug_reported |= NAMEPLATE_LOG_ARENA;
            recomp_printf("[nameplates] load: arena plan failed (cursor=%p planned=%d capacity=%u stride=%u)\n",
                          D_80167FC0_168BC0[i].data, planned,
                          planned ? capacity : 0u,
                          (unsigned int)(sizeof(NameplateBankData) * NAMEPLATE_BANKS));
        }
#endif
        return;
    }
    s_slots = recomp_alloc(capacity * sizeof(NameplateSlot));
    if (!s_slots)
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_ALLOC))
        {
            s_debug_reported |= NAMEPLATE_LOG_ALLOC;
            recomp_printf("[nameplates] load: slot allocation failed (capacity=%u bytes=%u)\n",
                          capacity, (unsigned int)(capacity * sizeof(NameplateSlot)));
        }
#endif
        return;
    }
    s_capacity = (int)capacity;
    clear_bytes(s_slots, capacity * sizeof(NameplateSlot));
    s_banks = (NameplateBankData *)(unsigned long)(start | 0x80000000u);
    D_80167FC0_168BC0[i].data =
        (unsigned char *)(unsigned long)(end | 0x80000000u);
#if DEBUG_BUTTON_ENABLED
    if (!(s_debug_reported & NAMEPLATE_LOG_READY))
    {
        s_debug_reported |= NAMEPLATE_LOG_READY;
        recomp_printf("[nameplates] load: ready (capacity=%u start=%08x end=%08x)\n",
                      capacity, start, end);
    }
#endif
}

static NameplateSlot *find_cid(int cid)
{
    int i;
    for (i = 0; i < s_capacity; ++i)
        if (s_slots[i].active && s_slots[i].cid == cid)
            return &s_slots[i];
    return 0;
}

static NameplateSlot *find_or_take(int cid, void *task)
{
    NameplateSlot *slot = find_cid(cid);
    int i;
    if (slot)
        return slot;
    /* The model pool normally reuses a hidden native task after a peer leaves.
     * Reuse its plate object too, avoiding kind-2 allocation on roster churn. */
    for (i = 0; i < s_capacity; ++i)
        if (!s_slots[i].active && s_slots[i].owner_task == task)
            return &s_slots[i];
    for (i = 0; i < s_capacity; ++i)
        if (!s_slots[i].active)
            return &s_slots[i];
    return 0;
}

void anchor_nameplates_hide(int cid)
{
    NameplateSlot *slot = find_cid(cid);
    if (!slot)
        return;
    release_plate(slot);
    slot->active = 0;
    slot->cid = 0;
    slot->owner_task = 0;
    slot->body = 0;
}

void anchor_nameplates_sync(int cid, const char *name, int same_team,
                            void *task, const void *body, unsigned short room,
                            float height)
{
    NameplateSlot *slot;
    unsigned char *object;
    unsigned int i;
    int changed;
    if (nameplates_disabled())
    {
        anchor_nameplates_hide(cid);
        return;
    }
    if (cid <= 0 || !s_banks || !task || !name || !body ||
        room != D_800C7AB2 ||
        !anchor_player_models_is_remote_pair(task, body))
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_SYNC_GATE))
        {
            const char *reason = cid <= 0 ? "cid" :
                                 !s_banks ? "no arena" :
                                 !task ? "no owner task" :
                                 !name ? "no name buffer" :
                                 !body ? "no body" :
                                 room != D_800C7AB2 ? "room" : "owner pair";
            s_debug_reported |= NAMEPLATE_LOG_SYNC_GATE;
            recomp_printf("[nameplates] sync: rejected (%s room=%04x current=%04x)\n",
                          reason, room, D_800C7AB2);
        }
#endif
        anchor_nameplates_hide(cid);
        return;
    }
    slot = find_or_take(cid, task);
    if (!slot)
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_SYNC_FULL))
        {
            s_debug_reported |= NAMEPLATE_LOG_SYNC_FULL;
            recomp_printf("[nameplates] sync: no free plate slot (capacity=%d)\n",
                          s_capacity);
        }
#endif
        return;
    }
    if (slot->owner_task && slot->owner_task != task)
    {
        release_plate(slot);
        clear_bytes(slot, sizeof(*slot));
    }
    slot->owner_task = task;
    slot->body = body;
    /* A native child may have been retired while its remote body was kept for
     * reuse. Never write an object whose task backlink or first-object link
     * no longer matches our retained handle. */
    if (slot->plate_task &&
        (!linked_plate_task(slot->plate_task) ||
         (slot->object && !plate_object_live(slot))))
        release_plate(slot);
    if (!slot->plate_task)
        slot->plate_task = func_80034E08_35A08(task,
                                               nameplate_task_update, 0);
    if (!slot->plate_task)
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_CHILD))
        {
            s_debug_reported |= NAMEPLATE_LOG_CHILD;
            recomp_printf("[nameplates] sync: child task allocation failed\n");
        }
#endif
        return;
    }
    if (!slot->object)
    {
        object = func_8000DBF0_E7F0(slot->plate_task, 0, 0,
                                     0.0f, 0.0f, 0.0f,
                                     (short)0x8000u, (short)0x8000u,
                                     (short)0x8000u,
                                     NAMEPLATE_SCALE, NAMEPLATE_SCALE,
                                     NAMEPLATE_SCALE, 0, 0);
        if (!object)
        {
#if DEBUG_BUTTON_ENABLED
            if (!(s_debug_reported & NAMEPLATE_LOG_OBJECT))
            {
                s_debug_reported |= NAMEPLATE_LOG_OBJECT;
                recomp_printf("[nameplates] sync: kind-2 object allocation failed\n");
            }
#endif
            release_plate(slot);
            return;
        }
        slot->object = object;
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_CONSTRUCTOR_GROUP))
        {
            s_debug_reported |= NAMEPLATE_LOG_CONSTRUCTOR_GROUP;
            recomp_printf("[nameplates] sync: constructor group=%u assigned group=10\n",
                          (unsigned int)object[5]);
        }
#endif
        /* The later native file binder rewrites private segment bases when
         * either file ID is nonzero. These bases are supplied per bank below. */
        *(unsigned short *)(object + 0x3c) = 0;
        *(unsigned short *)(object + 0x4c) = 0;
        slot->object[5] = 10u;
        slot->object[0x64] |= 1u;
    }
    changed = !slot->active || slot->cid != cid ||
              slot->same_team != (same_team != 0);
    for (i = 0; i < sizeof(slot->name) - 1u; ++i)
    {
        unsigned char c = (unsigned char)name[i];
        if (slot->name[i] != c)
            changed = 1;
        slot->name[i] = c;
        if (!c)
            break;
    }
    slot->name[sizeof(slot->name) - 1u] = 0;
    if (changed)
    {
        ++slot->revision;
        if (!slot->revision)
            slot->revision = 1u;
    }
    slot->cid = cid;
    slot->active = 1;
    slot->same_team = same_team != 0;
    slot->room = room;
    slot->height = height;
    slot->object[0x64] |= 1u;
#if DEBUG_BUTTON_ENABLED
    if (!(s_debug_reported & NAMEPLATE_LOG_SYNC_READY))
    {
        s_debug_reported |= NAMEPLATE_LOG_SYNC_READY;
        recomp_printf("[nameplates] sync: plate object ready (room=%04x group=%u)\n",
                      room, (unsigned int)slot->object[5]);
    }
#endif
}

static void set_vertex(NameplateVertex *v, short x, short y, short s, short t)
{
    v->x = x;
    v->y = y;
    v->z = 0;
    v->flag = 0;
    v->s = s;
    v->t = t;
    /* Match the resident case-4 quad's vertex color/normal bytes. */
    v->r = 0;
    v->g = 0;
    v->b = 126u;
    v->a = 255u;
}

static void init_bank(NameplateBankData *data)
{
    static const unsigned int display[NAMEPLATE_COMMANDS * 2u] = {
        0xe7000000u, 0x00000000u,
        0xfd700000u, 0x0b000000u,
        0xf5700000u, 0x07090280u,
        0xe6000000u, 0x00000000u,
        0xf3000000u, 0x077ff040u,
        0xe7000000u, 0x00000000u,
        0xf5684000u, 0x00090280u,
        0xf2000000u, 0x003fc03cu,
        0xb6000000u, 0x000e0000u,
        0xb7000000u, 0x00002000u,
        0xbb000001u, 0xffffffffu,
        0xba001001u, 0x00000000u,
        0xba001102u, 0x00000000u,
        0xfc129a25u, 0xff37ffffu,
        0xba000e02u, 0x00000000u,
        0xba000c02u, 0x00000000u,
        0xfa000000u, 0x000000ffu,
        0x0400103fu, 0x09001000u,
        0xb1040200u, 0x00060400u,
        0xb8000000u, 0x00000000u,
    };
    static const unsigned int material_template[20] = {
        0xe7000000u, 0x00000000u,
        0xba001402u, 0x00000000u,
        0xba001301u, 0x00080000u,
        0xb6000000u, 0x001f3205u,
        0xb7000000u, 0x00002205u,
        0xb900031du, 0x005049d8u,
        0xfb000000u, 0x000000ffu,
        0xb9000002u, 0x00000001u,
        0xf9000000u, 0x00000001u,
        0xb8000000u, 0x00000000u,
    };
    unsigned int i;
    /* A 256:16 rectangle in model space. The native model transform uses
     * 0x8000 rotation sentinels to turn this plane toward the camera. */
    set_vertex(&data->vertices[0], -128,  NAMEPLATE_HALF_HEIGHT, -16, -16);
    set_vertex(&data->vertices[1],  128,  NAMEPLATE_HALF_HEIGHT, 8176, -16);
    set_vertex(&data->vertices[2],  128, -NAMEPLATE_HALF_HEIGHT, 8176, 496);
    set_vertex(&data->vertices[3], -128, -NAMEPLATE_HALF_HEIGHT, -16, 496);
    for (i = 0; i < NAMEPLATE_COMMANDS * 2u; ++i)
        data->display[i] = display[i];
    for (i = 0; i < 20u; ++i)
        data->material_template[i] = material_template[i];
}

static void write_material(NameplateBankData *data, int same_team)
{
    /* Native switch/jet smoke uses this task-local material wrapper. It
     * establishes the translucent environment color before the quad list. */
    data->material[0] = 0x06000000u;
    data->material[1] = (unsigned int)(unsigned long)data->material_template;
    data->material[2] = 0xfb000000u;
    data->material[3] = same_team ? 0xfffffff5u : 0xff4848f5u;
    data->material[4] = 0xb8000000u;
    data->material[5] = 0u;
    data->material[6] = 0u;
    data->material[7] = 0u;
}

/* 16950 returns after selecting a reusable graphics bank. The scheduler has
 * already applied the final remote pose, but 87C4 has not yet drawn objects. */
RECOMP_HOOK_RETURN("func_80016950_17550")
void anchor_nameplates_refresh_bank(void)
{
    unsigned int bank = (unsigned int)D_800C7A72_C8672;
    int i;
    /* A menu change can arrive after the remote task's sync. Hide existing
     * plate objects now; the next enabled sync may reuse the scene arena. */
    if (nameplates_disabled())
    {
        for (i = 0; i < s_capacity; ++i)
            if (s_slots[i].active)
                hide_plate(&s_slots[i]);
        return;
    }
    if (!s_banks || bank >= NAMEPLATE_BANKS)
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_REFRESH))
        {
            s_debug_reported |= NAMEPLATE_LOG_REFRESH;
            recomp_printf("[nameplates] refresh: unavailable (arena=%d bank=%u)\n",
                          s_banks != 0, bank);
        }
#endif
        return;
    }
#if DEBUG_BUTTON_ENABLED
    if (!(s_debug_reported & NAMEPLATE_LOG_REFRESH))
    {
        s_debug_reported |= NAMEPLATE_LOG_REFRESH;
        recomp_printf("[nameplates] refresh: hook reached (bank=%u capacity=%d)\n",
                      bank, s_capacity);
    }
#endif
    for (i = 0; i < s_capacity; ++i)
    {
        NameplateSlot *slot = &s_slots[i];
        NameplateBankData *data;
        unsigned char *object;
        const unsigned char *body;
        if (!current_body(slot) || !slot->object)
        {
#if DEBUG_BUTTON_ENABLED
            if (slot->active &&
                !(s_debug_reported & NAMEPLATE_LOG_REFRESH_BODY))
            {
                s_debug_reported |= NAMEPLATE_LOG_REFRESH_BODY;
                recomp_printf("[nameplates] refresh: active body rejected (%s)\n",
                              body_failure_reason(slot));
            }
#endif
            hide_plate(slot);
            continue;
        }
        data = &s_banks[i * NAMEPLATE_BANKS + bank];
        if (!slot->initialized[bank])
        {
            init_bank(data);
            slot->initialized[bank] = 1u;
        }
        if (slot->uploaded[bank] != slot->revision)
        {
            anchor_nameplate_bitmap_build(
                data->texture, sizeof(data->texture), slot->name,
                D_800629A0_635A0, D_8005BB10_5C710);
            write_material(data, slot->same_team);
            slot->uploaded[bank] = slot->revision;
        }
        /* Flush the selected reusable bank's texture, Vtx and Gfx before the
         * native draw traversal. The other bank remains untouched in flight. */
        osWritebackDCache(data, sizeof(*data));
        object = slot->object;
        body = slot->body;
        *(float *)(object + 8) = *(const float *)(body + 8);
        /* slot->height is the top above the body's origin. Lift the plate
         * center by its scaled half-height and a small gap so the lower edge
         * clears both the normal head and a frozen cube. */
        *(float *)(object + 0xc) = *(const float *)(body + 0xc) +
                                   slot->height +
                                   NAMEPLATE_HALF_HEIGHT * NAMEPLATE_SCALE +
                                   NAMEPLATE_CLEARANCE;
        *(float *)(object + 0x10) = *(const float *)(body + 0x10);
        *(unsigned short *)(object + 0x14) = 0x8000u;
        *(unsigned short *)(object + 0x16) = 0x8000u;
        *(unsigned short *)(object + 0x18) = 0x8000u;
        *(unsigned int *)(object + 0x40) = (unsigned int)(unsigned long)data;
        *(unsigned int *)(object + 0x50) = (unsigned int)(unsigned long)data->texture;
        *(unsigned int *)(object + 0x30) =
            (unsigned int)(unsigned long)data->material | 0x20000000u;
        *(unsigned int *)(object + 0x2c) = NAMEPLATE_MODEL;
        object[5] = 10u;
        object[0x64] &= ~1u;
        object[0x65] = 0;
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_REFRESH_READY))
        {
            s_debug_reported |= NAMEPLATE_LOG_REFRESH_READY;
            recomp_printf("[nameplates] refresh: bound visible plate (bank=%u)\n",
                          bank);
        }
#endif
    }
}

/* Case-4 native drawing uses the stock graphics bank, not the remote body's
 * private scratch redirect. Reserve command and matrix headroom independently. */
RECOMP_HOOK("func_80016C44_17844")
void anchor_nameplates_before_draw(void *pointer)
{
    unsigned int bank, base, head, matrix, command_end, matrix_start, matrix_end;
    unsigned char *object = pointer;
    int i;
#if DEBUG_BUTTON_ENABLED
    const char *gate_reason = 0;
#endif
    s_guard_object = 0;
    for (i = 0; i < s_capacity; ++i)
        if (s_slots[i].object == object)
            break;
    if (i == s_capacity)
        return;
    s_guard_object = object;
    s_guard_hidden = object[0x64] & 1u;
#if DEBUG_BUTTON_ENABLED
    if (!(s_debug_reported & NAMEPLATE_LOG_DRAW_ATTEMPT))
    {
        s_debug_reported |= NAMEPLATE_LOG_DRAW_ATTEMPT;
        recomp_printf("[nameplates] draw: first plate callback (group=%u hidden=%u model=%08x)\n",
                      (unsigned int)object[5],
                      (unsigned int)s_guard_hidden,
                      *(unsigned int *)(object + 0x2c));
    }
#endif
    if (nameplates_disabled())
    {
#if DEBUG_BUTTON_ENABLED
        gate_reason = "config";
#endif
        goto skip;
    }
    if (!current_body(&s_slots[i]))
    {
#if DEBUG_BUTTON_ENABLED
        gate_reason = body_failure_reason(&s_slots[i]);
#endif
        goto skip;
    }
    bank = (unsigned int)D_800C7A72_C8672;
    if (bank >= NAMEPLATE_BANKS || !low_rdram(D_8015C5C8_15D1C8))
    {
#if DEBUG_BUTTON_ENABLED
        gate_reason = bank >= NAMEPLATE_BANKS ? "bank" : "graphics base";
#endif
        goto skip;
    }
    base = (unsigned int)(unsigned long)D_8015C5C8_15D1C8 +
           bank * NATIVE_BANK_BYTES;
    head = (unsigned int)(unsigned long)D_8015C5CC_15D1CC;
    matrix = (unsigned int)(unsigned long)D_80168504_169104;
    command_end = base + NATIVE_COMMAND_BYTES - NATIVE_COMMAND_TAIL -
                  ((anchor_dialog_busy() || anchor_freeze_prompt_visible()) ?
                   NATIVE_DIALOG_TAIL : 0u);
    matrix_start = base + NATIVE_COMMAND_BYTES;
    matrix_end = base + NATIVE_MATRIX_END - PLATE_MATRIX_RESERVE;
    if (head >= base && head <= command_end &&
        PLATE_DRAW_COMMAND_ALLOWANCE <= command_end - head &&
        matrix >= matrix_start && matrix <= matrix_end &&
        64u <= matrix_end - matrix)
    {
#if DEBUG_BUTTON_ENABLED
        if (!(s_debug_reported & NAMEPLATE_LOG_DRAW_READY))
        {
            s_debug_reported |= NAMEPLATE_LOG_DRAW_READY;
            recomp_printf("[nameplates] draw: graphics budget passed (bank=%u)\n",
                          bank);
        }
#endif
        return;
    }
#if DEBUG_BUTTON_ENABLED
    if (!(s_debug_reported & NAMEPLATE_LOG_DRAW_HEADROOM))
    {
        s_debug_reported |= NAMEPLATE_LOG_DRAW_HEADROOM;
        recomp_printf("[nameplates] draw: headroom skip (head=%08x end=%08x matrix=%08x limit=%08x)\n",
                      head, command_end, matrix, matrix_end);
    }
#endif
skip:
#if DEBUG_BUTTON_ENABLED
    if (gate_reason && !(s_debug_reported & NAMEPLATE_LOG_DRAW_GATE))
    {
        s_debug_reported |= NAMEPLATE_LOG_DRAW_GATE;
        recomp_printf("[nameplates] draw: gate skip (%s)\n", gate_reason);
    }
#endif
    object[0x64] |= 1u;
}

RECOMP_HOOK_RETURN("func_80016C44_17844")
void anchor_nameplates_after_draw(void)
{
    if (s_guard_object)
        s_guard_object[0x64] =
            (unsigned char)((s_guard_object[0x64] & ~1u) | s_guard_hidden);
    s_guard_object = 0;
}
