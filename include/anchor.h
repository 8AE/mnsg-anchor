#ifndef ANCHOR_H
#define ANCHOR_H

/**
 * @file anchor.h
 * @brief Anchor co-op client API for Mystical Ninja Starring Goemon: Recompiled.
 *
 * Anchor is a TCP-based co-op/multiplayer service.
 * https://github.com/garrettjoecox/anchor
 *
 * This API is a thin C wrapper around the Python anchor_mnsg module, which is
 * executed through the REPY (RecompExternalPython) extlib.
 * https://github.com/LT-Schmiddy/zelda64recomp-python-extlibs-mod
 *
 * Quick-start
 * -----------
 * 1. Call anchor_connect() once (e.g. from a menu action or mod init hook).
 * 2. Call anchor_poll_packet() every game frame to receive incoming packets.
 *    - Parse the returned JSON string to react to game events from teammates.
 * 3. Call anchor_update_client_state() whenever your game state changes.
 * 4. Call anchor_send_flag() when the player earns a check.
 * 5. Call anchor_disconnect() on quit or when the player chooses to leave.
 *
 * All strings passed to / returned from these functions are UTF-8 encoded.
 * Returned char* values are allocated in recompiled memory with recomp_alloc
 * and must be freed by the caller with recomp_free().
 */

#include "modding.h"
#include "recomputils.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* =========================================================================
       Connection management
       ========================================================================= */

    /**
     * @brief Connect to an Anchor server and send the HANDSHAKE packet.
     *
     * @param host        Server hostname or IP address.
     *                    Pass "" or NULL to use the public default (anchor.hm64.org).
     * @param port        Server TCP port.
     *                    Pass 0 to use the public default (43383).
     * @param room_id     The room to join (created automatically if absent).
     * @param player_name Display name visible to other players in the room.
     * @param client_id   Previous session client ID for reconnection (0 = new).
     * @param team_id     Team identifier within the room ("default" if NULL/empty).
     *
     * @return 1 on success, 0 on failure.
     */
    int anchor_connect(const char *host, int port,
                       const char *room_id, const char *player_name,
                       unsigned int client_id, const char *team_id);

    /**
     * @brief Disconnect from the Anchor server.
     */
    void anchor_disconnect(void);

    /**
     * @brief Return whether the connection is currently active.
     * @return 1 if connected, 0 otherwise.
     */
    int anchor_is_connected(void);

    /**
     * @brief Return whether the server sent DISABLE_ANCHOR to this client.
     * @return 1 if disabled, 0 otherwise.
     */
    int anchor_is_disabled(void);

    /* =========================================================================
       Client identity
       ========================================================================= */

    /**
     * @brief Return the client ID assigned by the server.
     *
     * The value is 0 until the server sends an ALL_CLIENT_STATE packet confirming
     * the assigned ID. This typically happens within milliseconds of connecting.
     */
    unsigned int anchor_get_client_id(void);

    /**
     * @brief Return the current room ID as a newly allocated C string.
     *        The caller must free the result with recomp_free().
     */
    char *anchor_get_room_id(void);

    /**
     * @brief Return the current team ID as a newly allocated C string.
     *        The caller must free the result with recomp_free().
     */
    char *anchor_get_team_id(void);

    /* =========================================================================
       Packet polling
       ========================================================================= */

    /**
     * @brief Return 1 if at least one incoming packet is waiting in the queue.
     */
    int anchor_has_packet(void);

    /**
     * @brief Dequeue and return the next incoming packet as a JSON string.
     *
     * Returns a newly allocated C string that must be freed with recomp_free(), or
     * NULL if the queue is empty.  Call every game frame to process server events.
     *
     * The JSON object will always contain a "type" field identifying the packet:
     *   - "ALL_CLIENT_STATE"   Full room snapshot (arrives after handshake).
     *   - "UPDATE_TEAM_STATE"  Teammate save state (response to REQUEST_TEAM_STATE).
     *   - "UPDATE_ROOM_STATE"  Room settings broadcast.
     *   - "SERVER_MESSAGE"     Text message from the server operator.
     *   - "SET_FLAG"           A flag/check set by a teammate (add_to_queue packets).
     *   - <custom type>        Any custom packet broadcast by another client.
     */
    char *anchor_poll_packet(void);

    /**
     * @brief Consume and return the latest SERVER_MESSAGE text as a C string.
     *        Returns NULL if no new message is available.
     *        The caller must free the result with recomp_free().
     */
    char *anchor_get_server_message(void);

    /* =========================================================================
       State updates
       ========================================================================= */

    /**
     * @brief Broadcast your client state to the room.
     *
     * @param state_json  JSON object string with your state fields.
     *                    'clientId', 'name', 'teamId', and 'online' are managed
     *                    automatically; any additional fields are passed through.
     *                    Example: "{\"scene\":5,\"health\":100}"
     *
     * @return 1 on success, 0 on failure.
     */
    int anchor_update_client_state(const char *state_json);

    /**
     * @brief Report the local player's current room to the room.
     *
     * Reads the 16-bit game room ID, looks up the corresponding area name (e.g.
     * "Oedo Town", "Ghost Toys Castle"), and broadcasts it as @p currentRoom in
     * an UPDATE_CLIENT_STATE packet.  Only sends when the room ID actually changes
     * so it is safe to call every game frame.
     *
     * @param room_id  The 16-bit room/scene ID from D_800C7AB2.
     * @return 1 if a packet was sent, 0 otherwise.
     */
    int anchor_set_local_room(unsigned int room_id);

    /**
     * @brief Signal to the server whether a save file is currently loaded.
     *
     * Set to 1 once the player's save is loaded; teammates can then request your
     * team state via anchor_request_team_state().
     *
     * @return 1 on success, 0 on failure.
     */
    int anchor_set_save_loaded(int is_loaded);

    /* =========================================================================
       Team / save state sync
       ========================================================================= */

    /**
     * @brief Switch the current team within the room.
     *
     * @param team_id  New team identifier.
     * @return 1 on success, 0 on failure.
     */
    int anchor_set_team(const char *team_id);

    /**
     * @brief Request the current team save state from online teammates.
     *
     * A response UPDATE_TEAM_STATE packet will arrive in the poll queue when a
     * teammate responds.  Call this after anchor_connect() to sync saves.
     *
     * @param team_id  Team to request from, or "" to use the current team.
     * @return 1 on success, 0 on failure.
     */
    int anchor_request_team_state(const char *team_id);

    /**
     * @brief Push your save state to the current team.
     *
     * Teammates who join later will receive this state via a REQUEST_TEAM_STATE
     * / UPDATE_TEAM_STATE exchange.
     *
     * @param state_json  JSON object of your save state fields.
     * @return 1 on success, 0 on failure.
     */
    int anchor_update_team_state(const char *state_json);

    /* =========================================================================
       Flag / check sending
       ========================================================================= */

    /**
     * @brief Send a game flag/check to the team.
     *
     * The server relays this to all online teammates immediately and queues it for
     * any offline teammates so they receive it when they reconnect.
     *
     * @param flag_name    Identifier string for the flag (e.g. "treasure_chest_3").
     * @param flag_value   Integer value (e.g. 1 = obtained, 0 = cleared).
     * @param add_to_queue If non-zero, the server queues the packet for offline players.
     *
     * @return 1 on success, 0 on failure.
     */
    int anchor_send_flag(const char *flag_name, int flag_value, int add_to_queue);

    /* =========================================================================
       Custom packets
       ========================================================================= */

    /**
     * @brief Send a custom-typed packet to the room, a team, or a specific client.
     *
     * @param packet_type      The "type" field value (e.g. "MY_CUSTOM_EVENT").
     * @param payload_json     JSON object with additional fields, or "{}"/NULL.
     * @param target_team_id   Restrict delivery to this team (or "" for entire room).
     * @param target_client_id Restrict delivery to this client ID (0 = no filter).
     * @param add_to_queue     If non-zero, queue for offline recipients.
     *
     * @return 1 on success, 0 on failure.
     */
    int anchor_send_custom_packet(const char *packet_type, const char *payload_json,
                                  const char *target_team_id, unsigned int target_client_id,
                                  int add_to_queue);

    /**
     * @brief Send a raw JSON packet string directly to the server.
     *
     * Use this for any Anchor packet type not covered by the helpers above.
     *
     * @param packet_json  A complete JSON object string.
     * @return 1 on success, 0 on failure.
     */
    int anchor_send_raw_packet(const char *packet_json);

    /* =========================================================================
       Misc
       ========================================================================= */

    /**
     * @brief Signal to the server that the game has been completed.
     * @return 1 on success, 0 on failure.
     */
    int anchor_send_game_complete(void);

    /**
     * @brief Request server statistics (online count, game-complete count, etc.).
     *
     * A STATS packet will arrive in the poll queue.
     * @return 1 on success, 0 on failure.
     */
    int anchor_request_stats(void);

    /* =========================================================================
       Player list
       ========================================================================= */

    /**
     * @brief Return a JSON array of player name strings for everyone in the room.
     *
     * Example return value: ``["Alice","Bob","Player3"]``
     *
     * Returns ``"[]"`` when not connected or the room is empty.
     *
     * The returned string is allocated in recompiled memory and must be freed
     * by the caller with recomp_free().
     */
    char *anchor_get_player_names_json(void);

    /* =========================================================================
       UI helpers
       ========================================================================= */

    /**
     * @brief Display a brief connection notification banner on screen.
     *
     * @param msg        UTF-8 message to show.  Pass NULL for a default string.
     * @param is_success Non-zero => green accent (success); zero => red (failed).
     */
    void anchor_ui_show_notification(const char *msg, int is_success);

#ifdef __cplusplus
}
#endif

#endif /* ANCHOR_H */
