/**
 * @file anchor.c
 * @brief Anchor co-op client – C-side implementation for MNSG: Recompiled.
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
     anchor_room_id       String  room to join
     anchor_player_name   String  display name
     anchor_team_id       String  team within the room
   ========================================================================= */

REPY_ON_POST_INIT void anchor_init(void)
{
    /* Preload the Python module so the first real call is instant.
     * Connection is now handled by the startup UI in anchor_connect_ui.c –
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
                   unsigned int client_id, const char *team_id)
{
    REPY_FN_SETUP;

    REPY_FN_SET_STR("host", (host && host[0]) ? host : "");
    REPY_FN_SET_S32("port", port);
    REPY_FN_SET_STR("room_id", (room_id && room_id[0]) ? room_id : "");
    REPY_FN_SET_STR("player_name", (player_name && player_name[0]) ? player_name : "Player");
    REPY_FN_SET_U32("client_id", client_id);
    REPY_FN_SET_STR("team_id", (team_id && team_id[0]) ? team_id : "default");

    REPY_FN_EXEC_CACHE(anchor_connect_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.connect(\n"
                       "    host, port, room_id, player_name, client_id, team_id\n"
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

int anchor_set_team(const char *team_id)
{
    REPY_FN_SETUP;
    REPY_FN_SET_STR("team_id", (team_id && team_id[0]) ? team_id : "default");
    REPY_FN_EXEC_CACHE(anchor_set_team_code,
                       "import anchor_mnsg\n"
                       "result = anchor_mnsg.set_team(team_id)\n");
    int result = (int)REPY_FN_GET_BOOL("result");
    REPY_FN_CLEANUP;
    return result;
}

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
