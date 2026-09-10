/**
 * @file anchor.c
 * @brief Anchor multiplayer client – C-side implementation for MNSG: Recompiled.
 *
 * This file provides the C API defined in include/anchor.h.  Every function
 * uses the REPY_FN macro collection to call the corresponding function in the
 * anchor_mnsg Python module (py/anchor_mnsg.py) through the REPY extlib.
 *
 * Build requirements
 * ------------------
 * 1. Place repy_api.h (from the REPY 2.0.0 release) in include/repy_api.h.
 *    Download: https://github.com/LT-Schmiddy/zelda64recomp-python-extlibs-mod/
 *              blob/release-2.0.0/include_in_dependents/repy_api.h
 *
 * 2. mod.toml must list "RecompExternalPython:2.0.0" in dependencies[].
 *
 * 3. py/anchor_mnsg.py must be listed in additional_files[] in mod.toml so it
 *    is included in the .nrm archive.
 *
 * Pattern used throughout
 * -----------------------
 *   REPY_FN_SETUP;                   // create Python scope
 *   REPY_FN_SET_*(name, c_value);    // push C values into the scope
 *   REPY_FN_EXEC_CACHE(id, code);    // run cached Python code in the scope
 *   result = REPY_FN_GET_*(name);    // pull result out of the scope
 *   REPY_FN_CLEANUP;                 // release scope (while result is safe)
 *   return result;
 *
 * All strings returned from REPY_FN_GET_STR are allocated in recompiled memory
 * and must be freed with recomp_free() by the caller.
 */

#include "modding.h"
#include "recomputils.h" /* also defines NULL and recomp_get_mod_file_path */
#include "anchor.h"
#include "anchor_player_models.h"
#include "repy_api.h"

/* =========================================================================
   Module registration
   =========================================================================
   REPY_PREINIT_ADD_NRM_TO_ALL_INTERPRETERS places this .nrm on Python's
   sys.path before the interpreter starts, enabling:
       import anchor_mnsg
   from within any REPY_FN scope in this mod.
   Place this macro exactly once in the whole mod (one C file only).
   ========================================================================= */

REPY_PREINIT_ADD_NRM_TO_ALL_INTERPRETERS;

/* =========================================================================
   On REPY init: preload the Python module and, if the user has enabled
   auto-connect, read the config values and connect to the Anchor server.

   Config keys (all in mod.toml manifest.config_options):
     anchor_auto_connect  Enum  index 0 = "Enabled", 1 = "Disabled"
     anchor_host          String  server hostname / IP
     anchor_port          Number  TCP port (stored as double)
     anchor_room_id_new       String  room to join
     anchor_player_name   String  display name
   ========================================================================= */

REPY_ON_POST_INIT void anchor_init(void)
{
    /* Preload the Python module so the first real call is instant.
     * Connection is now handled by the startup UI in startup_menu.c and
     * startup_multiplayer_ui.c –
     * the player fills in their settings interactively before the game
     * begins, rather than reading values from the mod config file.         */
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_preload_code, "import anchor_mnsg\n");
    REPY_FN_CLEANUP;
}

/* =========================================================================
   Connection management
   ========================================================================= */

int anchor_connect(const char *host, int port,
                   const char *room_id, const char *player_name,
                   unsigned int client_id)
{
    REPY_FN_SETUP;

    REPY_FN_SET_STR("host", (host && host[0]) ? host : "");
    REPY_FN_SET_S32("port", port);
    REPY_FN_SET_STR("room_id", (room_id && room_id[0]) ? room_id : "");
    REPY_FN_SET_STR("player_name", (player_name && player_name[0]) ? player_name : "Player");
    REPY_FN_SET_U32("client_id", client_id);

    REPY_FN_EXEC_CACHE(anchor_connect_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.connect(\n"
                       "    host, port, room_id, player_name, client_id\n"
                       ")\n");

    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

void anchor_disconnect(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_disconnect_code,
                       "import anchor_mnsg\n"
                       "anchor_mnsg.disconnect()\n");
    REPY_FN_CLEANUP;
}

int anchor_is_connected(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_is_connected_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.is_connected()\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_is_disabled(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_is_disabled_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.is_disabled()\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

/* =========================================================================
   Client identity
   ========================================================================= */

unsigned int anchor_get_client_id(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_client_id_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_client_id()\n");
    unsigned int result = (unsigned int)REPY_FN_GET_U32("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_room_id(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_room_id_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_room_id()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

char *anchor_get_team_id(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_team_id_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_team_id()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

/* =========================================================================
   Packet polling
   ========================================================================= */

int anchor_has_packet(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_has_packet_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.has_packet()\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_poll_packet(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_poll_packet_code,
                       "import anchor_mnsg\n"
                       "packet = anchor_mnsg.poll_packet()\n");
    char *out = REPY_FN_GET_STR("packet");
    REPY_FN_CLEANUP;

    /* Python returns "" when the queue is empty – convert to NULL for C. */
    if (out != NULL && out[0] == '\0')
    {
        recomp_free(out);
        return NULL;
    }
    return out; /* caller must recomp_free() */
}

char *anchor_get_server_message(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_server_message_code,
                       "import anchor_mnsg\n"
                       "msg = anchor_mnsg.get_server_message()\n");
    char *out = REPY_FN_GET_STR("msg");
    REPY_FN_CLEANUP;

    if (out != NULL && out[0] == '\0')
    {
        recomp_free(out);
        return NULL;
    }
    return out; /* caller must recomp_free() */
}

/* =========================================================================
   State updates
   ========================================================================= */

int anchor_update_client_state(const char *state_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("state_json", (state_json && state_json[0]) ? state_json : "{}");
    REPY_FN_EXEC_CACHE(anchor_update_client_state_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.update_client_state(state_json)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_local_room(unsigned int room_id)
{
    REPY_FN_SETUP;
    REPY_FN_SET_U32("room_id", room_id);
    REPY_FN_EXEC_CACHE(anchor_set_local_room_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_local_room(room_id)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_enemy_room_state(unsigned int room_id, unsigned int signature,
                                const char *bits)
{
    REPY_FN_SETUP;
    REPY_FN_SET_U32("room_id", room_id);
    REPY_FN_SET_U32("signature", signature);
    REPY_FN_SET_STR("bits", bits ? bits : "");
    REPY_FN_EXEC_CACHE(anchor_set_enemy_room_state_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_enemy_room_state(\n"
                       "    room_id, signature, bits\n"
                       ")\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_enemy_room_state(unsigned int room_id, unsigned int signature)
{
    REPY_FN_SETUP;
    REPY_FN_SET_U32("room_id", room_id);
    REPY_FN_SET_U32("signature", signature);
    REPY_FN_EXEC_CACHE(anchor_get_enemy_room_state_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_enemy_room_state(room_id, signature)\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

unsigned int anchor_get_enemy_authority(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_enemy_authority_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_enemy_authority()\n");
    unsigned int result = (unsigned int)REPY_FN_GET_U32("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_world_map_location(unsigned int room_id,
                                  float world_x, float world_z)
{
    REPY_FN_SETUP;
    REPY_FN_SET_U32("room_id", room_id);
    REPY_FN_SET_F32("world_x", world_x);
    REPY_FN_SET_F32("world_z", world_z);
    REPY_FN_EXEC_CACHE(anchor_set_world_map_location_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_world_map_location(\n"
                       "    room_id, world_x, world_z\n"
                       ")\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_update_boss_arena(int arena, int visit, int stage,
                             unsigned int field90, unsigned int field91)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("arena", arena);
    REPY_FN_SET_S32("visit", visit);
    REPY_FN_SET_S32("stage", stage);
    REPY_FN_SET_U32("field90", field90);
    REPY_FN_SET_U32("field91", field91);
    REPY_FN_EXEC_CACHE(anchor_update_boss_arena_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.set_boss_arena(arena, visit, stage, field90, field91)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_boss_invitation_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_boss_invitation_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.get_boss_invitation_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_boss_invitation_is_current(int cid, int session, int sequence)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("cid", cid);
    REPY_FN_SET_S32("session", session);
    REPY_FN_SET_S32("sequence", sequence);
    REPY_FN_EXEC_CACHE(anchor_boss_invitation_current_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.boss_invitation_is_current(cid, session, sequence)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

void anchor_dismiss_boss_invitation(int cid, int session, int sequence)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("cid", cid);
    REPY_FN_SET_S32("session", session);
    REPY_FN_SET_S32("sequence", sequence);
    REPY_FN_EXEC_CACHE(anchor_dismiss_boss_invitation_code,
                      "import anchor_mnsg\n"
                      "anchor_mnsg.dismiss_boss_invitation(cid, session, sequence)\n");
    REPY_FN_CLEANUP;
}

char *anchor_congo_update(int ready, unsigned int visit, int paused,
                          const char *state_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("ready", ready);
    REPY_FN_SET_U32("visit", visit);
    REPY_FN_SET_S32("paused", paused);
    REPY_FN_SET_STR("state_json", state_json ? state_json : "null");
    REPY_FN_EXEC_CACHE(anchor_congo_update_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.update_congo(ready, visit, paused, state_json)\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_congo_hit(int sequence, int amount)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("sequence", sequence);
    REPY_FN_SET_S32("amount", amount);
    REPY_FN_EXEC_CACHE(anchor_send_congo_hit_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.send_congo_hit(sequence, amount)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_tsurami_update(int ready, unsigned int visit, int paused,
                          const char *state_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("ready", ready);
    REPY_FN_SET_U32("visit", visit);
    REPY_FN_SET_S32("paused", paused);
    REPY_FN_SET_STR("state_json", state_json ? state_json : "null");
    REPY_FN_EXEC_CACHE(anchor_tsurami_update_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.update_tsurami(ready, visit, paused, state_json)\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_tsurami_hit(int sequence, int amount, unsigned int target)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("sequence", sequence);
    REPY_FN_SET_S32("amount", amount);
    REPY_FN_SET_U32("target", target);
    REPY_FN_EXEC_CACHE(anchor_send_tsurami_hit_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.send_tsurami_hit(sequence, amount, target)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_dharumanyo_update(int ready, unsigned int visit, int paused,
                               const char *state_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("ready", ready);
    REPY_FN_SET_U32("visit", visit);
    REPY_FN_SET_S32("paused", paused);
    REPY_FN_SET_STR("state_json", state_json ? state_json : "null");
    REPY_FN_EXEC_CACHE(anchor_dharumanyo_update_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.update_dharumanyo(ready, visit, paused, state_json)\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_dharumanyo_hit(int sequence)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("sequence", sequence);
    REPY_FN_EXEC_CACHE(anchor_send_dharumanyo_hit_code,
                      "import anchor_mnsg\n"
                      "result = anchor_mnsg.send_dharumanyo_hit(sequence)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_position(int pos_x, int pos_y, int pos_z)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("pos_x", pos_x);
    REPY_FN_SET_S32("pos_y", pos_y);
    REPY_FN_SET_S32("pos_z", pos_z);
    REPY_FN_EXEC_CACHE(anchor_set_position_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_position(pos_x, pos_y, pos_z)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_position_anim(int pos_x, int pos_y, int pos_z,
                             int action, int frame_100, int frame_count_100,
                             int rot_x, int rot_y, int rot_z,
                             int appearance_flags, int velocity_x,
                             int velocity_y, int velocity_z,
                             int angular_velocity_x,
                             int angular_velocity_y,
                             int angular_velocity_z,
                             int force_motion_edge,
                             int animation_step_100,
                             int has_animation_step,
                             int collision_disabled,
                             int drive_x, int drive_z, int player_epoch)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("pos_x", pos_x);
    REPY_FN_SET_S32("pos_y", pos_y);
    REPY_FN_SET_S32("pos_z", pos_z);
    REPY_FN_SET_S32("action", action);
    REPY_FN_SET_S32("frame_100", frame_100);
    REPY_FN_SET_S32("frame_count_100", frame_count_100);
    REPY_FN_SET_S32("rot_x", rot_x);
    REPY_FN_SET_S32("rot_y", rot_y);
    REPY_FN_SET_S32("rot_z", rot_z);
    REPY_FN_SET_S32("appearance_flags", appearance_flags);
    REPY_FN_SET_S32("velocity_x", velocity_x);
    REPY_FN_SET_S32("velocity_y", velocity_y);
    REPY_FN_SET_S32("velocity_z", velocity_z);
    REPY_FN_SET_S32("angular_velocity_x", angular_velocity_x);
    REPY_FN_SET_S32("angular_velocity_y", angular_velocity_y);
    REPY_FN_SET_S32("angular_velocity_z", angular_velocity_z);
    REPY_FN_SET_S32("force_motion_edge", force_motion_edge);
    REPY_FN_SET_S32("animation_step_100", animation_step_100);
    REPY_FN_SET_S32("has_animation_step", has_animation_step);
    REPY_FN_SET_S32("collision_disabled", collision_disabled);
    REPY_FN_SET_S32("drive_x", drive_x);
    REPY_FN_SET_S32("drive_z", drive_z);
    REPY_FN_SET_S32("player_epoch", player_epoch);
    REPY_FN_EXEC_CACHE(anchor_set_position_anim_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_position_anim(\n"
                       "    pos_x, pos_y, pos_z, action, frame_100,\n"
                       "    frame_count_100, rot_x, rot_y, rot_z,\n"
                       "    appearance_flags, velocity_x, velocity_y, velocity_z,\n"
                       "    angular_velocity_x, angular_velocity_y, angular_velocity_z,\n"
                       "    force_motion_edge, animation_step_100, has_animation_step,\n"
                       "    collision_disabled, drive_x, drive_z, player_epoch\n"
                       ")\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_get_projectile_session(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_projectile_session_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_projectile_session()\n");
    int result = (int)REPY_FN_GET_S32("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_projectile_spawn_json(int session, int owner_epoch, const char *event_json)
{
    if (!event_json)
        return 0;
    REPY_FN_SETUP;
    REPY_FN_SET_S32("session", session);
    REPY_FN_SET_S32("owner_epoch", owner_epoch);
    REPY_FN_SET_STR("event_json", event_json);
    REPY_FN_EXEC_CACHE(anchor_send_projectile_spawn_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.send_projectile_spawn_json(session, owner_epoch, event_json)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_projectile_spawns_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("expected_epoch", anchor_player_models_get_epoch());
    REPY_FN_EXEC_CACHE(anchor_get_projectile_spawns_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_projectile_spawns_json(expected_epoch)\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

int anchor_ack_projectile_spawn(int cid, int session, int epoch, int event_id)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("cid", cid);
    REPY_FN_SET_S32("session", session);
    REPY_FN_SET_S32("epoch", epoch);
    REPY_FN_SET_S32("event_id", event_id);
    REPY_FN_EXEC_CACHE(anchor_ack_projectile_spawn_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.ack_projectile_spawn(cid, session, epoch, event_id)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_player_hit(int target_cid, int target_epoch,
                           float hit_x, float hit_y, float hit_z)
{
    REPY_FN_SETUP;
    REPY_FN_SET_S32("target_cid", target_cid);
    REPY_FN_SET_S32("target_epoch", target_epoch);
    REPY_FN_SET_S32("source_epoch", anchor_player_models_get_epoch());
    REPY_FN_SET_F32("hit_x", hit_x);
    REPY_FN_SET_F32("hit_y", hit_y);
    REPY_FN_SET_F32("hit_z", hit_z);
    REPY_FN_EXEC_CACHE(anchor_send_player_hit_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.send_player_hit(\n"
                       "    target_cid, target_epoch, hit_x, hit_y, hit_z, source_epoch)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_poll_player_hit(int *sender_cid, int *target_epoch,
                           float *x, float *y, float *z)
{
    if (!sender_cid || !target_epoch || !x || !y || !z)
        return 0;
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_poll_player_hit_code,
                       "import anchor_mnsg\n"
                       "hit = anchor_mnsg.poll_player_hit()\n"
                       "has_hit = hit is not None\n"
                       "if has_hit:\n"
                       "    sender_cid, target_epoch, hit_x, hit_y, hit_z = hit\n");
    int result = (int)REPY_FN_GET_BOOL("has_hit");
    if (result)
    {
        *sender_cid = (int)REPY_FN_GET_S32("sender_cid");
        *target_epoch = (int)REPY_FN_GET_S32("target_epoch");
        *x = REPY_FN_GET_F32("hit_x");
        *y = REPY_FN_GET_F32("hit_y");
        *z = REPY_FN_GET_F32("hit_z");
    }
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_player_sounds(int interaction_session, int player_epoch,
                              const unsigned short *sound_ids,
                              int sound_count)
{
    unsigned int sounds[8] = {0};
    int i;
    if (interaction_session <= 0 || player_epoch <= 0 || !sound_ids ||
        sound_count < 1 || sound_count > 8)
        return 0;
    for (i = 0; i < sound_count; ++i)
        sounds[i] = sound_ids[i];

    REPY_FN_SETUP;
    REPY_FN_SET_S32("interaction_session", interaction_session);
    REPY_FN_SET_S32("player_epoch", player_epoch);
    REPY_FN_SET_S32("sound_count", sound_count);
    REPY_FN_SET_U32("sound_0", sounds[0]);
    REPY_FN_SET_U32("sound_1", sounds[1]);
    REPY_FN_SET_U32("sound_2", sounds[2]);
    REPY_FN_SET_U32("sound_3", sounds[3]);
    REPY_FN_SET_U32("sound_4", sounds[4]);
    REPY_FN_SET_U32("sound_5", sounds[5]);
    REPY_FN_SET_U32("sound_6", sounds[6]);
    REPY_FN_SET_U32("sound_7", sounds[7]);
    REPY_FN_EXEC_CACHE(anchor_send_player_sounds_code,
                       "import anchor_mnsg\n"
                       "sound_ids = [sound_0, sound_1, sound_2, sound_3, "
                       "sound_4, sound_5, sound_6, sound_7][:sound_count]\n"
                       "result = anchor_mnsg.send_player_sounds(\n"
                       "    interaction_session, player_epoch, sound_ids)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_poll_player_sound(int *sender_cid, int *sender_session,
                             int *sender_epoch, unsigned int *sound_id,
                             unsigned int *remaining_ms)
{
    if (!sender_cid || !sender_session || !sender_epoch || !sound_id ||
        !remaining_ms)
        return 0;
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_poll_player_sound_code,
                       "import anchor_mnsg\n"
                       "sound = anchor_mnsg.poll_player_sound()\n"
                       "has_sound = sound is not None\n"
                       "if has_sound:\n"
                       "    sender_cid, sender_session, sender_epoch, "
                       "sound_id, remaining_ms = sound\n");
    int result = (int)REPY_FN_GET_BOOL("has_sound");
    if (result)
    {
        *sender_cid = (int)REPY_FN_GET_S32("sender_cid");
        *sender_session = (int)REPY_FN_GET_S32("sender_session");
        *sender_epoch = (int)REPY_FN_GET_S32("sender_epoch");
        *sound_id = (unsigned int)REPY_FN_GET_U32("sound_id");
        *remaining_ms = (unsigned int)REPY_FN_GET_U32("remaining_ms");
    }
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_character(const char *char_name)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("char_name", (char_name && char_name[0]) ? char_name : "Goemon");
    REPY_FN_EXEC_CACHE(anchor_set_character_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_character(char_name)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_save_loaded(int is_loaded)
{
    REPY_FN_SETUP;
    REPY_FN_SET_BOOL("is_loaded", is_loaded != 0);
    REPY_FN_EXEC_CACHE(anchor_set_save_loaded_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_save_loaded(is_loaded)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

/* =========================================================================
   Team / save state sync
   ========================================================================= */

int anchor_request_team_state(const char *team_id)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("team_id", (team_id && team_id[0]) ? team_id : "");
    REPY_FN_EXEC_CACHE(anchor_request_team_state_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.request_team_state(team_id)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_update_team_state(const char *state_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("state_json", (state_json && state_json[0]) ? state_json : "{}");
    REPY_FN_EXEC_CACHE(anchor_update_team_state_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.update_team_state(state_json)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

/* =========================================================================
   Flag / check sending
   ========================================================================= */

int anchor_send_flag(const char *flag_name, int flag_value, int add_to_queue)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("flag_name", (flag_name && flag_name[0]) ? flag_name : "");
    REPY_FN_SET_S32("flag_value", flag_value);
    REPY_FN_SET_BOOL("add_to_queue", add_to_queue != 0);
    REPY_FN_EXEC_CACHE(anchor_send_flag_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.send_flag(flag_name, flag_value, add_to_queue)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

/* =========================================================================
   Custom packets
   ========================================================================= */

int anchor_send_custom_packet(const char *packet_type, const char *payload_json,
                              const char *target_team_id, unsigned int target_client_id,
                              int add_to_queue)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("packet_type", (packet_type && packet_type[0]) ? packet_type : "CUSTOM");
    REPY_FN_SET_STR("payload_json", (payload_json && payload_json[0]) ? payload_json : "{}");
    REPY_FN_SET_STR("target_team_id", (target_team_id && target_team_id[0]) ? target_team_id : "");
    REPY_FN_SET_U32("target_client_id", target_client_id);
    REPY_FN_SET_BOOL("add_to_queue", add_to_queue != 0);
    REPY_FN_EXEC_CACHE(anchor_send_custom_packet_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.send_custom_packet(\n"
                       "    packet_type, payload_json,\n"
                       "    target_team_id, target_client_id,\n"
                       "    add_to_queue\n"
                       ")\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_send_raw_packet(const char *packet_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("packet_json", (packet_json && packet_json[0]) ? packet_json : "{}");
    REPY_FN_EXEC_CACHE(anchor_send_raw_packet_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.send_packet(packet_json)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

/* =========================================================================
   Misc
   ========================================================================= */

int anchor_send_game_complete(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_send_game_complete_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.send_game_complete()\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_request_stats(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_request_stats_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_stats()\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

/* =========================================================================
   Player list
   ========================================================================= */

char *anchor_get_player_names_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_player_names_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_player_names_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

char *anchor_get_player_info_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_player_info_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_player_info_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

char *anchor_get_transfer_target_json(unsigned int client_id)
{
    REPY_FN_SETUP;
    REPY_FN_SET_U32("client_id", client_id);
    REPY_FN_EXEC_CACHE(anchor_get_transfer_target_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_transfer_target_json(client_id)\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

char *anchor_get_teammate_positions_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_teammate_positions_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_teammate_positions_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

char *anchor_get_lobby_positions_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_lobby_positions_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_lobby_positions_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result; /* caller must recomp_free() */
}

int anchor_set_race_lobby_state(const char *status, const char *config_json)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("status", (status && status[0]) ? status : "lobby");
    REPY_FN_SET_STR("config_json", (config_json && config_json[0]) ? config_json : "");
    REPY_FN_EXEC_CACHE(anchor_set_race_lobby_state_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_race_lobby_state(status, config_json)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_race_lobby_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_race_lobby_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_race_lobby_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

unsigned int anchor_get_race_host_id(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_race_host_id_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_race_host_id()\n");
    unsigned int result = (unsigned int)REPY_FN_GET_U32("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_race_has_started(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_race_has_started_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.race_has_started()\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_host_race_config_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_host_race_config_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_host_race_config_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_race_finish_payload_json(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_race_finish_payload_json_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_race_finish_payload_json()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}

int anchor_set_clipboard_text(const char *text)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("text", (text && text[0]) ? text : "");
    REPY_FN_EXEC_CACHE(anchor_set_clipboard_text_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_clipboard_text(text)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

char *anchor_get_clipboard_text(void)
{
    REPY_FN_SETUP;
    REPY_FN_EXEC_CACHE(anchor_get_clipboard_text_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.get_clipboard_text()\n");
    char *result = REPY_FN_GET_STR("result");
    REPY_FN_CLEANUP;
    return result;
}
