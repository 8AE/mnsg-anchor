/* A room-local native sign at the Gorgeous Music Castle entrance. The sign
 * actor supplies its game model; its native Kai Highway text is suppressed. */
#include "core/anchor_dialog.h"
#include "bosses/anchor_boss_invite_world.h"
#include "world/anchor_castle_return_sign.h"
#ifndef ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
#include "platform/modding.h"
#include "platform/recomputils.h"
#else
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#define RECOMP_PATCH
#endif

typedef struct {
    short x, y, z;
    short pitch, yaw, roll;
    void *definition;
    unsigned char spawned;
    unsigned char reserved[3];
} CastleSignInstance;

typedef struct {
    unsigned short actor_id, params;
    unsigned int data[3];
} CastleSignDefinition;

typedef struct {
    CastleSignInstance *resident_instances;
    const char **definition_names;
    CastleSignInstance *normal_instances;
    void *partitions;
    void *partition_config;
    unsigned short actor_data_file_id;
    unsigned short reserved;
    void (*load_files)(void);
} CastleSignRoomMetadata;

extern unsigned short D_800C7AB2;
extern void *D_802287BC_5E3C8C[];
extern CastleSignRoomMetadata *D_80231300_5EC7D0[];
extern float D_80239AFC_5F4FCC;
extern int func_800141C4_14DC4(unsigned int file_id);
extern int *func_80219CA0_5D5170(int *task, unsigned char actor_type);
extern int func_8003555C_3615C(int *owner, void *entry, int arg2, int flags,
                                int x, int y, int z, short pitch, short yaw,
                                short roll, float sx, float sy, float sz,
                                short arg13, short arg14, short model_id);
extern void func_80218A54_5D3F24(int actor, CastleSignInstance *instance);
extern void *D_8016DAB4_16E6B4;
extern void *D_801FC604_5B8514;
extern void func_8003521C_35E1C(void (*callback)(void *, void *));
extern void func_8003D388_3DF88(unsigned short scenario, int player);
extern void func_802213A4_5DC874(void *task, void *object);
extern int anchor_boss_invites_active(void);

enum { CASTLE_ROOM = 0x00a8, SIGN_ACTOR = 0x008b, SIGN_CATEGORY = 7 };

/* The direct KSEG0 definition pointer is left unchanged by the native
 * segmented-pointer resolver. Native actor 0x8B uses model ID zero. Its
 * instance yaw is a 16-bit full turn, so 0x8000 faces the opposite way. */
static CastleSignDefinition s_definition = {SIGN_ACTOR, 0, {0, 0, 0}};
static CastleSignInstance s_instance = {
    38, -35, -136, 0, -32768, 0, &s_definition, 0, {0, 0, 0}
};
static int s_spawned;
static unsigned char *s_task;
static unsigned char s_generation;
static unsigned int s_visit;
static unsigned int s_dialog_visit;
static int s_placed;
static int s_prompt_active;
static int s_warp_pending;
static int s_logged_talk;

int anchor_castle_return_sign_pending(void)
{
    return s_prompt_active || s_warp_pending;
}

#ifdef ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
extern unsigned char *anchor_castle_return_sign_host_task_bytes(int actor);
#endif

static unsigned char *task_bytes(int actor)
{
#ifdef ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
    return anchor_castle_return_sign_host_task_bytes(actor);
#else
    return (unsigned char *)(unsigned long)(unsigned int)actor;
#endif
}

#ifndef ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
typedef char CastleSignInstanceSize[(sizeof(CastleSignInstance) == 0x14) ? 1 : -1];
typedef char CastleSignDefinitionSize[(sizeof(CastleSignDefinition) == 0x10) ? 1 : -1];
typedef char CastleSignMetadataFileOffset[
    (__builtin_offsetof(CastleSignRoomMetadata, actor_data_file_id) == 0x14) ? 1 : -1];
#endif

/* A same-room reload is a new visit and must get a fresh native actor. */
RECOMP_HOOK("func_8020D6BC_5C8B8C")
void anchor_castle_return_sign_begin_load(void)
{
    if (s_prompt_active) {
        anchor_dialog_cancel_for(ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
        (void)anchor_dialog_poll_for(ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
    }
    s_prompt_active = 0;
    s_warp_pending = 0;
    s_spawned = 0;
    s_task = 0;
    s_placed = 0;
    s_instance.spawned = 0;
    s_logged_talk = 0;
    s_visit = s_visit == 0x7fffffffu ? 1u : s_visit + 1u;
}

/* Stage actor data can be retried while its wave is loading. Spawn after the
 * native stage actor pass, only when the room's normal actor data is resident. */
RECOMP_HOOK_RETURN("func_8020D848_5C8D18")
void anchor_castle_return_sign_finish_actor_stage(void)
{
    CastleSignRoomMetadata *metadata;
    int *owner;
    int actor;
    float scale;

    if (D_800C7AB2 != CASTLE_ROOM || s_spawned)
        return;
    metadata = D_80231300_5EC7D0[CASTLE_ROOM];
    if (!metadata || !metadata->actor_data_file_id ||
        func_800141C4_14DC4(metadata->actor_data_file_id) == -1 ||
        !D_802287BC_5E3C8C[SIGN_ACTOR])
        return;

    owner = func_80219CA0_5D5170(0, SIGN_CATEGORY);
    if (!owner)
        return;
    scale = D_80239AFC_5F4FCC;
    actor = func_8003555C_3615C(owner, D_802287BC_5E3C8C[SIGN_ACTOR],
                                 0, 0xC006D920, 0, 0, 0, 0, 0, 0,
                                 scale, scale, scale, 0, 0, 0);
    if (!actor)
        return;

    /* The native initializer copies this placement and the definition payload
     * into its task. Retain the source records for the whole room visit. */
    s_spawned = 1;
    func_80218A54_5D3F24(actor, &s_instance);
    s_task = task_bytes(actor);
    s_generation = s_task[0x74];
#ifndef ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
    recomp_printf("[CastleSign] spawned task=%08X gen=%u actor=%04X room=%04X\n",
                  (unsigned int)(unsigned long)s_task,
                  (unsigned int)s_generation,
                  (unsigned int)*(unsigned short *)(s_task + 0x5e),
                  (unsigned int)D_800C7AB2);
#endif
}

static int owns_task(void *task)
{
    unsigned char *bytes = task;
    return D_800C7AB2 == CASTLE_ROOM && s_spawned && s_task &&
           bytes == s_task && bytes[0x74] == s_generation &&
           *(unsigned short *)(bytes + 0x5e) == SIGN_ACTOR;
}

int anchor_castle_return_sign_owns_task(void *task)
{
    return task && owns_task(task);
}

/* The constructor return runs after common NPC initialization and its ground
 * adjustment. Preserve the requested exact placement once. */
static void place_sign(unsigned char *task, unsigned char *object)
{
    if (s_placed)
        return;
    *(float *)(object + 0x08) = 38.0f;
    *(float *)(object + 0x0c) = -35.0f;
    *(float *)(object + 0x10) = -136.0f;
    *(float *)(task + 0x78) = 0.0f;
    *(float *)(task + 0x7c) = 0.0f;
    *(float *)(task + 0x80) = 0.0f;
    s_placed = 1;
}

RECOMP_HOOK_RETURN("func_80214BBC_5D008C")
void anchor_castle_return_sign_after_constructor(void)
{
    unsigned char *task = D_8016DAB4_16E6B4;
    unsigned char *object;
    if (!anchor_castle_return_sign_owns_task(task))
        return;
    object = *(unsigned char **)(task + 0x18);
    if (object)
        place_sign(task, object);
}

/* Native player contact sets task+0x68 bit 0x100 without setting the object
 * talk bit. Keep the sign's native idle/gate handoff, then replace only its
 * scenario start. The native completion callback restores that idle callback
 * and clears the contact bit after our private scenario closes. */
RECOMP_PATCH void func_80221338_5DC808(void *task, void *object)
{
    unsigned char *bytes = task;
    unsigned char *player = *(unsigned char **)(bytes + 0xec);
    int began = 0;
    (void)object;
    if (player[0xcd] != 3)
        return;
    player[0xcd] = 4;
    if (!anchor_castle_return_sign_owns_task(task)) {
        func_8003D388_3DF88(*(unsigned short *)(bytes + 0xa4),
                             player == D_801FC604_5B8514 ? 1 : 2);
    } else {
        /* A native accepted talk holds scripted input, so the ordinary
         * can_prompt predicate intentionally cannot gate this continuation.
         * Even a failed private begin must reach native cleanup. */
        if (player == D_801FC604_5B8514 &&
            !anchor_castle_return_sign_pending() &&
            !anchor_boss_invites_active()) {
            began = anchor_dialog_begin_castle_return();
            if (began) {
                s_prompt_active = 1;
                s_dialog_visit = s_visit;
            }
        }
#ifndef ANCHOR_CASTLE_RETURN_SIGN_HOST_TEST
        if (!s_logged_talk) {
            recomp_printf("[CastleSign] accepted talk task=%08X flags=%08X began=%d\n",
                          (unsigned int)(unsigned long)bytes,
                          (unsigned int)*(unsigned int *)(bytes + 0x68), began);
            s_logged_talk = 1;
        }
#endif
    }
    func_8003521C_35E1C(func_802213A4_5DC874);
}

/* Poll only the sign's dialog result. The owner token prevents a boss invite
 * from reading Yes or cancelling the sign's message. */
RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_castle_return_sign_frame(void)
{
    AnchorDialogResult result;
    if (!s_prompt_active && !s_warp_pending)
        return;
    if (D_800C7AB2 != CASTLE_ROOM || s_dialog_visit != s_visit) {
        if (s_prompt_active) {
            anchor_dialog_cancel_for(ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
            (void)anchor_dialog_poll_for(ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
        }
        s_prompt_active = 0;
        s_warp_pending = 0;
        return;
    }
    if (s_prompt_active) {
        result = anchor_dialog_poll_for(ANCHOR_DIALOG_OWNER_CASTLE_RETURN);
        if (result == ANCHOR_DIALOG_PENDING)
            return;
        s_prompt_active = 0;
        s_warp_pending = result == ANCHOR_DIALOG_YES;
    }
    if (s_warp_pending &&
        anchor_boss_invite_world_transfer_to(0x014c, 0, -46, -576))
        s_warp_pending = 0;
}
