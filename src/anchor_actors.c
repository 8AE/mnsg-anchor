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
 *   2. We append one extra ActorInstance PER LOBBY PLAYER (not just those
 *      already in the same room).  Players who share the local room get their
 *      server-reported position; players in a different room are placed at an
 *      off-screen "hidden" coordinate so they are invisible but still have a
 *      live in-game object ready for this session.
 *
 *   3. The game's original func_8020D724_5C8BF4 continues, reads our modified
 *      buffer, and spawns the injected entries as real game objects.
 *
 * Behavior rules
 * --------------
 *  1. On room load, one actor is created for EVERY player in the lobby.
 *  2. Players whose raw room ID does not match the local player are placed at
 *     a hidden coordinate (HIDDEN_Y far below any walkable surface).
 *  3. Players who ARE in the same room are placed at their last server-reported
 *     world-space position.
 *  4. On the NEXT room load the positions are re-evaluated: a player who has
 *     since left the current room will be hidden; one who has entered will be
 *     visible at their latest broadcast position.
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

/**
 * Y coordinate used to "park" a phantom actor that belongs to a player who is
 * not currently in the local room.  Chosen to be well below any walkable
 * surface in every room in the game (~26000 units below ground).
 */
#define HIDDEN_Y ((short)-26000)

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

/*
 * file_id of the redirect installed by the current room load.
 * Set to 0 at start of hook; written at the end if injection succeeds.
 * RECOMP_HOOK_RETURN uses this to decide which entry to KEEP.
 */
static unsigned short s_current_room_file_id = 0;

/* =========================================================================
   Phantom tracking (for per-frame live position update)
   ========================================================================= */

/* Index into the room's ActorInstance array where our injections begin. */
static int s_phantom_start_idx = 0;
/* Number of phantom instances currently injected in the room. */
static int s_phantom_count = 0;
/* World-space positions at which each phantom was spawned (floats). */
static float s_phantom_spawn_x[MAX_PHANTOMS];
static float s_phantom_spawn_y[MAX_PHANTOMS];
static float s_phantom_spawn_z[MAX_PHANTOMS];
/* Host pointer to each phantom's live CLS_BG_W object (NULL = not found yet). */
static void *s_phantom_live_obj[MAX_PHANTOMS];
/* Frame counter for throttling the per-frame update. */
static int s_update_timer = 0;

/*
 * Player's background-world object pointer (host ptr to CLS_BG_W).
 * Declared extern so we can derive the RDRAM base address for CLS_W
 * linked-list traversal in the per-frame hook.
 */
extern void *D_801FC60C_5B851C;

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
   Phantom NPC actor definition

   We use actor 0x02BE (Old Man) from vanilla.mn64.export.txt as the
   online player phantom.  Old Man is the most common humanoid NPC in
   the game (19 appearances in vanilla), appears as a standing human
   character, and has a simple definition with no mandatory parameters.

   Actor file definitions are addressed with an 0x08000000 base:
       func_80014840_15440(0x08000000 + byte_offset, file_id)
   resolves to (file_buf + byte_offset) after our redirect is installed.

   We write the 16-byte definition at PHANTOM_DEF_OFFSET near the end of
   our 64 KB buffer, well clear of any real room data (typical actor files
   are a few KB at most).
   ========================================================================= */

#define PHANTOM_DEF_OFFSET 0xF000U /* byte offset in our 64 KB buffer   */

/* =========================================================================
   Lobby positions parser
   Parses the JSON produced by anchor_mnsg.get_lobby_positions_json():
     [{"cid":<int>,"room":<int>,"x":<int>,"y":<int>,"z":<int>,"hp":<int>}, ...]

   Fills `out[0..max_out-1]` and returns the number of entries written.
   ========================================================================= */

typedef struct
{
    int cid;     /* client ID                              */
    int room;    /* raw room ID (-1 = unknown)             */
    int x, y, z; /* last broadcast world-space position   */
    int has_pos; /* 1 if position has been received        */
    int ch;      /* character index: 0=Goemon 1=Ebisumaru 2=Sasuke 3=Yae */
} LobbyPlayer;

static int parse_lobby_positions(const char *json, LobbyPlayer *out, int max_out)
{
    int count = 0;
    if (!json || json[0] != '[')
        return 0;

    const char *p = json + 1;
    while (*p && count < max_out)
    {
        /* Skip whitespace / commas between objects. */
        while (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n')
            p++;
        if (*p != '{')
            break;
        p++;

        int cid = 0, room = -1, x = 0, y = 0, z = 0, hp = 0, ch = 0;

        while (*p && *p != '}')
        {
            /* Locate opening quote of the key. */
            while (*p && *p != '"' && *p != '}')
                p++;
            if (*p != '"')
                break;
            p++; /* skip '"' */

            /* Read key characters until closing '"'.
             * Keys are short ("cid","room","x","y","z","hp"); store first 4. */
            char key[5] = {0};
            int ki = 0;
            while (*p && *p != '"' && ki < 4)
                key[ki++] = *p++;
            while (*p && *p != '"')
                p++; /* drain any remaining key chars */
            if (*p == '"')
                p++;

            /* Skip ':' and optional spaces. */
            while (*p == ':' || *p == ' ')
                p++;

            /* Read signed integer value. */
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

            /* Assign to the right field. */
            if (key[0] == 'c' && key[1] == 'i' && key[2] == 'd')
                cid = val;
            else if (key[0] == 'r' && key[1] == 'o')
                room = val;
            else if (key[0] == 'x' && key[1] == '\0')
                x = val;
            else if (key[0] == 'y' && key[1] == '\0')
                y = val;
            else if (key[0] == 'z' && key[1] == '\0')
                z = val;
            else if (key[0] == 'h' && key[1] == 'p')
                hp = val;
            else if (key[0] == 'c' && key[1] == 'h' && key[2] == '\0')
                ch = val;
        }

        if (*p == '}')
            p++;

        /* Only record entries with a valid client ID. */
        if (cid > 0)
        {
            out[count].cid = cid;
            out[count].room = room;
            out[count].x = x;
            out[count].y = y;
            out[count].z = z;
            out[count].has_pos = hp;
            out[count].ch = (ch >= 0 && ch <= 3) ? ch : 0;
            count++;
        }
    }
    return count;
}

/* =========================================================================
   Actor definition safety filter

   Returns 1 when actor_id is safe to use as a standing phantom.
   We exclude actors that would teleport the player, kill them, give items,
   or otherwise cause havoc when spawned at an arbitrary world position.
   Everything else (signs, shopkeepers, NPCs, enemies) is considered safe
   enough for a temporary phantom – it keeps the actor code that is already
   loaded for this room.
   ========================================================================= */

/*
 * actor_id_must_exclude: truly unsafe actors that would harm the player,
 * teleport them, give items, or alter room state.  Never use these.
 */
static int actor_id_must_exclude(unsigned int aid)
{
    if (aid == 0x000U)
        return 1; /* null / terminator        */
    if (aid == 0x08cU)
        return 1; /* Exit – teleports player  */
    if (aid == 0x08eU)
        return 1; /* Death boundary           */
    if (aid == 0x085U)
        return 1; /* Gold Dango (pickup)      */
    if (aid == 0x088U)
        return 1; /* Silver Doll (pickup)     */
    if (aid == 0x089U)
        return 1; /* Gold Doll (pickup)       */
    if (aid == 0x091U)
        return 1; /* Surprise Pack (1-UP)     */
    if (aid == 0x193U)
        return 1; /* Key (pickup)             */
    if (aid == 0x190U || aid == 0x191U)
        return 1; /* Flying tiles (kills)  */
    if (aid == 0x3caU)
        return 1; /* Spike floor              */
    if (aid == 0x3fcU)
        return 1; /* Floor flamethrower       */
    if (aid >= 0x00caU && aid <= 0x00cfU)
        return 1; /* Boss event triggers  */
    return 0;
}

/*
 * actor_id_is_phantom_safe: preferred actors for phantoms – NPCs and
 * enemies that will just stand/animate harmlessly at an arbitrary position.
 * Excludes interactive objects (doors, cameras, etc.) that could confuse
 * or block the player even if not physically dangerous.
 */
static int actor_id_is_phantom_safe(unsigned int aid)
{
    if (actor_id_must_exclude(aid))
        return 0;
    /* Door actors – spawning extras creates duplicate locked / swinging doors */
    if (aid == 0x23cU || aid == 0x23eU)
        return 0;
    if (aid == 0x241U || aid == 0x242U)
        return 0;
    if (aid == 0x24dU || aid == 0x256U)
        return 0;
    if (aid == 0x31fU || aid == 0x321U || aid == 0x32fU)
        return 0;
    if (aid == 0x287U)
        return 0; /* Water                    */
    if (aid == 0x308U)
        return 0; /* Fixed camera             */
    if (aid == 0x3ccU)
        return 0; /* Minimap                  */
    return 1;
}

/* =========================================================================
   RECOMP_HOOK: room actor-setup  (fires BEFORE the original function)

   Creates one phantom ActorInstance for every player currently in the lobby:
     • Players in the same raw room as the local client → placed at their
       last server-reported world-space position.
     • Players in a different room (or whose position is not yet known) →
       placed at the HIDDEN_Y coordinate so they are not visible.

   On the next room load the positions are re-evaluated, so a player who
   enters the room will become visible and one who leaves will be hidden.
   ========================================================================= */

RECOMP_HOOK("func_8020D724_5C8BF4")
void anchor_actors_room_load(void)
{
    /*
     * Reset s_current_room_file_id so that RECOMP_HOOK_RETURN will free ALL
     * existing redirects if we bail out early (no players, no file, etc.).
     * It will be set to the real file_id at the end of successful injection.
     *
     * We do NOT call redirections_clear() here – that would free the previous
     * room's buffer while the original func_8020D724_5C8BF4 still needs it to
     * clean up the old room's phantom actors.  RECOMP_HOOK_RETURN handles the
     * cleanup once the original function has returned.
     */
    s_current_room_file_id = 0;

    if (!anchor_is_connected())
        return;

    if (!D_80231300_5EC7D0[D_800C7AB2])
        return;

    unsigned short file_id =
        (unsigned short)D_80231300_5EC7D0[D_800C7AB2]->actor_data_file_id;
    if (file_id == 0)
        return;

    /* Inform the Python layer of our current room BEFORE querying player
     * states so that get_lobby_positions_json() sees the up-to-date
     * _local_room_id when comparing room IDs.                          */
    anchor_set_local_room(D_800C7AB2);

    /* Fetch all online lobby members with their room IDs and positions. */
    char *json = anchor_get_lobby_positions_json();
    if (!json)
        return;

    LobbyPlayer players[MAX_PHANTOMS];
    int n = parse_lobby_positions(json, players, MAX_PHANTOMS);
    recomp_free(json);

    /* Nothing to do if no other players are online. */
    if (n == 0)
        return;

    /* Get the original file buffer before installing our redirect.
     * func_800141C4_14DC4 returns -1 (0xFFFFFFFF) when the file is not yet
     * present in the wave table (e.g. a transition / cutscene room).  A byte
     * read at that address XOR-3 maps to rdram_base+0x7FFFFFFC which is
     * outside RDRAM and causes KERN_PROTECTION_FAILURE.  Bail out on both
     * NULL (0) and not-found (-1). */
    int orig_int = func_800141C4_14DC4(file_id);
    if (orig_int == 0 || orig_int == -1)
        return;
    void *orig = (void *)orig_int;

    /* Allocate a writable copy and register the redirect. */
    unsigned char *buf =
        (unsigned char *)redirections_add(file_id, ACTOR_FILE_BUF_SIZE);
    if (!buf)
        return;

    for (int i = 0; i < ACTOR_FILE_BUF_SIZE; i++)
        buf[i] = ((unsigned char *)orig)[i];

    /* ------------------------------------------------------------------
     * Pick a single phantom definition from this room's actor list.
     *
     * Preference order:
     *   1. Any phantom-safe actor (safe NPC / enemy that won't harm player)
     *   2. Any non-lethal actor (last resort)
     *
     * One definition is written at PHANTOM_DEF_OFFSET and shared by all
     * phantoms regardless of which character the remote player is using.
     * ------------------------------------------------------------------ */
    unsigned int raw_ai_ptr =
        (unsigned int)D_80231300_5EC7D0[D_800C7AB2]->actor_instances;
    int def_count = 0;
    if (raw_ai_ptr >= 0x08000000U && raw_ai_ptr < 0x09000000U)
        def_count = (int)((raw_ai_ptr - 0x08000000U) /
                          (unsigned int)sizeof(ActorDefinition));

    ActorDefinition *phantom_def = (ActorDefinition *)(buf + PHANTOM_DEF_OFFSET);
    int found_def = 0;

    if (def_count > 0 && def_count <= 512)
    {
        ActorDefinition *room_defs = (ActorDefinition *)buf;

        /* Pass 1: any phantom-safe actor. */
        for (int di = 0; di < def_count && !found_def; di++)
        {
            unsigned int aid =
                (unsigned int)(unsigned short)(room_defs[di].data[0] >> 16);
            if (actor_id_is_phantom_safe(aid))
            {
                *phantom_def = room_defs[di];
                found_def = 1;
                recomp_printf("[anchor_actors] phantom actor 0x%04X\n", aid);
            }
        }
        /* Pass 2: any non-lethal actor (last resort). */
        for (int di = 0; di < def_count && !found_def; di++)
        {
            unsigned int aid =
                (unsigned int)(unsigned short)(room_defs[di].data[0] >> 16);
            if (!actor_id_must_exclude(aid))
            {
                *phantom_def = room_defs[di];
                found_def = 1;
                recomp_printf("[anchor_actors] phantom actor (resort) 0x%04X\n", aid);
            }
        }
    }

    if (!found_def)
    {
        recomp_printf("[anchor_actors] no usable actor in room 0x%03X – skipping\n",
                      (unsigned int)D_800C7AB2);
        return;
    }

    /* Resolve the ActorInstance array into our new copy.
     * Check for both NULL (0) and not-found (-1) return values. */
    int instances_int = func_80014840_15440(
        (int)D_80231300_5EC7D0[D_800C7AB2]->actor_instances, file_id);
    if (instances_int == 0 || instances_int == -1)
        return;
    ActorInstance *instances = (ActorInstance *)instances_int;

    /* Reset live-object tracking for phantoms in the new room. */
    for (int i = 0; i < MAX_PHANTOMS; i++)
        s_phantom_live_obj[i] = NULL;

    /* Count existing (room-native) instances. */
    int count = 0;
    while (instances[count].actor_definition != NULL)
        count++;

    /* ------------------------------------------------------------------
     * Inject one ActorInstance per lobby player.
     *
     * Rule: if the player's raw room ID matches the local room AND their
     * position has been received, place them at that position.  Otherwise
     * park them at the hidden Y coordinate so they are not visible.
     * ------------------------------------------------------------------ */
    int injected = 0;
    for (int i = 0; i < n && injected < MAX_PHANTOMS; i++)
    {
        ActorInstance *entry = &instances[count + injected];

        int same_room = (players[i].room == (int)D_800C7AB2);

        if (same_room && players[i].has_pos)
        {
            /* Visible: place at last known position. */
            entry->position.x = (short)players[i].x;
            entry->position.y = (short)players[i].y;
            entry->position.z = (short)players[i].z;
            recomp_printf("[anchor_actors] player cid=%d at (%d,%d,%d)\n",
                          players[i].cid,
                          players[i].x, players[i].y, players[i].z);
        }
        else
        {
            /* Hidden: park off-screen below the map. */
            entry->position.x = 0;
            entry->position.y = HIDDEN_Y;
            entry->position.z = 0;
            recomp_printf("[anchor_actors] player cid=%d hidden (room=0x%03X, hp=%d)\n",
                          players[i].cid, players[i].room, players[i].has_pos);
        }

        /* All phantoms share the single definition written at PHANTOM_DEF_OFFSET. */
        ActorDefinition *phantom_def_ptr =
            (ActorDefinition *)(0x08000000U + PHANTOM_DEF_OFFSET);

        entry->rotation.pitch = 0;
        entry->rotation.yaw = 0;
        entry->rotation.roll = 0;
        entry->actor_definition = phantom_def_ptr;
        entry->is_spawned = 0;
        entry->unk_11 = 0;
        entry->unk_12 = 0;
        entry->unk_13 = 0;

        /* Record spawn position for per-frame live update. */
        s_phantom_spawn_x[injected] = (same_room && players[i].has_pos)
                                          ? (float)players[i].x
                                          : 0.0f;
        s_phantom_spawn_y[injected] = (same_room && players[i].has_pos)
                                          ? (float)players[i].y
                                          : (float)HIDDEN_Y;
        s_phantom_spawn_z[injected] = (same_room && players[i].has_pos)
                                          ? (float)players[i].z
                                          : 0.0f;
        injected++;
    }

    /* Re-insert the null terminator after all new entries. */
    ActorInstance *term = &instances[count + injected];
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

    /* Record the file_id we just redirected so HOOK_RETURN preserves it. */
    s_current_room_file_id = file_id;

    /* Store phantom instance range for per-frame live updates. */
    s_phantom_start_idx = count;
    s_phantom_count = injected;
    s_update_timer = 0;

    recomp_printf(
        "[anchor_actors] room 0x%03X: injected %d phantom actor(s) for %d lobby player(s)\n",
        (unsigned int)D_800C7AB2, injected, n);
}

/* =========================================================================
   RECOMP_HOOK_RETURN: post-room-setup redirect cleanup

   Fires after func_8020D724_5C8BF4 returns.  By this point:
     • The original function has finished setting up the NEW room's actors.
     • The original function has finished cleaning up the OLD room's actors
       (which needed the old redirect to resolve phantom definition pointers).
   It is now safe to free every redirect EXCEPT the one that was just
   installed for the current room.
   ========================================================================= */

RECOMP_HOOK_RETURN("func_8020D724_5C8BF4")
void anchor_actors_room_load_return(void)
{
    redirections_init();
    for (int i = 0; i < WAVE_MAX; i++)
    {
        if (s_redirections[i].file_id != 0 &&
            s_redirections[i].file_id != s_current_room_file_id)
        {
            recomp_free(s_redirections[i].redirected_data);
            s_redirections[i].redirected_data = NULL;
            s_redirections[i].file_id = 0;
        }
    }
    /* Reset live-object cache since the old room's objects are gone. */
    for (int i = 0; i < MAX_PHANTOMS; i++)
        s_phantom_live_obj[i] = NULL;
}

/* =========================================================================
   RECOMP_HOOK_RETURN: per-frame real-time position update

   Fires every game frame after func_80002040_2C40 (the main per-frame tick).
   Every UPDATE_INTERVAL frames it:
     1. Polls the latest lobby positions from Python.
     2. Updates the ActorInstance position fields (for any future re-spawns).
     3. Walks the CLS_W linked list starting from the player's CLS_BG_W
        object to find our live phantom actors and update their world-space
        float positions directly, giving real-time movement.

   CLS_W layout (all world objects):
     [0x00] N64 vaddr of next node  (4 bytes, 0 = end of list)
     [0x04] kind / priority         (4 bytes)
     [0x08] float x                 (VEC3F_W)
     [0x0C] float y
     [0x10] float z
   ========================================================================= */

#define UPDATE_INTERVAL 20 /* frames between position refreshes (~3 Hz) */
/* stride and count of the CLS_BG_W object_heap pool in SYS_W */
#define CLSBGW_STRIDE 0x98U  /* sizeof(CLS_BG_W)  */
#define CLSBGW_POOL_SIZE 192 /* object_heap count */

RECOMP_HOOK_RETURN("func_80002040_2C40")
void anchor_actors_per_frame(void)
{
    if (s_phantom_count == 0)
        return;
    if (!anchor_is_connected())
        return;

    s_update_timer++;
    if (s_update_timer < UPDATE_INTERVAL)
        return;
    s_update_timer = 0;

    /* Guard: room metadata must still be set up. */
    if (!D_80231300_5EC7D0[D_800C7AB2])
        return;

    /* Fetch the latest positions from Python. */
    char *json = anchor_get_lobby_positions_json();
    if (!json)
        return;
    LobbyPlayer players[MAX_PHANTOMS];
    int n = parse_lobby_positions(json, players, MAX_PHANTOMS);
    recomp_free(json);

    /* Bail if player count changed – wait for next room load to re-sync. */
    if (n != s_phantom_count)
        return;

    /* ---- Step 1: update ActorInstance positions (used for future spawns) ---- */
    if (s_current_room_file_id != 0)
    {
        int inst_int = func_80014840_15440(
            (int)D_80231300_5EC7D0[D_800C7AB2]->actor_instances,
            s_current_room_file_id);
        if (inst_int != 0 && inst_int != -1)
        {
            ActorInstance *instances = (ActorInstance *)inst_int;
            for (int i = 0; i < n; i++)
            {
                ActorInstance *entry = &instances[s_phantom_start_idx + i];
                int same_room = (players[i].room == (int)D_800C7AB2);
                if (same_room && players[i].has_pos)
                {
                    entry->position.x = (short)players[i].x;
                    entry->position.y = (short)players[i].y;
                    entry->position.z = (short)players[i].z;
                }
                else
                {
                    entry->position.x = 0;
                    entry->position.y = HIDDEN_Y;
                    entry->position.z = 0;
                }
            }
        }
    }

    /* ---- Step 2: scan CLS_BG_W object pool to update live positions --------
     *
     * All CLS_BG_W objects live in object_heap[192] – a contiguous array with
     * stride sizeof(CLS_BG_W) = CLSBGW_STRIDE (0x98) bytes.  The player's
     * object bg_w is one slot in this pool.  Walking ±(CLSBGW_POOL_SIZE-1)
     * strides from bg_w covers every pool slot without needing the global
     * CLS_W linked-list head (which the player may be at the tail of).
     * CLS_BG_W layout:
     *   +0x00  CLS_W* next (list link, 4B)
     *   +0x04  unsigned char kind  ← 0 = inactive slot
     *   +0x08  float tx  (world X)
     *   +0x0C  float ty  (world Y)
     *   +0x10  float tz  (world Z)
     * -------------------------------------------------------------------- */
    void *player_hw = D_801FC60C_5B851C;
    if (!player_hw)
        return;

    for (int i = 0; i < n; i++)
    {
        int same_room = (players[i].room == (int)D_800C7AB2);
        float new_x = (same_room && players[i].has_pos) ? (float)players[i].x : 0.0f;
        float new_y = (same_room && players[i].has_pos) ? (float)players[i].y : (float)HIDDEN_Y;
        float new_z = (same_room && players[i].has_pos) ? (float)players[i].z : 0.0f;

        if (s_phantom_live_obj[i] != NULL)
        {
            /* Cached live pointer – update directly. */
            *(float *)((char *)s_phantom_live_obj[i] + 0x08) = new_x;
            *(float *)((char *)s_phantom_live_obj[i] + 0x0C) = new_y;
            *(float *)((char *)s_phantom_live_obj[i] + 0x10) = new_z;
            s_phantom_spawn_x[i] = new_x;
            s_phantom_spawn_y[i] = new_y;
            s_phantom_spawn_z[i] = new_z;
            continue;
        }

        /* Search for an active pool entry near this phantom's spawn position. */
        float ex = s_phantom_spawn_x[i];
        float ey = s_phantom_spawn_y[i];
        float ez = s_phantom_spawn_z[i];

        unsigned char *base = (unsigned char *)player_hw;
        int ph_found = 0;
        for (int d = -(CLSBGW_POOL_SIZE - 1); d < CLSBGW_POOL_SIZE && !ph_found; d++)
        {
            unsigned char *cand = base + d * (int)CLSBGW_STRIDE;
            /* Validate plausible N64 RDRAM address */
            unsigned int caddr = (unsigned int)(void *)cand;
            if (caddr < 0x80010000U || caddr >= 0x80800000U)
                continue;
            /* Skip inactive slots (CLS_W.kind at +0x04) */
            if (cand[4] == 0)
                continue;
            /* Skip the player's own slot */
            if ((void *)cand == player_hw)
                continue;
            float cx = *(float *)(cand + 0x08);
            float cy = *(float *)(cand + 0x0C);
            float cz = *(float *)(cand + 0x10);
            float dx = cx - ex;
            if (dx < 0)
                dx = -dx;
            float dy = cy - ey;
            if (dy < 0)
                dy = -dy;
            float dz = cz - ez;
            if (dz < 0)
                dz = -dz;
            if (dx < 300.0f && dy < 300.0f && dz < 300.0f)
            {
                s_phantom_live_obj[i] = (void *)cand;
                *(float *)(cand + 0x08) = new_x;
                *(float *)(cand + 0x0C) = new_y;
                *(float *)(cand + 0x10) = new_z;
                s_phantom_spawn_x[i] = new_x;
                s_phantom_spawn_y[i] = new_y;
                s_phantom_spawn_z[i] = new_z;
                ph_found = 1;
            }
        }
    }
}
