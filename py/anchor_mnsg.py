"""
anchor_mnsg - Anchor client for Mystical Ninja Starring Goemon: Recompiled.

Anchor is a TCP-based multiplayer service used by N64 recompiled ports.
https://github.com/garrettjoecox/anchor

Protocol details:
  - Raw TCP connection to the Anchor server (default: anchor.hm64.org:43383).
  - Messages are JSON objects terminated by a null byte (\\x00).
  - First message after connecting must be a HANDSHAKE packet.

Key packet types sent by the client:
  HANDSHAKE          - Establish identity and join a room.
  UPDATE_CLIENT_STATE- Broadcast your local state (scene, position, etc.) to the room.
  UPDATE_TEAM_STATE  - Push your save state to your team (for save sync).
  REQUEST_TEAM_STATE - Ask teammates for their save state (on connect).
  UPDATE_ROOM_STATE  - Broadcast room-wide settings.
  GAME_COMPLETE      - Signal that you finished the game.
  SET_FLAG           - Broadcast a single item/flag acquisition to the team.
                       With addToQueue=true the server queues it for offline players.
  <custom type>      - Any other type is broadcast to your room or team.

Key packet types received from the server:
  ALL_CLIENT_STATE   - Full snapshot of every client state in the room.
  UPDATE_TEAM_STATE  - A teammate's save state (response to REQUEST_TEAM_STATE).
  UPDATE_ROOM_STATE  - Room settings changed.
  HEARTBEAT          - Keep-alive ping; we echo it back.
  SERVER_MESSAGE     - Text message from the server operator.
  DISABLE_ANCHOR     - Server is kicking this client; disconnect.
  SET_FLAG           - An item/flag acquired by a teammate (queued by the server).
  REQUEST_TEAM_STATE - A teammate is requesting the full team state.
  <custom type>      - Custom packet broadcast by another client.

Item-sync protocol (implemented in src/item_sync.c):
  On connect with a valid save:
    1. anchor_mnsg.request_team_state() is called so the server delivers
       the compact team snapshot followed by queued SET_FLAG deltas.
    2. One loaded teammate is elected to answer with a single
       UPDATE_TEAM_STATE packet, avoiding duplicate join-time floods.
  Each frame:
    - Incoming SET_FLAG packets are applied to the local save data.
    - Incoming REQUEST_TEAM_STATE packets schedule one compact snapshot.
    - Newly obtained items are broadcast immediately as SET_FLAG packets.

  Tracked items (from MNSGRecompRando / save_data_tool.h):
    - Characters: Goemon, Ebisumaru, Sasuke, Yae
    - Equipment: Chain Pipe, Meat Hammer, Firecracker, Flute,
                 Wind-up Camera, Ice Kunai, Bazooka, Medal of Flames
    - Abilities:  Sudden Impact, Mini Ebisumaru, Jetpack, Mermaid
    - Quest items: Triton Shell, Super Pass, Achilles' Heel, Cucumber,
                   Map of Japan
    - Miracle items: Star, Moon, Flower, Snow
    - Dungeon key inventory counts (silver / gold / diamond / jump gym / misc)
    - Total HP maximum  (take-max – health upgrades)
    - Fortune Doll counts
    - Mr. Elly Fant and Mr. Arrow collection progress per dungeon
    - Boss-defeat flags: Dharmanyo, Thaisamba, Tsurami, Benkei, Congo
    - Character-acquisition flags, quest-critical flags

Usage from C (via REPY_FN macros):
  import anchor_mnsg
  anchor_mnsg.connect(host, port, room_id, player_name)
  anchor_mnsg.update_client_state('{"scene": 5}')
  packet = anchor_mnsg.poll_packet()
  anchor_mnsg.disconnect()
"""

import socket
import threading
import queue
import json
import os
import platform
import subprocess
import time
import logging

logger = logging.getLogger("anchor_mnsg")

###############################################################################
# Module-level state
###############################################################################

_sock: "socket.socket | None" = None
_connected: bool = False
_disabled: bool = False
_client_id: int = 0
_room_id: str = ""
_team_id: str = "default"
_player_name: str = ""

# Queue of raw JSON strings received from the server, ready to be polled by C.
_recv_queue: "queue.Queue[str]" = queue.Queue()

# Latest SERVER_MESSAGE text (consumed on read).
_server_message: str = ""

# Map of clientId -> client state dict, updated from ALL_CLIENT_STATE packets.
_player_states: "dict[int, dict]" = {}
_player_states_lock = threading.Lock()

# Lock protecting _sock writes to prevent concurrent send races.
_send_lock = threading.Lock()

# Background receiver thread.
_rx_thread: "threading.Thread | None" = None

# Last room ID sent to the server (avoids redundant state updates).
_local_room_id: int = -1
_local_character: str = ""
_local_save_loaded: bool = False
_last_position_sent: "tuple[int, int, int] | None" = None
_last_position_sent_ms: int = 0
_last_position_action: int = -2
_last_position_frame_100: int = 0
_last_position_appearance_flags: int = -1
_position_seq: int = 0
_race_status: str = ""
_race_config_json: str = ""

###############################################################################
# Constants
###############################################################################

DEFAULT_HOST: str = "anchor.hm64.org"
DEFAULT_PORT: int = 43383
MOVEMENT_MIN_INTERVAL_MS: int = 50
ANIMATION_RESTART_DELTA_100: int = 50
APPEARANCE_SUDDEN_IMPACT: int = 1 << 0
APPEARANCE_MINI_EBISUMARU: int = 1 << 1
APPEARANCE_MASK: int = APPEARANCE_SUDDEN_IMPACT | APPEARANCE_MINI_EBISUMARU


def _appearance_flags_from_payload(payload: dict, current: int = 0) -> int:
    """Read the compact bitmap, with receive-only compatibility for old peers."""
    if "appearanceFlags" in payload:
        return int(payload.get("appearanceFlags", 0)) & APPEARANCE_MASK

    flags = int(current) & APPEARANCE_MASK
    if "suddenImpact" in payload:
        if payload.get("suddenImpact", False):
            flags |= APPEARANCE_SUDDEN_IMPACT
        else:
            flags &= ~APPEARANCE_SUDDEN_IMPACT
    if "modelScale100000" in payload:
        if int(payload.get("modelScale100000", 10000)) < 10000:
            flags |= APPEARANCE_MINI_EBISUMARU
        else:
            flags &= ~APPEARANCE_MINI_EBISUMARU
    return flags

###############################################################################
# Room ID → area name lookup table
# Sourced from MNSGRecompRando apworld/Logic/ files.
###############################################################################

_ROOM_NAMES: "dict[int, str]" = {}


def _build_room_names() -> None:
    t = _ROOM_NAMES
    # Oedo Castle (interior)
    for rid in [0x000, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007, 0x008, 0x009,
                0x00A, 0x00B, 0x00C, 0x00D, 0x00E, 0x00F, 0x010, 0x011, 0x012,
                0x014, 0x015, 0x016, 0x01A, 0x028]:
        t[rid] = "Oedo Castle"
    # Ghost Toys Castle
    for rid in [0x02F, 0x030, 0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x039,
                0x03A, 0x03B, 0x03C, 0x03E, 0x03F, 0x040, 0x041, 0x042, 0x043,
                0x044, 0x045, 0x046, 0x049]:
        t[rid] = "Ghost Toys Castle"
    # Festival Temple Castle
    for rid in [0x05A, 0x05B, 0x05C, 0x05D, 0x05E, 0x05F, 0x060, 0x061, 0x062,
                0x063, 0x064, 0x065, 0x066, 0x067, 0x068, 0x069, 0x06A, 0x06B,
                0x06C, 0x06D, 0x06E, 0x06F, 0x070, 0x071]:
        t[rid] = "Festival Temple Castle"
    # Gourmet Submarine
    for rid in [0x081, 0x082, 0x083, 0x085, 0x086, 0x087, 0x089, 0x08A, 0x08B,
                0x08C, 0x08D, 0x08E, 0x090, 0x091, 0x094, 0x095, 0x096, 0x097,
                0x098, 0x099, 0x09D]:
        t[rid] = "Gourmet Submarine"
    # Gorgeous Music Castle
    for rid in [0x0A8, 0x0A9, 0x0AA, 0x0AB, 0x0AC, 0x0AD, 0x0AE, 0x0AF,
                0x0B0, 0x0B1, 0x0B2, 0x0B3, 0x0B4, 0x0B5, 0x0B6, 0x0B7,
                0x0B8, 0x0B9, 0x0BA, 0x0BB, 0x0BC, 0x0BD, 0x0BE, 0x0BF,
                0x0C0, 0x0C1]:
        t[rid] = "Gorgeous Music Castle"
    # Kai Highway / Mt. Fuji
    for rid in [0x12C, 0x12D, 0x12E, 0x12F, 0x1B1, 0x1D2]:
        t[rid] = "Kai / Mt. Fuji"
    # Musashi
    t[0x130] = "Musashi Beach"
    t[0x131] = "Musashi Tunnel"
    t[0x14F] = "Tunnel to Northeast"
    t[0x150] = "Tunnel to Northeast"
    # Iga
    t[0x132] = "Iga"
    # Yamato
    for rid in [0x133, 0x134, 0x135, 0x136, 0x137]:
        t[rid] = "Yamato"
    t[0x138] = "Kii Awaji Island"
    t[0x139] = "Husband and Wife Rocks"
    t[0x1B3] = "Kii Coffee Shop"
    t[0x1B9] = "Awaji Tourist Center"
    # Tosa
    t[0x13A] = "Tosa Fields"
    t[0x13B] = "Tosa Bridge"
    # Sanuki / Kompira Mountain
    for rid in [0x13C, 0x13D, 0x13E, 0x13F, 0x140]:
        t[rid] = "Kompira Mountain"
    t[0x1B4] = "Kompira Coffee Shop"
    # Iyo
    t[0x141] = "Iyo Hills"
    t[0x142] = "Dogo Hotsprings"
    t[0x1B5] = "Iyo Coffee Shop"
    # Bizen
    t[0x143] = "Kurashiki"
    t[0x144] = "Nagato"
    t[0x145] = "Hagi"
    t[0x146] = "Akiyoshidai"
    t[0x147] = "Shuhodo"
    t[0x148] = "Izumo"
    t[0x149] = "Lake with a Large Tree"
    t[0x14A] = "Inaba"
    t[0x153] = "Gateway Viewpoint"
    t[0x1B6] = "Izumo Coffee Shop"
    t[0x1E2] = "Jump Challenge Training"
    # Mutsu
    t[0x14B] = "Mt. Fear"
    t[0x14C] = "Ugo Stone Circle"
    t[0x14D] = "Shoreline"
    t[0x14E] = "Underwater Japan Sea"
    t[0x151] = "Mutsu Crossroads"
    t[0x152] = "Uzen Tunnel"
    t[0x154] = "Waterfall of Kegon"
    # Festival Village (Mutsu area)
    for rid in [0x179, 0x17A, 0x17B, 0x17C, 0x17D]:
        t[rid] = "Festival Village"
    # Oedo Town overworld
    for rid in [0x15E, 0x15F, 0x160, 0x161, 0x162, 0x163, 0x1D1, 0x1E0]:
        t[rid] = "Oedo Town"
    # Oedo Castle exterior / approaches
    for rid in [0x164, 0x165, 0x166]:
        t[rid] = "Oedo Castle Exterior"
    # Zazen Town
    for rid in [0x167, 0x168, 0x169, 0x16A, 0x16B, 0x16C, 0x16D, 0x16E,
                0x16F, 0x170, 0x171, 0x172, 0x173]:
        t[rid] = "Zazen Town"
    # Folkypoke Village
    for rid in [0x175, 0x176, 0x177, 0x178, 0x1B8]:
        t[rid] = "Folkypoke Village"
    # World Map
    t[0x226] = "World Map"


_build_room_names()

###############################################################################
# Internal helpers
###############################################################################


def _send_raw(packet: dict) -> bool:
    """Serialise *packet* as JSON, append \\x00, and send over TCP."""
    sock = _sock
    if not _connected or sock is None:
        return False
    try:
        data = (json.dumps(packet, separators=(",", ":")) + "\x00").encode("utf-8")
        with _send_lock:
            if not _connected or _sock is not sock:
                return False
            sock.sendall(data)
        return True
    except Exception as exc:
        logger.warning("anchor_mnsg: send error: %s", exc)
        _do_disconnect(sock)
        return False


def _should_handle_team_state_request(packet: dict) -> bool:
    """Elect one loaded teammate to answer a team-state request."""
    requester = int(packet.get("clientId", 0))
    if _client_id <= 0:
        return True

    with _player_states_lock:
        candidates = [
            cid
            for cid, state in _player_states.items()
            if cid != requester
            and bool(state.get("online", True))
            and bool(state.get("isSaveLoaded", False))
            and state.get("teamId", "") == _team_id
        ]
    return not candidates or _client_id == min(candidates)


def _recv_loop(sock: socket.socket) -> None:
    """Background thread: read null-terminated packets and push to _recv_queue."""
    global _connected, _client_id, _server_message, _disabled, _local_room_id, _local_character
    buf = b""
    while _connected and _sock is sock:
        try:
            chunk = sock.recv(4096)
            if not chunk:
                break
            buf += chunk
            # Process all complete messages in the buffer.
            while b"\x00" in buf:
                sep = buf.index(b"\x00")
                raw = buf[:sep].decode("utf-8", errors="replace").strip()
                buf = buf[sep + 1:]
                if not raw:
                    continue
                try:
                    packet = json.loads(raw)
                except json.JSONDecodeError:
                    logger.debug("anchor_mnsg: invalid JSON: %.120s", raw)
                    continue

                ptype = packet.get("type", "")

                # ---- Server-managed special packets -------------------------
                if ptype == "DISABLE_ANCHOR":
                    _disabled = True
                    logger.info("anchor_mnsg: DISABLE_ANCHOR received, disconnecting.")
                    _do_disconnect(sock)
                    return

                if ptype == "HEARTBEAT":
                    # Echo heartbeat back so the server knows we are alive.
                    _send_raw({"type": "HEARTBEAT", "quiet": True})
                    continue  # Don't forward heartbeats to game code.

                if ptype == "SERVER_MESSAGE":
                    _server_message = packet.get("message", "")
                    logger.info("anchor_mnsg: SERVER_MESSAGE: %s", _server_message)
                    # Fall through so game code can also react if desired.

                # Track our own assigned client ID and player list from ALL_CLIENT_STATE.
                if ptype == "ALL_CLIENT_STATE":
                    states = packet.get("state", [])
                    new_players: dict = {}
                    for s in states:
                        cid = s.get("clientId", 0)
                        if s.get("self"):
                            if cid:
                                _client_id = cid
                        if cid:
                            cs = s.get("clientState", s)
                            name = cs.get("name", "") or s.get("name", f"Player{cid}")
                            online = bool(cs.get("online", True))
                            location = cs.get("currentRoom", "")
                            new_players[cid] = {
                                "name": name,
                                "teamId": cs.get("teamId", ""),
                                "online": online,
                                "isSaveLoaded": bool(cs.get("isSaveLoaded", False)),
                                "self": bool(s.get("self")),
                                "location": location,
                                "roomId": int(cs.get("currentRoomId", -1)),
                                "mnsgRace": str(cs.get("mnsgRace", "")),
                                "mnsgRaceConfig": str(cs.get("mnsgRaceConfig", "")),
                            }
                    with _player_states_lock:
                        _player_states.clear()
                        _player_states.update(new_players)
                    # Reset so the next set_local_room() call re-broadcasts our room
                    # and character even if the values haven't changed (our entry was
                    # just rebuilt, and older movement packets may have wiped metadata).
                    _local_room_id = -1
                    _local_character = ""

                if ptype == "MNSG_PLAYER_POS":
                    cid = packet.get("clientId", 0)
                    if cid:
                        with _player_states_lock:
                            if cid not in _player_states:
                                _player_states[cid] = {
                                    "name": f"Player{cid}",
                                    "teamId": "",
                                    "online": True,
                                    "self": False,
                                    "location": "",
                                    "roomId": int(packet.get("currentRoomId", -1)),
                                    "character": "",
                                }
                            if "currentRoomId" in packet:
                                _player_states[cid]["roomId"] = int(packet["currentRoomId"])
                            _player_states[cid]["posX"] = int(packet.get("posX", 0))
                            _player_states[cid]["posY"] = int(packet.get("posY", 0))
                            _player_states[cid]["posZ"] = int(packet.get("posZ", 0))
                            _player_states[cid]["velX"] = int(packet.get("velX", 0))
                            _player_states[cid]["velY"] = int(packet.get("velY", 0))
                            _player_states[cid]["velZ"] = int(packet.get("velZ", 0))
                            _player_states[cid]["posSeq"] = int(packet.get("posSeq", 0))
                            _player_states[cid]["posT"] = int(packet.get("posT", 0))
                            if "action" in packet:
                                _player_states[cid]["action"] = int(packet.get("action", -1))
                            if "animFrame100" in packet:
                                _player_states[cid]["animFrame100"] = int(packet.get("animFrame100", 0))
                            if "animFrameCount100" in packet:
                                _player_states[cid]["animFrameCount100"] = int(packet.get("animFrameCount100", 0))
                            if "rotX" in packet:
                                _player_states[cid]["rotX"] = int(packet.get("rotX", 0))
                            if "rotY" in packet:
                                _player_states[cid]["rotY"] = int(packet.get("rotY", 0))
                            if "rotZ" in packet:
                                _player_states[cid]["rotZ"] = int(packet.get("rotZ", 0))
                            if ("appearanceFlags" in packet or
                                    "suddenImpact" in packet or
                                    "modelScale100000" in packet):
                                current = int(_player_states[cid].get("appearanceFlags", 0))
                                _player_states[cid]["appearanceFlags"] = \
                                    _appearance_flags_from_payload(packet, current)
                    # Movement is already coalesced into _player_states. Do not
                    # duplicate this hot path in the general game-event queue.
                    continue

                # Update a single player's status when they broadcast their state.
                if ptype == "UPDATE_CLIENT_STATE":
                    # The server relays the packet as-is. We put clientId inside
                    # "state", so look there first, then fall back to the root.
                    cs = packet.get("state") or packet.get("clientState") or {}
                    cid = cs.get("clientId") or packet.get("clientId", 0)
                    if cid:
                        with _player_states_lock:
                            if cid in _player_states:
                                if "name" in cs:
                                    _player_states[cid]["name"] = cs["name"]
                                if "teamId" in cs:
                                    _player_states[cid]["teamId"] = cs["teamId"]
                                if "online" in cs:
                                    _player_states[cid]["online"] = bool(cs["online"])
                                if "isSaveLoaded" in cs:
                                    _player_states[cid]["isSaveLoaded"] = bool(cs["isSaveLoaded"])
                                if "mnsgRace" in cs:
                                    _player_states[cid]["mnsgRace"] = str(cs["mnsgRace"])
                                if "mnsgRaceConfig" in cs:
                                    _player_states[cid]["mnsgRaceConfig"] = str(cs["mnsgRaceConfig"])
                                if "currentRoom" in cs:
                                    _player_states[cid]["location"] = cs["currentRoom"]
                                if "currentRoomId" in cs:
                                    _player_states[cid]["roomId"] = int(cs["currentRoomId"])
                                if "posX" in cs:
                                    _player_states[cid]["posX"] = int(cs["posX"])
                                if "posY" in cs:
                                    _player_states[cid]["posY"] = int(cs["posY"])
                                if "posZ" in cs:
                                    _player_states[cid]["posZ"] = int(cs["posZ"])
                                if "velX" in cs:
                                    _player_states[cid]["velX"] = int(cs["velX"])
                                if "velY" in cs:
                                    _player_states[cid]["velY"] = int(cs["velY"])
                                if "velZ" in cs:
                                    _player_states[cid]["velZ"] = int(cs["velZ"])
                                if "posSeq" in cs:
                                    _player_states[cid]["posSeq"] = int(cs["posSeq"])
                                if "posT" in cs:
                                    _player_states[cid]["posT"] = int(cs["posT"])
                                if "action" in cs:
                                    _player_states[cid]["action"] = int(cs["action"])
                                if "animFrame100" in cs:
                                    _player_states[cid]["animFrame100"] = int(cs["animFrame100"])
                                if "animFrameCount100" in cs:
                                    _player_states[cid]["animFrameCount100"] = int(cs["animFrameCount100"])
                                if "rotX" in cs:
                                    _player_states[cid]["rotX"] = int(cs["rotX"])
                                if "rotY" in cs:
                                    _player_states[cid]["rotY"] = int(cs["rotY"])
                                if "rotZ" in cs:
                                    _player_states[cid]["rotZ"] = int(cs["rotZ"])
                                if ("appearanceFlags" in cs or
                                        "suddenImpact" in cs or
                                        "modelScale100000" in cs):
                                    current = int(_player_states[cid].get("appearanceFlags", 0))
                                    _player_states[cid]["appearanceFlags"] = \
                                        _appearance_flags_from_payload(cs, current)
                                if "currentCharacter" in cs:
                                    _player_states[cid]["character"] = str(cs["currentCharacter"])
                            else:
                                name = cs.get("name", f"Player{cid}")
                                location = cs.get("currentRoom", "")
                                _player_states[cid] = {
                                    "name": name,
                                    "teamId": cs.get("teamId", ""),
                                    "online": bool(cs.get("online", True)),
                                    "isSaveLoaded": bool(cs.get("isSaveLoaded", False)),
                                    "self": False,
                                    "location": location,
                                    "roomId": int(cs.get("currentRoomId", -1)),
                                    "mnsgRace": str(cs.get("mnsgRace", "")),
                                    "mnsgRaceConfig": str(cs.get("mnsgRaceConfig", "")),
                                    "posX": int(cs.get("posX", 0)),
                                    "posY": int(cs.get("posY", 0)),
                                    "posZ": int(cs.get("posZ", 0)),
                                    "velX": int(cs.get("velX", 0)),
                                    "velY": int(cs.get("velY", 0)),
                                    "velZ": int(cs.get("velZ", 0)),
                                    "posSeq": int(cs.get("posSeq", 0)),
                                    "posT": int(cs.get("posT", 0)),
                                    "action": int(cs.get("action", -1)),
                                    "animFrame100": int(cs.get("animFrame100", 0)),
                                    "animFrameCount100": int(cs.get("animFrameCount100", 0)),
                                    "rotX": int(cs.get("rotX", 0)),
                                    "rotY": int(cs.get("rotY", 0)),
                                    "rotZ": int(cs.get("rotZ", 0)),
                                    "appearanceFlags": _appearance_flags_from_payload(cs),
                                    "character": str(cs.get("currentCharacter", "")),
                                }

                if ptype == "REQUEST_TEAM_STATE" and not _should_handle_team_state_request(packet):
                    continue

                # Enqueue durable/gameplay packets for C-side polling. For a
                # stored response, apply the snapshot before queued deltas.
                _recv_queue.put(raw)
                if ptype == "UPDATE_TEAM_STATE":
                    queued_packets = packet.get("queue", [])
                    if isinstance(queued_packets, list):
                        for queued_packet in queued_packets:
                            if isinstance(queued_packet, str) and queued_packet:
                                _recv_queue.put(queued_packet)

        except Exception as exc:
            if _connected and _sock is sock:
                logger.warning("anchor_mnsg: recv error: %s", exc)
            break

    _do_disconnect(sock)


def _do_disconnect(expected_sock: "socket.socket | None" = None) -> None:
    """Close the socket and mark as disconnected (idempotent)."""
    global _sock, _connected, _local_room_id, _local_character, _local_save_loaded
    global _last_position_sent, _last_position_sent_ms
    global _last_position_action, _last_position_frame_100
    global _last_position_appearance_flags

    # A receiver from an older connection must never close a newer socket.
    if expected_sock is not None and _sock is not expected_sock:
        try:
            expected_sock.close()
        except Exception:
            pass
        return

    _local_room_id = -1
    _local_character = ""
    _local_save_loaded = False
    _last_position_sent = None
    _last_position_sent_ms = 0
    _last_position_action = -2
    _last_position_frame_100 = 0
    _last_position_appearance_flags = -1
    _connected = False
    s = _sock
    _sock = None
    if s is not None:
        try:
            s.close()
        except Exception:
            pass
    with _player_states_lock:
        _player_states.clear()


###############################################################################
# Public API  (called from C via REPY_FN macros)
###############################################################################


def connect(
    host: str,
    port: int,
    room_id: str,
    player_name: str,
    client_id: int = 0,
    team_id: str = "default",
) -> bool:
    """
    Connect to an Anchor server and send the HANDSHAKE packet.

    Args:
        host:        Server hostname or IP. Use '' for the public default.
        port:        Server port. Use 0 for the public default (43383).
        room_id:     The room to join (creates it if it doesn't exist).
        player_name: Display name for this player.
        client_id:   Previous client ID for session resumption (0 = new session).
        team_id:     Team identifier within the room (default: "default").

    Returns True on success, False on failure.
    """
    global _sock, _connected, _client_id, _room_id, _team_id, _player_name
    global _last_position_sent, _last_position_sent_ms, _position_seq, _local_character
    global _last_position_action, _last_position_frame_100
    global _last_position_appearance_flags
    global _rx_thread, _disabled, _race_status, _race_config_json, _local_save_loaded

    if _connected:
        logger.info("anchor_mnsg: already connected.")
        return True

    _room_id = room_id if room_id.startswith("mnsg_") else "mnsg_" + room_id
    _team_id = team_id or "default"
    _player_name = player_name
    _client_id = client_id
    _disabled = False
    _last_position_sent = None
    _last_position_sent_ms = 0
    _last_position_action = -2
    _last_position_frame_100 = 0
    _last_position_appearance_flags = -1
    _position_seq = 0
    _local_character = ""
    _local_save_loaded = False
    _race_status = ""
    _race_config_json = ""

    # Drain stale queued messages.
    while not _recv_queue.empty():
        try:
            _recv_queue.get_nowait()
        except queue.Empty:
            break

    resolved_host = host if host else DEFAULT_HOST
    resolved_port = int(port) if port else DEFAULT_PORT

    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10.0)
        sock.connect((resolved_host, resolved_port))
        sock.settimeout(None)  # Switch to blocking mode for the recv loop.
        _sock = sock
        _connected = True
    except Exception as exc:
        logger.warning("anchor_mnsg: connection failed to %s:%d: %s", resolved_host, resolved_port, exc)
        if sock is not None:
            try:
                sock.close()
            except Exception:
                pass
        return False

    # Send the HANDSHAKE packet.
    handshake = {
        "type": "HANDSHAKE",
        "clientId": _client_id,
        "roomId": _room_id,
        "clientState": {
            "teamId": _team_id,
            "name": _player_name,
            "online": True,
            "isSaveLoaded": False,
        },
        "roomState": {},
    }
    data = (json.dumps(handshake, separators=(",", ":")) + "\x00").encode("utf-8")
    try:
        sock.sendall(data)
    except Exception as exc:
        logger.warning("anchor_mnsg: handshake send failed: %s", exc)
        _do_disconnect(sock)
        return False

    # Launch the background receiver thread.
    _rx_thread = threading.Thread(target=_recv_loop, args=(sock,), daemon=True, name="anchor_rx")
    _rx_thread.start()

    logger.info(
        "anchor_mnsg: connected to %s:%d, room=%r, player=%r",
        resolved_host, resolved_port, _room_id, _player_name,
    )
    return True


def disconnect() -> None:
    """Disconnect from the Anchor server."""
    logger.info("anchor_mnsg: disconnect requested.")
    _do_disconnect()


def is_connected() -> bool:
    """Return True if the TCP connection is currently active."""
    return _connected


def is_disabled() -> bool:
    """Return True if the server sent DISABLE_ANCHOR."""
    return _disabled


def get_client_id() -> int:
    """Return this client's assigned ID (0 until the server confirms it)."""
    return _client_id


def get_room_id() -> str:
    """Return the room this client is in."""
    return _room_id


def get_team_id() -> str:
    """Return this client's current team ID."""
    return _team_id


def get_player_name() -> str:
    """Return this client's player name."""
    return _player_name


def has_packet() -> bool:
    """Return True if there is at least one packet waiting to be polled."""
    return not _recv_queue.empty()


def poll_packet() -> str:
    """
    Return the next queued incoming packet as a JSON string.
    Returns an empty string ('') if no packet is available.
    Non-blocking – call this every frame from C code.
    """
    try:
        return _recv_queue.get_nowait()
    except queue.Empty:
        return ""


def get_server_message() -> str:
    """
    Return the latest SERVER_MESSAGE text received from the server, and clear it.
    Returns '' if no new message is available.
    """
    global _server_message
    msg = _server_message
    _server_message = ""
    return msg


def update_client_state(state_json: str) -> bool:
    """
    Send an UPDATE_CLIENT_STATE packet, broadcasting your state to the room.

    Args:
        state_json: A JSON object string with arbitrary state fields.
                    'clientId', 'name', 'teamId', and 'online' are set automatically.

    Returns True on successful send.
    """
    global _team_id
    try:
        state: dict = json.loads(state_json) if state_json else {}
    except json.JSONDecodeError:
        state = {}

    # Keep internal team_id in sync.
    if "teamId" in state:
        _team_id = state["teamId"]

    # Server requires these fields.
    state.setdefault("teamId", _team_id)
    state.setdefault("isSaveLoaded", _local_save_loaded)
    if _local_room_id >= 0:
        state.setdefault("currentRoomId", _local_room_id)
        state.setdefault("currentRoom", _ROOM_NAMES.get(_local_room_id, ""))
    if _local_character:
        state.setdefault("currentCharacter", _local_character)
    if _race_status:
        state.setdefault("mnsgRace", _race_status)
    if _race_config_json:
        state.setdefault("mnsgRaceConfig", _race_config_json)
    state["clientId"] = _client_id
    state["name"] = _player_name
    state["online"] = True

    sent = _send_raw({"type": "UPDATE_CLIENT_STATE", "state": state})
    if sent and _client_id:
        with _player_states_lock:
            local = _player_states.setdefault(_client_id, {})
            local.update(state)
            local["self"] = True
    return sent


def set_save_loaded(is_loaded: bool) -> bool:
    """
    Convenience wrapper: set isSaveLoaded flag, making yourself eligible
    to share your save state with teammates who join.
    """
    global _local_save_loaded
    _local_save_loaded = bool(is_loaded)
    return update_client_state(json.dumps({"isSaveLoaded": _local_save_loaded}))


def request_team_state(target_team_id: str = "") -> bool:
    """
    Ask the team for their current save state.
    Call this shortly after connecting so you can receive a teammate's save.

    Args:
        target_team_id: The team to request from. Defaults to your own team.
    """
    return _send_raw({
        "type": "REQUEST_TEAM_STATE",
        "clientId": _client_id,
        "targetTeamId": target_team_id or _team_id,
    })


def update_team_state(state_json: str) -> bool:
    """
    Push a save state to your team so teammates who join later receive it.

    Args:
        state_json: JSON object containing your save state fields.
    """
    try:
        state: dict = json.loads(state_json) if state_json else {}
    except json.JSONDecodeError:
        state = {}

    return _send_raw({
        "type": "UPDATE_TEAM_STATE",
        "clientId": _client_id,
        "targetTeamId": _team_id,
        "state": state,
    })


def send_flag(flag_name: str, flag_value: int, add_to_queue: bool = True) -> bool:
    """
    Send a game flag/check to your team.

    Anchor servers relay these to every team member and optionally queue them
    so offline teammates receive them when they log in.

    Args:
        flag_name:    Identifier for the flag (e.g. "chest_1_opened").
        flag_value:   Integer value for the flag (e.g. 1 for set, 0 for clear).
        add_to_queue: If True the server queues the packet for offline teammates.
    """
    packet: dict = {
        "type": "SET_FLAG",
        "clientId": _client_id,
        "targetTeamId": _team_id,
        "flag": flag_name,
        "value": int(flag_value),
    }
    if add_to_queue:
        packet["addToQueue"] = True
    return _send_raw(packet)


def send_custom_packet(
    packet_type: str,
    payload_json: str = "",
    target_team_id: str = "",
    target_client_id: int = 0,
    add_to_queue: bool = False,
) -> bool:
    """
    Send a custom-typed packet to the room, a specific team, or a specific client.

    Args:
        packet_type:      The 'type' field value.
        payload_json:     Optional JSON object with additional fields.
        target_team_id:   If non-empty, send only to that team.
        target_client_id: If non-zero, send only to that specific client.
        add_to_queue:     If True, the server queues the packet for offline recipients.
    """
    try:
        payload: dict = json.loads(payload_json) if payload_json else {}
    except json.JSONDecodeError:
        payload = {}

    packet: dict = {"type": packet_type, "clientId": _client_id}
    packet.update(payload)

    if target_client_id:
        packet["targetClientId"] = int(target_client_id)
    elif target_team_id:
        packet["targetTeamId"] = target_team_id
    if add_to_queue:
        packet["addToQueue"] = True

    return _send_raw(packet)


def set_position(x: int, y: int, z: int) -> bool:
    """Backward-compatible position-only sender."""
    return set_position_anim(x, y, z, -1, 0, 0, 0, 0, 0, 0)


def set_position_anim(
    x: int,
    y: int,
    z: int,
    action: int,
    frame_100: int,
    frame_count_100: int,
    rot_x: int,
    rot_y: int,
    rot_z: int,
    appearance_flags: int = 0,
) -> bool:
    """
    Broadcast world-space position and the live animation phase to teammates.

    This is the hot movement payload, so it intentionally avoids repeating
    identity/team fields that are sent by handshake, room, and character
    updates.  A small velocity estimate and sequence number are included so
    clients can smooth short gaps between server updates.

    Args:
        x: World X coordinate (int, from CLS_BG_W::position.x truncated).
        y: World Y coordinate.
        z: World Z coordinate.
        action: Current player action id.
        frame_100: Current animation frame multiplied by 100.
        frame_count_100: Current animation loop length multiplied by 100.
        rot_x: Current model X rotation.
        rot_y: Current model Y rotation.
        rot_z: Current model Z rotation.
        appearance_flags: Bitmap containing Sudden Impact (bit 0) and Mini
            Ebisumaru (bit 1).

    Returns True if the packet was sent.
    """
    global _last_position_sent, _last_position_sent_ms, _position_seq
    global _last_position_action, _last_position_frame_100
    global _last_position_appearance_flags

    if not _connected:
        return False
    now_ms = int(time.monotonic() * 1000)
    action_changed = int(action) != _last_position_action
    appearance_flags = int(appearance_flags) & APPEARANCE_MASK
    appearance_changed = appearance_flags != _last_position_appearance_flags
    frame_restarted = (
        not action_changed
        and int(frame_100) + ANIMATION_RESTART_DELTA_100 < _last_position_frame_100
    )
    if (
        _last_position_sent_ms > 0
        and now_ms - _last_position_sent_ms < MOVEMENT_MIN_INTERVAL_MS
        and not action_changed
        and not appearance_changed
        and not frame_restarted
    ):
        return False

    vel_x = 0
    vel_y = 0
    vel_z = 0
    if _last_position_sent is not None and _last_position_sent_ms > 0:
        dt_ms = max(1, now_ms - _last_position_sent_ms)
        vel_x = int((x - _last_position_sent[0]) * 1000 / dt_ms)
        vel_y = int((y - _last_position_sent[1]) * 1000 / dt_ms)
        vel_z = int((z - _last_position_sent[2]) * 1000 / dt_ms)
    next_seq = (_position_seq + 1) & 0x7fffffff
    sent = _send_raw({
        "type": "MNSG_PLAYER_POS",
        "clientId": _client_id,
        "currentRoomId": _local_room_id,
        "posX": x,
        "posY": y,
        "posZ": z,
        "velX": vel_x,
        "velY": vel_y,
        "velZ": vel_z,
        "posSeq": next_seq,
        "posT": now_ms,
        "action": int(action),
        "animFrame100": int(frame_100),
        "animFrameCount100": int(frame_count_100),
        "rotX": int(rot_x),
        "rotY": int(rot_y),
        "rotZ": int(rot_z),
        "appearanceFlags": appearance_flags,
        "quiet": True,
    })
    if not sent:
        return False

    _last_position_sent = (x, y, z)
    _last_position_sent_ms = now_ms
    _last_position_action = int(action)
    _last_position_frame_100 = int(frame_100)
    _last_position_appearance_flags = appearance_flags
    _position_seq = next_seq

    if _client_id:
        with _player_states_lock:
            local = _player_states.setdefault(_client_id, {})
            local["posX"] = x
            local["posY"] = y
            local["posZ"] = z
            local["velX"] = vel_x
            local["velY"] = vel_y
            local["velZ"] = vel_z
            local["posSeq"] = _position_seq
            local["posT"] = now_ms
            local["action"] = int(action)
            local["animFrame100"] = int(frame_100)
            local["animFrameCount100"] = int(frame_count_100)
            local["rotX"] = int(rot_x)
            local["rotY"] = int(rot_y)
            local["rotZ"] = int(rot_z)
            local["appearanceFlags"] = appearance_flags
    return True


def set_character(char_name: str) -> bool:
    """
    Broadcast this client's currently selected character to teammates.

    Sends an UPDATE_CLIENT_STATE packet carrying ``currentCharacter`` so the
    player-list panel on every peer shows which character is being played.
    Also updates the local player's own ``_player_states`` entry immediately.

    Args:
        char_name: One of ``"Goemon"``, ``"Ebisumaru"``, ``"Sasuke"``,
                   ``"Yae"``.

    Returns True if the packet was sent.
    """
    global _local_character
    if not _connected:
        return False
    if char_name == _local_character:
        return False
    _local_character = char_name
    return update_client_state(json.dumps({"currentCharacter": char_name}))


def set_local_room(room_id: int) -> bool:
    """
    Report this client's current room ID to the Anchor server.

    Looks up the area name in the built-in table and broadcasts it as
    ``currentRoom`` in an UPDATE_CLIENT_STATE packet.  Only sends when the
    room actually changes to avoid flooding the server.

    Also updates the local player's own entry in ``_player_states`` immediately
    so the player list shows our own location without waiting for a server echo.

    Args:
        room_id: The 16-bit room/scene ID read from the game (D_800C7AB2).

    Returns True if a packet was sent, False otherwise.
    """
    global _local_room_id
    if not _connected:
        return False
    if room_id == _local_room_id:
        return False
    _local_room_id = room_id
    area_name = _ROOM_NAMES.get(room_id, "")
    # Update our own local entry immediately – the server won't echo us back.
    with _player_states_lock:
        if _client_id in _player_states:
            _player_states[_client_id]["location"] = area_name
            _player_states[_client_id]["roomId"] = room_id
    return update_client_state(json.dumps({"currentRoom": area_name, "currentRoomId": room_id}))


def send_game_complete() -> bool:
    """Signal to the server that the game has been completed."""
    return _send_raw({"type": "GAME_COMPLETE", "clientId": _client_id})


def get_stats() -> bool:
    """
    Request server statistics (online count, game complete count, etc.).
    The response STATS packet will arrive in the poll queue.
    """
    return _send_raw({"type": "STATS"})


def set_team(new_team_id: str) -> bool:
    """
    Move to a different team within the same room.

    Args:
        new_team_id: The new team identifier.
    """
    global _team_id
    _team_id = new_team_id
    return update_client_state(json.dumps({"teamId": new_team_id}))


def get_player_names_json() -> str:
    """
    Return a JSON array of ``[CharName] Name - Location`` strings for all
    *online* clients currently in the room, sorted by client ID.  The
    character prefix (e.g. ``[Goemon]``) is omitted when no character has
    been broadcast yet.  Disconnected players are omitted entirely.

    Returns ``'[]'`` when not connected or no online players are present.
    """
    with _player_states_lock:
        entries = []
        for _k, v in sorted(_player_states.items()):
            if not v.get("online", True):
                continue  # hide disconnected players from the list
            char = v.get("character", "")
            name_str = v["name"]
            if char:
                name_str = "[" + char + "] " + name_str
            loc = v.get("location", "")
            if loc:
                name_str += " - " + loc
            # Append world-space coordinates if available.
            px = v.get("posX")
            py = v.get("posY")
            pz = v.get("posZ")
            if px is not None and py is not None and pz is not None:
                name_str += f" ({px}, {py}, {pz})"
            entries.append(name_str)
    return json.dumps(entries, separators=(",", ":"))


# ---------------------------------------------------------------------------
# Character icon support
# ---------------------------------------------------------------------------

_CHAR_TO_IDX: dict[str, int] = {
    "Goemon": 0,
    "Ebisumaru": 1,
    "Sasuke": 2,
    "Yae": 3,
}


def get_player_info_json() -> str:
    """
    Return a compact JSON array of per-player objects for all *online* players,
    sorted by client ID.

    Each object has these keys:
        ``n``  – display string: "Name - Location"
        ``c``  – character index (int): 0=Goemon, 1=Ebisumaru, 2=Sasuke, 3=Yae.
                 -1 if the character has not been broadcast yet.
        ``r``  – raw room ID, or -1 if unknown.
        ``t``  – team ID string.
        ``hp`` – 1 if the player has sent position data, else 0.
        ``x``/``y``/``z`` – last broadcast world position, 0 when hp is 0.

    Returns ``'[]'`` when not connected or no online players are present.
    """
    with _player_states_lock:
        entries = []
        # Sort by (teamId, clientId) so teammates are grouped together.
        for _k, v in sorted(_player_states.items(), key=lambda kv: (kv[1].get("teamId", ""), kv[0])):
            if not v.get("online", True):
                continue
            name_str = v["name"]
            loc = v.get("location", "")
            if loc:
                name_str += " - " + loc
            char_idx = _CHAR_TO_IDX.get(v.get("character", ""), -1)
            room_id  = v.get("roomId", -1)
            team_id  = v.get("teamId", "")
            has_pos = "posX" in v and "posY" in v and "posZ" in v
            entries.append({
                "n": name_str,
                "c": char_idx,
                "r": room_id,
                "t": team_id,
                "hp": 1 if has_pos else 0,
                "x": int(v.get("posX", 0)) if has_pos else 0,
                "y": int(v.get("posY", 0)) if has_pos else 0,
                "z": int(v.get("posZ", 0)) if has_pos else 0,
            })
    return json.dumps(entries, separators=(",", ":"))


def get_teammate_positions_json() -> str:
    """
    Return a compact JSON array of same-team, same-room teammates with their
    latest world-space positions, for use by the phantom actor system.

    Each entry: {"x": int, "y": int, "z": int}

    Only includes players that:
      - Are on the same team as the local client (_team_id).
      - Share the same raw room ID as the local client (_local_room_id).
      - Have at least one position coordinate stored (posX/posY/posZ).
      - Are not the local client.
      - Are marked online.

    Returns '[]' if not connected, no teammates are present, or
    no same-room teammates have position data.
    """
    if not _connected:
        return "[]"
    with _player_states_lock:
        if _local_room_id < 0:
            return "[]"
        result = []
        for cid, v in _player_states.items():
            if cid == _client_id:
                continue
            if not v.get("online", True):
                continue
            if v.get("teamId", "") != _team_id:
                continue
            if int(v.get("roomId", -1)) != _local_room_id:
                continue
            px = v.get("posX")
            py = v.get("posY")
            pz = v.get("posZ")
            if px is None or py is None or pz is None:
                continue
            result.append({"x": int(px), "y": int(py), "z": int(pz)})
    return json.dumps(result, separators=(",", ":"))


def get_lobby_positions_json() -> str:
    """
    Return a compact JSON array of all online non-self lobby members with their
    current room ID and world-space position, for use by the phantom actor system.

    Each entry includes identity, position, velocity, animation, rotation,
    character, and appearance fields used by the remote cutscene-model
    renderer.

    Fields:
      "cid"  – client ID (unique per player, stable within a session).
      "room" – raw 16-bit room ID the player last reported, -1 if unknown.
      "x","y","z" – last broadcast world-space position (0 if not yet received).
      "hp"   – 1 if the player has sent at least one position update, 0 otherwise.
      "ap"   – appearance bitmap: Sudden Impact bit 0, Mini Ebisumaru bit 1.

    Unlike get_teammate_positions_json(), this function:
      - Does NOT filter by team.
      - Does NOT filter by room.
      - Includes ALL online non-self players in the lobby.

    Returns '[]' if not connected or no other players are present.
    """
    if not _connected:
        return "[]"
    with _player_states_lock:
        result = []
        for cid, v in sorted(_player_states.items()):
            if cid == _client_id:
                continue
            if not v.get("online", True):
                continue
            room_id = int(v.get("roomId", -1))
            has_pos = "posX" in v
            px = int(v.get("posX", 0)) if has_pos else 0
            py = int(v.get("posY", 0)) if has_pos else 0
            pz = int(v.get("posZ", 0)) if has_pos else 0
            char_lookup = {"Goemon": 0, "Ebisumaru": 1, "Sasuke": 2, "Yae": 3}
            ch = char_lookup.get(v.get("character", ""), -1)
            result.append({
                "cid": cid,
                "n": v.get("name", f"Player{cid}"),
                "room": room_id,
                "x": px,
                "y": py,
                "z": pz,
                "hp": 1 if has_pos else 0,
                "ch": ch,
                "vx": int(v.get("velX", 0)),
                "vy": int(v.get("velY", 0)),
                "vz": int(v.get("velZ", 0)),
                "s": int(v.get("posSeq", 0)),
                "a": int(v.get("action", -1)),
                "af": int(v.get("animFrame100", 0)),
                "al": int(v.get("animFrameCount100", 0)),
                "rx": int(v.get("rotX", 0)),
                "ry": int(v.get("rotY", 0)),
                "rz": int(v.get("rotZ", 0)),
                "ap": int(v.get("appearanceFlags", 0)) & APPEARANCE_MASK,
                "tm": 1 if v.get("teamId", "") == _team_id else 0,
            })
    return json.dumps(result, separators=(",", ":"))


def set_race_lobby_state(status: str, config_json: str = "") -> bool:
    """
    Publish this client's race-lobby status in Anchor client state.

    status values used by the mod are:
      lobby   - waiting in the race lobby
      started - race has begun
    """
    global _race_status, _race_config_json
    _race_status = status or "lobby"
    if config_json:
        _race_config_json = config_json
    payload = {"mnsgRace": _race_status}
    if _race_config_json:
        payload["mnsgRaceConfig"] = _race_config_json
    return update_client_state(json.dumps(payload, separators=(",", ":")))


def get_race_lobby_json() -> str:
    """
    Return compact online lobby membership grouped by team on the C side.

    Each entry: {"cid": int, "n": str, "t": str, "s": str, "self": 0|1}
    """
    if not _connected:
        return "[]"
    with _player_states_lock:
        result = []
        for cid, v in sorted(_player_states.items(), key=lambda kv: (kv[1].get("teamId", ""), kv[0])):
            if not v.get("online", True):
                continue
            result.append({
                "cid": int(cid),
                "n": v.get("name", f"Player{cid}"),
                "t": v.get("teamId", "") or "default",
                "s": v.get("mnsgRace", ""),
                "self": 1 if cid == _client_id or v.get("self") else 0,
            })
    return json.dumps(result, separators=(",", ":"))


def get_race_host_id() -> int:
    """
    Elect host as the lowest online client id in the room.
    """
    if not _connected:
        return 0
    with _player_states_lock:
        ids = [
            int(cid)
            for cid, v in _player_states.items()
            if v.get("online", True)
        ]
    return min(ids) if ids else int(_client_id)


def race_has_started() -> bool:
    """
    Return True when any online player is advertising an active race.
    """
    if not _connected:
        return False
    with _player_states_lock:
        for _cid, v in _player_states.items():
            if v.get("online", True) and v.get("mnsgRace", "") == "started":
                return True
    return False


def get_host_race_config_json() -> str:
    """
    Return the elected host's published race config, if available.
    """
    host_id = get_race_host_id()
    if not host_id:
        return ""
    with _player_states_lock:
        host = _player_states.get(host_id, {})
        return str(host.get("mnsgRaceConfig", ""))


def get_race_finish_payload_json() -> str:
    """
    Return a JSON payload describing the local team for a race finish packet.

    The C side sends this as an MNSG_RACE_FINISH custom packet when the
    configured goal flag is reached.
    """
    with _player_states_lock:
        names = [
            v.get("name", f"Player{cid}")
            for cid, v in sorted(_player_states.items())
            if v.get("online", True) and v.get("teamId", "") == _team_id
        ]
    if not names:
        names = [_player_name or "Player"]
    return json.dumps({
        "team": _team_id or "default",
        "players": ", ".join(names),
    }, separators=(",", ":"))


def set_clipboard_text(text: str) -> bool:
    """
    Copy text to the host clipboard for UI export buttons.
    """
    value = str(text or "")
    try:
        import tkinter

        root = tkinter.Tk()
        root.withdraw()
        root.clipboard_clear()
        root.clipboard_append(value)
        root.update()
        root.destroy()
        return True
    except Exception:
        pass

    try:
        import pyperclip

        pyperclip.copy(value)
        return True
    except Exception:
        pass

    try:
        system = platform.system()
        if system == "Darwin":
            subprocess.run(["pbcopy"], input=value.encode("utf-8"), check=True)
            return True
        if system == "Windows":
            subprocess.run(["clip"], input=value.encode("utf-16le"), check=True)
            return True
        if os.environ.get("WAYLAND_DISPLAY"):
            subprocess.run(["wl-copy"], input=value.encode("utf-8"), check=True)
            return True
        if os.environ.get("DISPLAY"):
            subprocess.run(["xclip", "-selection", "clipboard"], input=value.encode("utf-8"), check=True)
            return True
    except Exception:
        pass

    return False


def get_clipboard_text() -> str:
    """
    Return text from the host clipboard when available.
    """
    try:
        import tkinter

        root = tkinter.Tk()
        root.withdraw()
        value = root.clipboard_get()
        root.destroy()
        return str(value or "")
    except Exception:
        pass

    try:
        import pyperclip

        return str(pyperclip.paste() or "")
    except Exception:
        pass

    try:
        system = platform.system()
        if system == "Darwin":
            return subprocess.check_output(["pbpaste"]).decode("utf-8", "ignore")
        if system == "Windows":
            cmd = [
                "powershell",
                "-NoProfile",
                "-Command",
                "Get-Clipboard -Raw",
            ]
            return subprocess.check_output(cmd).decode("utf-8", "ignore")
        if os.environ.get("WAYLAND_DISPLAY"):
            return subprocess.check_output(["wl-paste", "-n"]).decode("utf-8", "ignore")
        if os.environ.get("DISPLAY"):
            return subprocess.check_output(["xclip", "-selection", "clipboard", "-o"]).decode("utf-8", "ignore")
    except Exception:
        pass

    return ""


def _decode_png_rgba(png: bytes) -> bytes:
    """
    Minimal PNG → RGBA32 decoder using stdlib only (``zlib`` + ``struct``).

    Supports 8-bit-per-channel colour types:
        0  – Grayscale
        2  – RGB
        4  – Grayscale + Alpha
        6  – RGBA

    Returns ``struct.pack(">II", width, height) + rgba_data``.
    Raises ``ValueError`` on any unsupported or malformed PNG.
    """
    import struct
    import zlib

    if png[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")

    width = height = bit_depth = color_type = 0
    idat_chunks: list[bytes] = []
    pos = 8

    while pos + 12 <= len(png):
        length = struct.unpack_from(">I", png, pos)[0]
        ctype = png[pos + 4: pos + 8]
        data = png[pos + 8: pos + 8 + length]
        pos += 12 + length

        if ctype == b"IHDR":
            width, height = struct.unpack_from(">II", data)
            bit_depth = data[8]
            color_type = data[9]
        elif ctype == b"IDAT":
            idat_chunks.append(data)
        elif ctype == b"IEND":
            break

    if width == 0 or height == 0:
        raise ValueError(f"IHDR not found or zero dimensions ({width}x{height})")
    if bit_depth != 8:
        raise ValueError(f"unsupported bit depth {bit_depth} (only 8 supported)")

    cpp = {0: 1, 2: 3, 4: 2, 6: 4}.get(color_type)
    if cpp is None:
        raise ValueError(f"unsupported PNG color_type {color_type}")

    raw = zlib.decompress(b"".join(idat_chunks))
    stride = width * cpp + 1  # +1 for per-row filter byte

    rgba = bytearray(width * height * 4)
    prev_row = bytearray(width * cpp)

    for y in range(height):
        off = y * stride
        f = raw[off]
        row = bytearray(raw[off + 1: off + 1 + width * cpp])

        if f == 1:      # Sub
            for x in range(cpp, len(row)):
                row[x] = (row[x] + row[x - cpp]) & 0xFF
        elif f == 2:    # Up
            for x in range(len(row)):
                row[x] = (row[x] + prev_row[x]) & 0xFF
        elif f == 3:    # Average
            for x in range(len(row)):
                a = row[x - cpp] if x >= cpp else 0
                b = prev_row[x]
                row[x] = (row[x] + (a + b) // 2) & 0xFF
        elif f == 4:    # Paeth
            for x in range(len(row)):
                a = row[x - cpp] if x >= cpp else 0
                b = prev_row[x]
                c = prev_row[x - cpp] if x >= cpp else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                row[x] = (row[x] + pr) & 0xFF
        # f == 0: None — row unchanged

        for x in range(width):
            idx = (y * width + x) * 4
            if color_type == 6:     # RGBA
                rgba[idx:idx + 4] = row[x * 4: x * 4 + 4]
            elif color_type == 2:   # RGB
                rgba[idx:idx + 3] = row[x * 3: x * 3 + 3]
                rgba[idx + 3] = 255
            elif color_type == 0:   # Grayscale
                g = row[x]
                rgba[idx] = rgba[idx + 1] = rgba[idx + 2] = g
                rgba[idx + 3] = 255
            else:                   # Grayscale + Alpha
                g = row[x * 2]
                rgba[idx] = rgba[idx + 1] = rgba[idx + 2] = g
                rgba[idx + 3] = row[x * 2 + 1]

        prev_row = row

    import struct as _struct
    return _struct.pack(">II", width, height) + bytes(rgba)


def load_icon_rgba(nrm_path: str, char_name: str) -> bytes:
    """
    Load a character icon from the NRM archive as raw RGBA32 pixel data.

    The NRM file is a zip archive.  Icons are stored as raw RGBA32 files at
    ``icons/{char_name}_icon.rgba`` (200x200 pixels, 160000 bytes).

    Returns:
        ``struct.pack(">II", width, height) + rgba_bytes``  on success.
        ``struct.pack(">II", 0, 0)``  (8 zero bytes) on any error.
    """
    import struct
    import zipfile

    _ICON_SIZE = 200
    icon_key = f"icons/{char_name.lower()}_icon.rgba"
    try:
        with zipfile.ZipFile(nrm_path, "r") as nrm:
            rgba = nrm.read(icon_key)
    except Exception as exc:
        logger.warning("anchor_mnsg.load_icon_rgba: cannot read %s from %s: %s",
                       icon_key, nrm_path, exc)
        return struct.pack(">II", 0, 0)

    expected = _ICON_SIZE * _ICON_SIZE * 4
    if len(rgba) != expected:
        logger.warning("anchor_mnsg.load_icon_rgba: unexpected size %d for %s (expected %d)",
                       len(rgba), icon_key, expected)
        return struct.pack(">II", 0, 0)

    return struct.pack(">II", _ICON_SIZE, _ICON_SIZE) + rgba
