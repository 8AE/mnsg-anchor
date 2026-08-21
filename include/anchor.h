#ifndef ANCHOR_H
#define ANCHOR_H

/**
 * @file anchor.h
 * @brief Anchor multiplayer client API for Mystical Ninja Starring Goemon: Recompiled.
 *
 * Anchor is a TCP-based multiplayer service.
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
    * @brief Publish the local dead-enemy bitmap for one raw game room.
    *
    * The compact state is retained in every UPDATE_CLIENT_STATE packet so a
    * teammate entering an already-occupied room can query it before spawning
    * actors.  @p bits is a hexadecimal value of at most 64 characters (256
    * bits); it is normalized to lowercase and an empty value means zero.
    *
    * @param room_id    Raw 16-bit room ID from D_800C7AB2.
    * @param signature  Enemy-roster signature used to reject mismatched layouts.
    * @param bits       Compact dead-enemy bitmap as hexadecimal text.
    * @return 1 if the state update was sent, 0 on validation/send failure.
    */
   int anchor_set_enemy_room_state(unsigned int room_id, unsigned int signature,
                                   const char *bits);

   /**
    * @brief Query matching online teammates' dead-enemy state for one room.
    *
    * Only same-team, non-self peers whose current raw room, enemy room, and
    * roster signature all match are included.  Their bitmaps are ORed.
    *
    * @return Newly allocated lowercase hexadecimal text, or an empty string
    *         when no matching peer state exists.  Caller must recomp_free().
    */
   char *anchor_get_enemy_room_state(unsigned int room_id,
                                     unsigned int signature);

   /**
    * @brief Broadcast the local player's world-space position to teammates.
    *
    * Sends a compact MNSG_PLAYER_POS custom packet containing posX/posY/posZ,
    * short velocity estimates, and a sequence number. Identity fields stay in
    * UPDATE_CLIENT_STATE metadata packets so hot movement traffic cannot wipe
    * team/name/character state on the Anchor server. Also updates the local
    * player's own entry in the Python _player_states dict so the position is
    * immediately reflected in the player-list panel.
    *
    * Call once per player-list refresh cycle (~1 s) from anchor_ui.c.
    *
    * Coordinates are the floating-point world position of the CLS_BG_W struct
    * (VEC3F_W at offset 0x08) truncated to int.
    *
    * @param pos_x  World X coordinate.
    * @param pos_y  World Y coordinate.
    * @param pos_z  World Z coordinate.
    * @return 1 if sent, 0 otherwise.
    */
   int anchor_set_position(int pos_x, int pos_y, int pos_z);

   /**
    * @brief Broadcast position plus the live player animation state.
    *
    * The animation frame and clip length are scaled by 100 so receivers can
    * map the player's normalized loop phase onto a different model clip.
    *
    * @param pos_x            World X coordinate.
    * @param pos_y            World Y coordinate.
    * @param pos_z            World Z coordinate.
    * @param action           Current action id from player task offset 0xCC.
    * @param frame_100        Current model frame at object offset 0x28, times 100.
    * @param frame_count_100  Current model clip length, times 100.
    * @param rot_x            Model X rotation from object offset 0x14.
    * @param rot_y            Model Y rotation from object offset 0x16.
    * @param rot_z            Model Z rotation from object offset 0x18.
    * @param appearance_flags Appearance bitmap: bit 0 is Goemon's Sudden
    *                         Impact; bit 1 is Mini Ebisumaru.
    * @return 1 if sent, 0 otherwise.
    */
   int anchor_set_position_anim(int pos_x, int pos_y, int pos_z,
                                int action, int frame_100, int frame_count_100,
                                int rot_x, int rot_y, int rot_z,
                                int appearance_flags);

   /**
    * @brief Broadcast the local player's currently selected character.
    *
    * Sends an UPDATE_CLIENT_STATE packet with a ``currentCharacter`` field so
    * teammates can see which character you are playing.  Also updates the
    * local player's own entry immediately.
    *
    * Call once per player-list refresh cycle from anchor_ui.c.
    *
    * @param char_name  One of: "Goemon", "Ebisumaru", "Sasuke", "Yae".
    * @return 1 if sent, 0 otherwise.
    */
   int anchor_set_character(const char *char_name);

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

   /**
    * @brief Return structured player info as a compact JSON array.
    *
    * Each element is an object with these keys:
    *   ``n``  – display string: "Name - Location"
    *   ``c``  – character index: 0=Goemon, 1=Ebisumaru, 2=Sasuke, 3=Yae.
    *            -1 if the character has not been broadcast by that player yet.
    *   ``r``  – raw room ID, or -1 if unknown.
    *   ``t``  – team ID string.
    *   ``hp`` – 1 if x/y/z position data is available.
    *   ``x``/``y``/``z`` – last broadcast world-space position.
    *
    * Example: ``[{"n":"Alice - Oedo Town","c":0,"r":165,"t":"default","hp":1,"x":10,"y":20,"z":30}]``
    *
    * Returns ``"[]"`` when not connected or the room is empty.
    * The caller must free the result with recomp_free().
    */
   char *anchor_get_player_info_json(void);

   /**
    * @brief Return positions of same-team, same-raw-room teammates as JSON.
    *
    * Returns a compact JSON array of position objects for every teammate that
    * shares the local client's team ID AND current raw room ID and has sent at
    * least one position update.  The local player is excluded.
    *
    * Each element: ``{"x":<int>,"y":<int>,"z":<int>}``
    *
    * Returns ``"[]"`` when not connected, no same-room teammates exist, or
    * none of them have position data.
    *
    * The returned string is allocated in recompiled memory and must be freed
    * by the caller with recomp_free().
    */
   char *anchor_get_teammate_positions_json(void);

   /**
    * @brief Return all online non-self lobby members with their room IDs and positions.
    *
    * Unlike anchor_get_teammate_positions_json(), this function does not filter
    * by team or room – it returns every online player so the phantom actor system
    * can allocate one actor per lobby member at room-load time.
    *
    * Each element: ``{"cid":<int>,"n":"<name>","room":<int>,"x":<int>,"y":<int>,"z":<int>,"hp":<int>}``
    *   cid  – stable client ID for this player.
    *   n    – player display name.
    *   room – raw 16-bit room ID (-1 if not yet known).
    *   x/y/z – last broadcast world-space position (0 if hp==0).
    *   hp   – 1 if the player has sent at least one position update, else 0.
    *
    * Returns ``"[]"`` when not connected or no other players are online.
    * The caller must free the result with recomp_free().
    */
   char *anchor_get_lobby_positions_json(void);

   /**
    * @brief Publish this client's race lobby status and optional race config.
    *
    * @param status      "lobby" or "started".
    * @param config_json Host-authored race config JSON, or ""/NULL.
    */
   int anchor_set_race_lobby_state(const char *status, const char *config_json);

   /**
    * @brief Return compact race lobby player info as JSON.
    *
    * Each element: {"cid":int,"n":"name","t":"team","s":"status","self":0|1}
    */
   char *anchor_get_race_lobby_json(void);

   /**
    * @brief Return the elected race host client id, lowest online client id.
    */
   unsigned int anchor_get_race_host_id(void);

   /**
    * @brief Return 1 if any online player in the room has started the race.
    */
   int anchor_race_has_started(void);

   /**
    * @brief Return the elected host's published race config JSON.
    */
   char *anchor_get_host_race_config_json(void);

   /**
    * @brief Return {"team":"...","players":"..."} for the local race team.
    *
    * Used as the payload for MNSG_RACE_FINISH packets. The caller must free
    * the returned string with recomp_free().
    */
   char *anchor_get_race_finish_payload_json(void);

   /**
    * @brief Copy text to the host clipboard when the platform supports it.
    */
   int anchor_set_clipboard_text(const char *text);

   /**
    * @brief Return host clipboard text, or an empty string if unavailable.
    *
    * The returned string is allocated in recompiled memory and must be freed
    * by the caller with recomp_free().
    */
   char *anchor_get_clipboard_text(void);

#ifdef __cplusplus
}
#endif

#endif /* ANCHOR_H */
