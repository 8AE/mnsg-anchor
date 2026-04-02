/**
 * @file anchor_actors.c
 * @brief Phantom actor system using the game's native actor-instance injection.
 *
 * Strategy
 * --------
 * Instead of directly manipulating RDRAM rendering structures, this module
 * hooks func_8020D724_5C8BF4 (the room actor-setup function) to inject extra
 * ActorInstance entries into the room's actor-data file before the game
 * spawns anything.  The game then instantiates our entries through its own
 * normal pathway.
 *
 * This is the same pattern used by MNSGRecompRando (actor_manager.c).
 *
 *   1. On room load we copy the actor-data file into a fresh recomp_alloc()
 *      buffer and register an overlay redirect so all subsequent calls to
 *      func_800141C4_14DC4 (overlay_get_data_pointer) return our copy.
 *
 *   2. We append extra ActorInstance entries to the copy, borrowing the
 *      actor_definition pointer of the first existing instance (a safe,
 *      always-valid definition type for whatever room we are in).
 *
 *   3. The game's original func_8020D724_5C8BF4 continues, reads our modified
 *      buffer, and spawns the injected entries as real game objects.
 *
 * Current status: DUMMY + POSITIONS
 * Queries anchor for same-room teammate positions and injects one actor per
 * teammate using a safe visible actor definition from the room.  Invisible /
 * hazardous actor types (death boundaries, exits, doors) are skipped when
 * selecting the template definition to avoid visual corruption.
 */

#include "modding.h"
#include "recomputils.h"
#include "anchor.h"

/* =========================================================================
   Constants
   ========================================================================= */

#define WAVE_MAX 48
#define ACTOR_FILE_BUF_SIZE 0x10000
#define MAX_PHANTOMS 7

/* =========================================================================
   Types
   Compiled as MIPS32, so sizeof(pointer) == 4 and all struct sizes match
   the N64 game layout exactly.  Same definitions as MNSGRecompRando's
   common_structs.h / actor_management.h.
   ========================================================================= */

typedef struct
{
    short x, y, z;
} ActorPos;
typedef struct
{
    short pitch, yaw, roll;
} ActorRot;

typedef struct ActorDefinition
{
    int data[4]; /* 16 bytes */
} ActorDefinition;

typedef struct ActorInstance
{
    ActorPos position;                 /* 0x00 - 6 bytes  */
    ActorRot rotation;                 /* 0x06 - 6 bytes  */
    ActorDefinition *actor_definition; /* 0x0C - 4 bytes (MIPS32 ptr) */
    unsigned char is_spawned;          /* 0x10 */
    unsigned char unk_11;              /* 0x11 */
    unsigned char unk_12;              /* 0x12 */
    unsigned char unk_13;              /* 0x13 */
} ActorInstance;                       /* 0x14 = 20 bytes */

typedef struct
{
    ActorInstance *persistent_actor_instances; /* 0x00 */
    const char **actor_definition_names;       /* 0x04 */
    ActorInstance *actor_instances;            /* 0x08 */
    void *actor_partitions;                    /* 0x0C */
    void *actor_partition_configuration;       /* 0x10 */
    short actor_data_file_id;                  /* 0x14 */
    short unk_16;                              /* 0x16 */
    void (*load_stage_data_files)(void);       /* 0x18 */
} StageActorMetadata;                          /* 0x1C bytes */

typedef struct
{
    unsigned short file_id;
    int data; /* N64 vaddr of the loaded file, stored as int */
} OverlayEntry;

typedef struct
{
    unsigned short file_id;
    void *redirected_data;
} OverlayRedirection;

/* =========================================================================
   Game externs
   ========================================================================= */

extern unsigned short D_800C7AB2;
extern StageActorMetadata *D_80231300_5EC7D0[];
extern OverlayEntry D_80167FC0_168BC0[WAVE_MAX];

/* overlay_get_data_pointer */
extern int func_800141C4_14DC4(unsigned int file_id);
/* overlay_resolve_pointer  */
extern int func_80014840_15440(int pointer, unsigned int file_id);

/* =========================================================================
   Overlay redirection table  (same pattern as MNSGRecompRando)
   ========================================================================= */

static OverlayRedirection s_redirections[WAVE_MAX];
static int s_redirections_ready = 0;

static void redirections_init(void)
{
    if (s_redirections_ready)
        return;
    for (int i = 0; i < WAVE_MAX; i++)
    {
        s_redirections[i].file_id = 0;
        s_redirections[i].redirected_data = NULL;
    }
    s_redirections_ready = 1;
}

static void redirections_clear(void)
{
    redirections_init();
    for (int i = 0; i < WAVE_MAX; i++)
    {
        s_redirections[i].file_id = 0;
        if (s_redirections[i].redirected_data)
        {
            recomp_free(s_redirections[i].redirected_data);
            s_redirections[i].redirected_data = NULL;
        }
    }
}

static void *redirections_add(unsigned short file_id, unsigned int size)
{
    redirections_init();
    for (int i = 0; i < WAVE_MAX; i++)
    {
        if (s_redirections[i].file_id == 0)
        {
            s_redirections[i].file_id = file_id;
            s_redirections[i].redirected_data = recomp_alloc(size);
            return s_redirections[i].redirected_data;
        }
    }
    return NULL;
}

/* =========================================================================
   RECOMP_PATCH: overlay_get_data_pointer
   Check our redirect table first; fall through to original wave-table walk.
   ========================================================================= */

RECOMP_PATCH int func_800141C4_14DC4(unsigned int file_id)
{
    if (file_id == 0)
        return 0;

    redirections_init();

    for (int i = 0; i < WAVE_MAX; i++)
    {
        if (s_redirections[i].file_id == file_id)
            return (int)s_redirections[i].redirected_data;
    }

    /* Original function body: walk the loaded-wave table. */
    OverlayEntry *entry = (OverlayEntry *)&D_80167FC0_168BC0;
    while (entry->file_id != 0)
    {
        if (entry->file_id == file_id)
            return entry->data;
        entry++;
    }
    return -1;
}

/* =========================================================================
   Actor-ID safety check
   Returns 1 if this actor type is safe to use as a visual clone template.
   Invisible / hazardous types are excluded:
     0x08c  Exit trigger        – invisible, teleports player
     0x08e  Death Boundary      – invisible, kills player AND corrupts lighting
     0x23c  Sliding Door        – no standalone model
     0x23e  Locked Door         – no standalone model
     0x345  various door flags  – no standalone model
     0x082  Ryo (money)         – tiny coin, bad placeholder
     0x193  Key item            – small pickup, bad placeholder
     0x086-0x089  Flag actors   – invisible triggers
   ========================================================================= */

static int actor_id_is_safe_template(unsigned short id)
{
    if (id == 0x08c || id == 0x08e)
        return 0; /* exits / death boundaries */
    if (id == 0x23c || id == 0x23e)
        return 0; /* sliding / locked doors   */
    if (id == 0x345)
        return 0; /* door-flag actor          */
    if (id == 0x082)
        return 0; /* ryo coin                 */
    if (id == 0x193)
        return 0; /* key                      */
    if (id >= 0x086 && id <= 0x089)
        return 0; /* flag actors              */
    return 1;
}

/* =========================================================================
   Minimal JSON position parser
   Expects: [{"x":<int>,"y":<int>,"z":<int>}, ...]
   Returns number of entries written (<= MAX_PHANTOMS).
   ========================================================================= */

typedef struct
{
    int x, y, z;
} PhantomPos;

static int parse_positions(const char *json, PhantomPos *out, int max_out)
{
    int count = 0;
    if (!json || json[0] != '[')
        return 0;
    const char *p = json + 1;
    while (*p && count < max_out)
    {
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n')
            p++;
        if (*p != '{')
            break;
        p++;
        int x = 0, y = 0, z = 0, got = 0;
        while (*p && *p != '}')
        {
            while (*p && *p != '"' && *p != '}')
                p++;
            if (*p != '"')
                break;
            p++;
            char key = *p;
            if (key)
                p++;
            while (*p && *p != ':')
                p++;
            if (*p == ':')
                p++;
            while (*p == ' ')
                p++;
            int sign = 1;
            if (*p == '-')
            {
                sign = -1;
                p++;
            }
            int val = 0;
            while (*p >= '0' && *p <= '9')
                val = val * 10 + (*p++ - '0');
            val *= sign;
            if (key == 'x')
            {
                x = val;
                got |= 1;
            }
            else if (key == 'y')
            {
                y = val;
                got |= 2;
            }
            else if (key == 'z')
            {
                z = val;
                got |= 4;
            }
        }
        if (*p == '}')
            p++;
        if (got == 7)
        {
            out[count].x = x;
            out[count].y = y;
            out[count].z = z;
            count++;
        }
    }
    return count;
}

/* =========================================================================
   RECOMP_HOOK: room actor-setup  (fires BEFORE the original function)
   Copy the actor-data file and inject one ActorInstance per same-room
   teammate using their network-reported position.
   ========================================================================= */

RECOMP_HOOK("func_8020D724_5C8BF4")
void anchor_actors_room_load(void)
{
    /* Always clear stale redirections from the previous room. */
    redirections_clear();

    if (!anchor_is_connected())
        return;

    if (!D_80231300_5EC7D0[D_800C7AB2])
        return;

    unsigned short file_id =
        (unsigned short)D_80231300_5EC7D0[D_800C7AB2]->actor_data_file_id;
    if (file_id == 0)
        return;

    /* Update our local room in the Python layer FIRST so that
     * get_teammate_positions_json() filters by the NEW room, not the
     * old one.  Without this call _local_room_id is still the previous
     * room when the hook fires and all same-new-room teammates are
     * filtered out, causing n == 0 every time.                         */
    anchor_set_local_room(D_800C7AB2);

    /* Query same-room teammate positions. */
    char *json = anchor_get_teammate_positions_json();
    PhantomPos positions[MAX_PHANTOMS];
    int n = 0;
    if (json)
    {
        recomp_printf("[anchor_actors] room 0x%03X positions json: %s\n",
                      (unsigned int)D_800C7AB2, json);
        n = parse_positions(json, positions, MAX_PHANTOMS);
        recomp_free(json);
    }
    if (n == 0)
        return; /* no teammates with position data in this room */

    /* Get the original file buffer before installing our redirect. */
    void *orig = (void *)func_800141C4_14DC4(file_id);
    if (!orig)
        return;

    /* Allocate a writable copy and register the redirect. */
    unsigned char *buf =
        (unsigned char *)redirections_add(file_id, ACTOR_FILE_BUF_SIZE);
    if (!buf)
        return;

    for (int i = 0; i < ACTOR_FILE_BUF_SIZE; i++)
        buf[i] = ((unsigned char *)orig)[i];

    /* Resolve the ActorInstance array into our copy. */
    ActorInstance *instances = (ActorInstance *)func_80014840_15440(
        (int)D_80231300_5EC7D0[D_800C7AB2]->actor_instances, file_id);
    if (!instances)
        return;

    /* Count existing instances. */
    int count = 0;
    while (instances[count].actor_definition != NULL)
        count++;
    if (count == 0)
        return;

    /* ------------------------------------------------------------------
     * Find the first safe visible actor definition to use as a template.
     * We resolve each instance's actor_definition to read its actor_id
     * (upper 16 bits of data[0]), then skip invisible / hazardous types.
     * ------------------------------------------------------------------ */
    ActorInstance *tmpl = NULL;
    for (int i = 0; i < count; i++)
    {
        ActorDefinition *def = (ActorDefinition *)func_80014840_15440(
            (int)instances[i].actor_definition, file_id);
        if (!def)
            continue;
        unsigned short actor_id = (unsigned short)((def->data[0] >> 16) & 0xFFFF);
        if (actor_id_is_safe_template(actor_id))
        {
            tmpl = &instances[i];
            recomp_printf("[anchor_actors] using actor 0x%03X as template\n",
                          (unsigned int)actor_id);
            break;
        }
    }
    if (!tmpl)
    {
        recomp_printf("[anchor_actors] room 0x%03X: no safe template actor found\n",
                      (unsigned int)D_800C7AB2);
        return;
    }

    /* ------------------------------------------------------------------
     * Inject one ActorInstance per teammate at their reported position.
     * ------------------------------------------------------------------ */
    for (int i = 0; i < n && i < MAX_PHANTOMS; i++)
    {
        ActorInstance *entry = &instances[count + i];
        entry->position.x = (short)positions[i].x;
        entry->position.y = (short)positions[i].y;
        entry->position.z = (short)positions[i].z;
        entry->rotation.pitch = 0;
        entry->rotation.yaw = 0;
        entry->rotation.roll = 0;
        entry->actor_definition = tmpl->actor_definition;
        entry->is_spawned = 0;
        entry->unk_11 = 0;
        entry->unk_12 = 0;
        entry->unk_13 = 0;
        recomp_printf("[anchor_actors] teammate %d at (%d, %d, %d)\n",
                      i, positions[i].x, positions[i].y, positions[i].z);
    }

    /* Re-insert the null terminator after all new entries. */
    ActorInstance *term = &instances[count + n];
    term->position.x = 0;
    term->position.y = 0;
    term->position.z = 0;
    term->rotation.pitch = 0;
    term->rotation.yaw = 0;
    term->rotation.roll = 0;
    term->actor_definition = NULL;
    term->is_spawned = 0;
    term->unk_11 = 0;
    term->unk_12 = 0;
    term->unk_13 = 0;

    recomp_printf(
        "[anchor_actors] room 0x%03X: injected %d teammate actor(s)\n",
        (unsigned int)D_800C7AB2, n);
}
