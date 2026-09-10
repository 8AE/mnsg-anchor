/**
 * @file anchor_player_models.c
 * @brief Render all four remote characters with cutscene-style model tasks.
 *
 * The opening-cutscene trace shows the game does NOT use the player manager
 * or the slot-indexed staging chain to display character models in scripted
 * scenes. Instead it:
 *
 *   1. Loads whole files resident with FUN_80013B14 during the stage resource
 *      hook, never from a per-frame callback.
 *   2. Spawns plain kind-2 render objects with FUN_8000DBF0: model command
 *      pointer at object+0x2c, per-segment (file id, base) pairs starting at
 *      object+0x34, position/rotation/scale, all with no player task.
 *   3. Animates through object+0x28 (frame) and object+0x30 (anim state).
 *
 * This file uses that task/object architecture, then binds Goemon, Ebisumaru,
 * Sasuke, and Yae render assets and action records so the clothed model and the
 * sender's exact action clip can be displayed. It never calls a playable
 * character constructor, playable action callback, or player-manager update.
 * The pristine action files are cached whole in mod memory. A transformed
 * Goemon gets a slot-private copy of only the current action slice so the
 * stock Sudden Impact display-pointer replacement cannot mutate another
 * remote's model. The broad character files are registered by the normal scene
 * resource loader. Face/part resources are different: their pixels are
 * referenced by generated N64 display-list commands, so shared immutable
 * expression pages are carved from the stock scene arena below 0x80800000 instead of
 * the extended recomp heap.
 *
 * Object segment binding (matches the player object layout):
 *   +0x38 action file base (segment 8; normally the cached file, or a
 *         slot-private current-action slice rebased by its file offset)
 *   +0x3c broad file id / +0x40 broad file base (segment 9)
 *   +0x50 / +0x58 aux face/part resources (segments from D_80203FF0),
 *         loaded per resource with FUN_800145B4 into shared texture pages
 *   +0x2c action record model pointer | 0x60000000
 */

#include "anchor_player_models.h"
#include "anchor_dialog.h"
#include "anchor_remote_model_pool.h"
#include "anchor_remote_animation.h"
#include "anchor_remote_appearance.h"
#include "anchor_remote_collision.h"
#include "anchor_collision_actors.h"
#include "anchor_player_damage.h"
#include "item_sync.h"
#include "anchor.h"
#include "modding.h"
#include "recomputils.h"
#include "utils/array_utils.h"
#include "utils/texture_cache.h"

#define REMOTE_PLAYER_ACTION_IDLE 0
#define REMOTE_PLAYER_ACTION_MAX 0xe8
#define REMOTE_PLAYER_ACTION_CHARACTER_SWITCH 0xba
#define REMOTE_PLAYER_ACTION_MINI_SHRINK 0x8e
#define REMOTE_PLAYER_ACTION_MINI_GROW 0x8f
#define PLAYER_MODEL_RENDER_SEGMENT 0x60000000u
#define ACTION_MODEL_FILE_SEGMENT 0x07000000u
#define CLOTHED_CHARACTER_ANIM_CONTEXT 0xc01fc680u
#define CLOTHED_CHARACTER_OBJECT_MODE 2u
#define REMOTE_YAW_SPEED_THRESHOLD_SQ 64
#define REMOTE_MODEL_SCALE 0.1f

#define CHARACTER_GOEMON 0
#define CHARACTER_EBISUMARU 1
#define CHARACTER_COUNT 4
#define MINI_MODEL_SCALE 0.025f
#define MINI_SCALE_START_FRAME 12.0f
#define MINI_SCALE_END_FRAME 20.0f
#define MINI_SCALE_CURVE 0.001171875f
#define AUX_BUFFER_SIZE 0x1000u
#define AUX_RESOURCE_COUNT 2
#define AUX_SEQUENCE_MAX_STEPS 32
#define AUX_RESOURCE_ID_LIMIT 0x8770u
#define BUFFER_ALIGN 16u
#define SCENE_RESOURCE_ENTRY_COUNT 48
#define RENDER_RDRAM_END 0x80800000u
#define AUX_CACHE_PAGES 100
#define AUX_ARENA_SIZE (AUX_CACHE_PAGES * AUX_BUFFER_SIZE)

typedef struct SceneResourceEntry
{
    unsigned short file_id;
    unsigned short padding;
    unsigned char *data;
} SceneResourceEntry;

typedef struct CharacterModelCache
{
    int ready;
    unsigned char *broad;       /* resident broad file from the scene registry */
    unsigned int broad_size;
    unsigned char *action;      /* whole action-model file, raw ROM image */
    unsigned int action_size;
    unsigned int max_action_model_size;
} CharacterModelCache;

typedef struct RemoteModelSlot
{
    int active;
    int cid;
    int seen;
    int bound_ch;     /* character currently bound to the object, -1 if none */
    int bound_action; /* action currently bound, -1 if none */
    int bound_sudden_impact;
    int last_seq;
    int last_remote_frame_100;
    int last_remote_frame_count_100;
    int last_remote_anim_step_100;
    int last_remote_has_anim_step;
    short yaw;
    float frame;
    float frame_step;
    float native_frame_step;
    AnchorRemoteAnimationState animation;
    void *task;
    void *object;
    unsigned char *private_action;
    unsigned int private_action_size;
    int aux_cache_index[AUX_RESOURCE_COUNT];
    unsigned int aux_resource_id[AUX_RESOURCE_COUNT];
    unsigned char aux_cursor[AUX_RESOURCE_COUNT];
    unsigned char aux_skip_update[AUX_RESOURCE_COUNT];
    float aux_last_frame;
    AnchorPlayerModelRemote pending_remote;
    int pending_valid;
    unsigned short pending_room;
    AnchorCollisionBody collision_body;
    int collision_ready;
    unsigned int drive_sample_tick;
} RemoteModelSlot;

/* Allocate and insert an engine task under `task_list` with an update
 * callback. Same allocator the opening cutscene uses for its scene tasks. */
extern void *func_80034E08_35A08(void *task_list, void (*update)(void *, void *),
                                 unsigned short flags);

/* Spawn a kind-2 render object under a task: sets object+0x2c (model command
 * pointer), +0x30 (anim state pointer), position/rotation/scale, the segment
 * file ids at +0x34/+0x3c, then registers segment bases via FUN_80014218.
 * This is the primitive every cutscene prop/character is spawned with. */
extern void *func_8000DBF0_E7F0(void *task, unsigned int model_ptr, unsigned int anim_ptr,
                                float x, float y, float z,
                                short rot_x, short rot_y, short rot_z,
                                float scale_x, float scale_y, float scale_z,
                                short seg8_file_id, short seg9_file_id);

/* Load and decompress a broad character resource into the game's scene
 * registry. This is called only by the stage-load return hook. */
extern void *func_80013B14_14714(unsigned int file_id);

/* Look up a file already resident in the scene registry. The returned base is
 * bound to segment 9 of each remote model object. */
extern void *func_800141C4_14DC4(unsigned int file_id);

/* Scene resource registry and resident-arena cursor. Ghidra shows
 * func_80013B14 using the first file_id == 0 entry's data pointer as the next
 * broad-resource destination; func_801DC630 carves the stock player's
 * persistent face buffers from this same arena. */
extern SceneResourceEntry D_80167FC0_168BC0[SCENE_RESOURCE_ENTRY_COUNT];

/* Blocking DMA copy from ROM. The action-model file is stored raw (the stock
 * func_801DC70C DMAs ranges of it directly), so the whole file is copied. */
extern void func_80001640_2240(unsigned int rom_addr, void *dst, unsigned int size);

/* ROM start / end address of a file id. */
extern unsigned int func_80001D68_2968(unsigned int file_id);
extern unsigned int func_80001D94_2994(unsigned int file_id);

/* Native frame-submission counter. The player recovery updater uses its
 * low-bit parity for object +0x64 flicker; each client applies that cadence. */
extern unsigned short D_800C7A78;

/* Load a compressed resource by id into dst; returns end pointer. Used for
 * the per-action aux face/part resources (like func_801DC87C). */
extern unsigned char *func_800145B4_151B4(unsigned int resource_id, void *dst);

/* Return the stored size of a resource id without loading it. The remote
 * renderer uses this to reject invalid aux ids before they can write display
 * data outside a stock-sized face buffer. */
extern int func_80014698_15298(unsigned int resource_id, void *rom_address_out);

/* Frame count of the object's currently bound model; used to wrap the
 * remote animation frame the same way the stock player update does. */
extern float func_8001B5AC_1C1AC(void *object);

/* Immutable per-character action record arrays (0x1C-byte records): +0x00 model
 * command pointer, +0x04 anim speed *100, +0x0c/+0x10 model data range,
 * +0x14 aux selector byte, +0x18 aux resource table. */
extern unsigned char *D_80203F34_5BFE44[];

/* Aux staging descriptor bytes: two (?, segment) pairs; the segment bytes
 * pick the action-start object segment bases at +0x38 + segment*8. */
extern unsigned char D_80203FF0_5BFF00[];

/* Per-frame aux descriptors used by FUN_801DB060/FUN_801DB1D4. Each low byte
 * selects the object segment rebound when a timed face/part resource changes. */
extern unsigned short D_80203FF8_5BFF08[];

/* Per-character broad character-resource file ids. All four entries are read
 * to stage the correct clothed Goemon/Ebisumaru/Sasuke/Yae render data. */
extern unsigned short D_80204020_5BFF30[];

/* Per-character raw action-model file ids. All four entries are copied into
 * mod-owned render caches for exact remote action selection. */
extern unsigned short D_80204028_5BFF38[];

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned short D_800C7AB2;
/* Native per-character body extents, scaled by the rendered model. */
extern unsigned short D_801FC660_5B8570[];
extern unsigned short D_801FC668_5B8578[];

/* Stock player model-replacement tables. Index 1 replaces Goemon's normal
 * hair display pointers with the Sudden Impact variants; index 2 reverses
 * that mutation. The remote renderer starts from a pristine private action
 * copy, so it only needs the forward table. */
extern void *D_80204048_5BFF58[];

/* Walk a bound model tree and apply one of the stock display-pointer
 * replacement tables. FUN_801DD498 calls this for the live player. */
extern int func_8001C3E0_1CFE0(void *object, unsigned int model_ptr,
                               const void *replacement_table);

/* DMA mode byte the stock action copy (func_801DC70C) forces to 1 around
 * its ROM DMA and then restores; mirrored here for the whole-file copy.
 * Symbol D_8015C5D4_15D1D4 lives in the ABSOLUTE_SYMS pseudo-section, so it
 * is addressed directly. */
#define STOCK_DMA_MODE (*(volatile unsigned char *)0x8015C5D4)

static CharacterModelCache s_char_cache[CHARACTER_COUNT];
static RemoteModelSlot *s_slots;
static int s_slot_capacity;
static AnchorCollisionBody *s_collision_peers;
static int s_collision_peer_capacity;
static void *s_owner_task;
static unsigned int s_interaction_tick;
static void *s_interaction_task;
static void *s_interaction_object;
static unsigned short s_interaction_room;
static int s_interaction_scripted;
static int s_interaction_alive;
static int s_player_epoch = 1;
static int s_drive_x;
static int s_drive_z;
static unsigned int s_drive_tick;
static unsigned char *s_aux_arena;
static MnsgTextureCacheEntry s_aux_cache[AUX_CACHE_PAGES];

static void remote_model_task_update(void *task, void *object);

static void write_u8_at(void *obj, unsigned int offset, unsigned char value)
{
    *(unsigned char *)((unsigned char *)obj + offset) = value;
}

static void write_u16_at(void *obj, unsigned int offset, unsigned short value)
{
    *(unsigned short *)((unsigned char *)obj + offset) = value;
}

static void write_u32_at(void *obj, unsigned int offset, unsigned int value)
{
    *(unsigned int *)((unsigned char *)obj + offset) = value;
}

static void write_float_at(void *obj, unsigned int offset, float value)
{
    *(float *)((unsigned char *)obj + offset) = value;
}

static int is_rdram_pointer(const void *ptr)
{
    unsigned int addr = (unsigned int)(unsigned long)ptr;
    unsigned int phys = addr & 0x1fffffffu;

    /* Exclude the engine's 0x80000000 invalid-link sentinel as well as null.
     * Stock resources live above the first RDRAM page; additional CPU task
     * and skeleton records must belong to a registered pool chunk. */
    return (phys >= 0x00001000u && phys < 0x00800000u) ||
           anchor_remote_model_pool_contains(ptr);
}

/* Ghidra: a live task's +0x04 field points to the list word that currently
 * references that task. FUN_80034A10 clears +0x04 when a task is freed, while
 * FUN_800350C4 assumes the backlink is live and splices through it. Validate
 * both halves before touching a retained remote child during owner teardown. */
static int is_linked_task(const void *task)
{
    void *backlink;

    if (!is_rdram_pointer(task))
        return 0;
    backlink = *(void *const *)((const unsigned char *)task + 0x04);
    if (!is_rdram_pointer(backlink))
        return 0;
    return *(void *const *)backlink == task;
}

/* A task-pool address can be reused for an unrelated task after teardown.
 * FUN_80034B58 stores the update callback at task+0x0C, so require Anchor's
 * callback in addition to valid list linkage before retaining a child. */
static int is_linked_remote_task(const void *task)
{
    if (!is_linked_task(task))
        return 0;
    return *(void *const *)((const unsigned char *)task + 0x0c) ==
           (void *)remote_model_task_update;
}

static void *read_sound_task_pointer(const void *base, unsigned int offset)
{
    void *value;

    /* Native pointers are four-byte aligned; host regression builds use
     * eight-byte pointers against the same N64 byte offsets. A fixed-size
     * builtin copy keeps both layouts defined and lowers inline for MIPS. */
    __builtin_memcpy(&value, (const unsigned char *)base + offset,
                     sizeof(value));
    return value;
}

int anchor_player_models_is_local_sound_task(const void *task)
{
    void *player = D_801FC604_5B8514;
    void *object = D_801FC60C_5B851C;

    if (!is_linked_task(player) || !is_rdram_pointer(object) ||
        read_sound_task_pointer(player, 0x18) != object ||
        !is_linked_task(task))
        return 0;
    if (task == player)
        return 1;

    /* Remote models are render-only children of the local player. Keep that
     * synthetic ownership relationship out of local sound attribution even
     * if a future renderer change happens to enqueue a native cue. */
    return !is_linked_remote_task(task) &&
           read_sound_task_pointer(task, 0x5c) == player;
}

static unsigned char *alloc_aligned(unsigned int size)
{
    unsigned int addr = (unsigned int)(unsigned long)recomp_alloc(size + BUFFER_ALIGN);

    if (addr == 0)
        return 0;
    return (unsigned char *)(unsigned long)((addr + (BUFFER_ALIGN - 1u)) & ~(BUFFER_ALIGN - 1u));
}

static void invalidate_aux_render_arena(void)
{
    int slot_index;
    int channel;
    int i;

    s_aux_arena = 0;
    for (i = 0; i < AUX_CACHE_PAGES; ++i)
    {
        s_aux_cache[i].resource = 0;
        s_aux_cache[i].references = 0;
        s_aux_cache[i].released_frame = 0;
    }
    for (slot_index = 0; slot_index < s_slot_capacity; ++slot_index)
    {
        RemoteModelSlot *slot = &s_slots[slot_index];

        for (channel = 0; channel < AUX_RESOURCE_COUNT; ++channel)
        {
            slot->aux_cache_index[channel] = -1;
            slot->aux_resource_id[channel] = 0;
            slot->aux_cursor[channel] = 0;
            slot->aux_skip_update[channel] = 0;
        }
        /* Force the next scheduled task update to rebind segment 8/9 and both
         * face segments from the newly rebuilt external scene arena. */
        slot->bound_ch = -1;
        slot->bound_action = -1;
        slot->bound_sudden_impact = 0;
        slot->aux_last_frame = 0.0f;
        slot->collision_ready = 0;
        slot->pending_valid = 0;
    }
}

/* Reserve all renderer-visible face buffers from the stock resident resource
 * arena. FUN_801DC630 uses the same bump-allocation pattern for the local
 * player's four 0x1000-byte buffers. Keeping these addresses in the original
 * 8 MiB RDRAM window prevents N64 texture commands from truncating an extended
 * recomp_alloc address such as 0x81000000 to unrelated texture memory. */
static int reserve_aux_render_arena(void)
{
    SceneResourceEntry *free_entry = 0;
    unsigned int start;
    unsigned int end;
    int i;

    /* Read the external scene registry to find the loader's sentinel cursor;
     * reserving at that cursor keeps all already-loaded broad dependencies
     * below the remote face arena. */
    for (i = 0; i < SCENE_RESOURCE_ENTRY_COUNT; ++i)
    {
        if (D_80167FC0_168BC0[i].file_id == 0)
        {
            free_entry = &D_80167FC0_168BC0[i];
            break;
        }
    }
    if (!free_entry || !free_entry->data)
        return 0;

    start = ((unsigned int)(unsigned long)free_entry->data & 0xbfffffffu);
    start = (start + (BUFFER_ALIGN - 1u)) & ~(BUFFER_ALIGN - 1u);
    end = start + AUX_ARENA_SIZE;
    if (end < start || end > RENDER_RDRAM_END ||
        !is_rdram_pointer((void *)(unsigned long)start) ||
        !is_rdram_pointer((void *)(unsigned long)(end - 1u)))
    {
        recomp_printf("[remote_models] face arena %x..%x is outside render RDRAM\n",
                      start, end);
        return 0;
    }

    /* Advance the external scene loader's sentinel because later resident
     * resources must begin after the remote face buffers, not overwrite them. */
    free_entry->data = (unsigned char *)(unsigned long)end;
    s_aux_arena = (unsigned char *)(unsigned long)start;
    recomp_printf("[remote_models] face arena reserved at %x..%x\n", start, end);
    return 1;
}

static void release_slot_aux(RemoteModelSlot *slot)
{
    int channel;
    for (channel = 0; channel < AUX_RESOURCE_COUNT; ++channel)
    {
        if (slot->aux_resource_id[channel])
            mnsg_texture_cache_release(&s_aux_cache[slot->aux_cache_index[channel]],
                                        D_800C7A78);
        slot->aux_resource_id[channel] = 0;
        slot->aux_cache_index[channel] = -1;
    }
}

/* ------------------------------------------------------------------ */
/* Character model cache                                              */
/* ------------------------------------------------------------------ */

static unsigned char *resident_resource_base(unsigned int file_id)
{
    void *resource;
    unsigned int address;

    /* Use the engine registry lookup because FUN_80013B14 owns the broad-file
     * allocation and may return a C0-tagged cached address. */
    resource = func_800141C4_14DC4(file_id);
    if (!resource || resource == (void *)(unsigned long)0xffffffffu)
        return 0;
    address = (unsigned int)(unsigned long)resource & 0xbfffffffu;
    if (!is_rdram_pointer((void *)(unsigned long)address))
        return 0;
    return (unsigned char *)(unsigned long)address;
}

/* The next registry allocation (including the sentinel cursor) bounds the
 * resident file. Use the closest later base so preflight cannot walk into
 * another asset or into the graphics arenas reserved after staging. */
static unsigned int resident_resource_size(const unsigned char *base)
{
    unsigned int start = (unsigned int)(unsigned long)base;
    unsigned int end = 0;
    int i;
    if (!base)
        return 0;
    for (i = 0; i < SCENE_RESOURCE_ENTRY_COUNT; ++i)
    {
        unsigned int next = (unsigned int)(unsigned long)
            D_80167FC0_168BC0[i].data & 0xbfffffffu;
        if (next > start && next <= RENDER_RDRAM_END && (!end || next < end))
            end = next;
        if (!D_80167FC0_168BC0[i].file_id)
            break;
    }
    return end ? end - start : 0;
}

static int action_model_range(int ch, int action, unsigned int *offset_out,
                              unsigned int *size_out)
{
    CharacterModelCache *cache;
    unsigned char *entry;
    unsigned int start;
    unsigned int end;
    unsigned int offset;
    unsigned int end_offset;

    if (ch < 0 || ch >= CHARACTER_COUNT || action < 0 ||
        action >= REMOTE_PLAYER_ACTION_MAX)
        return 0;
    cache = &s_char_cache[ch];
    entry = D_80203F34_5BFE44[ch] + action * 0x1c;
    start = *(unsigned int *)(entry + 0x0c);
    end = *(unsigned int *)(entry + 0x10);
    if ((start & 0xff000000u) != ACTION_MODEL_FILE_SEGMENT)
        return 0;
    offset = start - ACTION_MODEL_FILE_SEGMENT;
    if (end == 0)
        end_offset = cache->action_size;
    else
    {
        if ((end & 0xff000000u) != ACTION_MODEL_FILE_SEGMENT)
            return 0;
        end_offset = end - ACTION_MODEL_FILE_SEGMENT;
    }
    if (offset >= cache->action_size || end_offset <= offset ||
        end_offset > cache->action_size)
        return 0;
    *offset_out = offset;
    *size_out = end_offset - offset;
    return 1;
}

static void copy_bytes(unsigned char *dst, const unsigned char *src,
                       unsigned int size)
{
    unsigned int i;

    for (i = 0; i < size; ++i)
        dst[i] = src[i];
}

static int cache_action_file(int ch)
{
    CharacterModelCache *cache = &s_char_cache[ch];
    /* Use the immutable file-id table to select only this remote character's
     * raw display data, without asking a player task to stage an action. */
    unsigned int action_id = D_80204028_5BFF38[ch];
    unsigned int file_start;
    unsigned int file_end;
    unsigned char saved_dma_mode;

    if (cache->action)
        return 1;

    /* Read the immutable action file's ROM bounds so the entire file can be
     * cached once and shared by every cutscene-style remote of this character. */
    file_start = func_80001D68_2968(action_id);
    file_end = func_80001D94_2994(action_id);
    if (file_end <= file_start)
    {
        recomp_printf("[remote_models] ch %d action file %x bad range\n",
                      ch, action_id);
        return 0;
    }

    cache->action_size = file_end - file_start;
    cache->action = alloc_aligned(cache->action_size);
    if (!cache->action)
        return 0;

    /* Match the stock action DMA's synchronous mode so model data is complete
     * before a remote object can bind a pointer into this cache. */
    saved_dma_mode = STOCK_DMA_MODE;
    STOCK_DMA_MODE = 1;
    func_80001640_2240(file_start, cache->action, cache->action_size);
    STOCK_DMA_MODE = saved_dma_mode;

    cache->max_action_model_size = 0;
    {
        int action;

        for (action = 0; action < REMOTE_PLAYER_ACTION_MAX; ++action)
        {
            unsigned int offset;
            unsigned int size;

            if (action_model_range(ch, action, &offset, &size) &&
                size > cache->max_action_model_size)
                cache->max_action_model_size = size;
        }
    }
    return 1;
}

void anchor_player_models_load_resources(void)
{
    int ch;

    /* A new stage rebuilds the external scene registry and its resident arena,
     * so discard every pointer into the previous stage before reserving again. */
    invalidate_aux_render_arena();

    for (ch = 0; ch < CHARACTER_COUNT; ++ch)
    {
        CharacterModelCache *cache = &s_char_cache[ch];
        /* Use the immutable broad-file table to keep all four character
         * resources independent of the local player's selected character. */
        unsigned int broad_id = D_80204020_5BFF30[ch];

        cache->ready = 0;
        cache->broad = 0;
        cache->broad_size = 0;

        /* Use the scene loader during the stage-load return hook so every
         * character's clothed broad render resources are resident before any
         * per-frame remote task runs. */
        func_80013B14_14714(broad_id);
        cache->broad = resident_resource_base(broad_id);
        cache->broad_size = resident_resource_size(cache->broad);
        if (!cache->broad || !cache_action_file(ch))
        {
            recomp_printf("[remote_models] ch %d resource staging failed\n", ch);
            continue;
        }

        cache->ready = 1;
        recomp_printf("[remote_models] ch %d clothed model/action cache ready\n",
                      ch);
    }

    if (!reserve_aux_render_arena())
    {
        /* Without original-RDRAM face storage, leave all external character
         * resources unavailable instead of submitting corrupt texture data. */
        for (ch = 0; ch < CHARACTER_COUNT; ++ch)
            s_char_cache[ch].ready = 0;
        recomp_printf("[remote_models] face arena reservation failed\n");
    }
}

/* ------------------------------------------------------------------ */
/* Remote model slots                                                 */
/* ------------------------------------------------------------------ */

static void hide_object(void *object)
{
    int i;
    if (!object)
        return;

    for (i = 0; i < s_slot_capacity; ++i)
        if (s_slots[i].object == object)
        {
            s_slots[i].collision_ready = 0;
            /* A failed new expression must not leave a hidden model pinning
             * all cache pages forever. Retirement still protects GPU reads. */
            release_slot_aux(&s_slots[i]);
        }

    write_u32_at(object, 0x2c, 0);
    write_float_at(object, 0x1c, 0.0f);
    write_float_at(object, 0x20, 0.0f);
    write_float_at(object, 0x24, 0.0f);
    write_u8_at(object, 0x65, 1);
    anchor_remote_appearance_apply_hurt(object, 0, 0);
}

static void show_object(void *object)
{
    float scale = REMOTE_MODEL_SCALE;

    write_float_at(object, 0x1c, scale);
    write_float_at(object, 0x20, scale);
    write_float_at(object, 0x24, scale);
    write_u8_at(object, 0x65, 0);
    anchor_remote_appearance_apply_hurt(object, 0, 0);
}

/* Retire a peer without directly destroying its engine task. Remote children
 * belong to the live player-owner tree, so the engine reclaims them exactly
 * once when that owner is destroyed. While the owner remains linked, keep the
 * hidden task/object pair for reuse by the next peer assigned to this slot. */
static void clear_slot_state(RemoteModelSlot *slot, int preserve_live_task)
{
    void *retained_task = 0;
    void *retained_object = 0;
    int i;

    if (preserve_live_task && is_linked_remote_task(slot->task))
    {
        retained_task = slot->task;
        if (is_rdram_pointer(slot->object))
        {
            hide_object(slot->object);
            retained_object = slot->object;
        }
    }

    release_slot_aux(slot);
    slot->active = 0;
    slot->cid = 0;
    slot->seen = 0;
    slot->bound_ch = -1;
    slot->bound_action = -1;
    slot->bound_sudden_impact = 0;
    slot->last_seq = 0;
    slot->last_remote_frame_100 = 0;
    slot->last_remote_frame_count_100 = 0;
    slot->last_remote_anim_step_100 = 0;
    slot->last_remote_has_anim_step = 0;
    slot->yaw = 0;
    slot->frame = 0.0f;
    slot->frame_step = 1.0f;
    slot->native_frame_step = 1.0f;
    anchor_remote_animation_reset(&slot->animation);
    slot->aux_last_frame = 0.0f;
    slot->pending_valid = 0;
    slot->collision_ready = 0;
    slot->drive_sample_tick = 0;
    slot->task = retained_task;
    slot->object = retained_object;
    for (i = 0; i < AUX_RESOURCE_COUNT; ++i)
    {
        slot->aux_resource_id[i] = 0;
        slot->aux_cursor[i] = 0;
        slot->aux_skip_update[i] = 0;
    }
}

void anchor_player_models_reset(void)
{
    int i;

    for (i = 0; i < s_slot_capacity; ++i)
        clear_slot_state(&s_slots[i], 0);
    s_owner_task = 0;
}

static RemoteModelSlot *find_slot(int cid)
{
    int i;

    for (i = 0; i < s_slot_capacity; ++i)
    {
        if (s_slots[i].active && s_slots[i].cid == cid)
            return &s_slots[i];
    }
    return 0;
}

int anchor_player_models_get_position(int cid, float *x, float *y, float *z)
{
    RemoteModelSlot *slot = find_slot(cid);
    if (!slot || !slot->collision_ready ||
        slot->pending_room != D_800C7AB2 ||
        !is_linked_remote_task(slot->task) ||
        s_owner_task != D_801FC604_5B8514)
        return 0;
    *x = slot->collision_body.position.x;
    *y = slot->collision_body.position.y;
    *z = slot->collision_body.position.z;
    return 1;
}

int anchor_player_models_get_sound_position(int cid, int session, int epoch,
                                            float *x, float *y, float *z)
{
    RemoteModelSlot *slot = find_slot(cid);
    if (!x || !y || !z || !slot || !slot->active || !slot->pending_valid ||
        slot->pending_room != D_800C7AB2 ||
        slot->pending_remote.interaction_session != session ||
        slot->pending_remote.player_epoch != epoch ||
        s_owner_task != D_801FC604_5B8514 ||
        !is_linked_task(s_owner_task) || !is_linked_remote_task(slot->task))
        return 0;

    if (slot->collision_ready)
    {
        *x = slot->collision_body.position.x;
        *y = slot->collision_body.position.y;
        *z = slot->collision_body.position.z;
    }
    else
    {
        /* Scripted movement deliberately has no collision body. Its pending
         * smoothed transform is still the authoritative audible position and
         * also covers the frame before a newly bound child task first runs. */
        *x = slot->pending_remote.x;
        *y = slot->pending_remote.y;
        *z = slot->pending_remote.z;
    }
    return 1;
}

static RemoteModelSlot *alloc_slot(int cid)
{
    int i;

    for (i = 0; i < s_slot_capacity; ++i)
    {
        if (!s_slots[i].active)
        {
            /* Reuse an existing hidden task/object before growing the roster. */
            clear_slot_state(&s_slots[i], 1);
            s_slots[i].active = 1;
            s_slots[i].cid = cid;
            return &s_slots[i];
        }
    }
    i = s_slot_capacity;
    if (!mnsg_array_reserve((void **)&s_slots, &s_slot_capacity, i + 1,
                            sizeof(*s_slots)))
        return 0;
    clear_slot_state(&s_slots[i], 0);
    s_slots[i].active = 1;
    s_slots[i].cid = cid;
    return &s_slots[i];
}

int anchor_player_models_capacity(void)
{
    return s_slot_capacity;
}

int anchor_player_models_is_remote_object(const void *object)
{
    int i;
    if (!object || s_owner_task != D_801FC604_5B8514)
        return 0;
    for (i = 0; i < s_slot_capacity; ++i)
        if (s_slots[i].active && s_slots[i].object == object &&
            is_linked_remote_task(s_slots[i].task))
            return 1;
    return 0;
}

static int range_contains(const void *base, unsigned int size,
                           unsigned int address, unsigned int bytes)
{
    unsigned int start = (unsigned int)(unsigned long)base;
    return base && address >= start && bytes <= size &&
           address - start <= size - bytes;
}

const void *anchor_player_models_resolve_render_address(const void *object,
    unsigned int encoded, unsigned int bytes)
{
    unsigned int address = encoded;
    int i;
    if (!object || !encoded || !bytes)
        return 0;
    if (!(encoded & 0x80000000u))
    {
        unsigned int segment = encoded <= 0x08000000u ? 0u :
            ((encoded >> 24) & 15u) - 8u;
        unsigned int base;
        unsigned int offset = encoded & 0x00ffffffu;
        if (segment >= 6)
            return 0;
        base = *(const unsigned int *)((const unsigned char *)object +
                                       0x38 + segment * 8);
        address = base + offset;
        if (address < base)
            return 0;
    }
    address &= 0x8fffffffu;
    for (i = 0; i < s_slot_capacity; ++i)
    {
        const RemoteModelSlot *slot = &s_slots[i];
        const CharacterModelCache *cache;
        if (!slot->active || slot->object != object ||
            slot->bound_ch < 0 || slot->bound_ch >= CHARACTER_COUNT)
            continue;
        cache = &s_char_cache[slot->bound_ch];
        if (range_contains(cache->action, cache->action_size, address, bytes) ||
            range_contains(cache->broad, cache->broad_size, address, bytes) ||
            range_contains(slot->private_action, slot->private_action_size,
                           address, bytes))
            return (const void *)(unsigned long)address;
        return 0;
    }
    return 0;
}

static int ensure_slot_task(RemoteModelSlot *slot, const AnchorPlayerModelRemote *remote,
                            void *render_parent_task)
{
    float scale;
    void *object;

    /* If the task pool reused this address across an owner transition, discard
     * both stale handles before any object write or callback reuse. */
    if (slot->task && !is_linked_remote_task(slot->task))
    {
        slot->collision_ready = 0;
        slot->task = 0;
        slot->object = 0;
        slot->bound_ch = -1;
        slot->bound_action = -1;
        slot->bound_sudden_impact = 0;
    }
    if (slot->task && slot->object)
        return 1;

    if (!slot->task)
    {
        /* Use the external cutscene task allocator only when this owner-scoped
         * slot has no retained child task to reuse. */
        slot->task = func_80034E08_35A08(render_parent_task,
                                         remote_model_task_update, 0);
    }
    if (!slot->task)
        return 0;

    scale = REMOTE_MODEL_SCALE;

    /* Spawn hidden: model pointer 0 renders nothing. +0x30 gets the immutable
     * renderer context required by the clothed four-character display data;
     * no player initialization or behavior function is called. */
    object = func_8000DBF0_E7F0(slot->task, 0,
                                CLOTHED_CHARACTER_ANIM_CONTEXT,
                                remote->x, remote->y, remote->z,
                                0, slot->yaw, 0,
                                scale, scale, scale,
                                0, 0);
    if (!object)
    {
        /* Keep the linked task and retry object allocation next frame. The
         * parent owner will reclaim it if the scene tears down meanwhile. */
        return 0;
    }

    /* Ghidra: FUN_801CC30C writes object+0x05 = 2 immediately after assigning
     * the clothed player animation context. FUN_8000DBF0 and its free-list
     * allocator do not initialize this byte, so explicitly select the same
     * render-object mode before binding the selected character display data.
     * This is renderer state only; no playable task or player behavior is
     * invoked. */
    write_u8_at(object, 0x05, CLOTHED_CHARACTER_OBJECT_MODE);
    slot->object = object;
    hide_object(object);
    return 1;
}

static unsigned char *get_action_entry(int ch, int action)
{
    if (ch < 0 || ch >= CHARACTER_COUNT || action < 0 || action >= REMOTE_PLAYER_ACTION_MAX)
        return 0;
    /* Read only the model metadata record matching the received character and
     * action; its associated playable callback table is never accessed. */
    return D_80203F34_5BFE44[ch] + action * 0x1c;
}

static int remote_action_or_idle(int action)
{
    if (action >= 0 &&
        action < REMOTE_PLAYER_ACTION_MAX &&
        action != REMOTE_PLAYER_ACTION_CHARACTER_SWITCH)
    {
        return action;
    }
    return REMOTE_PLAYER_ACTION_IDLE;
}

static int bind_aux_resource(RemoteModelSlot *slot, int channel,
                             unsigned int segment, unsigned int resource)
{
    unsigned char *buffer;
    unsigned char *end;
    int index;

    if (resource == 0)
        return 1;
    if (!s_aux_arena || channel < 0 || channel >= AUX_RESOURCE_COUNT ||
        segment < 1 || segment > 5 || resource == 0xffu ||
        resource >= AUX_RESOURCE_ID_LIMIT)
        return 0;

    /* Every character using the same expression can share immutable pixels.
     * References prevent eviction while a model binds a page; retired pages
     * also survive the native renderer's two in-flight display lists. */
    index = mnsg_texture_cache_find(s_aux_cache, AUX_CACHE_PAGES, resource);
    if (index < 0)
    {
        int stored_size = func_80014698_15298(resource, 0);
        if (stored_size <= 0 || stored_size > (int)AUX_BUFFER_SIZE)
            return 0;
        index = mnsg_texture_cache_victim(s_aux_cache, AUX_CACHE_PAGES,
                                         D_800C7A78);
        if (index < 0)
            return 0;
        buffer = s_aux_arena + (unsigned int)index * AUX_BUFFER_SIZE;
        s_aux_cache[index].resource = 0;
        end = func_800145B4_151B4(resource, buffer);
        if (!end || end < buffer || end > buffer + AUX_BUFFER_SIZE)
            return 0;
        s_aux_cache[index].resource = resource;
    }
    buffer = s_aux_arena + (unsigned int)index * AUX_BUFFER_SIZE;
    if (slot->aux_resource_id[channel] != resource)
    {
        if (slot->aux_resource_id[channel])
            mnsg_texture_cache_release(&s_aux_cache[slot->aux_cache_index[channel]],
                                        D_800C7A78);
        ++s_aux_cache[index].references;
        slot->aux_cache_index[channel] = index;
        slot->aux_resource_id[channel] = resource;
    }
    write_u32_at(slot->object, 0x38 + segment * 8,
                 (unsigned int)(unsigned long)buffer);
    return 1;
}

/* Stage the two initial face/part resources exactly as FUN_801DAF54 does when
 * a new action is bound. Timed expression updates are handled separately. */
static int bind_initial_aux_resources(RemoteModelSlot *slot,
                                      unsigned char *entry)
{
    unsigned char *aux_table = *(unsigned char **)(entry + 0x18);
    int channel;

    if (!aux_table)
        return 1;
    for (channel = 0; channel < AUX_RESOURCE_COUNT; ++channel)
    {
        /* Mirror FUN_801DB180: channel zero begins at row zero; channel one
         * begins at the action record's +0x14 row. */
        unsigned int row = channel == 0 ? 0u : (unsigned int)*(entry + 0x14);
        unsigned int segment = D_80203FF0_5BFF00[channel * 2 + 1];
        unsigned int resource = *(unsigned int *)(aux_table + row * 8);

        if (!bind_aux_resource(slot, channel, segment, resource))
            return 0;
        slot->aux_cursor[channel] = 0;
        /* FUN_801DAF54 marks each channel so FUN_801DB060 skips exactly the
         * first per-frame update after an action-start resource bind. */
        slot->aux_skip_update[channel] = 1;
    }
    return 1;
}

/* Advance the two face/part cursors at most one row per rendered frame, as
 * FUN_801DB060/FUN_801DB1D4 do. Keeping cursor state is important when a peer
 * joins mid-animation: a stateless multi-row scan can leave the known action
 * sequence and submit an unrelated resource as an RT64 display list. */
static int sync_timed_aux_resources(RemoteModelSlot *slot,
                                    unsigned char *entry, float frame)
{
    unsigned char *aux_table = *(unsigned char **)(entry + 0x18);
    int rewound = frame < slot->aux_last_frame;
    int channel;

    if (!aux_table)
        return 1;
    for (channel = 0; channel < AUX_RESOURCE_COUNT; ++channel)
    {
        unsigned int base_row =
            channel == 0 ? 0u : (unsigned int)*(entry + 0x14);
        unsigned int cursor;
        unsigned int row;
        unsigned int resource;
        unsigned int segment;
        unsigned int threshold;

        if (slot->aux_skip_update[channel])
        {
            slot->aux_skip_update[channel] = 0;
            continue;
        }

        if (frame == 0.0f || rewound)
            slot->aux_cursor[channel] = 0;
        cursor = slot->aux_cursor[channel];
        if (cursor >= AUX_SEQUENCE_MAX_STEPS)
            return 0;

        row = base_row + cursor;
        threshold = *(aux_table + row * 8 + 4);
        if (frame != 0.0f && !rewound && (float)threshold > frame)
            continue;

        /* Stock advances no more than one record in a call, and a zero
         * threshold keeps the current record selected. */
        if (frame != 0.0f && !rewound && threshold != 0)
        {
            cursor++;
            if (cursor >= AUX_SEQUENCE_MAX_STEPS)
                return 0;
            slot->aux_cursor[channel] = (unsigned char)cursor;
            row = base_row + cursor;
        }

        resource = *(unsigned int *)(aux_table + row * 8);
        if (resource == 0xffu)
        {
            slot->aux_cursor[channel] = 0;
            resource = *(unsigned int *)(aux_table + base_row * 8);
        }
        /* Use the stock per-frame descriptor array because it can differ from
         * the action-start segment descriptor used by FUN_801DAF54. */
        segment = (unsigned int)(D_80203FF8_5BFF08[channel] & 0xffu);
        if (!bind_aux_resource(slot, channel, segment, resource))
            return 0;
    }

    slot->aux_last_frame = frame;
    return 1;
}

static int bind_action_model_data(RemoteModelSlot *slot, int ch, int action,
                                  int sudden_impact)
{
    CharacterModelCache *cache = &s_char_cache[ch];
    unsigned int offset;
    unsigned int size;
    unsigned int base;

    if (!sudden_impact)
    {
        write_u32_at(slot->object, 0x38,
                     (unsigned int)(unsigned long)cache->action);
        return 1;
    }

    /* func_8001C3E0 mutates display pointers in the bound model tree. The
     * ordinary raw action cache is shared by every remote slot, so applying
     * Sudden Impact there would turn unrelated Goemon players gold. Mirror
     * the stock player's per-action DMA into a slot-private buffer first. */
    if (!action_model_range(ch, action, &offset, &size) ||
        cache->max_action_model_size == 0 ||
        size > cache->max_action_model_size)
        return 0;
    if (!slot->private_action)
    {
        slot->private_action = alloc_aligned(cache->max_action_model_size);
        if (!slot->private_action)
            return 0;
        slot->private_action_size = cache->max_action_model_size;
    }
    if (size > slot->private_action_size)
        return 0;

    copy_bytes(slot->private_action, cache->action + offset, size);
    base = (unsigned int)(unsigned long)slot->private_action - offset;
    write_u32_at(slot->object, 0x38, base);
    return 1;
}

/* Bind a character/action to the slot's render object from the character
 * cache. Sudden Impact uses a private mutable action slice; ordinary models
 * keep using the shared pristine raw action file. */
static int bind_model(RemoteModelSlot *slot, int ch, int action,
                      int sudden_impact)
{
    CharacterModelCache *cache = &s_char_cache[ch];
    unsigned char *entry = get_action_entry(ch, action);
    short speed;

    if (!entry || !cache->ready || !cache->broad || !cache->action)
        return 0;

    sudden_impact = ch == CHARACTER_GOEMON && sudden_impact != 0;

    /* Segment 8: action model file base. Ordinary models use the whole shared
     * file; the mutable Sudden Impact variant uses a slot-private slice. */
    if (!bind_action_model_data(slot, ch, action, sudden_impact))
        return 0;
    /* Segment 9: broad character file id + base (object+0x3c/+0x40). */
    write_u16_at(slot->object, 0x3c, D_80204020_5BFF30[ch]);
    write_u32_at(slot->object, 0x40, (unsigned int)(unsigned long)cache->broad);

    write_u32_at(slot->object, 0x2c,
                 *(unsigned int *)(entry + 0x00) + PLAYER_MODEL_RENDER_SEGMENT);
    if (sudden_impact &&
        !func_8001C3E0_1CFE0(slot->object, 0, D_80204048_5BFF58[1]))
        return 0;

    if (!bind_initial_aux_resources(slot, entry))
        return 0;

    speed = *(short *)(entry + 0x04);
    slot->native_frame_step = (float)speed / 100.0f;
    slot->frame_step = slot->native_frame_step;
    slot->frame = 0.0f;
    anchor_remote_animation_reset(&slot->animation);
    slot->aux_last_frame = 0.0f;
    write_float_at(slot->object, 0x28, 0.0f);

    slot->bound_ch = ch;
    slot->bound_action = action;
    slot->bound_sudden_impact = sudden_impact;
    show_object(slot->object);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Pose / animation                                                   */
/* ------------------------------------------------------------------ */

static short yaw_from_velocity(int vx, int vz, short fallback)
{
    int avx;
    int avz;

    if (vx * vx + vz * vz < REMOTE_YAW_SPEED_THRESHOLD_SQ)
        return fallback;

    avx = vx < 0 ? -vx : vx;
    avz = vz < 0 ? -vz : vz;

    if (avx > avz * 2)
        return vx >= 0 ? 0x4000 : (short)0xc000;
    if (avz > avx * 2)
        return vz >= 0 ? (short)0x8000 : 0;
    if (vx >= 0 && vz >= 0)
        return 0x6000;
    if (vx < 0 && vz >= 0)
        return (short)0xa000;
    if (vx < 0 && vz < 0)
        return (short)0xe000;
    return 0x2000;
}

static float remote_model_scale(const AnchorPlayerModelRemote *remote,
                                float frame)
{
    float delta;

    /* Derive Mini Ebisumaru's live scale locally from one appearance bit plus
     * the synchronized stock action/frame. FUN_801F5734 uses these same
     * thresholds and quadratic coefficient for actions 0x8E and 0x8F. */
    if (!remote || remote->ch != CHARACTER_EBISUMARU ||
        !(remote->appearance_flags & ANCHOR_APPEARANCE_MINI_EBISUMARU))
        return REMOTE_MODEL_SCALE;

    if (remote->action == REMOTE_PLAYER_ACTION_MINI_SHRINK)
    {
        if (frame <= MINI_SCALE_START_FRAME)
            return REMOTE_MODEL_SCALE;
        if (frame < MINI_SCALE_END_FRAME)
        {
            delta = frame - MINI_SCALE_END_FRAME;
            return delta * delta * MINI_SCALE_CURVE + MINI_MODEL_SCALE;
        }
    }
    else if (remote->action == REMOTE_PLAYER_ACTION_MINI_GROW)
    {
        if (frame <= MINI_SCALE_START_FRAME)
            return MINI_MODEL_SCALE;
        if (frame < MINI_SCALE_END_FRAME)
        {
            delta = frame - MINI_SCALE_START_FRAME;
            return delta * delta * MINI_SCALE_CURVE + MINI_MODEL_SCALE;
        }
        return REMOTE_MODEL_SCALE;
    }
    return MINI_MODEL_SCALE;
}

static AnchorCollisionVec3 object_position(const void *object)
{
    const float *position = (const float *)((const unsigned char *)object + 8);
    AnchorCollisionVec3 result = {position[0], position[1], position[2]};
    return result;
}

static void set_object_position(void *object, AnchorCollisionVec3 position)
{
    write_float_at(object, 0x08, position.x);
    write_float_at(object, 0x0c, position.y);
    write_float_at(object, 0x10, position.z);
}

static AnchorCollisionBody collision_body_at(AnchorCollisionVec3 position,
                                             int ch, float scale)
{
    AnchorCollisionBody body;
    body.position = position;
    body.radius = (float)D_801FC660_5B8570[ch] * scale;
    body.height = (float)D_801FC668_5B8578[ch] * scale;
    return body;
}

static int collect_collision_peers(const RemoteModelSlot *self,
                                   AnchorCollisionBody *bodies)
{
    int i;
    int count = 0;
    if (s_owner_task != D_801FC604_5B8514 ||
        !is_linked_task(s_owner_task) || !anchor_is_connected())
        return 0;
    for (i = 0; i < s_slot_capacity; ++i)
    {
        RemoteModelSlot *peer = &s_slots[i];
        if (peer == self || !peer->active || !peer->pending_valid ||
            peer->pending_room != D_800C7AB2 ||
            !peer->collision_ready || peer->pending_remote.collision_disabled ||
            !is_linked_remote_task(peer->task))
            continue;
        bodies[count++] = peer->collision_body;
    }
    return count;
}

static int local_collision_body(AnchorCollisionBody *body, float *scale)
{
    void *task = D_801FC604_5B8514;
    void *object = D_801FC60C_5B851C;
    unsigned int ch;
    if (!is_linked_task(task) || !is_rdram_pointer(object) ||
        *(void **)((unsigned char *)task + 0x18) != object)
        return 0;
    ch = *(unsigned char *)((unsigned char *)task + 0x60);
    *scale = *(float *)((unsigned char *)object + 0x1c);
    if (ch >= CHARACTER_COUNT || !(*scale > 0.0f && *scale <= 1.0f))
        return 0;
    *body = collision_body_at(object_position(object), (int)ch, *scale);
    return 1;
}

/* A new owner, room, death or scripted-control transition invalidates queued
 * hits. Refresh at both the native movement boundary and the publisher, so
 * an old event cannot affect a newly created player at the same address. */
int anchor_player_models_get_epoch(void)
{
    AnchorCollisionBody body;
    float scale;
    void *task = 0;
    void *object = 0;
    int alive = 0;
    int scripted = anchor_remote_collision_is_scripted();
    if (anchor_is_connected() && local_collision_body(&body, &scale))
    {
        unsigned char *work;
        task = D_801FC604_5B8514;
        object = D_801FC60C_5B851C;
        work = *(unsigned char **)((unsigned char *)task + 0x5c);
        alive = is_rdram_pointer(work) && work[0x69] == 0 &&
                item_sync_save_is_loaded() && item_sync_local_player_health() > 0;
    }
    if (task != s_interaction_task || object != s_interaction_object ||
        D_800C7AB2 != s_interaction_room ||
        scripted != s_interaction_scripted || alive != s_interaction_alive)
    {
        s_player_epoch = s_player_epoch == 0x7fffffff ? 1 : s_player_epoch + 1;
        s_interaction_task = task;
        s_interaction_object = object;
        s_interaction_room = D_800C7AB2;
        s_interaction_scripted = scripted;
        s_interaction_alive = alive;
        s_drive_x = s_drive_z = 0;
    }
    return s_player_epoch;
}

int anchor_player_models_peek_epoch(void)
{
    return s_player_epoch;
}

int anchor_player_models_peek_scripted(void)
{
    return s_interaction_scripted;
}

void anchor_player_models_get_drive(int *x, int *z)
{
    (void)anchor_player_models_get_epoch();
    *x = s_interaction_tick - s_drive_tick <= 1u ? s_drive_x : 0;
    *z = s_interaction_tick - s_drive_tick <= 1u ? s_drive_z : 0;
}

int anchor_player_models_peer_is_current(int cid, int session, int epoch)
{
    int i;
    if (cid <= 0 || session <= 0 || epoch <= 0 ||
        s_owner_task != D_801FC604_5B8514 || !is_linked_task(s_owner_task))
        return 0;
    for (i = 0; i < s_slot_capacity; ++i)
    {
        const RemoteModelSlot *slot = &s_slots[i];
        if (slot->active && slot->cid == cid && slot->pending_valid &&
            slot->pending_room == D_800C7AB2 &&
            slot->pending_remote.interaction_session == session &&
            slot->pending_remote.player_epoch == epoch)
            return 1;
    }
    return 0;
}

int anchor_player_models_get_hit_targets(AnchorPlayerHitTarget *out, int capacity)
{
    int i;
    int count = 0;
    (void)anchor_player_models_get_epoch();
    if (!out || capacity <= 0 || !s_interaction_alive ||
        s_interaction_scripted || !anchor_is_connected() ||
        s_owner_task != D_801FC604_5B8514 || !is_linked_task(s_owner_task))
        return 0;
    for (i = 0; i < s_slot_capacity && count < capacity; ++i)
    {
        RemoteModelSlot *slot = &s_slots[i];
        if (!slot->active || !slot->pending_valid || !slot->collision_ready ||
            slot->pending_room != D_800C7AB2 ||
            slot->pending_remote.collision_disabled ||
            slot->pending_remote.player_epoch <= 0 ||
            slot->pending_remote.interaction_session <= 0 ||
            !is_linked_remote_task(slot->task))
            continue;
        out[count].cid = slot->cid;
        out[count].epoch = slot->pending_remote.player_epoch;
        out[count++].body = slot->collision_body;
    }
    return count;
}

static void consider_boss_target(const AnchorBossTarget *candidate,
                                 int current_cid, AnchorBossTarget *current,
                                 AnchorBossTarget *next, AnchorBossTarget *first)
{
    if (candidate->cid <= 0)
        return;
    if (candidate->cid == current_cid)
        *current = *candidate;
    if (!first->cid || candidate->cid < first->cid)
        *first = *candidate;
    if (candidate->cid > current_cid &&
        (!next->cid || candidate->cid < next->cid))
        *next = *candidate;
}

int anchor_player_models_get_boss_target(int current_cid, int rotate,
                                         AnchorBossTarget *out)
{
    AnchorBossTarget current = {0}, next = {0}, first = {0};
    AnchorBossTarget candidate;
    AnchorCollisionBody body;
    float scale;
    int i;
    (void)anchor_player_models_get_epoch();
    if (!out || !anchor_is_connected() || !item_sync_save_is_loaded() ||
        s_owner_task != D_801FC604_5B8514 || !is_linked_task(s_owner_task))
        return 0;
    if (s_interaction_alive && !s_interaction_scripted &&
        local_collision_body(&body, &scale))
    {
        candidate.cid = (int)anchor_get_client_id();
        candidate.x = body.position.x;
        candidate.y = body.position.y;
        candidate.z = body.position.z;
        consider_boss_target(&candidate, current_cid, &current, &next, &first);
    }
    for (i = 0; i < s_slot_capacity; ++i)
    {
        const RemoteModelSlot *slot = &s_slots[i];
        if (!slot->active || !slot->pending_valid || !slot->collision_ready ||
            slot->pending_room != D_800C7AB2 ||
            !slot->pending_remote.same_team ||
            slot->pending_remote.collision_disabled ||
            slot->pending_remote.player_epoch <= 0 ||
            slot->pending_remote.interaction_session <= 0 ||
            s_interaction_tick - slot->drive_sample_tick > 60u ||
            !is_linked_remote_task(slot->task))
            continue;
        candidate.cid = slot->cid;
        candidate.x = slot->collision_body.position.x;
        candidate.y = slot->collision_body.position.y;
        candidate.z = slot->collision_body.position.z;
        consider_boss_target(&candidate, current_cid, &current, &next, &first);
    }
    *out = !rotate && current.cid ? current : next.cid ? next : first;
    return out->cid != 0;
}

int anchor_player_models_get_boss_targets(AnchorBossTarget *out, int capacity)
{
    AnchorCollisionBody body;
    float scale;
    int count = 0;
    int i;
    (void)anchor_player_models_get_epoch();
    if (!out || capacity <= 0 || !anchor_is_connected() ||
        !item_sync_save_is_loaded() ||
        s_owner_task != D_801FC604_5B8514 || !is_linked_task(s_owner_task))
        return 0;
    if (s_interaction_alive && !s_interaction_scripted &&
        local_collision_body(&body, &scale))
    {
        out[count].cid = (int)anchor_get_client_id();
        out[count].x = body.position.x;
        out[count].y = body.position.y;
        out[count].z = body.position.z;
        ++count;
    }
    for (i = 0; i < s_slot_capacity && count < capacity; ++i)
    {
        const RemoteModelSlot *slot = &s_slots[i];
        if (!slot->active || !slot->pending_valid || !slot->collision_ready ||
            slot->pending_room != D_800C7AB2 ||
            !slot->pending_remote.same_team ||
            slot->pending_remote.collision_disabled ||
            slot->pending_remote.player_epoch <= 0 ||
            slot->pending_remote.interaction_session <= 0 ||
            s_interaction_tick - slot->drive_sample_tick > 60u ||
            !is_linked_remote_task(slot->task))
            continue;
        out[count].cid = slot->cid;
        out[count].x = slot->collision_body.position.x;
        out[count].y = slot->collision_body.position.y;
        out[count].z = slot->collision_body.position.z;
        ++count;
    }
    return count;
}

static void receive_player_hits(void)
{
    int sender;
    int epoch;
    int i;
    float x, y, z;
    int current_epoch = anchor_player_models_get_epoch();
    /* Drain even rejected hits instead of retaining them until control
     * resumes. The transport also bounds the queue by age and peer session. */
    for (i = 0; i < 16 && anchor_poll_player_hit(&sender, &epoch, &x, &y, &z); ++i)
        if (epoch == current_epoch && s_interaction_alive &&
            !s_interaction_scripted)
            (void)anchor_player_damage_apply(x, y, z);
}

/* Apply an accepted network hit at the real player's normal pre-update
 * boundary, before animation/action dispatch. The intake helper preserves
 * native environment-hit priority and hurt/armour/projectile cleanup. */
RECOMP_HOOK("func_801CB824_587734")
void anchor_player_interactions_before_update(void *task, void *object)
{
    if (task == D_801FC604_5B8514 && object == D_801FC60C_5B851C)
        receive_player_hits();
}

static AnchorCollisionVec3 incoming_player_push(const AnchorCollisionBody *body)
{
    AnchorCollisionVec3 total = {0.0f, 0.0f, 0.0f};
    float length_squared;
    int i;
    for (i = 0; i < s_slot_capacity; ++i)
    {
        RemoteModelSlot *slot = &s_slots[i];
        AnchorCollisionVec3 push;
        if (!slot->active || !slot->pending_valid || !slot->collision_ready ||
            slot->pending_room != D_800C7AB2 ||
            slot->pending_remote.collision_disabled ||
            slot->pending_remote.player_epoch <= 0 ||
            slot->pending_remote.interaction_session <= 0 ||
            s_interaction_tick - slot->drive_sample_tick > 12u ||
            !is_linked_remote_task(slot->task))
            continue;
        push = anchor_collision_push(body, &slot->collision_body,
                                      (float)slot->pending_remote.drive_x / 3000.0f,
                                      (float)slot->pending_remote.drive_z / 3000.0f);
        total.x += push.x;
        total.z += push.z;
    }
    /* A crowded room cannot multiply the per-frame push limit. */
    length_squared = total.x * total.x + total.z * total.z;
    if (length_squared > 2.25f)
    {
        float factor = 1.5f / __builtin_sqrtf(length_squared);
        total.x *= factor;
        total.z *= factor;
    }
    return total;
}

static int resolve_slot_collision(RemoteModelSlot *slot,
                                    const AnchorPlayerModelRemote *remote,
                                    float scale)
{
    AnchorCollisionVec3 target = {remote->x, remote->y, remote->z};
    AnchorCollisionVec3 position;
    AnchorCollisionVec3 from = slot->collision_ready ?
        slot->collision_body.position : target;
    AnchorCollisionBody *peers;
    AnchorCollisionBody moving = collision_body_at(from, remote->ch, scale);
    AnchorCollisionBody local;
    float local_scale;
    int count;

    if (remote->collision_disabled || anchor_remote_collision_is_scripted() ||
        !anchor_is_connected())
    {
        /* Clear the old contact baseline. Resuming control must not sweep
         * back through the entire scripted path to its former position. */
        slot->collision_ready = 0;
        set_object_position(slot->object, target);
        return 1;
    }

    if (!mnsg_array_reserve((void **)&s_collision_peers,
                            &s_collision_peer_capacity, s_slot_capacity + 1,
                            sizeof(*s_collision_peers)))
    {
        slot->collision_ready = 0;
        hide_object(slot->object);
        slot->bound_action = -1;
        return 0;
    }
    peers = s_collision_peers;
    count = collect_collision_peers(slot, peers);
    if (local_collision_body(&local, &local_scale))
        peers[count++] = local;
    count = anchor_collision_append_enemies(&s_collision_peers,
                                             &s_collision_peer_capacity, count);
    peers = s_collision_peers;
    if (count < 0 || !anchor_collision_move_actors(&moving, &target, peers, count,
                                                 &position))
    {
        /* A new body with no available space must not become an invisible
         * obstacle or be displayed inside another player. Retry the binding
         * and placement on the next scheduled update as space opens. */
        slot->collision_ready = 0;
        hide_object(slot->object);
        slot->bound_action = -1;
        return 0;
    }
    slot->collision_body = collision_body_at(position, remote->ch, scale);
    slot->collision_ready = 1;
    set_object_position(slot->object, position);
    return 1;
}

/* Capture before native late movement and resolve contact after it. The real
 * player's own wall/floor/action logic still runs exactly once. These hooks
 * add only contact with visible remote bodies, before frame-end publishing. */
static void *s_collision_local_task;
static void *s_collision_local_object;
static AnchorCollisionBody s_collision_local_body;
static float s_collision_local_scale;
static unsigned short s_collision_local_room;

RECOMP_HOOK("func_801CBAF8_587A08")
void anchor_collision_before_local_movement(void *task)
{
    s_collision_local_task = 0;
    if (task != D_801FC604_5B8514)
        return;
    s_drive_x = s_drive_z = 0;
    (void)anchor_player_models_get_epoch();
    if (!anchor_is_connected() || !s_interaction_alive ||
        anchor_remote_collision_is_scripted() ||
        !local_collision_body(&s_collision_local_body, &s_collision_local_scale))
        return;
    s_collision_local_task = task;
    s_collision_local_object = D_801FC60C_5B851C;
    s_collision_local_room = D_800C7AB2;
}

RECOMP_HOOK_RETURN("func_801CBAF8_587A08")
void anchor_collision_after_local_movement(void)
{
    AnchorCollisionBody *peers;
    AnchorCollisionVec3 target;
    AnchorCollisionVec3 native_target;
    AnchorCollisionVec3 push;
    AnchorCollisionVec3 contact;
    AnchorCollisionVec3 resolved;
    int count;
    if (!s_collision_local_task)
        return;
    if (s_collision_local_task != D_801FC604_5B8514 ||
        s_collision_local_object != D_801FC60C_5B851C ||
        s_collision_local_room != D_800C7AB2 ||
        !is_linked_task(s_collision_local_task) ||
        anchor_remote_collision_is_scripted())
    {
        s_collision_local_task = 0;
        return;
    }
    s_collision_local_task = 0;
    native_target = object_position(s_collision_local_object);
    /* Publish native attempted travel before player contact and before the
     * incoming pressure below. Otherwise a stationary blocked sender loses
     * its push, or a received push feeds back as voluntary pressure. */
    {
        float dx = native_target.x - s_collision_local_body.position.x;
        float dz = native_target.z - s_collision_local_body.position.z;
        s_drive_tick = s_interaction_tick;
        if (dx * dx + dz * dz <= 100.0f)
        {
            s_drive_x = (int)(dx * 3000.0f);
            s_drive_z = (int)(dz * 3000.0f);
        }
    }
    if (!mnsg_array_reserve((void **)&s_collision_peers,
                            &s_collision_peer_capacity, s_slot_capacity + 1,
                            sizeof(*s_collision_peers)))
        return;
    peers = s_collision_peers;
    count = collect_collision_peers(0, peers);
    if (!count)
        return;
    target = native_target;
    push = incoming_player_push(&s_collision_local_body);
    target.x += push.x;
    target.z += push.z;
    anchor_collision_move_peers(&s_collision_local_body, &target,
                                peers, count, &contact);
    if (push.x == 0.0f && push.z == 0.0f &&
        contact.x == target.x && contact.y == target.y && contact.z == target.z)
        return;
    if (!anchor_collision_move_body(&s_collision_local_body, &target,
                                    s_collision_local_scale, peers, count,
                                    &resolved))
        return;
    {
        /* Native func_801CD084 moves the primary and its following display
         * object; func_801CF3A0 maintains the shadow in the same three-record
         * chain. Keep those attachments aligned with a late contact fix. */
        void *object = s_collision_local_object;
        int i;
        for (i = 0; i < 3 && is_rdram_pointer(object); ++i)
        {
            AnchorCollisionVec3 position = object_position(object);
            position.x += resolved.x - native_target.x;
            position.y += resolved.y - native_target.y;
            position.z += resolved.z - native_target.z;
            set_object_position(object, position);
            object = *(void **)object;
        }
    }
}

static void update_slot_pose(RemoteModelSlot *slot, const AnchorPlayerModelRemote *remote,
                             int action_changed)
{
    unsigned char *entry;
    float frame_count;
    int new_remote_packet =
        remote->new_motion_sample ||
        remote->seq != slot->last_seq ||
        remote->anim_frame_100 != slot->last_remote_frame_100 ||
        remote->anim_frame_count_100 !=
            slot->last_remote_frame_count_100 ||
        remote->anim_step_100 != slot->last_remote_anim_step_100 ||
        remote->has_anim_step != slot->last_remote_has_anim_step;
    int use_remote_anim = remote->action == slot->bound_action &&
                          remote->action >= 0 &&
                          remote->action < REMOTE_PLAYER_ACTION_MAX;

    if (!use_remote_anim)
        slot->yaw = yaw_from_velocity(remote->vx, remote->vz, slot->yaw);

    /* Resolve the bound clip length through the engine because its model
     * command pointer is segmented and cannot be dereferenced directly. */
    frame_count = func_8001B5AC_1C1AC(slot->object);

    if (use_remote_anim)
    {
        AnchorRemoteAnimationInput animation_input;
        AnchorRemoteAnimationOutput animation_output;

        animation_input.action = remote->action;
        animation_input.seq = remote->seq;
        animation_input.new_sample = new_remote_packet || action_changed;
        animation_input.source_frame_count =
            (float)remote->anim_frame_count_100 / 100.0f;
        animation_input.target_frame_count = frame_count;
        animation_input.target_frame =
            (float)remote->anim_frame_100 / 100.0f;
        animation_input.endpoint_step_valid = remote->has_anim_step;
        animation_input.endpoint_step =
            (float)remote->anim_step_100 / 100.0f;
        animation_input.native_step = slot->native_frame_step;
        animation_input.root_phase_lead_frames =
            remote->motion_phase_frames;
        anchor_remote_animation_step(&slot->animation, &animation_input,
                                     &animation_output);
        slot->frame = animation_output.frame;
        slot->frame_step = animation_output.playback_step;
    }
    else
    {
        slot->frame += slot->native_frame_step;
        slot->frame_step = slot->native_frame_step;
    }

    if (frame_count > 1.0f)
    {
        while (slot->frame >= frame_count)
            slot->frame -= frame_count;
        while (slot->frame < 0.0f)
            slot->frame += frame_count;
    }
    else
    {
        slot->frame = 0.0f;
    }

    entry = get_action_entry(slot->bound_ch, slot->bound_action);
    if (!entry || !sync_timed_aux_resources(slot, entry, slot->frame))
    {
        /* Keep a model with incomplete face segments hidden so corrupt display
         * data is never submitted while an aux load or table check fails. */
        hide_object(slot->object);
        slot->bound_action = -1;
        slot->bound_sudden_impact = 0;
        return;
    }

    {
        float scale = remote_model_scale(remote, slot->frame);

        if (!resolve_slot_collision(slot, remote, scale))
            return;
        write_float_at(slot->object, 0x1c, scale);
        write_float_at(slot->object, 0x20, scale);
        write_float_at(slot->object, 0x24, scale);
    }
    if (use_remote_anim)
    {
        write_u16_at(slot->object, 0x14, (unsigned short)remote->rot_x);
        write_u16_at(slot->object, 0x16, (unsigned short)remote->rot_y);
        write_u16_at(slot->object, 0x18, (unsigned short)remote->rot_z);
        slot->yaw = (short)remote->rot_y;
    }
    else
    {
        write_u16_at(slot->object, 0x14, 0);
        write_u16_at(slot->object, 0x16, (unsigned short)slot->yaw);
        write_u16_at(slot->object, 0x18, 0);
    }
    write_float_at(slot->object, 0x28, slot->frame);
    /* Flicker is a render flag only. Retiring the object with hide_object
     * would incorrectly remove its collider on every hidden recovery frame.
     * Animation, nameplates and collision continue through both phases. */
    anchor_remote_appearance_apply_hurt(slot->object,
        remote->appearance_flags, D_800C7A78);
    slot->last_seq = remote->seq;
    slot->last_remote_frame_100 = remote->anim_frame_100;
    slot->last_remote_frame_count_100 =
        remote->anim_frame_count_100;
    slot->last_remote_anim_step_100 = remote->anim_step_100;
    slot->last_remote_has_anim_step = remote->has_anim_step;
}

static void update_slot_hidden_pose(RemoteModelSlot *slot, const AnchorPlayerModelRemote *remote)
{
    if (!slot->object)
        return;

    write_float_at(slot->object, 0x08, remote->x);
    write_float_at(slot->object, 0x0c, remote->y);
    write_float_at(slot->object, 0x10, remote->z);
    {
        float scale = remote_model_scale(
            remote, (float)remote->anim_frame_100 / 100.0f);

        write_float_at(slot->object, 0x1c, scale);
        write_float_at(slot->object, 0x20, scale);
        write_float_at(slot->object, 0x24, scale);
    }
}

/* Apply the queued network snapshot from the cutscene-style child task's
 * scheduled update. This matches the stock ordering: model pointers and aux
 * face memory are finalized before the engine walks kind-2 records to build
 * the frame's display list. The frame-end hook only publishes snapshots. */
static void remote_model_task_update(void *task, void *object)
{
    RemoteModelSlot *slot = 0;
    const AnchorPlayerModelRemote *remote;
    int ch;
    int action;
    int i;

    (void)object;
    for (i = 0; i < s_slot_capacity; ++i)
    {
        if (s_slots[i].active && s_slots[i].task == task)
        {
            slot = &s_slots[i];
            break;
        }
    }
    if (!slot || !slot->object || !slot->pending_valid)
        return;
    if (slot->pending_room != D_800C7AB2 ||
        s_owner_task != D_801FC604_5B8514)
    {
        hide_object(slot->object);
        slot->pending_valid = 0;
        slot->bound_ch = -1;
        slot->bound_action = -1;
        slot->bound_sudden_impact = 0;
        return;
    }

    /* Plain render children need not carry the native gameplay pause flag.
     * Preserve their last pose while the owned modal pauses the world, after
     * allowing stale owner/room handles to be hidden above. Rendering still
     * walks the existing kind-2 object normally. */
    if (anchor_dialog_world_paused())
        return;

    remote = &slot->pending_remote;
    ch = remote->ch;
    if (remote->cid <= 0 || ch < 0 || ch >= CHARACTER_COUNT)
    {
        hide_object(slot->object);
        slot->bound_ch = -1;
        slot->bound_action = -1;
        slot->bound_sudden_impact = 0;
        return;
    }

    action = remote_action_or_idle(remote->action);
    if (!s_char_cache[ch].ready)
    {
        if (slot->bound_ch != ch)
        {
            hide_object(slot->object);
            slot->bound_ch = -1;
            slot->bound_action = -1;
            slot->bound_sudden_impact = 0;
        }
        update_slot_hidden_pose(slot, remote);
        return;
    }

    if (slot->bound_ch != ch || slot->bound_action != action ||
        slot->bound_sudden_impact !=
            (ch == CHARACTER_GOEMON &&
             (remote->appearance_flags &
              ANCHOR_APPEARANCE_SUDDEN_IMPACT) != 0))
    {
        if (!bind_model(slot, ch, action,
                        remote->appearance_flags &
                            ANCHOR_APPEARANCE_SUDDEN_IMPACT))
        {
            hide_object(slot->object);
            slot->bound_ch = -1;
            slot->bound_action = -1;
            slot->bound_sudden_impact = 0;
            update_slot_hidden_pose(slot, remote);
            return;
        }
        update_slot_pose(slot, remote, 1);
        return;
    }

    update_slot_pose(slot, remote, 0);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

void anchor_player_models_update(const AnchorPlayerModelRemote *remotes, int count,
                                 void *render_parent_task)
{
    int i;
    ++s_interaction_tick;
    (void)anchor_player_models_get_epoch();

    if (!is_linked_task(render_parent_task))
    {
        anchor_player_models_reset();
        return;
    }

    if (s_owner_task != render_parent_task)
    {
        anchor_player_models_reset();
        s_owner_task = render_parent_task;
    }

    for (i = 0; i < s_slot_capacity; ++i)
        s_slots[i].seen = 0;

    for (i = 0; remotes && i < count; ++i)
    {
        const AnchorPlayerModelRemote *remote = &remotes[i];
        RemoteModelSlot *slot;

        if (remote->cid <= 0 || remote->ch < 0 || remote->ch >= CHARACTER_COUNT)
            continue;

        slot = find_slot(remote->cid);
        if (!slot)
            slot = alloc_slot(remote->cid);
        if (!slot)
            continue; /* Heap exhaustion: retry this peer next frame. */

        slot->seen = 1;
        if (!ensure_slot_task(slot, remote, render_parent_task))
            continue;
        /* Queue only plain network state here. The task callback consumes the
         * newest complete snapshot at the engine's safe pre-render point. */
        if (!slot->pending_valid || remote->seq != slot->pending_remote.seq ||
            remote->player_epoch != slot->pending_remote.player_epoch ||
            remote->interaction_session != slot->pending_remote.interaction_session)
            slot->drive_sample_tick = s_interaction_tick;
        if (slot->pending_room != D_800C7AB2 ||
            remote->player_epoch != slot->pending_remote.player_epoch ||
            remote->interaction_session != slot->pending_remote.interaction_session)
            slot->collision_ready = 0;
        slot->pending_room = D_800C7AB2;
        slot->pending_remote = *remote;
        slot->pending_valid = 1;
    }

    for (i = 0; i < s_slot_capacity; ++i)
    {
        if (s_slots[i].active && !s_slots[i].seen)
            clear_slot_state(&s_slots[i], 1);
    }
}
