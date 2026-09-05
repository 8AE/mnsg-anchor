#include "anchor.h"
#include "anchor_remote_model_pool.h"
#include "anchor_player_models.h"
#include "anchor_projectile_models.h"
#include "anchor_projectile_source.h"
#include "anchor_projectile_capture.h"
#include "item_sync.h"
#include "modding.h"
#include "recomputils.h"

extern void *D_801FC604_5B8514;
extern void *D_801FC60C_5B851C;
extern unsigned short D_800C7AB2;

static AnchorProjectileSourceState s_source;
static void *s_owner;
static int s_session, s_epoch;
static unsigned short s_room;
static unsigned int s_tick;

static int rdram(const void *pointer)
{
    unsigned int physical = (unsigned int)(unsigned long)pointer & 0x1fffffffu;
    return (physical >= 0x1000u && physical < 0x800000u) ||
           anchor_remote_model_pool_contains(pointer);
}

static int linked(const void *task)
{
    const void *backlink;
    if (!rdram(task))
        return 0;
    backlink = *(void *const *)((const unsigned char *)task + 4);
    return rdram(backlink) && *(void *const *)backlink == task;
}

RECOMP_HOOK("func_80034A10_35610")
void anchor_projectile_task_reinitialized(void *task)
{
    anchor_projectile_source_forget_task(&s_source, task);
}

static void capture_throw(void *pointer)
{
    unsigned char *task = pointer;
    void *object;
    AnchorProjectileSpawn spawn;
    int publish;
    if (!linked(task) || *(void **)(task + 0x5c) != D_801FC604_5B8514 ||
        task[0x65] != 0)
        return;
    object = *(void **)(task + 0x18);
    if (!rdram(object) || !anchor_projectile_capture_fields(task, object, &spawn))
        return;
    publish = s_session > 0 && s_owner == D_801FC604_5B8514 &&
              s_room == D_800C7AB2;
    if (anchor_projectile_source_capture(&s_source, task, &spawn, s_tick, publish))
    {
#if DEBUG_BUTTON_ENABLED
        recomp_printf("[projectiles] captured throw kind=%d\n", spawn.kind);
#endif
    }
}

/* Save task arguments at ENTRY: return hooks cannot assume the original a0
 * survives the native call. These six common update bodies cover actual
 * successful weapon activations, including delayed bomb initialization.
 * No animation inference, display-list parsing or full-scene scan is needed. */
static void *s_coin_task;
RECOMP_HOOK("func_801E936C_5A527C")
void anchor_projectile_coin_entry(void *task)
{
    s_coin_task = rdram(task) && anchor_projectile_capture_first_update(task) ? task : 0;
}
RECOMP_HOOK_RETURN("func_801E936C_5A527C")
void anchor_projectile_coin_return(void)
{
    capture_throw(s_coin_task);
    s_coin_task = 0;
}

static void *s_charged_task;
RECOMP_HOOK("func_801EA0D8_5A5FE8")
void anchor_projectile_charged_entry(void *task)
{
    s_charged_task = rdram(task) && anchor_projectile_capture_first_update(task) ? task : 0;
}
RECOMP_HOOK_RETURN("func_801EA0D8_5A5FE8")
void anchor_projectile_charged_return(void)
{
    capture_throw(s_charged_task);
    s_charged_task = 0;
}

static void *s_camera_task;
RECOMP_HOOK("func_801EBAA8_5A79B8")
void anchor_projectile_camera_entry(void *task)
{
    s_camera_task = rdram(task) && anchor_projectile_capture_first_update(task) ? task : 0;
}
RECOMP_HOOK_RETURN("func_801EBAA8_5A79B8")
void anchor_projectile_camera_return(void)
{
    capture_throw(s_camera_task);
    s_camera_task = 0;
}

static void *s_bomb_task;
RECOMP_HOOK("func_801EBF48_5A7E58")
void anchor_projectile_bomb_entry(void *task)
{
    s_bomb_task = rdram(task) && anchor_projectile_capture_first_update(task) ? task : 0;
}
RECOMP_HOOK_RETURN("func_801EBF48_5A7E58")
void anchor_projectile_bomb_return(void)
{
    capture_throw(s_bomb_task);
    s_bomb_task = 0;
}

static void *s_yae_task;
RECOMP_HOOK("func_801EEDF8_5AAD08")
void anchor_projectile_yae_entry(void *task)
{
    s_yae_task = rdram(task) && anchor_projectile_capture_first_update(task) ? task : 0;
}
RECOMP_HOOK_RETURN("func_801EEDF8_5AAD08")
void anchor_projectile_yae_return(void)
{
    capture_throw(s_yae_task);
    s_yae_task = 0;
}

static void *s_kunai_task;
RECOMP_HOOK("func_801EFBDC_5ABAEC")
void anchor_projectile_kunai_entry(void *task)
{
    s_kunai_task = rdram(task) && anchor_projectile_capture_first_update(task) ? task : 0;
}
RECOMP_HOOK_RETURN("func_801EFBDC_5ABAEC")
void anchor_projectile_kunai_return(void)
{
    capture_throw(s_kunai_task);
    s_kunai_task = 0;
}

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_projectiles_frame(void)
{
    AnchorProjectileRemote remotes[ANCHOR_PROJECTILE_BATCH_MAX];
    const AnchorProjectileSpawn *spawn;
    void *owner = D_801FC604_5B8514;
    int session = anchor_get_projectile_session();
    int epoch = anchor_player_models_get_epoch();
    int count, i;
    char encoded[ANCHOR_PROJECTILE_JSON_SIZE];
    char *json;
    ++s_tick;
    if (!item_sync_save_is_loaded() || !linked(owner) ||
        !rdram(D_801FC60C_5B851C) ||
        *(void **)((unsigned char *)owner + 0x18) != D_801FC60C_5B851C)
        owner = 0;
    if (owner != s_owner || D_800C7AB2 != s_room)
    {
        anchor_projectile_source_reset(&s_source);
        anchor_projectile_models_reset();
    }
    else if (session != s_session || epoch != s_epoch)
    {
        /* Keep observed native lifetimes, discard only old-session events. */
        anchor_projectile_source_clear_pending(&s_source);
        anchor_projectile_models_reset();
    }
    s_owner = owner;
    s_room = D_800C7AB2;
    s_session = session;
    s_epoch = epoch;
    if (!session || !owner)
    {
        anchor_projectile_source_clear_pending(&s_source);
        anchor_projectile_models_reset();
        return;
    }
    for (i = 0; i < ANCHOR_PROJECTILE_BATCH_MAX &&
         (spawn = anchor_projectile_source_peek(&s_source, s_tick)) != 0; ++i)
    {
        if (!anchor_projectile_spawn_encode(spawn, encoded, sizeof(encoded)) ||
            !anchor_send_projectile_spawn_json(session, epoch, encoded))
            break; /* Same event ID retries; it is never coalesced away. */
#if DEBUG_BUTTON_ENABLED
        recomp_printf("[projectiles] sent throw id=%d kind=%d\n", spawn->id, spawn->kind);
#endif
        anchor_projectile_source_ack(&s_source);
    }
    anchor_projectile_models_tick(owner);
    json = anchor_get_projectile_spawns_json();
    count = anchor_projectile_spawns_decode(json, remotes, ANCHOR_PROJECTILE_BATCH_MAX);
    for (i = 0; i < count; ++i)
    {
        AnchorProjectileRemote *remote = &remotes[i];
        if (anchor_projectile_models_spawn(remote, owner))
            anchor_ack_projectile_spawn(remote->cid, remote->session,
                                         remote->epoch, remote->spawn.id);
    }
    if (json)
        recomp_free(json);
}
