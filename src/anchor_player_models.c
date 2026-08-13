/**
 * @file anchor_player_models.c
 * @brief Render remote Goemon/Ebisumaru with cutscene-style model tasks.
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
 * This file uses that task/object architecture, then binds immutable Goemon
 * and Ebisumaru render assets and action records so the clothed model and the
 * sender's exact action clip can be displayed. It never calls a playable
 * character constructor, playable action callback, or player-manager update.
 * The action files are cached whole in mod memory; the broad character files
 * are registered by the normal scene resource loader.
 *
 * Object segment binding (matches the player object layout):
 *   +0x38 action file base (segment 8; action model pointers are file-
 *         relative, so the base is simply the cached file)
 *   +0x3c broad file id / +0x40 broad file base (segment 9)
 *   +0x50 / +0x58 aux face/part resources (segments from D_80203FF0),
 *         loaded per action with FUN_800145B4 into double buffers
 *   +0x2c action record model pointer | 0x60000000
 */

#include "anchor_player_models.h"
#include "modding.h"
#include "recomputils.h"

#define REMOTE_PLAYER_ACTION_IDLE 0
#define REMOTE_PLAYER_ACTION_MAX 0xe8
#define REMOTE_PLAYER_ACTION_CHARACTER_SWITCH 0xba
#define PLAYER_MODEL_RENDER_SEGMENT 0x60000000u
#define CLOTHED_CHARACTER_ANIM_CONTEXT 0xc01fc680u
#define REMOTE_YAW_SPEED_THRESHOLD_SQ 64
#define REMOTE_MODEL_SCALE 0.1f
#define REMOTE_FRAME_SNAP_THRESHOLD 4.0f
#define REMOTE_PACKET_FRAME_INTERVAL 2.0f

#define REMOTE_MODEL_SLOT_COUNT ANCHOR_PLAYER_MODEL_MAX
#define CHARACTER_COUNT 2
#define AUX_BUFFER_SIZE 0x2000u
#define AUX_RESOURCE_COUNT 2
#define AUX_FLIP_COUNT 2
#define BUFFER_ALIGN 16u

typedef struct CharacterModelCache
{
    int ready;
    unsigned char *broad;       /* resident broad file from the scene registry */
    unsigned char *action;      /* whole action-model file, raw ROM image */
    unsigned int action_size;
} CharacterModelCache;

typedef struct RemoteModelSlot
{
    int active;
    int cid;
    int seen;
    int bound_ch;     /* character currently bound to the object, -1 if none */
    int bound_action; /* action currently bound, -1 if none */
    int last_seq;
    int last_remote_frame_100;
    int last_remote_frame_count_100;
    short yaw;
    float frame;
    float frame_step;
    void *task;
    void *object;
    unsigned char *aux_buffer[AUX_RESOURCE_COUNT][AUX_FLIP_COUNT];
    unsigned int aux_resource_id[AUX_RESOURCE_COUNT];
    int aux_flip[AUX_RESOURCE_COUNT];
} RemoteModelSlot;

/* Allocate and insert an engine task under `task_list` with an update
 * callback. Same allocator the opening cutscene uses for its scene tasks. */
extern void *func_80034E08_35A08(void *task_list, void (*update)(void *, void *),
                                 unsigned short flags);

/* Delete a live cutscene-style task and its attached render records. The
 * remote renderer uses this when a peer leaves while the owner task is live. */
extern void func_80034EF8_35AF8(void *task);

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

/* Blocking DMA copy from ROM. The action-model file is stored raw (the stock
 * func_801DC70C DMAs ranges of it directly), so the whole file is copied. */
extern void func_80001640_2240(unsigned int rom_addr, void *dst, unsigned int size);

/* ROM start / end address of a file id. */
extern unsigned int func_80001D68_2968(unsigned int file_id);
extern unsigned int func_80001D94_2994(unsigned int file_id);

/* Load a compressed resource by id into dst; returns end pointer. Used for
 * the per-action aux face/part resources (like func_801DC87C). */
extern unsigned char *func_800145B4_151B4(unsigned int resource_id, void *dst);

/* Frame count of the object's currently bound model; used to wrap the
 * remote animation frame the same way the stock player update does. */
extern float func_8001B5AC_1C1AC(void *object);

/* Immutable per-character action record arrays (0x1C-byte records): +0x00 model
 * command pointer, +0x04 anim speed *100, +0x0c/+0x10 model data range,
 * +0x14 aux selector byte, +0x18 aux resource table. */
extern unsigned char *D_80203F34_5BFE44[];

/* Aux staging descriptor bytes: two (?, segment) pairs; the segment bytes
 * pick the object segment base at object + segment*8 + 0x38. */
extern unsigned char D_80203FF0_5BFF00[];

/* Per-character broad character-resource file ids. Only Goemon/Ebisumaru
 * entries are read to stage the correct clothed render data. */
extern unsigned short D_80204020_5BFF30[];

/* Per-character raw action-model file ids. Only Goemon/Ebisumaru entries are
 * copied into mod-owned render caches. */
extern unsigned short D_80204028_5BFF38[];

/* DMA mode byte the stock action copy (func_801DC70C) forces to 1 around
 * its ROM DMA and then restores; mirrored here for the whole-file copy.
 * Symbol D_8015C5D4_15D1D4 lives in the ABSOLUTE_SYMS pseudo-section, so it
 * is addressed directly. */
#define STOCK_DMA_MODE (*(volatile unsigned char *)0x8015C5D4)

static CharacterModelCache s_char_cache[CHARACTER_COUNT];
static RemoteModelSlot s_slots[REMOTE_MODEL_SLOT_COUNT];
static void *s_owner_task;

static void remote_model_task_update(void *task, void *object)
{
    (void)task;
    (void)object;
}

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

    return addr != 0 && phys < 0x00800000u;
}

static unsigned char *alloc_aligned(unsigned int size)
{
    unsigned int addr = (unsigned int)(unsigned long)recomp_alloc(size + BUFFER_ALIGN);

    if (addr == 0)
        return 0;
    return (unsigned char *)(unsigned long)((addr + (BUFFER_ALIGN - 1u)) & ~(BUFFER_ALIGN - 1u));
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
    return 1;
}

void anchor_player_models_load_resources(void)
{
    int ch;

    for (ch = 0; ch < CHARACTER_COUNT; ++ch)
    {
        CharacterModelCache *cache = &s_char_cache[ch];
        /* Use the immutable broad-file table to keep Goemon and Ebisumaru
         * resources independent of the local player's selected character. */
        unsigned int broad_id = D_80204020_5BFF30[ch];

        cache->ready = 0;
        cache->broad = 0;

        /* Use the scene loader during the stage-load return hook so Goemon's
         * and Ebisumaru's clothed broad render resources are resident before
         * any per-frame remote task runs. */
        func_80013B14_14714(broad_id);
        cache->broad = resident_resource_base(broad_id);
        if (!cache->broad || !cache_action_file(ch))
        {
            recomp_printf("[remote_models] ch %d resource staging failed\n", ch);
            continue;
        }

        cache->ready = 1;
        recomp_printf("[remote_models] ch %d clothed model/action cache ready\n",
                      ch);
    }
}

/* ------------------------------------------------------------------ */
/* Remote model slots                                                 */
/* ------------------------------------------------------------------ */

static void hide_object(void *object)
{
    if (!object)
        return;

    write_u32_at(object, 0x2c, 0);
    write_float_at(object, 0x1c, 0.0f);
    write_float_at(object, 0x20, 0.0f);
    write_float_at(object, 0x24, 0.0f);
    write_u8_at(object, 0x65, 1);
}

static void show_object(void *object)
{
    float scale = REMOTE_MODEL_SCALE;

    write_float_at(object, 0x1c, scale);
    write_float_at(object, 0x20, scale);
    write_float_at(object, 0x24, scale);
    write_u8_at(object, 0x65, 0);
}

static void kill_slot(RemoteModelSlot *slot, int delete_task)
{
    int i;

    if (delete_task && slot->task)
    {
        /* Use the engine's normal cutscene-task destructor while the remote
         * owner tree is known to be live, returning its render record pool. */
        func_80034EF8_35AF8(slot->task);
    }
    else if (delete_task)
    {
        hide_object(slot->object);
    }

    slot->active = 0;
    slot->cid = 0;
    slot->seen = 0;
    slot->bound_ch = -1;
    slot->bound_action = -1;
    slot->last_seq = 0;
    slot->last_remote_frame_100 = 0;
    slot->last_remote_frame_count_100 = 0;
    slot->yaw = 0;
    slot->frame = 0.0f;
    slot->frame_step = 1.0f;
    slot->task = 0;
    slot->object = 0;
    for (i = 0; i < AUX_RESOURCE_COUNT; ++i)
        slot->aux_resource_id[i] = 0;
}

void anchor_player_models_reset(void)
{
    int i;

    for (i = 0; i < REMOTE_MODEL_SLOT_COUNT; ++i)
        kill_slot(&s_slots[i], 0);
    s_owner_task = 0;
}

static RemoteModelSlot *find_slot(int cid)
{
    int i;

    for (i = 0; i < REMOTE_MODEL_SLOT_COUNT; ++i)
    {
        if (s_slots[i].active && s_slots[i].cid == cid)
            return &s_slots[i];
    }
    return 0;
}

static RemoteModelSlot *alloc_slot(int cid)
{
    int i;

    for (i = 0; i < REMOTE_MODEL_SLOT_COUNT; ++i)
    {
        if (!s_slots[i].active)
        {
            kill_slot(&s_slots[i], 0);
            s_slots[i].active = 1;
            s_slots[i].cid = cid;
            return &s_slots[i];
        }
    }
    return 0;
}

static int ensure_slot_task(RemoteModelSlot *slot, const AnchorPlayerModelRemote *remote,
                            void *render_parent_task)
{
    float scale;
    void *object;

    if (slot->task && slot->object)
        return 1;

    /* Cutscene pattern: a plain scheduler task owning render objects. */
    slot->task = func_80034E08_35A08(render_parent_task, remote_model_task_update, 0);
    if (!slot->task)
        return 0;

    scale = REMOTE_MODEL_SCALE;

    /* Spawn hidden: model pointer 0 renders nothing. +0x30 gets the immutable
     * renderer context required by the clothed Goemon/Ebisumaru display data;
     * no player initialization or behavior function is called. */
    object = func_8000DBF0_E7F0(slot->task, 0,
                                CLOTHED_CHARACTER_ANIM_CONTEXT,
                                (float)remote->x, (float)remote->y, (float)remote->z,
                                0, slot->yaw, 0,
                                scale, scale, scale,
                                0, 0);
    if (!object)
    {
        kill_slot(slot, 1);
        return 0;
    }

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

/* Stage the action's two aux face/part resources (what func_801DAF54 +
 * func_801DC87C do for the local player, into slot-owned double buffers)
 * and bind them to the object segment bases. */
static int bind_aux_resources(RemoteModelSlot *slot, unsigned char *entry)
{
    unsigned char *aux_table = *(unsigned char **)(entry + 0x18);
    int i;

    for (i = 0; i < AUX_RESOURCE_COUNT; ++i)
    {
        /* Resource index selection mirrors func_801DB180: entry 0 uses
         * table index 0, entry 1 uses the action record byte +0x14. */
        unsigned int index = (i == 0) ? 0u : (unsigned int)*(entry + 0x14);
        unsigned int segment = D_80203FF0_5BFF00[i * 2 + 1];
        unsigned int resource;
        unsigned char *buffer;
        unsigned char *end;
        int flip;

        if (!aux_table)
            continue;
        resource = *(unsigned int *)(aux_table + index * 8);
        if (resource == 0)
            continue;
        if (segment < 1 || segment > 5)
            return 0;

        if (slot->aux_resource_id[i] == resource)
        {
            buffer = slot->aux_buffer[i][slot->aux_flip[i]];
            if (!buffer)
                return 0;
            write_u32_at(slot->object, 0x38 + segment * 8, (unsigned int)(unsigned long)buffer);
            continue;
        }

        /* Double-buffer like the stock aux staging so a buffer still being
         * consumed by the renderer is never overwritten in place. */
        flip = slot->aux_flip[i] ^ 1;
        if (!slot->aux_buffer[i][flip])
            slot->aux_buffer[i][flip] = alloc_aligned(AUX_BUFFER_SIZE);
        buffer = slot->aux_buffer[i][flip];
        if (!buffer)
            return 0;

        end = func_800145B4_151B4(resource, buffer);
        if (!end || end < buffer || end > buffer + AUX_BUFFER_SIZE)
        {
            recomp_printf("[remote_models] aux resource %x overflow\n", resource);
            return 0;
        }
        slot->aux_flip[i] = flip;
        slot->aux_resource_id[i] = resource;
        write_u32_at(slot->object, 0x38 + segment * 8, (unsigned int)(unsigned long)buffer);
    }

    return 1;
}

/* Bind a character/action to the slot's render object from the character
 * cache. Pure pointer rebinding; no data is copied except aux resources on
 * their first use. */
static int bind_model(RemoteModelSlot *slot, int ch, int action)
{
    CharacterModelCache *cache = &s_char_cache[ch];
    unsigned char *entry = get_action_entry(ch, action);
    short speed;

    if (!entry || !cache->ready || !cache->broad || !cache->action)
        return 0;

    /* Segment 8: action model file base. The stock copy sets
     * object+0x38 = buffer - (range_start - 0x7000000); with the whole file
     * cached, that base is simply the file itself. */
    write_u32_at(slot->object, 0x38, (unsigned int)(unsigned long)cache->action);
    /* Segment 9: broad character file id + base (object+0x3c/+0x40). */
    write_u16_at(slot->object, 0x3c, D_80204020_5BFF30[ch]);
    write_u32_at(slot->object, 0x40, (unsigned int)(unsigned long)cache->broad);

    if (!bind_aux_resources(slot, entry))
        return 0;

    write_u32_at(slot->object, 0x2c,
                 *(unsigned int *)(entry + 0x00) + PLAYER_MODEL_RENDER_SEGMENT);

    speed = *(short *)(entry + 0x04);
    slot->frame_step = (float)speed / 100.0f;
    slot->frame = 0.0f;
    write_float_at(slot->object, 0x28, 0.0f);

    slot->bound_ch = ch;
    slot->bound_action = action;
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

static float frame_delta(float target, float current, float frame_count)
{
    float delta = target - current;

    if (frame_count > 1.0f)
    {
        float half = frame_count * 0.5f;

        if (delta > half)
            delta -= frame_count;
        else if (delta < -half)
            delta += frame_count;
    }
    return delta;
}

static float abs_float(float value)
{
    return value < 0.0f ? -value : value;
}

static void update_slot_pose(RemoteModelSlot *slot, const AnchorPlayerModelRemote *remote,
                             int action_changed)
{
    float frame_count;
    int new_remote_packet = remote->seq != slot->last_seq;
    int use_remote_anim = remote->action == slot->bound_action &&
                          remote->action >= 0 &&
                          remote->action < REMOTE_PLAYER_ACTION_MAX;

    if (!use_remote_anim)
        slot->yaw = yaw_from_velocity(remote->vx, remote->vz, slot->yaw);

    slot->frame += slot->frame_step;
    /* Resolve the bound clip length through the engine because its model
     * command pointer is segmented and cannot be dereferenced directly. */
    frame_count = func_8001B5AC_1C1AC(slot->object);

    if (use_remote_anim && new_remote_packet)
    {
        float target_frame = (float)remote->anim_frame_100 / 100.0f;
        float source_frame_count =
            (float)remote->anim_frame_count_100 / 100.0f;

        /* The sender and receiver use the same character/action record. The
         * normalized fallback only protects against a resolver count mismatch. */
        if (source_frame_count > 1.0f && frame_count > 1.0f &&
            abs_float(source_frame_count - frame_count) >= 0.01f)
        {
            target_frame = target_frame / source_frame_count * frame_count;
        }

        if (!action_changed && slot->last_seq != 0 &&
            remote->anim_frame_count_100 > 100 &&
            remote->anim_frame_count_100 == slot->last_remote_frame_count_100)
        {
            float previous_frame =
                (float)slot->last_remote_frame_100 / 100.0f;
            float delta;

            if (source_frame_count > 1.0f && frame_count > 1.0f &&
                abs_float(source_frame_count - frame_count) >= 0.01f)
                previous_frame = previous_frame / source_frame_count * frame_count;
            delta = frame_delta(target_frame, previous_frame, frame_count);
            if (abs_float(delta) < REMOTE_FRAME_SNAP_THRESHOLD)
                slot->frame_step = delta / REMOTE_PACKET_FRAME_INTERVAL;
        }

        /* Snap to every authoritative packet. The derived step above only
         * predicts the one intervening frame, including paused animations. */
        slot->frame = target_frame;
        slot->last_remote_frame_100 = remote->anim_frame_100;
        slot->last_remote_frame_count_100 = remote->anim_frame_count_100;
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

    write_float_at(slot->object, 0x08, (float)remote->x);
    write_float_at(slot->object, 0x0c, (float)remote->y);
    write_float_at(slot->object, 0x10, (float)remote->z);
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
    slot->last_seq = remote->seq;
}

static void update_slot_hidden_pose(RemoteModelSlot *slot, const AnchorPlayerModelRemote *remote)
{
    if (!slot->object)
        return;

    write_float_at(slot->object, 0x08, (float)remote->x);
    write_float_at(slot->object, 0x0c, (float)remote->y);
    write_float_at(slot->object, 0x10, (float)remote->z);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                        */
/* ------------------------------------------------------------------ */

void anchor_player_models_update(const AnchorPlayerModelRemote *remotes, int count,
                                 void *render_parent_task)
{
    int i;

    if (!is_rdram_pointer(render_parent_task))
    {
        anchor_player_models_reset();
        return;
    }

    if (s_owner_task != render_parent_task)
    {
        anchor_player_models_reset();
        s_owner_task = render_parent_task;
    }

    for (i = 0; i < REMOTE_MODEL_SLOT_COUNT; ++i)
        s_slots[i].seen = 0;

    for (i = 0; remotes && i < count; ++i)
    {
        const AnchorPlayerModelRemote *remote = &remotes[i];
        RemoteModelSlot *slot;
        int ch;
        int action;
        int cache_ready;

        if (remote->cid <= 0 || remote->ch < 0 || remote->ch >= CHARACTER_COUNT)
            continue;

        slot = find_slot(remote->cid);
        if (!slot)
            slot = alloc_slot(remote->cid);
        if (!slot)
            continue; /* more remotes than model slots; nameplate only */

        slot->seen = 1;
        if (!ensure_slot_task(slot, remote, render_parent_task))
            continue;

        ch = remote->ch;
        action = remote_action_or_idle(remote->action);
        cache_ready = s_char_cache[ch].ready;

        if (!cache_ready)
        {
            if (slot->bound_ch != ch)
            {
                /* No data for the new character yet; stop showing the old
                 * one but keep tracking position. */
                hide_object(slot->object);
                slot->bound_ch = -1;
                slot->bound_action = -1;
            }
            update_slot_hidden_pose(slot, remote);
            continue;
        }

        if (slot->bound_ch != ch || slot->bound_action != action)
        {
            if (!bind_model(slot, ch, action))
            {
                hide_object(slot->object);
                slot->bound_ch = -1;
                slot->bound_action = -1;
                update_slot_hidden_pose(slot, remote);
                continue;
            }
            update_slot_pose(slot, remote, 1);
            continue;
        }

        update_slot_pose(slot, remote, 0);
    }

    for (i = 0; i < REMOTE_MODEL_SLOT_COUNT; ++i)
    {
        if (s_slots[i].active && !s_slots[i].seen)
            kill_slot(&s_slots[i], 1);
    }
}
