/* Native world/attack rendering, sampled only by the authority. Replicas have
 * no native AI, collision callbacks, hitboxes or gameplay timers. */
#include "anchor_impact_visuals.h"
#include "anchor_impact_players.h"
#include "anchor_impact_native.h"
#include "anchor_remote_model_pool.h"
#include "utils/anchor_impact_visual_catalog.h"
#include "utils/anchor_impact_visual_codec.h"
#ifndef ANCHOR_IMPACT_VISUAL_HOST_TEST
#include "anchor.h"
#include "recomputils.h"
#endif

extern void *D_8020EED0_63A2B0;
extern unsigned char *D_8015C5C8_15D1C8;
extern unsigned char D_80167FC0_168BC0[];
extern void *func_80034E08_35A08(void *, void (*)(void *, void *), unsigned short);
extern void *func_8000DBF0_E7F0(void *, unsigned int, unsigned int,
    float, float, float, short, short, short, float, float, float, short, short);
extern void *func_8000DDF0_E9F0(void *, unsigned int, int, unsigned int, unsigned int, unsigned int);
extern void *func_8000DF10_EB10(void *, unsigned int, int, unsigned int, unsigned int, unsigned int);
extern int func_8003674C_3734C(void *);
extern int func_80036798_37398(void *);
#ifndef IV_PTR
#define IV_PTR(p,o) (*(void **)((unsigned char *)(p)+(o)))
#endif
#define IV_U8(p,o) (*(unsigned char *)((unsigned char *)(p)+(o)))
#define IV_U16(p,o) (*(unsigned short *)((unsigned char *)(p)+(o)))
#define IV_U32(p,o) (*(unsigned int *)((unsigned char *)(p)+(o)))
#define IV_ADDR(p) ((unsigned int)(unsigned long)(p))

typedef struct VisualSlot { void *task, *object; } VisualSlot;
typedef struct HiddenObject {
    void *task, *object, *callback;
    unsigned int model;
    unsigned char flags;
} HiddenObject;
static VisualSlot s_slots[ANCHOR_IMPACT_VISUAL_MAX];
/* A traversal index is not an actor identity: inserting a new projectile can
 * move every later row. Keep a generation and pool index across captures. */
typedef struct SourceSlot {
    void *task, *object;
    unsigned int generation;
    int seen;
} SourceSlot;
static SourceSlot s_sources[ANCHOR_IMPACT_VISUAL_MAX];
static unsigned int s_generation;
static unsigned int source_id(void *task, void *object)
{
    unsigned int i, free_slot = ANCHOR_IMPACT_VISUAL_MAX;
    for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i) {
        SourceSlot *s = &s_sources[i];
        if (s->task == task && s->object == object) {
            s->seen = 1; return (s->generation << 6) + i + 1;
        }
        if (!s->task && free_slot == ANCHOR_IMPACT_VISUAL_MAX) free_slot = i;
    }
    if (free_slot == ANCHOR_IMPACT_VISUAL_MAX) return 0;
    /* Reserve the last generation to keep even slot 63 nonzero. */
    if (++s_generation >= 0x3FFFFFFu) s_generation = 1;
    s_sources[free_slot] = (SourceSlot){task, object, s_generation, 1};
    return (s_generation << 6) + free_slot + 1;
}

static HiddenObject s_hidden[ANCHOR_IMPACT_VISUAL_MAX];
static unsigned int s_hidden_count, s_stage, s_boss, s_visit;
static void *s_manager;
static int s_ready, s_active;
static AnchorImpactVisualFrame s_capture, s_received;
static char s_json[ANCHOR_IMPACT_VISUAL_JSON];

static int valid(const void *p)
{
#ifdef ANCHOR_IMPACT_VISUAL_HOST_TEST
    return p != 0;
#else
    unsigned int a = IV_ADDR(p);
    return !(a & 3u) && ((a >= 0x80001000u && a < 0x80800000u) ||
                         anchor_remote_model_pool_contains(p));
#endif
}
static int linked(void *task)
{
    return valid(task) && valid(IV_PTR(task,4)) && IV_PTR(IV_PTR(task,4),0) == task;
}
static int owns_object(void *task, void *object)
{
    void *p; unsigned int n = 0;
    if (!linked(task) || !valid(object)) return 0;
    for (p = IV_PTR(task,0x18); valid(p) && n++ < 128; p = IV_PTR(p,0))
        if (p == object) return 1;
    return 0;
}
static void display_update(void *task, void *object) { (void)task; (void)object; }
static int slot_live(VisualSlot *s)
{
    return linked(s->task) && IV_PTR(s->task,0x0C) == (void *)display_update;
}
static void hide_slots(void)
{
    unsigned int i;
    for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i)
        if (slot_live(&s_slots[i]) && owns_object(s_slots[i].task,s_slots[i].object))
            IV_U8(s_slots[i].object,0x64) |= 1;
}
void anchor_impact_visuals_begin_frame(void)
{
    while (s_hidden_count) {
        HiddenObject *h = &s_hidden[--s_hidden_count];
        if (owns_object(h->task,h->object) && IV_PTR(h->task,0x0C) == h->callback &&
            IV_U32(h->object,0x2C) == h->model)
            IV_U8(h->object,0x64) = h->flags;
    }
}

/* Resident assets are packed in ascending address order; the following entry
 * bounds the current file. The final file-zero entry contains the arena end.
 * Never resolve an unloaded file through the native fatal resolver. */
static int file_bounds(unsigned int file, unsigned int *base, unsigned int *size)
{
    unsigned int i, a, b;
    if (!file) return 0;
    for (i = 0; i < 47; ++i) {
        void *entry = D_80167FC0_168BC0+i*8;
        if (!IV_U16(entry,0)) break;
        if (IV_U16(entry,0) != file) continue;
        a = IV_ADDR(IV_PTR(entry,4)) & 0xBFFFFFFFu;
        b = IV_ADDR(IV_PTR(D_80167FC0_168BC0+(i+1)*8,4)) & 0xBFFFFFFFu;
        if (!a || b <= a || b-a > 0x800000u) return 0;
        *base = a; *size = b-a; return 1;
    }
    return 0;
}
static unsigned int system_file(void)
{
    return valid(D_8015C5C8_15D1C8) ? IV_U16(D_8015C5C8_15D1C8,0xC2D90) : 0;
}
static int segment(unsigned int address, unsigned int file, unsigned int *out)
{
    unsigned int base, size, candidate, i;
    address &= 0xBFFFFFFFu;
    out[0] = out[1] = 0;
    if (!address) return 1;
    for (i = 0; i < 2; ++i) {
        candidate = i ? system_file() : file;
        if (file_bounds(candidate,&base,&size) && address >= base && address-base < size) {
            out[0] = candidate; out[1] = address-base; return 1;
        }
    }
    return 0;
}
static int material(void *task, void *object, unsigned int *r)
{
    unsigned int p = IV_U32(object,0x30), id, offset;
    r[3] = r[4] = 0;
    id = anchor_impact_visual_material_id(p);
    if (id != 0xFFFFFFFFu) { r[2] = id; return 1; }
    p &= ~0x60000000u;
    for (offset = 0xB0; offset <= 0xD0; offset += 0x20) {
        if (p != IV_ADDR((unsigned char *)task+offset) ||
            IV_U32(task,offset) != 0x06000000u ||
            IV_U32(task,offset+16) != 0xB8000000u || IV_U32(task,offset+20)) continue;
        id = anchor_impact_visual_material_id(IV_U32(task,offset+4));
        if (!id || id == 0xFFFFFFFFu) return 0;
        r[2] = id;
        p = IV_U32(task,offset+8);
        if (p != 0xFA000000u && p != 0xFB000000u) return 0;
        r[3] = p == 0xFA000000u ? 1 : 2;
        r[4] = IV_U32(task,offset+12); return 1;
    }
    return 0;
}
static int capture_object(void *task, void *object, unsigned int *r)
{
    unsigned int i, file = IV_U16(object,0x34);
    if (IV_U8(object,4) != 2) return 0;
    r[0] = 1;
    r[1] = anchor_impact_visual_recipe_id(IV_U32(object,0x2C),file);
    if (!r[1] || !material(task,object,r)) return 0;
    r[5] = IV_U8(object,5);
    r[6] = (IV_U8(object,0x64)&1) | (IV_U8(object,0x65)<<8) |
           ((IV_U32(object,0x30)&0x60000000u)>>13);
    for (i = 0; i < 3; ++i) {
        r[7+i] = IV_U32(object,8+i*4);
        r[10+i] = IV_U16(object,0x14+i*2);
        r[13+i] = IV_U32(object,0x1C+i*4);
    }
    r[16] = IV_U32(object,0x28);
    for (i = 0; i < 6; ++i)
        if (!segment(IV_U32(object,0x38+i*8),file,&r[17+i*2])) return 0;
    return anchor_impact_visual_row_valid(r);
}

/* +1C4 is the world/HUD task, not the cockpit. Its dynamic HUD display lists
 * intentionally have no recipe and remain local. Use the scheduler's flat
 * depth-first links, not a fabricated child pointer or copied task graph. */
static int walk(int hide)
{
    void *parent, *task, *object, *root;
    unsigned int depth, tasks = 0, objects = 0, row[ANCHOR_IMPACT_VISUAL_WORDS], i;
    int root_captured;
    if (!valid(D_8020EED0_63A2B0)) return 0;
    parent = IV_PTR(D_8020EED0_63A2B0,0x1C4);
    root = IV_PTR(D_8020EED0_63A2B0,0x1E0);
    root_captured = !valid(root) || !IV_U32(root,0x2C) || (IV_U8(root,0x64)&1);
    if (!linked(parent)) return 0;
    depth = IV_U16(parent,0x20);
    for (task = parent; linked(task) && (task == parent || IV_U16(task,0x20) > depth);
         task = IV_PTR(task,0)) {
        if (++tasks > 512) return 0;
        for (object = IV_PTR(task,0x18); valid(object); object = IV_PTR(object,0)) {
            if (++objects > 1024) return 0;
            if ((IV_U8(object,0x64)&1) || !capture_object(task,object,row)) continue;
            if (object == root) root_captured = 1;
            if (hide) {
                HiddenObject *h;
                if (s_hidden_count == ANCHOR_IMPACT_VISUAL_MAX) return 0;
                h = &s_hidden[s_hidden_count++];
                h->task = task; h->object = object; h->callback = IV_PTR(task,0x0C);
                h->model = IV_U32(object,0x2C); h->flags = IV_U8(object,0x64);
                IV_U8(object,0x64) |= 1;
            } else {
                if (s_capture.count == ANCHOR_IMPACT_VISUAL_MAX) return 0;
                row[0] = source_id(task,object);
                if (!row[0]) return 0;
                for (i = 0; i < ANCHOR_IMPACT_VISUAL_WORDS; ++i)
                    s_capture.rows[s_capture.count][i] = row[i];
                ++s_capture.count;
            }
        }
    }
    /* An unsupported root must not publish a partial frame that could hide
     * the follower's otherwise visible boss. */
    return hide || root_captured;
}
static int resolve(const unsigned int *r, unsigned int *bases)
{
    const AnchorImpactVisualRecipe *recipe = anchor_impact_visual_recipe(r[1]);
    unsigned int i, base, size;
    if (!recipe || (r[2] && !anchor_impact_visual_material(r[2])) ||
        !file_bounds(recipe->file,&base,&size) || (recipe->model&0xFFFFFFu) >= size ||
        r[17] != recipe->file || r[18]) return 0;
    for (i = 0; i < 6; ++i) {
        unsigned int file = r[17+i*2], offset = r[18+i*2];
        bases[i] = 0;
        if (!file) { if (offset) return 0; continue; }
        if ((file != recipe->file && file != system_file()) ||
            !file_bounds(file,&base,&size) || offset >= size) return 0;
        bases[i] = base+offset;
    }
    return 1;
}
static int render_row(const unsigned int *r)
{
    VisualSlot *slot = &s_slots[(r[0]-1)&63u];
    const AnchorImpactVisualRecipe *recipe = anchor_impact_visual_recipe(r[1]);
    unsigned int i, bases[6], mat, rgba = r[4];
    void *object;
    if (!resolve(r,bases)) return 0;
    if (!slot_live(slot)) slot->task = slot->object = 0;
    if (!slot->task) slot->task = func_80034E08_35A08(s_manager,display_update,0);
    if (!valid(slot->task)) return 0;
    if (!owns_object(slot->task,slot->object)) slot->object = 0;
    if (!slot->object) {
        slot->object = func_8000DBF0_E7F0(slot->task,recipe->model,0,
            0,0,0,0,0,0,1,1,1,(short)recipe->file,0);
        /* No pre/post callback and no collision registration on replicas. */
        IV_PTR(slot->task,8) = IV_PTR(slot->task,0x10) = 0;
        IV_U32(slot->task,0x64) = 0;
        IV_U32(slot->task,0x30) = IV_U32(slot->task,0x34) = IV_U32(slot->task,0x38) = 0;
        IV_U32(slot->task,0x3C) = IV_U16(slot->task,0x40) = 0;
        IV_U32(slot->task,0x48) = IV_U32(slot->task,0x5C) = 0;
    }
    object = slot->object;
    if (!valid(object)) return 0;
    /* Slot numbers can be reused by a different asset. Native cache keys are
     * segmented commands; equal offsets in two files are not the same limbs.
     * Free only this replica's cached geometry, retaining its task/object. */
    if (IV_U16(object,0x34) != recipe->file ||
        ((IV_U32(object,0x2C)^recipe->model)&0x70000000u)) {
        (void)func_8003674C_3734C(object);
        (void)func_80036798_37398(object);
    }
    mat = IV_ADDR(anchor_impact_visual_material(r[2]));
    if (r[3]) {
        void *wrapper = r[3] == 1
            ? func_8000DDF0_E9F0(slot->task,mat,rgba>>24,(rgba>>16)&255,(rgba>>8)&255,rgba&255)
            : func_8000DF10_EB10(slot->task,mat,rgba>>24,(rgba>>16)&255,(rgba>>8)&255,rgba&255);
        mat = IV_ADDR(wrapper);
    }
    mat |= (r[6]&0x30000u)<<13;
    IV_U32(object,0x2C) = recipe->model; IV_U32(object,0x30) = mat;
    IV_U8(object,5) = r[5]; IV_U8(object,0x64) = r[6]&1; IV_U8(object,0x65) = r[6]>>8;
    for (i = 0; i < 3; ++i) {
        IV_U32(object,8+i*4) = r[7+i]; IV_U16(object,0x14+i*2) = r[10+i];
        IV_U32(object,0x1C+i*4) = r[13+i];
    }
    IV_U32(object,0x28) = r[16];
    for (i = 0; i < 6; ++i) {
        IV_U16(object,0x34+i*8) = r[17+i*2]; IV_U32(object,0x38+i*8) = bases[i];
    }
    return 1;
}
void anchor_impact_visuals_tick(int active)
{
    unsigned int i, stage = anchor_impact_native_stage(), boss = anchor_impact_native_encounter();
    unsigned int visit = anchor_impact_native_visit();
    void *manager;
    char *reply;
    const char *sample = "null";
    if (!active && !s_active) return;
    s_active = active;
    manager = valid(D_8020EED0_63A2B0) ? IV_PTR(D_8020EED0_63A2B0,0x1BC) : 0;
    anchor_impact_visuals_begin_frame(); hide_slots(); s_ready = 0;
    if (manager != s_manager) {
        for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i) s_slots[i].task = s_slots[i].object = 0;
        for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i) s_sources[i].task = 0;
        s_manager = manager;
    }
    if (!active || stage != s_stage || boss != s_boss || visit != s_visit ||
        !anchor_impact_native_is_owner())
        for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i) s_sources[i].task = 0;
    s_stage = stage; s_boss = boss; s_visit = visit;
    active = active && linked(manager) && anchor_impact_native_root_live();
    if (active) {
        anchor_impact_visual_catalog_init();
        if (anchor_impact_native_is_owner()) {
            s_capture.count = 0;
            for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i) s_sources[i].seen = 0;
            if (walk(0) && anchor_impact_visual_encode(&s_capture,s_json,sizeof(s_json))) sample = s_json;
            for (i = 0; i < ANCHOR_IMPACT_VISUAL_MAX; ++i)
                if (!s_sources[i].seen) s_sources[i].task = 0;
        }
    }
    reply = anchor_impact_visuals_update(active,stage,boss,visit,sample);
    if (reply) {
        if (active && !anchor_impact_native_is_owner() && anchor_impact_visual_decode(reply,&s_received)) {
            s_ready = 1;
            for (i = 0; i < s_received.count; ++i) {
                unsigned int bases[6];
                if (!resolve(s_received.rows[i],bases)) { s_ready = 0; break; }
            }
        }
        recomp_free(reply);
    }
}
void anchor_impact_visuals_render(void)
{
    unsigned int i;
    if (!s_ready || anchor_impact_native_is_owner() || !anchor_impact_native_root_live() ||
        s_stage != anchor_impact_native_stage() || s_boss != anchor_impact_native_encounter() ||
        s_visit != anchor_impact_native_visit()) { hide_slots(); return; }
    for (i = 0; i < s_received.count; ++i)
        if (!render_row(s_received.rows[i])) { s_ready = 0; hide_slots(); return; }
    if (!walk(1)) {
        anchor_impact_visuals_begin_frame(); hide_slots(); s_ready = 0;
    } else anchor_impact_players_hide_shots();
}
int anchor_impact_visuals_active(void) { return s_ready; }
