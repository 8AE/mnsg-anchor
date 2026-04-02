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

        int cid = 0, room = -1, x = 0, y = 0, z = 0, hp = 0;

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

static int actor_id_is_phantom_safe(unsigned int aid)
{
    if (aid == 0x000U)
        return 0; /* null terminator          */
    if (aid == 0x08cU)
        return 0; /* Exit – teleports player  */
    if (aid == 0x08eU)
        return 0; /* Death boundary           */
    if (aid == 0x085U)
        return 0; /* Gold Dango (pickup)      */
    if (aid == 0x088U)
        return 0; /* Silver Doll (pickup)     */
    if (aid == 0x193U)
        return 0; /* Key (pickup)             */
    if (aid == 0x190U || aid == 0x191U)
        return 0; /* Flying tiles (kills)     */
    if (aid >= 0x23cU && aid <= 0x270U)
        return 0; /* Door actors              */
    if (aid == 0x287U)
        return 0; /* Water                    */
    if (aid == 0x308U)
        return 0; /* Fixed camera             */
    if (aid == 0x3caU)
        return 0; /* Spike floor              */
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
     * Find a safe actor definition already present in this room's file.
     *
     * Room actor-data files pack their definitions starting at file offset 0,
     * 16 bytes (sizeof ActorDefinition) per entry.  The instance array begins
     * immediately after, at the in-file offset stored in actor_instances.
     * We use that offset to count definitions, then scan for the first entry
     * whose actor_id won't harm the player or cause a broken spawn.  The
     * chosen bytes are COPIED into our buffer at PHANTOM_DEF_OFFSET so the
     * phantom references only our copy – not a stale pointer into the
     * original file.
     *
     * Using a room-native actor_id guarantees its overlay code is loaded for
     * this room (unlike a hard-coded ID like 0x02BE that only exists in rooms
     * with the Old Man overlay).
     * ------------------------------------------------------------------ */
    ActorDefinition *custom_def = (ActorDefinition *)(buf + PHANTOM_DEF_OFFSET);
    {
        unsigned int raw_ai_ptr =
            (unsigned int)D_80231300_5EC7D0[D_800C7AB2]->actor_instances;
        int def_count = 0;
        if (raw_ai_ptr >= 0x08000000U && raw_ai_ptr < 0x09000000U)
            def_count = (int)((raw_ai_ptr - 0x08000000U) /
                              (unsigned int)sizeof(ActorDefinition));

        int found_safe = 0;
        if (def_count > 0 && def_count <= 512)
        {
            ActorDefinition *room_defs = (ActorDefinition *)buf;
            for (int di = 0; di < def_count && !found_safe; di++)
            {
                unsigned int aid =
                    (unsigned int)(unsigned short)(room_defs[di].data[0] >> 16);
                if (actor_id_is_phantom_safe(aid))
                {
                    custom_def->data[0] = room_defs[di].data[0];
                    custom_def->data[1] = room_defs[di].data[1];
                    custom_def->data[2] = room_defs[di].data[2];
                    custom_def->data[3] = room_defs[di].data[3];
                    found_safe = 1;
                    recomp_printf("[anchor_actors] phantom actor_id=0x%04X (room def %d)\n",
                                  (unsigned int)aid, di);
                }
            }
        }
        if (!found_safe)
        {
            recomp_printf("[anchor_actors] no safe actor in room 0x%03X – skipping phantoms\n",
                          (unsigned int)D_800C7AB2);
            return;
        }
    }

    ActorDefinition *phantom_def_ptr =
        (ActorDefinition *)(0x08000000U + PHANTOM_DEF_OFFSET);

    /* Resolve the ActorInstance array into our new copy.
     * Check for both NULL (0) and not-found (-1) return values. */
    int instances_int = func_80014840_15440(
        (int)D_80231300_5EC7D0[D_800C7AB2]->actor_instances, file_id);
    if (instances_int == 0 || instances_int == -1)
        return;
    ActorInstance *instances = (ActorInstance *)instances_int;

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

        entry->rotation.pitch = 0;
        entry->rotation.yaw = 0;
        entry->rotation.roll = 0;
        entry->actor_definition = phantom_def_ptr;
        entry->is_spawned = 0;
        entry->unk_11 = 0;
        entry->unk_12 = 0;
        entry->unk_13 = 0;
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
}
