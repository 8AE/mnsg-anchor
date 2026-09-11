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
  MNSG_BOSS_DEFEAT   - Transient, ordered live-encounter boss finish event.
  <custom type>      - Any other type is broadcast to your room or team.

Key packet types received from the server:
  ALL_CLIENT_STATE   - Full snapshot of every client state in the room.
  UPDATE_TEAM_STATE  - A teammate's save state (response to REQUEST_TEAM_STATE).
  UPDATE_ROOM_STATE  - Room settings changed.
  HEARTBEAT          - Server-to-client liveness probe; consume without reply.
  SERVER_MESSAGE     - Text message from the server operator.
  DISABLE_ANCHOR     - Server is kicking this client; disconnect.
  SET_FLAG           - An item/flag acquired by a teammate (queued by the server).
  MNSG_BOSS_DEFEAT   - Sync the native boss kill state before reward flags.
  REQUEST_TEAM_STATE - A teammate is requesting the full team state.
  <custom type>      - Custom packet broadcast by another client.

Item-sync protocol (implemented in src/item_sync.c):
  On connect with a valid save:
    1. anchor_mnsg.request_team_state() is called so the server delivers
       the compact team snapshot followed by queued SET_FLAG deltas.
    2. One loaded teammate is elected to answer with a single
       UPDATE_TEAM_STATE packet, avoiding duplicate join-time floods.
  Each frame:
    - Incoming Congo defeat events sync kill flag 0x1A1 before reward 0x12D.
    - Boss flags are deferred while the matching local encounter is active.
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
import math
import secrets
from collections import deque
import anchor_congo
import anchor_dharumanyo
import anchor_tsurami
import anchor_impact

logger = logging.getLogger("anchor_mnsg")

###############################################################################
# Module-level state
###############################################################################

_sock: "socket.socket | None" = None
_connected: bool = False
_disabled: bool = False
_client_id: int = 0
_room_id: str = ""
DEFAULT_TEAM_ID: str = "default"
_team_id: str = DEFAULT_TEAM_ID
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
_local_enemy_room: int = -1
_local_enemy_sig: int = 0
_local_enemy_bits: str = ""
_last_position_sent: "tuple[int, int, int] | None" = None
_last_position_sent_ms: int = 0
_last_position_room_id: int = -1
_last_position_action: int = -2
_last_position_frame_100: int = 0
_last_position_appearance_flags: int = -1
_last_position_collision_disabled: int = -1
_last_position_drive: "tuple[int, int]" = (0, 0)
_last_position_epoch: int = 0
# Durable last-gameplay transform used only while the local client is in the
# World Map room.  currentRoomId must remain 0x226 for gameplay isolation, so
# this separate snapshot lets late joiners place the player on Japan's map.
# Coordinates use signed fixed-point hundredths of a world unit.
_local_map_snapshot: "tuple[int, int, int, int] | None" = None
_local_map_snapshot_explicit: bool = False
_interaction_session: int = 0
_player_hit_seq: int = 0
_player_hits = deque(maxlen=32)
_player_hit_seen: "dict[int, tuple[int, int, int, int]]" = {}
_player_sound_seq: int = 0
_player_sounds = deque(maxlen=64)
_player_sound_seen: "dict[tuple[int, int, int, int], tuple[int, int]]" = {}
_retired_interaction_sessions: "dict[int, set[int]]" = {}
_player_movement_order: "dict[int, dict]" = {}
_projectile_spawns: "dict[tuple[int, int, int, int], tuple[int, dict]]" = {}
_projectile_seen: "dict[tuple[int, int, int, int], tuple[int, int]]" = {}
_projectile_sent: "dict[tuple[int, int, int, int], tuple[int, ...]]" = {}
_projectile_stats = {key: 0 for key in
                     ("sent", "received", "acked", "expired", "rejected", "duplicate", "overflow", "deferred")}
_position_seq: int = 0
_race_status: str = ""
_race_config_json: str = ""

# Live arena events are separate from durable save/flag packets. One pending
# invitation per sender grows with the roster; room snapshots never create an
# invitation or reset its consumed marker.
_arena_local_state: "tuple[str, int, int]" = ("", 0, 0)
_arena_sequence: int = 0
_arena_events: dict = {}
_arena_seen: dict = {}
_arena_retired_sessions: "dict[int, set[int]]" = {}
_arena_confirmed_sessions: "dict[int, int]" = {}
_congo = anchor_congo.CongoTransport()
_dharumanyo = anchor_dharumanyo.DharumanyoTransport()
_tsurami = anchor_tsurami.TsuramiTransport()
_impact = anchor_impact.ImpactTransport()
_impact_encounter: int = 0
_impact_debug_str: str = ""

###############################################################################
# Constants
###############################################################################

DEFAULT_HOST: str = "anchor.hm64.org"
DEFAULT_PORT: int = 43383
ROOM_ID_PREFIX: str = "mnsg-"
ROOM_ID_TRIM_CHARS: str = " \t\n\r\v\f"
WORLD_MAP_ROOM_ID: int = 0x226
MAP_ROOM_METADATA_KEY: str = "mnsgMapRoomId"
MAP_X_METADATA_KEY: str = "mnsgMapX"
MAP_Y_METADATA_KEY: str = "mnsgMapY"
MAP_Z_METADATA_KEY: str = "mnsgMapZ"
MAP_METADATA_KEYS: "tuple[str, ...]" = (
    MAP_ROOM_METADATA_KEY,
    MAP_X_METADATA_KEY,
    MAP_Y_METADATA_KEY,
    MAP_Z_METADATA_KEY,
)
MAP_ROOM_MAX: int = 0xffff
MAP_COORD_SCALE: int = 100
MAP_COORD_MIN: int = -0x80000000
MAP_COORD_MAX: int = 0x7fffffff
MOVEMENT_MIN_INTERVAL_MS: int = 50
ANCHOR_MAX_PACKET_BYTES: int = 8 * 1024 * 1024
HOT_PACKET_MAX_BYTES: "dict[str, int]" = {
    "MNSG_PLAYER_POS": 640,
    "MNSG_PLAYER_HIT": 512,
    "MNSG_PLAYER_SOUND": 320,
    "MNSG_PROJECTILE_SPAWN": 512,
    "MNSG_ENEMY_LIVE": 3072,
    "MNSG_ENEMY_HIT": 256,
    anchor_congo.PACKET_TYPE: 8 * 1024,
    anchor_dharumanyo.PACKET_TYPE: 8 * 1024,
    anchor_tsurami.PACKET_TYPE: 8 * 1024,
    anchor_impact.PACKET_TYPE: 8 * 1024,
}
PLAYER_HIT_MAX_AGE_MS: int = 500
PLAYER_SOUND_MAX_AGE_MS: int = 500
PLAYER_SOUND_BATCH_COUNT: int = 8
PLAYER_SOUND_QUEUE_COUNT: int = 64
TRANSFER_TARGET_MAX_AGE_MS: int = 5000
TRANSFER_ROOM_MAX: int = 0x225
TRANSFER_COORD_MIN: int = -0x8000
TRANSFER_COORD_MAX: int = 0x7FFF
PLAYER_SOUND_TIMESTAMP_MAX: int = 0x7fffffffffffffff
# 0x026D has a matching 0x826D global stop command. The native mixer owns
# sounds by cue rather than player, so replaying that loop for one peer could
# stop (or be stopped by) the local player or another peer.
PLAYER_SOUND_BLOCKED_IDS = frozenset({0x026D})
PROJECTILE_MAX_AGE_MS: int = 750
PROJECTILE_BATCH_COUNT: int = 16
PROJECTILE_QUEUE_COUNT: int = 64
PROJECTILE_MAX_JSON_BYTES: int = 512
CONGO_ARENA: int = 1
CONGO_ROOM: int = 0x16
# The giant-robot Impact bosses. Each has its own arena ID/name; an Impact
# invitation also carries the exact native stage so the guest loads the same
# cutscene, minigame or boss stage as the sender.
KASHIWAGI_ARENA: int = 5
THAISAMBA_ARENA: int = 6
BALBERRA_ARENA: int = 7
DETOILE_ARENA: int = 8
IMPACT_ARENA_FIRST: int = KASHIWAGI_ARENA
IMPACT_ARENA_LAST: int = DETOILE_ARENA
# Intro cutscene stages 0x239..0x23C, one per boss (0x239 = Kashiwagi).
# Minigames 0x21C..0x21F and boss stages 0x220..0x223 follow the same order.
# (Stage 0x0224 is an unused fifth slot; the boss rush mode uses stage 0x0260.)
IMPACT_STAGE_FIRST: int = 0x21C
IMPACT_STAGE_LAST: int = 0x223
IMPACT_INTRO_FIRST: int = 0x239
IMPACT_INTRO_LAST: int = 0x23C


def impact_arena(arena: int) -> bool:
    return IMPACT_ARENA_FIRST <= arena <= IMPACT_ARENA_LAST


def impact_stage_valid(stage: int) -> bool:
    return (type(stage) is int and
            (IMPACT_STAGE_FIRST <= stage <= IMPACT_STAGE_LAST or
             IMPACT_INTRO_FIRST <= stage <= IMPACT_INTRO_LAST))
BOSS_ARENA_ROOMS: "dict[int, int]" = {
    CONGO_ARENA: CONGO_ROOM,
    2: 0x49,  # Dharumanyo
    3: 0x71,  # Tsurami
    4: 0x155,  # Control Machine (Koryuta dragon flight)
    KASHIWAGI_ARENA: 0x220,  # Kashiwagi (first giant-robot Impact boss)
    THAISAMBA_ARENA: 0x221,  # Thaisamba 2
    BALBERRA_ARENA: 0x222,  # Balberra
    DETOILE_ARENA: 0x223,  # D'Etoile
}
ARENA_METADATA_WAIT_MS: int = 5000
ANIMATION_RESTART_DELTA_100: int = 50
ENEMY_BITMAP_HEX_MAX: int = 64
APPEARANCE_SUDDEN_IMPACT: int = 1 << 0
APPEARANCE_MINI_EBISUMARU: int = 1 << 1
APPEARANCE_HURT_RECOVERY: int = 1 << 2
APPEARANCE_MASK: int = (APPEARANCE_SUDDEN_IMPACT | APPEARANCE_MINI_EBISUMARU |
                       APPEARANCE_HURT_RECOVERY)
_PROJECTILE_SPAWN_LIMITS = {
    "id": (1, 0x7fffffff), "kind": (1, 255),
    "x100": (-1000000000, 1000000000),
    "y100": (-1000000000, 1000000000),
    "z100": (-1000000000, 1000000000),
    "vx100": (-1000000, 1000000), "vy100": (-1000000, 1000000),
    "vz100": (-1000000, 1000000),
    "rx": (-32768, 32767), "ry": (-32768, 32767), "rz": (-32768, 32767),
    "scale100000": (1, 1000000),
}
_MOVEMENT_STATE_FIELDS: "tuple[str, ...]" = (
    "posX", "posY", "posZ", "velX", "velY", "velZ", "posSeq", "posT",
    "action", "animFrame100", "animFrameCount100", "rotX", "rotY", "rotZ",
    "rotVelX", "rotVelY", "rotVelZ", "animStep100", "hasAnimStep",
    "collisionDisabled",
    "driveX", "driveZ", "playerEpoch", "interactionSession",
)
_POSITION_SEQUENCE_MASK: int = 0x7fffffff
_POSITION_SEQUENCE_HALF_RANGE: int = 0x40000000


def normalize_room_id(room_id: str) -> str:
    """Return the private server room name for a user-facing MNSG room ID."""
    visible_room_id = (room_id or "").strip(ROOM_ID_TRIM_CHARS)
    if not visible_room_id:
        return ""
    if visible_room_id.startswith(ROOM_ID_PREFIX):
        return visible_room_id if len(visible_room_id) > len(ROOM_ID_PREFIX) else ""
    return ROOM_ID_PREFIX + visible_room_id


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


def _bounded_int(value: object, minimum: int, maximum: int, default: int) -> int:
    """Coerce untrusted client-state integers without letting bad peers break recv."""
    try:
        result = int(value)
    except (TypeError, ValueError, OverflowError):
        return default
    return result if minimum <= result <= maximum else default


def _normalize_enemy_bits(bits: object) -> "str | None":
    """Normalize the low-index-first 256-bit byte string used by enemy sync."""
    if not isinstance(bits, str):
        return None
    value = bits.strip().lower()
    if len(value) > ENEMY_BITMAP_HEX_MAX or (len(value) & 1) != 0:
        return None
    if any(ch not in "0123456789abcdef" for ch in value):
        return None
    # C serializes bitmap bytes from the lowest actor indices upward and omits
    # only zero bytes at the high-index end.  Leading zero bytes are therefore
    # meaningful and must never be stripped.
    while value.endswith("00"):
        value = value[:-2]
    return value


def _valid_map_room_id(value: object) -> bool:
    """Return whether value is a gameplay room suitable for a map snapshot."""
    return (
        type(value) is int
        and 0 <= value <= MAP_ROOM_MAX
        and value != WORLD_MAP_ROOM_ID
    )


def _valid_map_coordinate(value: object) -> bool:
    """Validate a signed fixed-point map coordinate stored in metadata."""
    return type(value) is int and MAP_COORD_MIN <= value <= MAP_COORD_MAX


def _scale_map_coordinate(value: object) -> "int | None":
    """Convert a finite world coordinate to signed fixed-point hundredths."""
    if type(value) is int:
        scaled = value * MAP_COORD_SCALE
    elif type(value) is float and math.isfinite(value):
        scaled_float = value * MAP_COORD_SCALE
        if not math.isfinite(scaled_float):
            return None
        scaled = int(round(scaled_float))
    else:
        return None
    return scaled if _valid_map_coordinate(scaled) else None


def _map_snapshot_from_payload(payload: dict) -> "tuple[int, int, int, int] | None":
    """Validate a complete durable map snapshot without accepting partial data."""
    if not all(key in payload for key in MAP_METADATA_KEYS):
        return None
    room = payload[MAP_ROOM_METADATA_KEY]
    x = payload[MAP_X_METADATA_KEY]
    y = payload[MAP_Y_METADATA_KEY]
    z = payload[MAP_Z_METADATA_KEY]
    if not _valid_map_room_id(room):
        return None
    if not all(_valid_map_coordinate(value) for value in (x, y, z)):
        return None
    return room, x, y, z


def _cache_map_snapshot(state: dict, payload: dict) -> bool:
    """Merge explicit map metadata, clearing it when an explicit record is invalid."""
    if not any(key in payload for key in MAP_METADATA_KEYS):
        return False
    snapshot = _map_snapshot_from_payload(payload)
    if snapshot is None:
        state[MAP_ROOM_METADATA_KEY] = -1
        state[MAP_X_METADATA_KEY] = 0
        state[MAP_Y_METADATA_KEY] = 0
        state[MAP_Z_METADATA_KEY] = 0
    else:
        room, x, y, z = snapshot
        state[MAP_ROOM_METADATA_KEY] = room
        state[MAP_X_METADATA_KEY] = x
        state[MAP_Y_METADATA_KEY] = y
        state[MAP_Z_METADATA_KEY] = z
    return True


def _local_map_metadata() -> dict:
    """Build the authoritative replacement-safe map record for client metadata."""
    snapshot = _local_map_snapshot
    if snapshot is None:
        snapshot = (-1, 0, 0, 0)
    return {
        MAP_ROOM_METADATA_KEY: snapshot[0],
        MAP_X_METADATA_KEY: snapshot[1],
        MAP_Y_METADATA_KEY: snapshot[2],
        MAP_Z_METADATA_KEY: snapshot[3],
    }


def _clear_movement_state(state: dict) -> None:
    """Invalidate a transform without discarding identity/room metadata."""
    for field in _MOVEMENT_STATE_FIELDS:
        state.pop(field, None)
    state.pop("_positionReceivedMs", None)


def _retire_interaction_session(cid: int, session: int) -> None:
    if session > 0:
        # Keep retired identities for this whole local connection. Evicting
        # an older token would let its delayed movement become current again.
        _retired_interaction_sessions.setdefault(cid, set()).add(session)


def _position_sample_is_fresh(state: dict, payload: dict) -> bool:
    """Return whether a hot movement sample supersedes the stored sample."""
    if "posSeq" not in payload or "posSeq" not in state:
        return True
    previous_seq = int(state["posSeq"]) & _POSITION_SEQUENCE_MASK
    current_seq = int(payload["posSeq"]) & _POSITION_SEQUENCE_MASK
    sequence_delta = (current_seq - previous_seq) & _POSITION_SEQUENCE_MASK
    previous_time = int(state.get("posT", 0))
    current_time = int(payload.get("posT", 0))

    if previous_time > 0 and current_time > 0 and current_time <= previous_time:
        # A delayed packet from before a reconnect can look sequence-forward
        # relative to the reset counter. Sender time is the stronger ordering
        # signal whenever both peers provide it.
        return False
    if sequence_delta == 0:
        # Compatibility for a legacy sender that reuses sequence zero.
        return current_time > previous_time > 0
    if sequence_delta < _POSITION_SEQUENCE_HALF_RANGE:
        return True
    # Reconnect resets the sender sequence while monotonic time keeps moving.
    # An actually reordered packet has an older timestamp and remains stale.
    return current_time > previous_time > 0


def _merge_client_state(
    cid: int, payload: dict, enforce_movement_order: bool = False
) -> bool:
    """Merge metadata, keeping a room change separate from its next transform."""
    state = _player_states.get(cid)
    if state is None:
        state = {
            "name": payload.get("name", f"Player{cid}"),
            "teamId": payload.get("teamId", ""),
            "online": bool(payload.get("online", True)),
            "isSaveLoaded": bool(payload.get("isSaveLoaded", False)),
            "self": False,
            "location": payload.get("currentRoom", ""),
            "roomId": int(payload.get("currentRoomId", -1)),
            "mnsgRace": str(payload.get("mnsgRace", "")),
            "mnsgRaceConfig": str(payload.get("mnsgRaceConfig", "")),
            "appearanceFlags": _appearance_flags_from_payload(payload),
            "character": str(payload.get("currentCharacter", "")),
            "er": _bounded_int(payload.get("er", -1), -1, 0xFFFF, -1),
            "es": _bounded_int(payload.get("es", -1), 0, 0xFFFFFFFF, -1),
            "eb": _normalize_enemy_bits(payload.get("eb")),
        }
        _player_states[cid] = state

    if enforce_movement_order:
        session = int(payload.get("interactionSession", 0))
        epoch = int(payload.get("playerEpoch", 0))
        if not (0 <= session <= _POSITION_SEQUENCE_MASK and
                0 <= epoch <= _POSITION_SEQUENCE_MASK):
            return False
        previous_order = _player_movement_order.get(cid, state)
        previous_session = int(previous_order.get("interactionSession", 0))
        if session in _retired_interaction_sessions.get(cid, ()):
            return False
        new_session = session > 0 and session != previous_session
        if not new_session and not _position_sample_is_fresh(previous_order, payload):
            return False
        if session == previous_session and session > 0:
            previous_epoch = int(previous_order.get("playerEpoch", 0))
            if epoch > 0 and epoch < previous_epoch:
                return False
        if session != previous_session:
            _retire_interaction_session(cid, previous_session)
            _clear_movement_state(state)

    if "currentRoomId" in payload:
        next_room = int(payload["currentRoomId"])
        previous_room = int(state.get("roomId", -1))
        if next_room != previous_room:
            # UPDATE_CLIENT_STATE room metadata normally arrives just before the
            # hot movement packet. Clear the complete old sample first; any
            # position fields carried atomically below repopulate a fresh one.
            _clear_movement_state(state)
        state["roomId"] = next_room

    string_fields = {
        "name": "name",
        "teamId": "teamId",
        "currentRoom": "location",
        "mnsgRace": "mnsgRace",
        "mnsgRaceConfig": "mnsgRaceConfig",
        "currentCharacter": "character",
    }
    for source, destination in string_fields.items():
        if source in payload:
            state[destination] = str(payload[source])
    if "online" in payload:
        state["online"] = bool(payload["online"])
        if not state["online"]:
            _drop_projectile_spawns(cid)
            _drop_player_sounds(cid)
    if "isSaveLoaded" in payload:
        state["isSaveLoaded"] = bool(payload["isSaveLoaded"])
    if "er" in payload:
        state["er"] = _bounded_int(payload["er"], -1, 0xFFFF, -1)
    if "es" in payload:
        state["es"] = _bounded_int(payload["es"], 0, 0xFFFFFFFF, -1)
    if "eb" in payload:
        state["eb"] = _normalize_enemy_bits(payload["eb"])
    _cache_map_snapshot(state, payload)
    for field in _MOVEMENT_STATE_FIELDS:
        if field in payload:
            state[field] = int(payload[field])
    if enforce_movement_order or "posX" in payload:
        # Collision bypass belongs to this exact movement sample. Legacy
        # senders omit it and must resume collision rather than inherit a
        # cutscene bypass from an earlier sender version or connection.
        state["collisionDisabled"] = (
            1 if int(payload.get("collisionDisabled", 0)) else 0
        )
        state["driveX"] = max(-30000, min(30000, int(payload.get("driveX", 0))))
        state["driveZ"] = max(-30000, min(30000, int(payload.get("driveZ", 0))))
        for field in ("playerEpoch", "interactionSession"):
            value = int(payload.get(field, 0))
            state[field] = value if 0 < value <= _POSITION_SEQUENCE_MASK else 0
        # Optional animation rate belongs to the same sample, including an
        # atomic room change. A legacy sender must clear any retained rate.
        if enforce_movement_order:
            state["animStep100"] = int(payload.get("animStep100", 0))
            state["hasAnimStep"] = int(payload.get("hasAnimStep", 0))
            _player_movement_order[cid] = {
                field: state[field] for field in
                ("posSeq", "posT", "interactionSession", "playerEpoch")
                if field in state
            }
            if session > 0 and epoch > 0:
                _drop_player_sounds(
                    cid, (cid, session, epoch, int(state.get("roomId", -1)))
                )
    if enforce_movement_order or "posX" in payload:
        # Legacy movement has no hurt-recovery bit; never retain a newer
        # sender's recovery state after that sender stops reporting it.
        state["appearanceFlags"] = _appearance_flags_from_payload(
            payload, int(state.get("appearanceFlags", 0)) & ~APPEARANCE_HURT_RECOVERY
        )
    elif ("appearanceFlags" in payload or
            "suddenImpact" in payload or
            "modelScale100000" in payload):
        state["appearanceFlags"] = _appearance_flags_from_payload(
            payload, int(state.get("appearanceFlags", 0))
        )

    # A confirmed visit ends at the metadata edge, even if the sender returns
    # before the game's next invitation poll. Unconfirmed entries retain their
    # grace period because room/session metadata may follow the arena event.
    if anchor_congo.METADATA_KEY in payload:
        value = anchor_congo.merge_metadata(
            state.get(anchor_congo.METADATA_KEY),
            payload[anchor_congo.METADATA_KEY],
            state.get("interactionSession"),
        )
        if value and value[5] not in _retired_interaction_sessions.get(cid, ()):
            state[anchor_congo.METADATA_KEY] = value
    if anchor_dharumanyo.METADATA_KEY in payload:
        value = anchor_dharumanyo.merge_metadata(
            state.get(anchor_dharumanyo.METADATA_KEY),
            payload[anchor_dharumanyo.METADATA_KEY],
            state.get("interactionSession"),
        )
        if value and value[5] not in _retired_interaction_sessions.get(cid, ()):
            state[anchor_dharumanyo.METADATA_KEY] = value
    if anchor_tsurami.METADATA_KEY in payload:
        value = anchor_tsurami.merge_metadata(
            state.get(anchor_tsurami.METADATA_KEY),
            payload[anchor_tsurami.METADATA_KEY],
            state.get("interactionSession"),
        )
        if value and value[5] not in _retired_interaction_sessions.get(cid, ()):
            state[anchor_tsurami.METADATA_KEY] = value
    if anchor_impact.METADATA_KEY in payload:
        value = anchor_impact.merge_metadata(
            state.get(anchor_impact.METADATA_KEY),
            payload[anchor_impact.METADATA_KEY],
            state.get("interactionSession"),
        )
        if value and value[5] not in _retired_interaction_sessions.get(cid, ()):
            state[anchor_impact.METADATA_KEY] = value
    now_ms = int(time.monotonic() * 1000)
    if enforce_movement_order:
        # Sender monotonic timestamps are useful only for packet ordering; they
        # cannot be compared across machines.  Keep a local receive timestamp
        # for actions (such as player transfer) that require a fresh target.
        state["_positionReceivedMs"] = now_ms
    context = _boss_context()
    _congo.observe(context)
    _dharumanyo.observe(context)
    _tsurami.observe(context)
    _impact.observe(context)
    _invalidate_confirmed_boss_invitation(cid)
    _prune_projectile_spawns(now_ms)
    return True


def _replace_all_client_states(states: list) -> None:
    """Apply membership metadata without popping live same-room transforms."""
    global _client_id

    with _player_states_lock:
        previous_players = dict(_player_states)
        new_players: dict = {}
        for member in states:
            cid = int(member.get("clientId", 0))
            if not cid:
                continue
            if member.get("self"):
                _client_id = cid
            client_state = member.get("clientState", member)
            room_id = int(client_state.get("currentRoomId", -1))
            previous = previous_players.get(cid, {})
            name = (client_state.get("name", "") or
                    member.get("name", f"Player{cid}"))
            merged = {
                "name": name,
                "teamId": client_state.get("teamId", ""),
                "online": bool(client_state.get("online", True)),
                "isSaveLoaded": bool(client_state.get("isSaveLoaded", False)),
                "self": bool(member.get("self")),
                "location": client_state.get("currentRoom", ""),
                "roomId": room_id,
                "mnsgRace": str(client_state.get("mnsgRace", "")),
                "mnsgRaceConfig": str(
                    client_state.get("mnsgRaceConfig", "")
                ),
                "character": str(
                    client_state.get(
                        "currentCharacter", previous.get("character", "")
                    )
                ),
                "er": _bounded_int(client_state.get("er", -1), -1, 0xFFFF, -1),
                "es": _bounded_int(
                    client_state.get("es", -1), 0, 0xFFFFFFFF, -1
                ),
                "eb": _normalize_enemy_bits(client_state.get("eb")),
            }
            _cache_map_snapshot(merged, client_state)
            if previous and int(previous.get("roomId", -1)) == room_id:
                for field in _MOVEMENT_STATE_FIELDS:
                    if field in previous:
                        merged[field] = previous[field]
                if "_positionReceivedMs" in previous:
                    merged["_positionReceivedMs"] = previous["_positionReceivedMs"]
                if "appearanceFlags" in previous:
                    merged["appearanceFlags"] = previous["appearanceFlags"]
            # A fresh handshake already identifies this live connection.
            # Arena events do not require movement to have started, but an
            # older room snapshot must not replace a newer motion session.
            snapshot_session = client_state.get("interactionSession")
            if ("interactionSession" not in merged and
                    type(snapshot_session) is int and
                    0 < snapshot_session <= _POSITION_SEQUENCE_MASK and
                    snapshot_session not in _retired_interaction_sessions.get(cid, ())):
                merged["interactionSession"] = snapshot_session
            if ("appearanceFlags" in client_state or
                    "suddenImpact" in client_state or
                    "modelScale100000" in client_state):
                merged["appearanceFlags"] = _appearance_flags_from_payload(
                    client_state, int(merged.get("appearanceFlags", 0))
                )
            congo = anchor_congo.merge_metadata(
                previous.get(anchor_congo.METADATA_KEY),
                client_state.get(anchor_congo.METADATA_KEY),
                merged.get("interactionSession"),
            )
            if congo and congo[5] not in _retired_interaction_sessions.get(cid, ()):
                merged[anchor_congo.METADATA_KEY] = congo
            dharumanyo = anchor_dharumanyo.merge_metadata(
                previous.get(anchor_dharumanyo.METADATA_KEY),
                client_state.get(anchor_dharumanyo.METADATA_KEY),
                merged.get("interactionSession"),
            )
            if (dharumanyo and
                    dharumanyo[5] not in _retired_interaction_sessions.get(cid, ())):
                merged[anchor_dharumanyo.METADATA_KEY] = dharumanyo
            tsurami = anchor_tsurami.merge_metadata(
                previous.get(anchor_tsurami.METADATA_KEY),
                client_state.get(anchor_tsurami.METADATA_KEY),
                merged.get("interactionSession"),
            )
            if (tsurami and
                    tsurami[5] not in _retired_interaction_sessions.get(cid, ())):
                merged[anchor_tsurami.METADATA_KEY] = tsurami
            impact = anchor_impact.merge_metadata(
                previous.get(anchor_impact.METADATA_KEY),
                client_state.get(anchor_impact.METADATA_KEY),
                merged.get("interactionSession"),
            )
            if (impact and
                    impact[5] not in _retired_interaction_sessions.get(cid, ())):
                merged[anchor_impact.METADATA_KEY] = impact
            new_players[cid] = merged
        for cid, previous in previous_players.items():
            replacement = new_players.get(cid, {})
            if not replacement.get("online", False):
                _drop_projectile_spawns(cid)
                _drop_player_sounds(cid)
            if cid not in new_players:
                order = _player_movement_order.pop(cid, previous)
                _retire_interaction_session(cid, int(order.get("interactionSession", 0)))
        _player_states.clear()
        _player_states.update(new_players)
        context = _boss_context()
        _congo.observe(context)
        _dharumanyo.observe(context)
        _tsurami.observe(context)
        _impact.observe(context)
        for cid in list(_arena_events):
            _invalidate_confirmed_boss_invitation(cid)
        now_ms = int(time.monotonic() * 1000)
        _prune_projectile_spawns(now_ms)
        _prune_player_sounds(now_ms)

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
    if not isinstance(packet, dict):
        return False
    packet_type = packet.get("type")
    if packet_type != "STATS":
        sender = packet.get("clientId")
        if type(sender) is not int or sender <= 0 or sender != _client_id:
            logger.warning(
                "anchor_mnsg: refusing %r packet without the assigned root clientId",
                packet_type,
            )
            return False
    try:
        data = (json.dumps(packet, separators=(",", ":"), allow_nan=False) + "\x00").encode("utf-8")
    except (TypeError, ValueError) as exc:
        logger.warning("anchor_mnsg: refusing invalid JSON packet: %s", exc)
        return False
    if len(data) > ANCHOR_MAX_PACKET_BYTES:
        logger.warning(
            "anchor_mnsg: refusing %d-byte packet over Anchor's %d-byte limit",
            len(data), ANCHOR_MAX_PACKET_BYTES,
        )
        return False
    packet_budget = HOT_PACKET_MAX_BYTES.get(str(packet_type or ""))
    if packet_budget is not None and len(data) > packet_budget:
        logger.warning(
            "anchor_mnsg: refusing %d-byte %s packet over its %d-byte budget",
            len(data), packet_type, packet_budget,
        )
        return False
    try:
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


def _should_handle_enemy_state_request(packet: dict) -> bool:
    """Elect one matching room occupant to answer a compact enemy request."""
    requester = _bounded_int(packet.get("clientId", 0), 0, 0xFFFFFFFF, 0)
    room = _bounded_int(packet.get("r", -1), 0, 0xFFFF, -1)
    signature = _bounded_int(packet.get("s", -1), 0, 0xFFFF, -1)
    if _client_id <= 0 or room < 0 or signature < 0:
        return True

    with _player_states_lock:
        candidates = [
            cid
            for cid, state in _player_states.items()
            if cid != requester
            and bool(state.get("online", False))
            and bool(state.get("isSaveLoaded", False))
            and state.get("teamId", "") == _team_id
            and _bounded_int(state.get("roomId", -1), -1, 0xFFFF, -1) == room
            and _bounded_int(state.get("er", -1), -1, 0xFFFF, -1) == room
            and _bounded_int(state.get("es", -1), 0, 0xFFFF, -1) == signature
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
                if not _connected or _sock is not sock:
                    return
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
                packet_budget = HOT_PACKET_MAX_BYTES.get(str(ptype or ""))
                if packet_budget is not None and sep + 1 > packet_budget:
                    logger.warning(
                        "anchor_mnsg: refusing received %d-byte %s packet over its %d-byte budget",
                        sep + 1, ptype, packet_budget,
                    )
                    continue

                # ---- Server-managed special packets -------------------------
                if ptype == "DISABLE_ANCHOR":
                    _disabled = True
                    logger.info("anchor_mnsg: DISABLE_ANCHOR received, disconnecting.")
                    _do_disconnect(sock)
                    return

                if ptype == "HEARTBEAT":
                    # Anchor uses this as a server-to-client write probe and
                    # refreshes activity after the write succeeds. Echoing it
                    # would take the generic room-broadcast route and can
                    # multiply into a heartbeat storm across idle clients.
                    continue  # Don't forward heartbeats to game code.

                if ptype == "SERVER_MESSAGE":
                    _server_message = packet.get("message", "")
                    logger.info("anchor_mnsg: SERVER_MESSAGE: %s", _server_message)
                    # Fall through so game code can also react if desired.

                # Track our own assigned client ID and player list from ALL_CLIENT_STATE.
                if ptype == "ALL_CLIENT_STATE":
                    states = packet.get("state", [])
                    _replace_all_client_states(states)
                    # Reset so the next set_local_room() call re-broadcasts our room
                    # and character even if the values haven't changed (our entry was
                    # just rebuilt, and older movement packets may have wiped metadata).
                    _local_room_id = -1
                    _local_character = ""

                if ptype == "MNSG_PLAYER_POS":
                    cid = packet.get("clientId", 0)
                    if cid:
                        with _player_states_lock:
                            _merge_client_state(
                                cid, packet, enforce_movement_order=True
                            )
                    # Movement is already coalesced into _player_states. Do not
                    # duplicate this hot path in the general game-event queue.
                    continue

                if ptype == "MNSG_PLAYER_HIT":
                    _receive_player_hit(packet)
                    continue

                if ptype == "MNSG_PLAYER_SOUND":
                    _receive_player_sound(packet)
                    continue

                if ptype == "MNSG_PROJECTILE_SPAWN":
                    _receive_projectile_spawn(packet)
                    continue

                if ptype == "MNSG_BOSS_ARENA":
                    _receive_boss_arena(packet)
                    continue

                if ptype == anchor_congo.PACKET_TYPE:
                    with _player_states_lock:
                        _congo.receive(_boss_context(), packet, time.monotonic())
                    continue

                if ptype == anchor_dharumanyo.PACKET_TYPE:
                    with _player_states_lock:
                        _dharumanyo.receive(
                            _boss_context(), packet, time.monotonic()
                        )
                    continue

                if ptype == anchor_tsurami.PACKET_TYPE:
                    with _player_states_lock:
                        _tsurami.receive(
                            _boss_context(), packet, time.monotonic()
                        )
                    continue

                if ptype == anchor_impact.PACKET_TYPE:
                    with _player_states_lock:
                        _impact.receive(_boss_context(), packet, time.monotonic())
                    continue

                if ptype == "MNSG_PROJECTILES":
                    # Retired continuous-visual protocol: never replay these
                    # old packets through the durable item/event queue.
                    continue

                # Update a single player's status when they broadcast their state.
                if ptype == "UPDATE_CLIENT_STATE":
                    # The server relays the packet as-is. We put clientId inside
                    # "state", so look there first, then fall back to the root.
                    cs = packet.get("state") or packet.get("clientState") or {}
                    cid = cs.get("clientId") or packet.get("clientId", 0)
                    if cid:
                        with _player_states_lock:
                            _merge_client_state(cid, cs)

                if ptype == "REQUEST_TEAM_STATE" and not _should_handle_team_state_request(packet):
                    continue
                if ptype == "MNSG_ER" and not _should_handle_enemy_state_request(packet):
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
    global _local_enemy_room, _local_enemy_sig, _local_enemy_bits
    global _last_position_sent, _last_position_sent_ms, _last_position_room_id
    global _last_position_action, _last_position_frame_100
    global _last_position_appearance_flags, _last_position_collision_disabled
    global _last_position_drive, _last_position_epoch, _local_map_snapshot
    global _local_map_snapshot_explicit
    global _interaction_session
    global _player_hit_seq, _player_sound_seq

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
    _local_enemy_room = -1
    _local_enemy_sig = 0
    _local_enemy_bits = ""
    _last_position_sent = None
    _last_position_sent_ms = 0
    _last_position_room_id = -1
    _last_position_action = -2
    _last_position_frame_100 = 0
    _last_position_appearance_flags = -1
    _last_position_collision_disabled = -1
    _last_position_drive = (0, 0)
    _last_position_epoch = 0
    _local_map_snapshot = None
    _local_map_snapshot_explicit = False
    _interaction_session = 0
    _player_hit_seq = 0
    _player_sound_seq = 0
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
        _player_hits.clear()
        _player_hit_seen.clear()
        _reset_player_sounds()
        _retired_interaction_sessions.clear()
        _player_movement_order.clear()
        _reset_projectile_spawns()
        _reset_boss_invitations()
        _congo.reset()
        _dharumanyo.reset()
        _tsurami.reset()
        _impact.reset()


###############################################################################
# Public API  (called from C via REPY_FN macros)
###############################################################################


def connect(
    host: str,
    port: int,
    room_id: str,
    player_name: str,
    client_id: int = 0,
) -> bool:
    """
    Connect to an Anchor server and send the HANDSHAKE packet.

    Args:
        host:        Server hostname or IP. Use '' for the public default.
        port:        Server port. Use 0 for the public default (43383).
        room_id:     User-facing room name (creates it if it doesn't exist).
        player_name: Display name for this player.
        client_id:   Previous client ID for session resumption (0 = new session).

    Returns True on success, False on failure.
    """
    global _sock, _connected, _client_id, _room_id, _team_id, _player_name
    global _last_position_sent, _last_position_sent_ms, _last_position_room_id
    global _position_seq, _local_character
    global _last_position_action, _last_position_frame_100
    global _last_position_appearance_flags, _last_position_collision_disabled
    global _last_position_drive, _last_position_epoch, _local_map_snapshot
    global _local_map_snapshot_explicit
    global _interaction_session
    global _player_hit_seq, _player_sound_seq
    global _rx_thread, _disabled, _race_status, _race_config_json, _local_save_loaded
    global _local_enemy_room, _local_enemy_sig, _local_enemy_bits

    normalized_room_id = normalize_room_id(room_id)
    if not normalized_room_id:
        logger.warning("anchor_mnsg: room id is required; connection cancelled.")
        return False

    if _connected:
        logger.info("anchor_mnsg: already connected.")
        return True

    # The MNSG namespace is deliberately added only at the protocol boundary;
    # connection screens and config continue to show the player's own value.
    _room_id = normalized_room_id
    # Anchor requires a team for shared-state routing. MNSG exposes one shared
    # group per room, so this protocol detail is fixed and never user supplied.
    _team_id = DEFAULT_TEAM_ID
    _player_name = player_name
    _client_id = client_id
    _disabled = False
    _last_position_sent = None
    _last_position_sent_ms = 0
    _last_position_room_id = -1
    _last_position_action = -2
    _last_position_frame_100 = 0
    _last_position_appearance_flags = -1
    _last_position_collision_disabled = -1
    _last_position_drive = (0, 0)
    _last_position_epoch = 0
    _local_map_snapshot = None
    _local_map_snapshot_explicit = False
    _interaction_session = secrets.randbelow(_POSITION_SEQUENCE_MASK) + 1
    _player_hit_seq = 0
    _player_sound_seq = 0
    _position_seq = 0
    _local_character = ""
    _local_save_loaded = False
    _local_enemy_room = -1
    _local_enemy_sig = 0
    _local_enemy_bits = ""
    _race_status = ""
    _race_config_json = ""
    with _player_states_lock:
        _player_hits.clear()
        _player_hit_seen.clear()
        _reset_player_sounds()
        _retired_interaction_sessions.clear()
        _player_movement_order.clear()
        _reset_projectile_spawns()
        _reset_boss_invitations()
        _congo.reset()
        _dharumanyo.reset()
        _tsurami.reset()
        _impact.reset()

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
            "interactionSession": _interaction_session,
            "er": _local_enemy_room,
            "es": _local_enemy_sig,
            "eb": _local_enemy_bits,
            **_local_map_metadata(),
            anchor_congo.METADATA_KEY: _congo.advertisement(_boss_context()),
            anchor_dharumanyo.METADATA_KEY: _dharumanyo.advertisement(
                _boss_context()
            ),
            anchor_tsurami.METADATA_KEY: _tsurami.advertisement(
                _boss_context()
            ),
            anchor_impact.METADATA_KEY: _impact.advertisement(_boss_context()),
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


def _player_hit_matches_live_state(packet: dict) -> bool:
    """Validate a hit against current movement identities; caller holds lock."""
    try:
        sender = int(packet.get("clientId", 0))
        target = int(packet.get("targetClientId", 0))
        source = _player_states.get(sender, {})
        local = _player_states.get(_client_id, {})
        room = int(packet.get("roomId", -1))
        if (packet.get("type") != "MNSG_PLAYER_HIT" or
                not _connected or _client_id <= 0 or sender <= 0 or
                sender == _client_id or target != _client_id or
                not source.get("online", False) or
                source.get("collisionDisabled", 0) or local.get("collisionDisabled", 0) or
                "posX" not in source or "posX" not in local or
                room != _local_room_id or room != int(source.get("roomId", -1)) or
                room != int(local.get("roomId", -1))):
            return False
        if (int(packet.get("sourceSession", 0)) <= 0 or
                int(packet["sourceSession"]) != int(source.get("interactionSession", 0)) or
                int(packet.get("targetSession", 0)) != _interaction_session or
                _interaction_session <= 0 or
                int(local.get("interactionSession", 0)) != _interaction_session or
                int(packet.get("sourceEpoch", 0)) <= 0 or
                int(packet["sourceEpoch"]) != int(source.get("playerEpoch", 0)) or
                int(packet.get("targetEpoch", 0)) <= 0 or
                int(packet["targetEpoch"]) != int(local.get("playerEpoch", 0))):
            return False
        sequence = int(packet.get("hitSeq", 0))
        if not 0 < sequence <= _POSITION_SEQUENCE_MASK:
            return False
        # More recent movement may arrive before C polls this hit. A request
        # cannot refer to a sender sample that the receiver has not seen yet.
        source_seq = int(packet.get("sourcePosSeq", -1))
        if not 0 <= source_seq <= _POSITION_SEQUENCE_MASK:
            return False
        sequence_lag = (int(source.get("posSeq", 0)) - source_seq) & _POSITION_SEQUENCE_MASK
        if sequence_lag >= _POSITION_SEQUENCE_HALF_RANGE:
            return False
        hit_time = int(packet.get("hitT", 0))
        source_time = int(source.get("posT", 0))
        if (hit_time <= 0 or source_time <= 0 or
                source_time - hit_time > PLAYER_HIT_MAX_AGE_MS or
                hit_time - source_time > 5000):
            return False
        for coordinate in ("hitX", "hitY", "hitZ"):
            value = float(packet[coordinate])
            if not math.isfinite(value) or abs(value) > 10000000.0:
                return False
        return True
    except (TypeError, ValueError, KeyError, OverflowError):
        return False


def _receive_player_hit(packet: dict) -> bool:
    """Accept a bounded transient hit once; never enqueue it for item sync."""
    received_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        if not _player_hit_matches_live_state(packet):
            return False
        sender = int(packet["clientId"])
        identity = (int(packet["sourceSession"]), int(packet["sourceEpoch"]),
                    int(packet["targetEpoch"]))
        sequence = int(packet["hitSeq"])
        previous = _player_hit_seen.get(sender)
        if previous and previous[:3] == identity:
            delta = (sequence - previous[3]) & _POSITION_SEQUENCE_MASK
            if delta == 0 or delta >= _POSITION_SEQUENCE_HALF_RANGE:
                return False
        _player_hit_seen[sender] = (*identity, sequence)
        _player_hits.append((received_ms, dict(packet)))
    return True


def send_player_hit(target_cid: int, target_epoch: int,
                    hit_x: float, hit_y: float, hit_z: float,
                    source_epoch: "int | None" = None) -> bool:
    """Request one native hit on the target owner using current identities."""
    global _player_hit_seq
    try:
        target_cid = int(target_cid)
        target_epoch = int(target_epoch)
        if source_epoch is not None:
            source_epoch = int(source_epoch)
        coordinates = tuple(float(value) for value in (hit_x, hit_y, hit_z))
        if any(not math.isfinite(value) or abs(value) > 10000000.0 for value in coordinates):
            return False
    except (TypeError, ValueError, OverflowError):
        return False
    with _player_states_lock:
        local = _player_states.get(_client_id, {})
        target = _player_states.get(target_cid, {})
        if (not _connected or _client_id <= 0 or target_cid <= 0 or
                target_cid == _client_id or not target.get("online", False) or
                local.get("collisionDisabled", 0) or target.get("collisionDisabled", 0) or
                _local_room_id < 0 or
                int(target.get("roomId", -1)) != _local_room_id or
                int(local.get("roomId", -1)) != _local_room_id or
                "posX" not in local or "posX" not in target or
                target_epoch <= 0 or int(target.get("playerEpoch", 0)) != target_epoch or
                _interaction_session <= 0 or
                int(local.get("interactionSession", 0)) != _interaction_session or
                int(local.get("playerEpoch", 0)) <= 0 or
                (source_epoch is not None and source_epoch != int(local.get("playerEpoch", 0))) or
                int(target.get("interactionSession", 0)) <= 0):
            return False
        next_seq = (_player_hit_seq % _POSITION_SEQUENCE_MASK) + 1
        packet = {
            "type": "MNSG_PLAYER_HIT", "clientId": _client_id,
            "targetClientId": target_cid, "roomId": _local_room_id,
            "sourceSession": _interaction_session,
            "targetSession": int(target["interactionSession"]),
            "sourceEpoch": int(local["playerEpoch"]),
            "targetEpoch": target_epoch, "hitSeq": next_seq,
            "sourcePosSeq": int(local.get("posSeq", 0)),
            "hitT": int(time.monotonic() * 1000),
            "hitX": coordinates[0], "hitY": coordinates[1], "hitZ": coordinates[2],
            "quiet": True,
        }
    if not _send_raw(packet):
        return False
    _player_hit_seq = next_seq
    return True


def poll_player_hit():
    """Return a fresh owner-targeted hit tuple, or None, without any echo."""
    now_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        while _player_hits:
            received_ms, packet = _player_hits.popleft()
            if (now_ms - received_ms > PLAYER_HIT_MAX_AGE_MS or
                    not _player_hit_matches_live_state(packet)):
                continue
            return (int(packet["clientId"]), int(packet["targetEpoch"]),
                    float(packet["hitX"]), float(packet["hitY"]), float(packet["hitZ"]))
    return None


def _reset_player_sounds() -> None:
    """Reset connection-scoped one-shot sound state; caller holds the lock."""
    _player_sounds.clear()
    _player_sound_seen.clear()


def _player_sound_identity(packet: dict) -> tuple:
    return (packet.get("clientId"), packet.get("interactionSession"),
            packet.get("playerEpoch"), packet.get("currentRoomId"))


def _drop_player_sounds(cid: int, keep_identity: "tuple | None" = None) -> None:
    """Drop one sender's stale generations, retaining only an exact match."""
    retained = [entry for entry in _player_sounds
                if entry[1].get("clientId") != cid or
                (keep_identity is not None and
                 _player_sound_identity(entry[1]) == keep_identity)]
    _player_sounds.clear()
    _player_sounds.extend(retained)
    for identity in list(_player_sound_seen):
        if identity[0] == cid and identity != keep_identity:
            del _player_sound_seen[identity]


def _validate_player_sound_ids(sound_ids):
    """Return a canonical one-frame cue batch, or None for unsafe commands."""
    if type(sound_ids) is not list or not 1 <= len(sound_ids) <= PLAYER_SOUND_BATCH_COUNT:
        return None
    validated = []
    seen = set()
    for sound_id in sound_ids:
        # High-bit commands stop a global cue, and IDs below 0x100 are music.
        if (type(sound_id) is not int or not 0x100 <= sound_id <= 0x7fff or
                sound_id in PLAYER_SOUND_BLOCKED_IDS or sound_id in seen):
            return None
        seen.add(sound_id)
        validated.append(sound_id)
    return tuple(validated)


def _player_sound_local_room() -> int:
    """Return the live game room despite a transient membership refresh."""
    local = _player_states.get(_client_id, {})
    room = _local_room_id if _local_room_id >= 0 else _last_position_room_id
    return room if local.get("roomId") == room else -1


def _player_sound_source_status(packet: dict) -> int:
    """Return 0 rejected, 1 awaiting movement, or 2 ready; lock held."""
    if (type(packet) is not dict or packet.get("type") != "MNSG_PLAYER_SOUND" or
            not _connected or _client_id <= 0 or
            _validate_player_sound_ids(packet.get("soundIds")) is None):
        return 0
    for key in ("clientId", "interactionSession", "playerEpoch", "soundSeq",
                "sourcePosSeq"):
        value = packet.get(key)
        if type(value) is not int or not 0 < value <= _POSITION_SEQUENCE_MASK:
            return 0
    sound_time = packet.get("soundT")
    if (type(sound_time) is not int or
            not 0 < sound_time <= PLAYER_SOUND_TIMESTAMP_MAX):
        return 0
    room = packet.get("currentRoomId")
    if type(room) is not int or not 0 <= room <= 0xffff:
        return 0

    sender = packet["clientId"]
    source = _player_states.get(sender)
    local = _player_states.get(_client_id, {})
    if (sender == _client_id or not source or not source.get("online", False) or
            room != _player_sound_local_room() or
            source.get("roomId") != room or
            _interaction_session <= 0 or
            local.get("interactionSession") != _interaction_session or
            local.get("playerEpoch", 0) <= 0 or
            packet.get("localEpoch", local.get("playerEpoch")) != local.get("playerEpoch")):
        return 0

    session = packet["interactionSession"]
    epoch = packet["playerEpoch"]
    if session in _retired_interaction_sessions.get(sender, ()):
        return 0
    order = _player_movement_order.get(sender, source)
    current_session = order.get("interactionSession", 0)
    current_epoch = order.get("playerEpoch", 0)
    # Movement and sound use the same sender TCP stream, and the native sender
    # publishes an epoch-changing movement sample before its sound batch. Do
    # not retain arbitrary unconfirmed generations from an unauthenticated
    # root clientId; they could otherwise grow replay bookkeeping forever.
    if session != current_session or epoch != current_epoch:
        return 0
    if ("posX" not in source or
            source.get("interactionSession") != session or
            source.get("playerEpoch") != epoch):
        return 1

    source_seq = int(source.get("posSeq", 0))
    event_seq = packet["sourcePosSeq"]
    sequence_lag = (source_seq - event_seq) & _POSITION_SEQUENCE_MASK
    if sequence_lag >= _POSITION_SEQUENCE_HALF_RANGE:
        return 1
    source_time = int(source.get("posT", 0))
    if source_time <= 0:
        return 1
    if source_time - sound_time > 5000:
        return 0
    if sound_time - source_time > 5000:
        return 1
    return 2


def _prune_player_sounds(now_ms: int) -> None:
    """Discard locally expired or invalid queued cues; caller holds lock."""
    retained = [entry for entry in _player_sounds
                if now_ms - entry[0] < PLAYER_SOUND_MAX_AGE_MS and
                _player_sound_source_status(entry[1]) != 0]
    _player_sounds.clear()
    _player_sounds.extend(retained)
    for identity in list(_player_sound_seen):
        cid, session, epoch, room = identity
        source = _player_states.get(cid)
        order = _player_movement_order.get(cid, source or {})
        if (cid == _client_id or not source or
                not source.get("online", False) or
                source.get("roomId") != room or
                room != _player_sound_local_room() or
                order.get("interactionSession", 0) != session or
                order.get("playerEpoch", 0) != epoch):
            del _player_sound_seen[identity]


def _receive_player_sound(packet: dict) -> bool:
    """Accept one bounded frame batch without using the durable event queue."""
    sound_ids = _validate_player_sound_ids(
        packet.get("soundIds") if type(packet) is dict else None
    )
    if sound_ids is None:
        return False
    now_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        if _player_sound_source_status(packet) == 0:
            return False
        identity = _player_sound_identity(packet)
        sequence = packet["soundSeq"]
        previous = _player_sound_seen.get(identity)
        highest, bits = sequence, 1
        if previous:
            delta = (sequence - previous[0]) & _POSITION_SEQUENCE_MASK
            if 0 < delta < _POSITION_SEQUENCE_HALF_RANGE:
                bits = ((previous[1] << delta) | 1) & ((1 << 64) - 1) if delta < 64 else 1
            else:
                lag = (previous[0] - sequence) & _POSITION_SEQUENCE_MASK
                if lag >= 64 or previous[1] & (1 << lag):
                    return False
                highest, bits = previous[0], previous[1] | (1 << lag)
        _prune_player_sounds(now_ms)
        if len(_player_sounds) + len(sound_ids) > PLAYER_SOUND_QUEUE_COUNT:
            return False
        pending = {key: packet[key] for key in
                   ("type", "clientId", "currentRoomId", "interactionSession",
                    "playerEpoch", "soundSeq", "sourcePosSeq", "soundT")}
        pending["soundIds"] = list(sound_ids)
        pending["localEpoch"] = _player_states[_client_id].get("playerEpoch", 0)
        _player_sound_seen[identity] = (highest, bits)
        for sound_id in sound_ids:
            _player_sounds.append((now_ms, pending, sound_id))
    return True


def send_player_sounds(interaction_session: int, player_epoch: int,
                       sound_ids: list) -> bool:
    """Broadcast one frame's local-player one-shot cues to the game room."""
    global _player_sound_seq
    validated = _validate_player_sound_ids(sound_ids)
    if (type(interaction_session) is not int or
            not 0 < interaction_session <= _POSITION_SEQUENCE_MASK or
            type(player_epoch) is not int or validated is None):
        return False
    with _player_states_lock:
        local = _player_states.get(_client_id, {})
        sound_time = int(time.monotonic() * 1000)
        if (not _connected or _client_id <= 0 or
                not 0 <= _local_room_id <= 0xffff or
                local.get("roomId") != _local_room_id or "posX" not in local or
                _interaction_session <= 0 or
                interaction_session != _interaction_session or
                local.get("interactionSession") != _interaction_session or
                not 0 < player_epoch <= _POSITION_SEQUENCE_MASK or
                local.get("playerEpoch") != player_epoch or
                not 0 < int(local.get("posSeq", 0)) <= _POSITION_SEQUENCE_MASK or
                not 0 < sound_time <= PLAYER_SOUND_TIMESTAMP_MAX):
            return False
        next_seq = (_player_sound_seq % _POSITION_SEQUENCE_MASK) + 1
        packet = {
            "type": "MNSG_PLAYER_SOUND", "clientId": _client_id,
            "currentRoomId": _local_room_id,
            "interactionSession": interaction_session,
            "playerEpoch": player_epoch, "soundSeq": next_seq,
            "sourcePosSeq": int(local["posSeq"]),
            "soundT": sound_time,
            "soundIds": list(validated), "quiet": True,
        }
    if not _send_raw(packet):
        return False
    _player_sound_seq = next_seq
    return True


def poll_player_sound():
    """Return one fresh spatial cue identity, or None, without echoing it."""
    now_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        _prune_player_sounds(now_ms)
        for _ in range(len(_player_sounds)):
            received_ms, packet, sound_id = _player_sounds.popleft()
            status = _player_sound_source_status(packet)
            if status == 1:
                _player_sounds.append((received_ms, packet, sound_id))
                continue
            if status == 2:
                age_ms = max(0, now_ms - received_ms)
                remaining_ms = max(0, PLAYER_SOUND_MAX_AGE_MS - age_ms)
                return (int(packet["clientId"]),
                        int(packet["interactionSession"]),
                        int(packet["playerEpoch"]), int(sound_id),
                        remaining_ms)
    return None


def _reset_projectile_spawns() -> None:
    """Reset connection-scoped transient state; caller holds the player lock."""
    _projectile_spawns.clear()
    _projectile_seen.clear()
    _projectile_sent.clear()
    for key in _projectile_stats:
        _projectile_stats[key] = 0


def _drop_projectile_spawns(cid: int) -> None:
    if cid == _client_id:
        _projectile_spawns.clear()
    else:
        for key in list(_projectile_spawns):
            if key[0] == cid:
                del _projectile_spawns[key]


def _validate_projectile_spawn(entry):
    """Validate the readable C schema; wire values use this exact field order."""
    if type(entry) is not dict or entry.keys() != _PROJECTILE_SPAWN_LIMITS.keys():
        return None
    for field, (minimum, maximum) in _PROJECTILE_SPAWN_LIMITS.items():
        value = entry[field]
        if type(value) is not int or not minimum <= value <= maximum:
            return None
    return {key: entry[key] for key in _PROJECTILE_SPAWN_LIMITS}


def _projectile_local_room() -> int:
    # Membership temporarily resets the room-broadcast sentinel. The retained
    # local hot room still identifies incoming throws during that short gap.
    local = _player_states.get(_client_id, {})
    room = _local_room_id if _local_room_id >= 0 else _last_position_room_id
    return room if local.get("roomId") == room else -1


def _projectile_owner_status(packet: dict) -> int:
    """Return 0 rejected, 1 awaiting owner movement, or 2 ready; lock held."""
    if packet.get("type") != "MNSG_PROJECTILE_SPAWN" or not _connected or _client_id <= 0:
        return 0
    for key in ("clientId", "interactionSession", "ownerEpoch"):
        value = packet.get(key)
        if type(value) is not int or not 0 < value <= _POSITION_SEQUENCE_MASK:
            return 0
    room = packet.get("currentRoomId")
    if type(room) is not int or not 0 <= room <= 0xffff:
        return 0
    sender = packet["clientId"]
    source = _player_states.get(sender, {})
    local = _player_states.get(_client_id, {})
    if (sender == _client_id or not source.get("online", False) or "posX" not in local or
            room != _projectile_local_room() or _interaction_session <= 0 or
            local.get("interactionSession") != _interaction_session or
            local.get("playerEpoch", 0) <= 0 or
            packet.get("localEpoch", local.get("playerEpoch")) != local.get("playerEpoch")):
        return 0
    session, epoch = packet["interactionSession"], packet["ownerEpoch"]
    if session in _retired_interaction_sessions.get(sender, ()):
        return 0
    # Anchor relays broadcasts from separate goroutines, so a throw can arrive
    # before its preceding movement. Retain it without exposing it to native
    # code until the exact source generation and room are confirmed.
    order = _player_movement_order.get(sender, source)
    current_session = order.get("interactionSession", 0)
    current_epoch = order.get("playerEpoch", 0)
    if session != current_session:
        return 1
    if epoch < current_epoch:
        return 0
    if epoch > current_epoch:
        return 1
    if source.get("roomId") != room:
        return 0
    return 2 if ("posX" in source and source.get("interactionSession") == session and
                 source.get("playerEpoch") == epoch) else 1


def _prune_projectile_spawns(now_ms: int) -> None:
    for key, (received_ms, packet) in list(_projectile_spawns.items()):
        expired = now_ms - received_ms > PROJECTILE_MAX_AGE_MS
        if expired or _projectile_owner_status(packet) == 0:
            del _projectile_spawns[key]
            if expired:
                _projectile_stats["expired"] += 1


def _receive_projectile_spawn(packet: dict) -> bool:
    """Keep each distinct throw until native spawn succeeds and C acknowledges."""
    if type(packet) is not dict:
        return False
    spawn = packet.get("spawn")
    if type(spawn) is not list or len(spawn) != len(_PROJECTILE_SPAWN_LIMITS):
        return False
    entry = _validate_projectile_spawn(dict(zip(_PROJECTILE_SPAWN_LIMITS, spawn)))
    now_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        status = _projectile_owner_status(packet)
        if entry is None or status == 0:
            _projectile_stats["rejected"] += 1
            return False
        sender, session, epoch = packet["clientId"], packet["interactionSession"], packet["ownerEpoch"]
        identity = (sender, session, epoch, packet["currentRoomId"])
        event_id = entry["id"]
        previous = _projectile_seen.get(identity)
        highest, bits = event_id, 1
        if previous:
            delta = (event_id - previous[0]) & _POSITION_SEQUENCE_MASK
            if 0 < delta < _POSITION_SEQUENCE_HALF_RANGE:
                bits = ((previous[1] << delta) | 1) & ((1 << 64) - 1) if delta < 64 else 1
            else:
                lag = (previous[0] - event_id) & _POSITION_SEQUENCE_MASK
                if lag >= 64 or previous[1] & (1 << lag):
                    _projectile_stats["duplicate"] += 1
                    return False
                highest, bits = previous[0], previous[1] | (1 << lag)
        _prune_projectile_spawns(now_ms)
        if len(_projectile_spawns) >= PROJECTILE_QUEUE_COUNT:
            _projectile_stats["overflow"] += 1
            return False
        _projectile_seen[identity] = (highest, bits)
        pending = {key: packet[key] for key in
                   ("type", "clientId", "interactionSession", "ownerEpoch", "currentRoomId")}
        pending["spawn"] = entry
        pending["localEpoch"] = _player_states[_client_id].get("playerEpoch", 0)
        _projectile_spawns[(sender, session, epoch, event_id)] = (now_ms, pending)
        _projectile_stats["received"] += 1
        if status == 1:
            _projectile_stats["deferred"] += 1
    logger.debug("anchor_mnsg: projectile accepted cid=%d epoch=%d id=%d", sender, epoch, event_id)
    return True


def get_projectile_session() -> int:
    return _interaction_session if _connected else 0


def send_projectile_spawn_json(session: int, owner_epoch: int, event_json: str) -> bool:
    """Send a captured throw once; a false return lets C retry the same event."""
    if (type(session) is not int or type(owner_epoch) is not int or
            not 0 < owner_epoch <= _POSITION_SEQUENCE_MASK or
            type(event_json) is not str or len(event_json) > PROJECTILE_MAX_JSON_BYTES):
        return False
    try:
        entry = _validate_projectile_spawn(json.loads(event_json))
    except (ValueError, RecursionError):
        return False
    if entry is None:
        return False
    spawn = tuple(entry.values())
    with _player_states_lock:
        local = _player_states.get(_client_id, {})
        if (not _connected or _client_id <= 0 or session <= 0 or session != _interaction_session or
                not 0 <= _local_room_id <= 0xffff or _last_position_room_id != _local_room_id or
                "posX" not in local or local.get("roomId") != _local_room_id or
                local.get("playerEpoch") != owner_epoch or local.get("interactionSession") != session):
            return False
        key = (session, owner_epoch, _local_room_id, entry["id"])
        if key in _projectile_sent:
            return _projectile_sent[key] == spawn
        packet = {"type": "MNSG_PROJECTILE_SPAWN", "clientId": _client_id,
                  "currentRoomId": _local_room_id, "interactionSession": session,
                  "ownerEpoch": owner_epoch, "spawn": list(spawn), "quiet": True}
    if not _send_raw(packet):
        return False
    with _player_states_lock:
        _projectile_sent[key] = spawn
        while len(_projectile_sent) > PROJECTILE_QUEUE_COUNT:
            del _projectile_sent[next(iter(_projectile_sent))]
        _projectile_stats["sent"] += 1
    logger.debug("anchor_mnsg: projectile sent epoch=%d id=%d", owner_epoch, entry["id"])
    return True


def get_projectile_spawns_json(expected_epoch: "int | None" = None) -> str:
    """Peek up to 16 throws, rotating retries so unavailable kinds cannot starve others."""
    now_ms = int(time.monotonic() * 1000)
    rows = []
    with _player_states_lock:
        local = _player_states.get(_client_id, {})
        if (expected_epoch is not None and
                (type(expected_epoch) is not int or expected_epoch <= 0 or
                 local.get("playerEpoch") != expected_epoch or _projectile_local_room() < 0)):
            return "[]"
        _prune_projectile_spawns(now_ms)
        peeked = []
        for (sender, session, epoch, _event_id), (received_ms, packet) in _projectile_spawns.items():
            if _projectile_owner_status(packet) != 2:
                continue
            rows.append({"cid": sender, "session": session, "epoch": epoch,
                         "age": max(0, now_ms - received_ms), **packet["spawn"]})
            peeked.append((sender, session, epoch, _event_id))
            if len(rows) == PROJECTILE_BATCH_COUNT:
                break
        for key in peeked:
            _projectile_spawns[key] = _projectile_spawns.pop(key)
    return json.dumps(rows, separators=(",", ":"))


def ack_projectile_spawn(cid: int, session: int, epoch: int, event_id: int) -> bool:
    """Acknowledge local native creation only; this sends no network packet."""
    identity = (cid, session, epoch, event_id)
    if any(type(value) is not int or not 0 < value <= _POSITION_SEQUENCE_MASK for value in identity):
        return False
    with _player_states_lock:
        _prune_projectile_spawns(int(time.monotonic() * 1000))
        if (identity not in _projectile_spawns or
                _projectile_owner_status(_projectile_spawns[identity][1]) != 2):
            return False
        del _projectile_spawns[identity]
        _projectile_stats["acked"] += 1
    logger.debug("anchor_mnsg: projectile acknowledged cid=%d epoch=%d id=%d", cid, epoch, event_id)
    return True


def get_projectile_spawn_stats_json() -> str:
    """Connection-scoped counters for tracing capture/send/receive/spawn checks."""
    with _player_states_lock:
        return json.dumps({**_projectile_stats, "pending": len(_projectile_spawns)}, separators=(",", ":"))


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
    try:
        state: dict = json.loads(state_json) if state_json else {}
    except json.JSONDecodeError:
        state = {}

    # Server routing requires this field, but callers cannot override the
    # client's single hidden team through an arbitrary state update.
    state["teamId"] = _team_id
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
    # Anchor replaces the stored clientState object rather than patching it.
    # Repeat the complete bounded record on every metadata update so unrelated
    # room/character/save edges cannot erase a map location needed by a late join.
    state.update(_local_map_metadata())
    state["er"] = _local_enemy_room
    state["es"] = _local_enemy_sig
    state["eb"] = _local_enemy_bits
    state["clientId"] = _client_id
    state["name"] = _player_name
    state["online"] = True
    state["interactionSession"] = _interaction_session
    with _player_states_lock:
        context = _boss_context()
        state[anchor_congo.METADATA_KEY] = _congo.advertisement(context)
        state[anchor_dharumanyo.METADATA_KEY] = (
            _dharumanyo.advertisement(context)
        )
        state[anchor_tsurami.METADATA_KEY] = _tsurami.advertisement(context)
        state[anchor_impact.METADATA_KEY] = _impact.advertisement(context)

    sent = _send_raw({
        "type": "UPDATE_CLIENT_STATE",
        "clientId": _client_id,
        "state": state,
    })
    if sent and _client_id:
        with _player_states_lock:
            # Keep the local cache in the same normalized schema used for
            # received metadata. A raw update stores currentCharacter while
            # roster readers consume character, leaving the self icon blank or
            # stuck on the previous selection until a later membership packet.
            local = _player_states.setdefault(_client_id, {})
            local.update(state)
            for source, destination in (
                ("currentRoomId", "roomId"),
                ("currentRoom", "location"),
                ("currentCharacter", "character"),
            ):
                if source in state:
                    local[destination] = state[source]
                    local.pop(source, None)
            _cache_map_snapshot(local, state)
            local["self"] = True
    return sent


def set_enemy_room_state(room_id: int, signature: int, bits: str) -> bool:
    """Publish this client's compact dead-enemy bitmap for one raw game room."""
    global _local_enemy_room, _local_enemy_sig, _local_enemy_bits

    room = _bounded_int(room_id, 0, 0xFFFF, -1)
    sig = _bounded_int(signature, 0, 0xFFFFFFFF, -1)
    normalized_bits = _normalize_enemy_bits(bits)
    if room < 0 or sig < 0 or normalized_bits is None:
        logger.warning(
            "anchor_mnsg: rejected enemy state room=%r sig=%r bits=%r",
            room_id, signature, bits,
        )
        return False

    _local_enemy_room = room
    _local_enemy_sig = sig
    _local_enemy_bits = normalized_bits
    return update_client_state("{}")


def get_enemy_room_state(room_id: int, signature: int) -> str:
    """OR matching online teammates' dead-enemy bitmaps for a raw game room."""
    room = _bounded_int(room_id, 0, 0xFFFF, -1)
    sig = _bounded_int(signature, 0, 0xFFFFFFFF, -1)
    if room < 0 or sig < 0:
        return ""

    combined = bytearray(ENEMY_BITMAP_HEX_MAX // 2)
    matched = False
    with _player_states_lock:
        for cid, state in _player_states.items():
            if cid == _client_id or bool(state.get("self", False)):
                continue
            if not bool(state.get("online", False)):
                continue
            if not bool(state.get("isSaveLoaded", False)):
                continue
            if state.get("teamId", "") != _team_id:
                continue
            if _bounded_int(state.get("roomId", -1), -1, 0xFFFF, -1) != room:
                continue
            if _bounded_int(state.get("er", -1), -1, 0xFFFF, -1) != room:
                continue
            if _bounded_int(state.get("es", -1), 0, 0xFFFFFFFF, -1) != sig:
                continue
            peer_bits = _normalize_enemy_bits(state.get("eb"))
            if peer_bits is None:
                continue
            for offset in range(0, len(peer_bits), 2):
                combined[offset // 2] |= int(peer_bits[offset:offset + 2], 16)
            matched = True

    if not matched:
        return ""
    last = len(combined) - 1
    while last >= 0 and combined[last] == 0:
        last -= 1
    return "".join(f"{value:02x}" for value in combined[:last + 1])


def get_enemy_authority() -> int:
    """Return the elected live-enemy authority client id for the local room.

    The authority is the lowest client id among same-team, save-loaded peers
    currently in the local raw room.  The local client counts, so a lone player
    is their own authority and still runs the shared simulation.
    """
    if _client_id <= 0 or not _connected:
        return 0
    with _player_states_lock:
        stored_room = _bounded_int(
            _player_states.get(_client_id, {}).get("roomId", -1),
            -1, 0xFFFF, -1,
        )
        room = stored_room if stored_room >= 0 else _local_room_id
        room = _bounded_int(room, -1, 0xFFFF, -1)
        if room < 0:
            return 0
        candidates = [
            cid
            for cid, state in _player_states.items()
            if bool(state.get("online", False))
            and bool(state.get("isSaveLoaded", False))
            and state.get("teamId", "") == _team_id
            and _bounded_int(state.get("roomId", -1), -1, 0xFFFF, -1) == room
        ]
    if _client_id not in candidates:
        candidates.append(_client_id)
    return min(candidates)


def _boss_context() -> dict:
    """Read under _player_states_lock; the helper never performs socket I/O."""
    return {"cid": _client_id, "session": _interaction_session, "team": _team_id,
            "connected": _connected, "loaded": _local_save_loaded,
            "room": _player_states.get(_client_id, {}).get("roomId", _local_room_id),
            "players": _player_states}


def _congo_context() -> dict:
    """Compatibility alias for tests and callers inspecting Congo state."""
    return _boss_context()


def _dharumanyo_context() -> dict:
    """Compatibility helper for callers inspecting Dharumanyo state."""
    return _boss_context()


def _tsurami_context() -> dict:
    """Compatibility helper for callers inspecting Tsurami state."""
    return _boss_context()


def _update_boss(transport, boss_module, ready: int, visit: int,
                 paused: int, state_json: str) -> str:
    supplied = None
    if (isinstance(state_json, str) and
            len(state_json.encode("utf-8")) <= boss_module.MAX_STATE_BYTES):
        try:
            supplied = json.loads(state_json) if state_json else None
        except (ValueError, RecursionError):
            pass
    with _player_states_lock:
        status, packets = transport.update(
            _boss_context(), ready, visit, paused, supplied, time.monotonic()
        )
        dirty = transport.advertisement_dirty
        transport.advertisement_dirty = False
    if dirty and _connected:
        if not update_client_state("{}"):
            with _player_states_lock:
                transport.advertisement_dirty = True
    for packet in packets:
        sent = _send_raw(packet)
        with _player_states_lock:
            transport.packet_send_result(packet, sent)
    return json.dumps(status, separators=(",", ":"), allow_nan=False)


def update_congo(ready: int, visit: int, paused: int,
                 state_json: str = "") -> str:
    """Publish previous-frame Congo state and return checkpoint/hit work.

    Hit arrays are [cid, session, playerEpoch, attackSequence, damage]. Native
    code must apply or reject every returned hit before its next valid state.
    Paused owners still renew their lease, while followers freeze the boss only.
    """
    return _update_boss(
        _congo, anchor_congo, ready, visit, paused, state_json
    )


def update_dharumanyo(ready: int, visit: int, paused: int,
                      state_json: str = "") -> str:
    """Publish previous-frame Dharumanyo state and return checkpoint/hit work."""
    return _update_boss(
        _dharumanyo, anchor_dharumanyo, ready, visit, paused, state_json
    )


def update_tsurami(ready: int, visit: int, paused: int,
                   state_json: str = "") -> str:
    """Publish previous-frame Tsurami state and return checkpoint/hit work."""
    return _update_boss(
        _tsurami, anchor_tsurami, ready, visit, paused, state_json
    )


def send_congo_hit(sequence: int, amount: int) -> bool:
    """Queue one physical attack; retries keep the same identity until acked."""
    with _player_states_lock:
        return _congo.send_hit(
            _boss_context(), sequence, amount, time.monotonic()
        )


def send_dharumanyo_hit(sequence: int) -> bool:
    """Queue one carrier-life hit; retries keep its identity until acked."""
    with _player_states_lock:
        return _dharumanyo.send_hit(
            _boss_context(), sequence, 1, time.monotonic()
        )


def send_tsurami_hit(sequence: int, amount: int, target: int = 0) -> bool:
    """Queue a boss hit (target zero) or a projectile reflection by stable ID."""
    with _player_states_lock:
        return _tsurami.send_hit(
            _boss_context(), sequence, amount, time.monotonic(), target
        )


def update_impact(ready: int, stage: int, encounter: int, visit: int,
                  paused: int, state_json: str = "") -> str:
    """Publish previous-frame Impact battle state and return hit work.

    The transport room tracks the live Impact stage so peers on a different
    giant-robot encounter never exchange checkpoints. The Impact battle has no
    save dependency, so it is reported as loaded even from the title-menu boss
    rush.
    """
    global _impact_encounter
    if not isinstance(visit, int) or visit <= 0:
        # Modes that never pass through the ordinary room loader report 0; the
        # coordinator requires a positive encounter visit.
        visit = 1
    # The boss rush keeps one stage while the selected boss changes, so reset
    # the election whenever the encounter selector advances to a new boss.
    if encounter and encounter != _impact_encounter:
        _impact.reset()
        _impact_encounter = encounter
    _impact.set_stage(stage)
    supplied = None
    if (isinstance(state_json, str) and
            len(state_json.encode("utf-8")) <= anchor_impact.MAX_STATE_BYTES):
        try:
            supplied = json.loads(state_json) if state_json else None
        except (ValueError, RecursionError):
            pass
    with _player_states_lock:
        context = _boss_context()
        context["loaded"] = True
        status, packets = _impact.update(
            context, ready, visit, paused, supplied, time.monotonic()
        )
        dirty = _impact.advertisement_dirty
        _impact.advertisement_dirty = False
    if dirty and _connected:
        if not update_client_state("{}"):
            with _player_states_lock:
                _impact.advertisement_dirty = True
    for packet in packets:
        sent = _send_raw(packet)
        with _player_states_lock:
            _impact.packet_send_result(packet, sent)
    global _impact_debug_str
    _impact_debug_str = json.dumps({
        "cid": context.get("cid", 0),
        "conn": int(bool(context.get("connected"))),
        "loaded": int(bool(context.get("loaded"))),
        "room": context.get("room", -1),
        "selfRoom": _impact.room,
        "visit": visit,
        "ready": int(bool(ready)),
        "role": status.get("role", -1),
        "e": status.get("encounter", []),
    }, separators=(",", ":"))
    return json.dumps(status, separators=(",", ":"), allow_nan=False)


def send_impact_hit(sequence: int, amount: int) -> bool:
    """Queue one physical Impact hit; retries keep its identity until acked."""
    with _player_states_lock:
        return _impact.send_hit(
            _boss_context(), sequence, amount, time.monotonic()
        )


def impact_debug() -> str:
    """Last Impact readiness snapshot, for bridging into the native log.

    Returned as a ready-to-print line (no printf ``%`` escapes) because the
    native side forwards it verbatim.
    """
    return "[Impact] dbg " + (_impact_debug_str or "{}") + "\n"


def _reset_boss_invitations() -> None:
    """Called with the roster lock during connection teardown/setup."""
    global _arena_local_state, _arena_sequence
    _arena_local_state = ("", 0, 0, 0, 0, 0)
    _arena_sequence = 0
    _arena_events.clear()
    _arena_seen.clear()
    _arena_retired_sessions.clear()
    _arena_confirmed_sessions.clear()


def _invalidate_confirmed_boss_invitation(cid: int) -> None:
    """Called under the roster lock after metadata is merged/replaced."""
    event = _arena_events.get(cid)
    if (event and event["confirmed"] and
            not _boss_invitation_is_current(event, int(time.monotonic() * 1000))):
        _arena_events.pop(cid, None)


def set_boss_arena(arena: int, visit: int = 0, stage: int = 0,
                   field90: int = 0, field91: int = 0) -> bool:
    """Publish entry/exit edges supplied by the native loaded-room observer.

    Metadata refreshes and movement packets cannot generate an edge. Each
    entry supersedes that sender's previous arena; exits name the last arena
    published successfully. Failed sends retain the old state, so the next
    frame retries the same sequence number. For the Impact arena ``stage`` is
    the first native stage (0x21C..0x224) of the sequence and ``field90`` /
    ``field91`` are the native load-from-start fields used to resume at the
    cutscene start.
    """
    global _arena_local_state, _arena_sequence
    if (type(arena) is not int or (arena != 0 and arena not in BOSS_ARENA_ROOMS) or
            type(visit) is not int or not 0 <= visit <= _POSITION_SEQUENCE_MASK or
            type(stage) is not int or not 0 <= stage <= 0xFFFF or
            type(field90) is not int or not 0 <= field90 <= 0xFFFF or
            type(field91) is not int or not 0 <= field91 <= _POSITION_SEQUENCE_MASK or
            not _connected or _client_id <= 0 or _interaction_session <= 0):
        return False
    if arena and not _local_save_loaded:
        return False
    if impact_arena(arena):
        # Default to the boss stage through a missing stage, so older callers
        # still work; the native side always supplies the exact stage.
        if stage == 0:
            stage = BOSS_ARENA_ROOMS[arena]
        elif not impact_stage_valid(stage):
            return False
    else:
        stage = 0
        field90 = 0
        field91 = 0
    next_state = (_team_id, arena, visit if arena else 0, stage, field90, field91)
    if next_state == _arena_local_state:
        return True
    if arena == 0 and _arena_local_state[1] == 0:
        _arena_local_state = next_state
        return True
    sequence = _arena_sequence + 1
    if sequence > _POSITION_SEQUENCE_MASK:
        return False
    effective_arena = arena or _arena_local_state[1]
    packet = {"type": "MNSG_BOSS_ARENA", "clientId": _client_id,
              "targetTeamId": _team_id, "arena": effective_arena,
              "entered": arena != 0, "session": _interaction_session,
              "seq": sequence}
    if impact_arena(effective_arena):
        packet["stage"] = stage if arena else _arena_local_state[3]
        packet["f90"] = field90 if arena else _arena_local_state[4]
        packet["f91"] = field91 if arena else _arena_local_state[5]
    if not _send_raw(packet):
        return False
    _arena_sequence = sequence
    _arena_local_state = next_state
    return True


def _receive_boss_arena(packet: dict) -> bool:
    """Receive a transient team event; membership and room are checked again
    when it is presented and accepted, including events racing metadata.
    """
    if (not isinstance(packet, dict) or
            packet.get("type") != "MNSG_BOSS_ARENA" or
            not _connected or _client_id <= 0 or not _local_save_loaded or
            packet.get("targetTeamId") != _team_id or
            type(packet.get("arena")) is not int or
            packet["arena"] not in BOSS_ARENA_ROOMS or
            type(packet.get("entered")) is not bool):
        return False
    for key in ("clientId", "session", "seq"):
        if type(packet.get(key)) is not int or not 0 < packet[key] <= _POSITION_SEQUENCE_MASK:
            return False
    cid, session, sequence = (packet[key] for key in ("clientId", "session", "seq"))
    stage = packet.get("stage", 0)
    field90 = packet.get("f90", 0)
    field91 = packet.get("f91", 0)
    if impact_arena(packet["arena"]):
        if "stage" not in packet:
            stage = BOSS_ARENA_ROOMS[packet["arena"]]
        elif not impact_stage_valid(stage):
            return False
        if (type(field90) is not int or not 0 <= field90 <= 0xFFFF or
                type(field91) is not int or
                not 0 <= field91 <= _POSITION_SEQUENCE_MASK):
            return False
    else:
        stage = 0
        field90 = 0
        field91 = 0
    if cid == _client_id:
        return False
    with _player_states_lock:
        peer = _player_states.get(cid)
        # Movement can create a row before its handshake metadata arrives.
        # An absent team waits for confirmation; a known other team cannot
        # inject an invitation simply by naming our targetTeamId.
        if peer and (not peer.get("online", False) or
                     (peer.get("teamId") and peer["teamId"] != _team_id)):
            return False
        if (session in _retired_interaction_sessions.get(cid, ()) or
                session in _arena_retired_sessions.get(cid, ())):
            return False
        key = (cid, session)
        if sequence <= _arena_seen.get(key, 0):
            return False
        event = _arena_events.get(cid)
        if event and event["session"] != session and key in _arena_seen:
            # A previously observed stream cannot displace its replacement
            # while the latter awaits metadata. If that candidate expires,
            # a fresh event from the still-current source can be accepted.
            if (event["confirmed"] or int(time.monotonic() * 1000) -
                    event["received"] <= ARENA_METADATA_WAIT_MS):
                return False
            _arena_events.pop(cid, None)
        _arena_seen[key] = sequence
        if not packet["entered"]:
            event = _arena_events.get(cid)
            if event and event["session"] == session:
                _arena_events.pop(cid, None)
            return True
        _arena_events[cid] = {
            "cid": cid, "session": session, "seq": sequence,
            "arena": packet["arena"], "stage": stage,
            "field90": field90, "field91": field91, "team": _team_id,
            "entered": packet["entered"], "consumed": False,
            "confirmed": False, "received": int(time.monotonic() * 1000),
        }
    return True


def _boss_invitation_is_current(event: dict, now: int) -> bool:
    """Under the roster lock. Pending metadata has a short grace period;
    confirmed invitations live until the sender exits, disconnects or changes
    teams, so a long local conversation does not lose a legitimate invitation.
    """
    if (not _connected or not _local_save_loaded or event["team"] != _team_id or
            not event["entered"] or event["consumed"] or
            _arena_local_state[:2] == (_team_id, event["arena"])):
        event["consumed"] = True
        return False
    cid, session = event["cid"], event["session"]
    peer = _player_states.get(cid, {})
    known_session = peer.get("interactionSession") or _player_movement_order.get(cid, {}).get("interactionSession", 0)
    expected_room = (event["stage"] if impact_arena(event["arena"])
                     else BOSS_ARENA_ROOMS[event["arena"]])
    eligible = (peer.get("online", False) and peer.get("isSaveLoaded", False) and
                peer.get("teamId") == _team_id and
                peer.get("roomId") == expected_room and
                known_session == session and
                session not in _retired_interaction_sessions.get(cid, ()))
    if eligible:
        previous_session = _arena_confirmed_sessions.get(cid, 0)
        if previous_session and previous_session != session:
            # Retire only a metadata-confirmed predecessor. A stray candidate
            # whose metadata never arrives cannot poison the valid stream.
            _arena_retired_sessions.setdefault(cid, set()).add(previous_session)
        _arena_confirmed_sessions[cid] = session
        event["confirmed"] = True
        return True
    if event["confirmed"] or now - event["received"] > ARENA_METADATA_WAIT_MS:
        event["consumed"] = True
    return False


def get_boss_invitation_json() -> str:
    """Peek one invitation. It remains available until the dialog responds;
    an unrelated native dialog can defer presentation without consuming it.
    """
    now = int(time.monotonic() * 1000)
    with _player_states_lock:
        for event in sorted(_arena_events.values(), key=lambda e: e["received"]):
            if _boss_invitation_is_current(event, now):
                name = _player_states[event["cid"]].get("name") or f'Player{event["cid"]}'
                payload = {"cid": event["cid"], "session": event["session"],
                           "seq": event["seq"], "arena": event["arena"],
                           "name": str(name)[:64]}
                if impact_arena(event["arena"]):
                    payload["stage"] = event["stage"]
                    payload["f90"] = event["field90"]
                    payload["f91"] = event["field91"]
                return json.dumps(payload, separators=(",", ":"), ensure_ascii=False)
            if event["consumed"]:
                _arena_events.pop(event["cid"], None)
    return ""


def boss_invitation_is_current(cid: int, session: int, sequence: int) -> bool:
    with _player_states_lock:
        event = _arena_events.get(cid)
        if not event or event["session"] != session or event["seq"] != sequence:
            return False
        current = _boss_invitation_is_current(event, int(time.monotonic() * 1000))
        if event["consumed"]:
            _arena_events.pop(cid, None)
        return current


def dismiss_boss_invitation(cid: int, session: int, sequence: int) -> None:
    with _player_states_lock:
        event = _arena_events.get(cid)
        if event and event["session"] == session and event["seq"] == sequence:
            _arena_events.pop(cid, None)


def set_save_loaded(is_loaded: bool) -> bool:
    """
    Convenience wrapper: set isSaveLoaded flag, making yourself eligible
    to share your save state with teammates who join.
    """
    global _local_save_loaded
    _local_save_loaded = bool(is_loaded)
    if not _local_save_loaded:
        set_boss_arena(0)
        with _player_states_lock:
            _arena_events.clear()
            if _congo.local[0] or _congo.e:
                _congo.update(
                    _boss_context(), False, 0, False, None, time.monotonic()
                )
            if _dharumanyo.local[0] or _dharumanyo.e:
                _dharumanyo.update(
                    _boss_context(), False, 0, False, None, time.monotonic()
                )
            if _tsurami.local[0] or _tsurami.e:
                _tsurami.update(
                    _boss_context(), False, 0, False, None, time.monotonic()
                )
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
        add_to_queue:     If True with a team target, queue for offline teammates.
                          Invalid for room or direct routes.
    """
    try:
        payload: dict = json.loads(payload_json) if payload_json else {}
    except json.JSONDecodeError:
        payload = {}
    if not isinstance(payload, dict):
        return False
    if add_to_queue and (not target_team_id or target_client_id):
        logger.warning(
            "anchor_mnsg: addToQueue requires a team target and no direct target"
        )
        return False

    # Payloads are feature data, never routing envelopes. Anchor trusts the
    # root clientId and routes by target fields, so write all reserved fields
    # after the merge to prevent accidental spoofing or scope changes.
    packet: dict = dict(payload)
    for reserved in ("type", "clientId", "targetClientId", "targetTeamId", "addToQueue"):
        packet.pop(reserved, None)
    packet["type"] = packet_type or "CUSTOM"
    packet["clientId"] = _client_id

    if target_client_id:
        packet["targetClientId"] = int(target_client_id)
    elif target_team_id:
        packet["targetTeamId"] = target_team_id
    if add_to_queue:
        packet["addToQueue"] = True

    return _send_raw(packet)


def send_packet(packet_json: str) -> bool:
    """Send a validated raw Anchor envelope for uncommon packet types.

    The connection owns the root clientId. Queueing is accepted only on the
    generic team route because Anchor ignores it on room and direct routes.
    """
    try:
        packet = json.loads(packet_json) if packet_json else {}
    except (json.JSONDecodeError, TypeError):
        return False
    if not isinstance(packet, dict):
        return False
    packet_type = packet.get("type")
    if not isinstance(packet_type, str) or not packet_type or packet_type in {"HANDSHAKE", "HEARTBEAT"}:
        return False
    if packet_type != "STATS":
        packet["clientId"] = _client_id
    target_client = packet.get("targetClientId")
    if target_client is not None and (type(target_client) is not int or target_client <= 0):
        return False
    target_team = packet.get("targetTeamId")
    if target_team is not None and (not isinstance(target_team, str) or not target_team):
        return False
    queued = packet.get("addToQueue", False)
    if type(queued) is not bool:
        return False
    if queued and (target_team is None or target_client is not None):
        return False
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
    velocity_x: "int | None" = None,
    velocity_y: "int | None" = None,
    velocity_z: "int | None" = None,
    angular_velocity_x: "int | None" = None,
    angular_velocity_y: "int | None" = None,
    angular_velocity_z: "int | None" = None,
    force_motion_edge: int = 0,
    animation_step_100: int = 0,
    has_animation_step: int = 0,
    collision_disabled: int = 0,
    drive_x: int = 0,
    drive_z: int = 0,
    player_epoch: int = 0,
) -> bool:
    """
    Broadcast world-space position and the live animation phase to teammates.

    This is the hot movement payload, so it intentionally avoids repeating
    identity/team fields that are sent by handshake, room, and character
    updates. The game-side caller supplies the final displacement of the
    current frame so clients can preserve acceleration, gravity, and stops
    while reconstructing only the frames between packets. Older callers may
    omit it and retain the packet-interval estimate.

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
        appearance_flags: Bitmap containing Sudden Impact (bit 0), Mini
            Ebisumaru (bit 1), and native hurt recovery (bit 2).
        velocity_x: Optional final-frame X velocity in world units per second.
        velocity_y: Optional final-frame Y velocity in world units per second.
        velocity_z: Optional final-frame Z velocity in world units per second.
        angular_velocity_x: Optional final-frame X rotation velocity.
        angular_velocity_y: Optional final-frame Y rotation velocity.
        angular_velocity_z: Optional final-frame Z rotation velocity.
        force_motion_edge: Nonzero when the game observed a start, stop, or
            strong reversal that must bypass the normal send interval.
        animation_step_100: Native animation advance for the latest 30 Hz game
            tick, in hundredths of a clip frame.
        has_animation_step: Nonzero when animation_step_100 is valid. A valid
            zero represents a paused native clip.
        collision_disabled: Nonzero during native cutscene/script movement.
            Both edges bypass the normal send interval.
        drive_x, drive_z: Intended horizontal drive, hundredths of world
            units per second. Each axis is bounded to +/-30000.
        player_epoch: Positive local-player lifecycle counter.

    Returns True if the packet was sent.
    """
    global _last_position_sent, _last_position_sent_ms, _last_position_room_id
    global _position_seq
    global _last_position_action, _last_position_frame_100
    global _last_position_appearance_flags, _last_position_collision_disabled
    global _last_position_drive, _last_position_epoch, _local_map_snapshot
    global _local_map_snapshot_explicit

    if not _connected:
        return False
    now_ms = int(time.monotonic() * 1000)
    action_changed = int(action) != _last_position_action
    room_changed = _last_position_room_id != _local_room_id
    appearance_flags = int(appearance_flags) & APPEARANCE_MASK
    appearance_changed = appearance_flags != _last_position_appearance_flags
    collision_disabled = 1 if collision_disabled else 0
    collision_changed = collision_disabled != _last_position_collision_disabled
    drive_x = max(-30000, min(30000, int(drive_x)))
    drive_z = max(-30000, min(30000, int(drive_z)))
    player_epoch = int(player_epoch)
    if not 0 < player_epoch <= _POSITION_SEQUENCE_MASK:
        player_epoch = 0
    drive_stopped = (
        (_last_position_drive[0] != 0 and drive_x == 0) or
        (_last_position_drive[1] != 0 and drive_z == 0)
    )
    epoch_changed = player_epoch != _last_position_epoch
    frame_restarted = (
        not action_changed
        and int(frame_100) + ANIMATION_RESTART_DELTA_100 < _last_position_frame_100
    )
    if (
        _last_position_sent_ms > 0
        and now_ms - _last_position_sent_ms < MOVEMENT_MIN_INTERVAL_MS
        and not room_changed
        and not action_changed
        and not appearance_changed
        and not collision_changed
        and not drive_stopped
        and not epoch_changed
        and not frame_restarted
        and not bool(force_motion_edge)
    ):
        return False

    has_endpoint_velocity = (
        velocity_x is not None
        and velocity_y is not None
        and velocity_z is not None
    )
    if has_endpoint_velocity:
        vel_x = int(velocity_x)
        vel_y = int(velocity_y)
        vel_z = int(velocity_z)
    else:
        vel_x = 0
        vel_y = 0
        vel_z = 0
    if (
        not has_endpoint_velocity
        and _last_position_sent is not None
        and _last_position_sent_ms > 0
        and _last_position_room_id == _local_room_id
    ):
        dt_ms = max(1, now_ms - _last_position_sent_ms)
        vel_x = int((x - _last_position_sent[0]) * 1000 / dt_ms)
        vel_y = int((y - _last_position_sent[1]) * 1000 / dt_ms)
        vel_z = int((z - _last_position_sent[2]) * 1000 / dt_ms)
    has_endpoint_angular_velocity = (
        angular_velocity_x is not None
        and angular_velocity_y is not None
        and angular_velocity_z is not None
    )
    if has_endpoint_angular_velocity:
        rot_vel_x = int(angular_velocity_x)
        rot_vel_y = int(angular_velocity_y)
        rot_vel_z = int(angular_velocity_z)
    else:
        rot_vel_x = 0
        rot_vel_y = 0
        rot_vel_z = 0
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
        "animStep100": int(animation_step_100),
        "hasAnimStep": 1 if has_animation_step else 0,
        "rotX": int(rot_x),
        "rotY": int(rot_y),
        "rotZ": int(rot_z),
        "rotVelX": rot_vel_x,
        "rotVelY": rot_vel_y,
        "rotVelZ": rot_vel_z,
        "appearanceFlags": appearance_flags,
        "collisionDisabled": collision_disabled,
        "driveX": drive_x,
        "driveZ": drive_z,
        "playerEpoch": player_epoch,
        "interactionSession": _interaction_session,
        "quiet": True,
    })
    if not sent:
        return False

    _last_position_sent = (x, y, z)
    _last_position_sent_ms = now_ms
    _last_position_room_id = _local_room_id
    _last_position_action = int(action)
    _last_position_frame_100 = int(frame_100)
    _last_position_appearance_flags = appearance_flags
    _last_position_collision_disabled = collision_disabled
    _last_position_drive = (drive_x, drive_z)
    _last_position_epoch = player_epoch
    _position_seq = next_seq
    if (_valid_map_room_id(_local_room_id) and
            not _local_map_snapshot_explicit):
        map_x = _scale_map_coordinate(x)
        map_y = _scale_map_coordinate(y)
        map_z = _scale_map_coordinate(z)
        if map_x is not None and map_y is not None and map_z is not None:
            _local_map_snapshot = (_local_room_id, map_x, map_y, map_z)

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
            local["animStep100"] = int(animation_step_100)
            local["hasAnimStep"] = 1 if has_animation_step else 0
            local["rotX"] = int(rot_x)
            local["rotY"] = int(rot_y)
            local["rotZ"] = int(rot_z)
            local["rotVelX"] = rot_vel_x
            local["rotVelY"] = rot_vel_y
            local["rotVelZ"] = rot_vel_z
            local["appearanceFlags"] = appearance_flags
            local["collisionDisabled"] = collision_disabled
            local["driveX"] = drive_x
            local["driveZ"] = drive_z
            local["playerEpoch"] = player_epoch
            local["interactionSession"] = _interaction_session
            local["roomId"] = _local_room_id
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
    sent = update_client_state(json.dumps({"currentCharacter": char_name}))
    if sent:
        _local_character = char_name
    return sent


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
    global _local_room_id, _local_map_snapshot, _local_map_snapshot_explicit
    if not _connected:
        return False
    if room_id == _local_room_id:
        return False
    previous_room = _local_room_id
    if (room_id == WORLD_MAP_ROOM_ID and
            _valid_map_room_id(previous_room) and
            not _local_map_snapshot_explicit):
        # Usually the successful hot-position sender already maintains this
        # fallback. Rebuild it here when necessary for older/native callers,
        # without replacing the more precise explicit world-map setter value.
        if (_local_map_snapshot is None or
                _local_map_snapshot[0] != previous_room):
            if (_last_position_room_id == previous_room and
                    _last_position_sent is not None):
                map_x = _scale_map_coordinate(_last_position_sent[0])
                map_y = _scale_map_coordinate(_last_position_sent[1])
                map_z = _scale_map_coordinate(_last_position_sent[2])
                if map_x is not None and map_y is not None and map_z is not None:
                    _local_map_snapshot = (previous_room, map_x, map_y, map_z)
                else:
                    _local_map_snapshot = None
            else:
                _local_map_snapshot = None
    elif previous_room == WORLD_MAP_ROOM_ID and _valid_map_room_id(room_id):
        # A stock-map tuple is authoritative only for that map visit. Resume
        # tracking successful hot gameplay positions after leaving the map.
        _local_map_snapshot_explicit = False
    _local_room_id = room_id
    area_name = _ROOM_NAMES.get(room_id, "")
    # Update our own local entry immediately – the server won't echo us back.
    with _player_states_lock:
        # ALL_CLIENT_STATE temporarily resets the broadcast baseline to -1;
        # a same-room metadata refresh must preserve active remote visuals.
        if _player_states.get(_client_id, {}).get("roomId") != room_id:
            _projectile_spawns.clear()
            _reset_player_sounds()
        if _client_id in _player_states:
            _player_states[_client_id]["location"] = area_name
            _player_states[_client_id]["roomId"] = room_id
        if room_id != anchor_congo.ROOM and (_congo.local[0] or _congo.e):
            _congo.update(
                _boss_context(), False, 0, False, None, time.monotonic()
            )
        if (room_id != anchor_dharumanyo.ROOM and
                (_dharumanyo.local[0] or _dharumanyo.e)):
            _dharumanyo.update(
                _boss_context(), False, 0, False, None, time.monotonic()
            )
        if (room_id != anchor_tsurami.ROOM and
                (_tsurami.local[0] or _tsurami.e)):
            _tsurami.update(
                _boss_context(), False, 0, False, None, time.monotonic()
            )
    return update_client_state(json.dumps({"currentRoom": area_name, "currentRoomId": room_id}))


def set_world_map_location(room_id: int, x: float, z: float) -> bool:
    """Publish the stock World Map marker source as durable client metadata.

    ``x`` and ``z`` are the native floating-point world coordinates passed to
    the stock marker routine. They are encoded as signed fixed-point hundredths
    in ``mnsgMapX``/``mnsgMapZ``. ``mnsgMapY`` retains a matching gameplay Y
    sample when available and is otherwise zero; Japan's marker uses X/Z.
    """
    global _local_map_snapshot, _local_map_snapshot_explicit

    if not _connected or not _valid_map_room_id(room_id):
        return False
    map_x = _scale_map_coordinate(x)
    map_z = _scale_map_coordinate(z)
    if map_x is None or map_z is None:
        return False

    map_y = 0
    if _local_map_snapshot is not None and _local_map_snapshot[0] == room_id:
        map_y = _local_map_snapshot[2]
    elif _last_position_room_id == room_id and _last_position_sent is not None:
        scaled_y = _scale_map_coordinate(_last_position_sent[1])
        if scaled_y is not None:
            map_y = scaled_y
    _local_map_snapshot = (room_id, map_x, map_y, map_z)
    _local_map_snapshot_explicit = True
    return update_client_state("{}")


def send_game_complete() -> bool:
    """Signal to the server that the game has been completed."""
    return _send_raw({"type": "GAME_COMPLETE", "clientId": _client_id})


def get_stats() -> bool:
    """
    Request server statistics (online count, game complete count, etc.).
    The response STATS packet will arrive in the poll queue.
    """
    return _send_raw({"type": "STATS"})


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
        ``cid`` – stable client ID for this connection.
        ``self`` – 1 for the local player, otherwise 0.
        ``ct`` – 1 when this row currently has a safe transfer target.
        ``n``  – display string: "Name - Location"
        ``c``  – character index (int): 0=Goemon, 1=Ebisumaru, 2=Sasuke, 3=Yae.
                 -1 if the character has not been broadcast yet.
        ``r``  – raw room ID, or -1 if unknown.
        ``hp`` – 1 if the player has sent position data, else 0.
        ``x``/``y``/``z`` – last broadcast world position, 0 when hp is 0.

    Returns ``'[]'`` when not connected or no online players are present.
    """
    now_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        entries = []
        for cid, v in sorted(_player_states.items()):
            if not v.get("online", True):
                continue
            name_str = v["name"]
            loc = v.get("location", "")
            if loc:
                name_str += " - " + loc
            char_idx = _CHAR_TO_IDX.get(v.get("character", ""), -1)
            room_id  = v.get("roomId", -1)
            has_pos = "posX" in v and "posY" in v and "posZ" in v
            is_self = cid == _client_id or bool(v.get("self", False))
            entries.append({
                "cid": int(cid),
                "self": 1 if is_self else 0,
                "ct": 1 if (_connected and _client_id > 0 and
                              _transfer_target_locked(cid, now_ms) is not None) else 0,
                "n": name_str,
                "c": char_idx,
                "r": room_id,
                "hp": 1 if has_pos else 0,
                "x": int(v.get("posX", 0)) if has_pos else 0,
                "y": int(v.get("posY", 0)) if has_pos else 0,
                "z": int(v.get("posZ", 0)) if has_pos else 0,
            })
    return json.dumps(entries, separators=(",", ":"))


def _transfer_target_locked(cid: int, now_ms: int) -> "dict | None":
    """Resolve a safe, current remote destination while the state lock is held."""
    if type(cid) is not int or cid <= 0 or cid == _client_id:
        return None
    state = _player_states.get(cid)
    if not state or not state.get("online", True):
        return None
    if state.get("self", False) or not state.get("isSaveLoaded", False):
        return None
    if int(state.get("collisionDisabled", 0)) != 0:
        return None
    if int(state.get("interactionSession", 0)) <= 0:
        return None
    if int(state.get("playerEpoch", 0)) <= 0:
        return None

    received_ms = int(state.get("_positionReceivedMs", 0))
    age_ms = now_ms - received_ms
    if received_ms <= 0 or age_ms < 0 or age_ms > TRANSFER_TARGET_MAX_AGE_MS:
        return None

    room = state.get("roomId")
    if type(room) is not int or not 0 <= room <= TRANSFER_ROOM_MAX:
        return None
    coordinates = (state.get("posX"), state.get("posY"), state.get("posZ"))
    if any(type(value) is not int for value in coordinates):
        return None
    if any(not TRANSFER_COORD_MIN <= value <= TRANSFER_COORD_MAX
           for value in coordinates):
        return None

    return {
        "cid": cid,
        "room": room,
        "x": coordinates[0],
        "y": coordinates[1],
        "z": coordinates[2],
    }


def get_transfer_target_json(cid: int) -> str:
    """Return one freshly validated remote transfer target, or ``'{}'``."""
    if not _connected or _client_id <= 0:
        return "{}"
    now_ms = int(time.monotonic() * 1000)
    with _player_states_lock:
        target = _transfer_target_locked(cid, now_ms)
    return json.dumps(target, separators=(",", ":")) if target else "{}"


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
    character, appearance, and collision fields used by the remote cutscene-model
    renderer.

    Fields:
      "cid"  – client ID (unique per player, stable within a session).
      "room" – raw 16-bit room ID the player last reported, -1 if unknown.
      "x","y","z" – last broadcast world-space position (0 if not yet received).
      "hp"   – 1 if the player has sent at least one position update, 0 otherwise.
      "t"    – sender monotonic milliseconds, masked to a positive 31-bit value.
      "ap"   – Sudden Impact bit 0, Mini Ebisumaru bit 1, hurt recovery bit 2.
      "cd"   – 1 while the sender requires cutscene/script collision bypass.
      "mr"   – gameplay room used by the Japan-map marker, -1 if unavailable.
      "mx","my","mz" – signed fixed-point map position in hundredths.
      "mhp"  – 1 when mr/mx/my/mz form a complete usable map snapshot.

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
            has_complete_pos = all(
                field in v for field in ("posX", "posY", "posZ")
            )
            px = int(v.get("posX", 0)) if has_pos else 0
            py = int(v.get("posY", 0)) if has_pos else 0
            pz = int(v.get("posZ", 0)) if has_pos else 0
            map_snapshot = None
            if room_id == WORLD_MAP_ROOM_ID:
                map_snapshot = _map_snapshot_from_payload(v)
            elif _valid_map_room_id(room_id) and has_complete_pos:
                map_x = _scale_map_coordinate(px)
                map_y = _scale_map_coordinate(py)
                map_z = _scale_map_coordinate(pz)
                if map_x is not None and map_y is not None and map_z is not None:
                    map_snapshot = (room_id, map_x, map_y, map_z)
            if map_snapshot is None:
                map_room, map_x, map_y, map_z, has_map_pos = -1, 0, 0, 0, 0
            else:
                map_room, map_x, map_y, map_z = map_snapshot
                has_map_pos = 1
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
                "mr": map_room,
                "mx": map_x,
                "my": map_y,
                "mz": map_z,
                "mhp": has_map_pos,
                "ch": ch,
                "vx": int(v.get("velX", 0)),
                "vy": int(v.get("velY", 0)),
                "vz": int(v.get("velZ", 0)),
                "s": int(v.get("posSeq", 0)),
                "t": int(v.get("posT", 0)) & 0x7fffffff,
                "a": int(v.get("action", -1)),
                "af": int(v.get("animFrame100", 0)),
                "al": int(v.get("animFrameCount100", 0)),
                "as": int(v.get("animStep100", 0)),
                "ah": int(v.get("hasAnimStep", 0)),
                "rx": int(v.get("rotX", 0)),
                "ry": int(v.get("rotY", 0)),
                "rz": int(v.get("rotZ", 0)),
                "rvx": int(v.get("rotVelX", 0)),
                "rvy": int(v.get("rotVelY", 0)),
                "rvz": int(v.get("rotVelZ", 0)),
                "ap": int(v.get("appearanceFlags", 0)) & APPEARANCE_MASK,
                "cd": 1 if v.get("collisionDisabled", 0) else 0,
                "dx": int(v.get("driveX", 0)),
                "dz": int(v.get("driveZ", 0)),
                "pe": int(v.get("playerEpoch", 0)),
                "ps": int(v.get("interactionSession", 0)),
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
    Return compact online lobby membership in client-ID order.

    Each entry: {"cid": int, "n": str, "s": str, "self": 0|1}
    """
    if not _connected:
        return "[]"
    with _player_states_lock:
        result = []
        for cid, v in sorted(_player_states.items()):
            if not v.get("online", True):
                continue
            result.append({
                "cid": int(cid),
                "n": v.get("name", f"Player{cid}"),
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
