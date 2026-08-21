#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""
Spawn many synthetic MNSG Anchor clients for multiplayer stress testing.

Run with uv:
    uv run tools/anchor_stress.py --clients 25 --room stress-test

Once running, type `help` for interactive commands.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
import json
import math
import signal
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "py"))

try:
    import anchor_mnsg  # type: ignore

    DEFAULT_HOST = anchor_mnsg.DEFAULT_HOST
    DEFAULT_PORT = anchor_mnsg.DEFAULT_PORT
    ROOM_ID_PREFIX = anchor_mnsg.ROOM_ID_PREFIX
    ROOM_ID_TRIM_CHARS = anchor_mnsg.ROOM_ID_TRIM_CHARS
    ROOM_NAMES = dict(getattr(anchor_mnsg, "_ROOM_NAMES", {}))
except Exception:
    DEFAULT_HOST = "anchor.hm64.org"
    DEFAULT_PORT = 43383
    ROOM_ID_PREFIX = "mnsg-"
    ROOM_ID_TRIM_CHARS = " \t\n\r\v\f"
    ROOM_NAMES: dict[int, str] = {}


CHARACTERS = ("Goemon", "Ebisumaru", "Sasuke", "Yae")


@dataclass
class RemoteState:
    client_id: int
    name: str = ""
    team_id: str = ""
    online: bool = True
    room_id: int = -1
    room_name: str = ""
    character: str = ""
    x: int | None = None
    y: int | None = None
    z: int | None = None
    vel_x: int = 0
    vel_y: int = 0
    vel_z: int = 0
    pos_seq: int = 0
    updated_at: float = field(default_factory=time.monotonic)


class WorldState:
    def __init__(self) -> None:
        self._lock = asyncio.Lock()
        self.players: dict[int, RemoteState] = {}

    async def update_from_state(self, packet_state: dict[str, Any], *, self_id: int | None = None) -> None:
        cid = int(packet_state.get("clientId") or 0)
        if cid <= 0:
            return
        async with self._lock:
            player = self.players.setdefault(cid, RemoteState(client_id=cid))
            player.updated_at = time.monotonic()
            if self_id is not None:
                player.client_id = self_id
            if "name" in packet_state:
                player.name = str(packet_state["name"])
            if "teamId" in packet_state:
                player.team_id = str(packet_state["teamId"])
            if "online" in packet_state:
                player.online = bool(packet_state["online"])
            if "currentRoom" in packet_state:
                player.room_name = str(packet_state["currentRoom"])
            if "currentRoomId" in packet_state:
                player.room_id = int(packet_state["currentRoomId"])
            if "currentCharacter" in packet_state:
                player.character = str(packet_state["currentCharacter"])
            self._update_position_fields(player, packet_state)

    async def update_from_all_client_state(self, packet: dict[str, Any]) -> int:
        assigned_self_id = 0
        states = packet.get("state", [])
        if not isinstance(states, list):
            return assigned_self_id
        async with self._lock:
            for entry in states:
                if not isinstance(entry, dict):
                    continue
                cid = int(entry.get("clientId") or 0)
                if cid <= 0:
                    continue
                if entry.get("self"):
                    assigned_self_id = cid
                client_state = entry.get("clientState")
                if not isinstance(client_state, dict):
                    client_state = entry
                player = self.players.setdefault(cid, RemoteState(client_id=cid))
                player.updated_at = time.monotonic()
                player.name = str(client_state.get("name") or entry.get("name") or player.name or f"Player{cid}")
                player.team_id = str(client_state.get("teamId", player.team_id))
                player.online = bool(client_state.get("online", player.online))
                player.room_name = str(client_state.get("currentRoom", player.room_name))
                player.room_id = int(client_state.get("currentRoomId", player.room_id))
                player.character = str(client_state.get("currentCharacter", player.character))
                self._update_position_fields(player, client_state)
        return assigned_self_id

    async def update_from_position_packet(self, packet: dict[str, Any]) -> None:
        cid = int(packet.get("clientId") or 0)
        if cid <= 0:
            return
        async with self._lock:
            player = self.players.setdefault(cid, RemoteState(client_id=cid, name=f"Player{cid}"))
            player.updated_at = time.monotonic()
            if "currentRoomId" in packet:
                player.room_id = int(packet["currentRoomId"])
            self._update_position_fields(player, packet)

    async def snapshot(self) -> list[RemoteState]:
        async with self._lock:
            return [RemoteState(**vars(p)) for p in self.players.values()]

    async def find_target(self, selector: str) -> RemoteState | None:
        selector = selector.strip()
        if not selector:
            return None
        players = await self.snapshot()
        if selector.isdigit():
            wanted = int(selector)
            return next((p for p in players if p.client_id == wanted), None)
        folded = selector.lower()
        exact = [p for p in players if p.name.lower() == folded]
        if exact:
            return exact[0]
        partial = [p for p in players if folded in p.name.lower()]
        return partial[0] if partial else None

    @staticmethod
    def _update_position_fields(player: RemoteState, data: dict[str, Any]) -> None:
        if "posX" in data:
            player.x = int(data["posX"])
        if "posY" in data:
            player.y = int(data["posY"])
        if "posZ" in data:
            player.z = int(data["posZ"])
        if "velX" in data:
            player.vel_x = int(data["velX"])
        if "velY" in data:
            player.vel_y = int(data["velY"])
        if "velZ" in data:
            player.vel_z = int(data["velZ"])
        if "posSeq" in data:
            player.pos_seq = int(data["posSeq"])


@dataclass
class BotConfig:
    host: str
    port: int
    room_id: str
    team_id: str
    name_prefix: str
    start_x: int
    start_y: int
    start_z: int
    start_room: int
    character: str
    rate_hz: float


class AnchorBot:
    def __init__(self, index: int, config: BotConfig, world: WorldState) -> None:
        self.index = index
        self.config = config
        self.world = world
        self.name = f"{config.name_prefix}{index:03d}"
        self.client_id = 0
        self.connected = False
        self.x = config.start_x
        self.y = config.start_y
        self.z = config.start_z
        self.room_id = config.start_room
        self.character = config.character
        self._last_pos: tuple[int, int, int] | None = None
        self._last_pos_ms = 0
        self._pos_seq = 0
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._recv_task: asyncio.Task[None] | None = None
        self._send_lock = asyncio.Lock()

    async def connect(self) -> None:
        self._reader, self._writer = await asyncio.open_connection(self.config.host, self.config.port)
        self.connected = True
        await self._send({
            "type": "HANDSHAKE",
            "clientId": self.client_id,
            "roomId": normalized_room_id(self.config.room_id),
            "clientState": {
                "teamId": self.config.team_id,
                "name": self.name,
                "online": True,
                "isSaveLoaded": True,
            },
            "roomState": {},
        })
        self._recv_task = asyncio.create_task(self._recv_loop(), name=f"anchor-stress-rx-{self.index}")
        await self.publish_metadata(force=True)
        await self.publish_position()

    async def disconnect(self) -> None:
        self.connected = False
        if self._recv_task:
            self._recv_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._recv_task
        if self._writer:
            self._writer.close()
            with contextlib.suppress(Exception):
                await self._writer.wait_closed()

    async def set_state(
        self,
        *,
        x: int | None = None,
        y: int | None = None,
        z: int | None = None,
        room_id: int | None = None,
        character: str | None = None,
    ) -> None:
        if x is not None:
            self.x = x
        if y is not None:
            self.y = y
        if z is not None:
            self.z = z
        metadata_changed = False
        if room_id is not None and room_id != self.room_id:
            self.room_id = room_id
            metadata_changed = True
        if character is not None and character != self.character:
            self.character = normalize_character(character)
            metadata_changed = True
        if metadata_changed:
            await self.publish_metadata(force=True)
        await self.publish_position()

    async def publish_metadata(self, *, force: bool = False) -> None:
        del force
        room_name = ROOM_NAMES.get(self.room_id, "")
        await self._send({
            "type": "UPDATE_CLIENT_STATE",
            "state": {
                "clientId": self.client_id,
                "teamId": self.config.team_id,
                "name": self.name,
                "online": True,
                "isSaveLoaded": True,
                "currentRoom": room_name,
                "currentRoomId": self.room_id,
                "currentCharacter": self.character,
            },
        })

    async def publish_position(self) -> None:
        now_ms = int(time.monotonic() * 1000)
        vx = vy = vz = 0
        if self._last_pos is not None and self._last_pos_ms:
            dt_ms = max(1, now_ms - self._last_pos_ms)
            vx = int((self.x - self._last_pos[0]) * 1000 / dt_ms)
            vy = int((self.y - self._last_pos[1]) * 1000 / dt_ms)
            vz = int((self.z - self._last_pos[2]) * 1000 / dt_ms)
        self._last_pos = (self.x, self.y, self.z)
        self._last_pos_ms = now_ms
        self._pos_seq = (self._pos_seq + 1) & 0x7FFFFFFF
        await self._send({
            "type": "MNSG_PLAYER_POS",
            "clientId": self.client_id,
            "currentRoomId": self.room_id,
            "posX": self.x,
            "posY": self.y,
            "posZ": self.z,
            "velX": vx,
            "velY": vy,
            "velZ": vz,
            "posSeq": self._pos_seq,
            "posT": now_ms,
            "quiet": True,
        })

    async def tick(self) -> None:
        if self.connected:
            await self.publish_position()

    async def _recv_loop(self) -> None:
        assert self._reader is not None
        buf = b""
        try:
            while self.connected:
                chunk = await self._reader.read(4096)
                if not chunk:
                    break
                buf += chunk
                while b"\x00" in buf:
                    raw, buf = buf.split(b"\x00", 1)
                    if not raw.strip():
                        continue
                    with contextlib.suppress(json.JSONDecodeError):
                        await self._handle_packet(json.loads(raw.decode("utf-8", errors="replace")))
        finally:
            self.connected = False

    async def _handle_packet(self, packet: dict[str, Any]) -> None:
        ptype = packet.get("type", "")
        if ptype == "HEARTBEAT":
            await self._send({"type": "HEARTBEAT", "quiet": True})
            return
        if ptype == "DISABLE_ANCHOR":
            self.connected = False
            return
        if ptype == "ALL_CLIENT_STATE":
            assigned = await self.world.update_from_all_client_state(packet)
            if assigned and not self.client_id:
                self.client_id = assigned
                await self.publish_metadata(force=True)
                await self.publish_position()
            return
        if ptype == "MNSG_PLAYER_POS":
            await self.world.update_from_position_packet(packet)
            return
        if ptype == "UPDATE_CLIENT_STATE":
            state = packet.get("state") or packet.get("clientState") or {}
            if isinstance(state, dict):
                await self.world.update_from_state(state)

    async def _send(self, packet: dict[str, Any]) -> None:
        if not self.connected or not self._writer:
            return
        data = (json.dumps(packet, separators=(",", ":")) + "\x00").encode("utf-8")
        async with self._send_lock:
            self._writer.write(data)
            await self._writer.drain()


class StressController:
    def __init__(self, config: BotConfig, count: int, follow: str = "") -> None:
        self.config = config
        self.world = WorldState()
        self.bots = [AnchorBot(i + 1, config, self.world) for i in range(count)]
        self.follow_selector = follow
        self.follow_spread = 80
        self.running = True
        self._ticker_task: asyncio.Task[None] | None = None

    async def start(self) -> None:
        print(f"Connecting {len(self.bots)} clients to {self.config.host}:{self.config.port} room={normalized_room_id(self.config.room_id)!r}")
        results = await asyncio.gather(*(bot.connect() for bot in self.bots), return_exceptions=True)
        failures = [result for result in results if isinstance(result, Exception)]
        if failures:
            print(f"{len(failures)} clients failed to connect. First error: {failures[0]}")
        print(f"Connected: {sum(1 for bot in self.bots if bot.connected)}/{len(self.bots)}")
        self._ticker_task = asyncio.create_task(self._ticker_loop(), name="anchor-stress-ticker")

    async def stop(self) -> None:
        self.running = False
        if self._ticker_task:
            self._ticker_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._ticker_task
        await asyncio.gather(*(bot.disconnect() for bot in self.bots), return_exceptions=True)

    async def command_loop(self) -> None:
        print("Type `help` for commands. Type `quit` to disconnect all clients.")
        while self.running:
            try:
                line = await asyncio.to_thread(input, "anchor-stress> ")
            except (EOFError, KeyboardInterrupt):
                print()
                break
            try:
                await self.handle_command(line.strip())
            except Exception as exc:
                print(f"error: {exc}")
        await self.stop()

    async def handle_command(self, line: str) -> None:
        if not line:
            return
        parts = line.split()
        cmd = parts[0].lower()
        args = parts[1:]
        if cmd in {"quit", "exit"}:
            self.running = False
            return
        if cmd == "help":
            print_help()
            return
        if cmd == "status":
            await self.print_status()
            return
        if cmd == "list":
            await self.print_players()
            return
        if cmd == "rate":
            if not args:
                print(f"rate_hz={self.config.rate_hz}")
                return
            self.config.rate_hz = max(0.1, float(args[0]))
            print(f"rate_hz={self.config.rate_hz}")
            return
        if cmd == "follow":
            self.follow_selector = " ".join(args)
            print(f"following {self.follow_selector!r}" if self.follow_selector else "follow target cleared")
            return
        if cmd in {"stop-follow", "unfollow"}:
            self.follow_selector = ""
            print("follow target cleared")
            return
        if cmd == "spread":
            self.follow_spread = max(0, int(args[0]))
            print(f"follow_spread={self.follow_spread}")
            return
        if cmd == "set":
            await self._command_set(args)
            return
        if cmd == "move":
            await self._command_move(args)
            return
        if cmd == "room":
            await self._command_room(args)
            return
        if cmd == "char":
            await self._command_char(args)
            return
        print(f"unknown command: {cmd}")

    async def print_status(self) -> None:
        connected = sum(1 for bot in self.bots if bot.connected)
        ids = [bot.client_id for bot in self.bots if bot.client_id]
        print(f"bots={len(self.bots)} connected={connected} assigned_ids={len(ids)} rate_hz={self.config.rate_hz}")
        if self.follow_selector:
            target = await self.world.find_target(self.follow_selector)
            if target and target.x is not None:
                print(f"follow={target.client_id} {target.name} room={target.room_id} pos=({target.x},{target.y},{target.z})")
            else:
                print(f"follow={self.follow_selector!r} not found or has no position yet")

    async def print_players(self) -> None:
        players = sorted(await self.world.snapshot(), key=lambda p: p.client_id)
        if not players:
            print("no server player state received yet")
            return
        for p in players:
            pos = "unknown" if p.x is None else f"({p.x},{p.y},{p.z})"
            age = time.monotonic() - p.updated_at
            print(f"{p.client_id:>6} {p.name:<24} team={p.team_id:<12} room={p.room_id:<5} char={p.character:<10} pos={pos:<20} age={age:4.1f}s")

    async def _ticker_loop(self) -> None:
        while self.running:
            start = time.monotonic()
            if self.follow_selector:
                await self._apply_follow()
            await asyncio.gather(*(bot.tick() for bot in self.bots if bot.connected), return_exceptions=True)
            delay = max(0.01, (1.0 / max(0.1, self.config.rate_hz)) - (time.monotonic() - start))
            await asyncio.sleep(delay)

    async def _apply_follow(self) -> None:
        target = await self.world.find_target(self.follow_selector)
        if not target or target.x is None or target.y is None or target.z is None:
            return
        total = max(1, len(self.bots))
        for i, bot in enumerate(self.bots):
            if bot.client_id and bot.client_id == target.client_id:
                continue
            angle = (math.tau * i) / total
            ring = 1 + (i // max(1, int(math.tau * max(1, self.follow_spread) / 48)))
            radius = self.follow_spread * ring
            bot.x = int(target.x + math.cos(angle) * radius)
            bot.y = int(target.y)
            bot.z = int(target.z + math.sin(angle) * radius)
            metadata_changed = False
            if target.room_id >= 0:
                metadata_changed = metadata_changed or bot.room_id != target.room_id
                bot.room_id = target.room_id
            if target.character:
                next_character = normalize_character(target.character)
                metadata_changed = metadata_changed or bot.character != next_character
                bot.character = next_character
            if metadata_changed:
                await bot.publish_metadata(force=True)

    async def _command_set(self, args: list[str]) -> None:
        if len(args) != 6:
            raise ValueError("usage: set <all|N|A-B> <x> <y> <z> <roomId> <character>")
        bots = self._select_bots(args[0])
        x, y, z = int(args[1], 0), int(args[2], 0), int(args[3], 0)
        room_id = int(args[4], 0)
        character = normalize_character(args[5])
        await asyncio.gather(*(bot.set_state(x=x, y=y, z=z, room_id=room_id, character=character) for bot in bots))
        print(f"updated {len(bots)} bots")

    async def _command_move(self, args: list[str]) -> None:
        if len(args) != 4:
            raise ValueError("usage: move <all|N|A-B> <x> <y> <z>")
        bots = self._select_bots(args[0])
        x, y, z = int(args[1], 0), int(args[2], 0), int(args[3], 0)
        await asyncio.gather(*(bot.set_state(x=x, y=y, z=z) for bot in bots))
        print(f"moved {len(bots)} bots")

    async def _command_room(self, args: list[str]) -> None:
        if len(args) != 2:
            raise ValueError("usage: room <all|N|A-B> <roomId>")
        bots = self._select_bots(args[0])
        room_id = int(args[1], 0)
        await asyncio.gather(*(bot.set_state(room_id=room_id) for bot in bots))
        print(f"changed room for {len(bots)} bots")

    async def _command_char(self, args: list[str]) -> None:
        if len(args) != 2:
            raise ValueError("usage: char <all|N|A-B> <Goemon|Ebisumaru|Sasuke|Yae>")
        bots = self._select_bots(args[0])
        character = normalize_character(args[1])
        await asyncio.gather(*(bot.set_state(character=character) for bot in bots))
        print(f"changed character for {len(bots)} bots")

    def _select_bots(self, selector: str) -> list[AnchorBot]:
        if selector.lower() == "all":
            return self.bots
        if "-" in selector:
            start_s, end_s = selector.split("-", 1)
            start_i = int(start_s)
            end_i = int(end_s)
            return [bot for bot in self.bots if start_i <= bot.index <= end_i]
        index = int(selector)
        for bot in self.bots:
            if bot.index == index:
                return [bot]
        raise ValueError(f"no bot index {index}")


def normalized_room_id(room_id: str) -> str:
    visible_room_id = room_id.strip(ROOM_ID_TRIM_CHARS)
    if not visible_room_id:
        raise ValueError("room id is required")
    if visible_room_id.startswith(ROOM_ID_PREFIX):
        if len(visible_room_id) == len(ROOM_ID_PREFIX):
            raise ValueError("room id must include a value after the namespace prefix")
        return visible_room_id
    return f"{ROOM_ID_PREFIX}{visible_room_id}"


def normalize_character(value: str) -> str:
    folded = value.strip().lower()
    lookup = {c.lower(): c for c in CHARACTERS}
    if folded not in lookup:
        raise ValueError(f"character must be one of: {', '.join(CHARACTERS)}")
    return lookup[folded]


def print_help() -> None:
    print(
        """
Commands:
  status
      Show bot connection counts and follow target state.
  list
      List players the server has reported, including real game clients.
  set <all|N|A-B> <x> <y> <z> <roomId> <character>
      Set position, room, and character for selected synthetic clients.
  move <all|N|A-B> <x> <y> <z>
      Set only position.
  room <all|N|A-B> <roomId>
      Set only room id. Hex like 0x1d1 is accepted.
  char <all|N|A-B> <Goemon|Ebisumaru|Sasuke|Yae>
      Set only selected character.
  follow <clientId|name substring>
      Move all synthetic clients in a ring around a server client.
      The bots copy the target's room and character while following.
  stop-follow
      Stop following; bots remain at their current coordinates.
  spread <distance>
      Set spacing around the follow target.
  rate <updates-per-second>
      Change movement packet rate.
  quit
      Disconnect all synthetic clients.
""".strip()
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-n", "--clients", type=int, required=True, help="number of synthetic clients to spawn")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--room", required=True, help="private room id for this test run")
    parser.add_argument("--team", default="default")
    parser.add_argument("--name-prefix", default="Stress")
    parser.add_argument("--x", type=int, default=0)
    parser.add_argument("--y", type=int, default=0)
    parser.add_argument("--z", type=int, default=0)
    parser.add_argument("--room-id", type=lambda s: int(s, 0), default=0x1D1)
    parser.add_argument("--character", default="Goemon", choices=CHARACTERS)
    parser.add_argument("--rate", type=float, default=5.0, help="position updates per second per connected client")
    parser.add_argument("--follow", default="", help="initial follow target, by server client id or name substring")
    return parser.parse_args()


async def amain() -> None:
    args = parse_args()
    if args.clients <= 0:
        raise SystemExit("--clients must be greater than 0")
    config = BotConfig(
        host=args.host,
        port=args.port,
        room_id=args.room,
        team_id=args.team,
        name_prefix=args.name_prefix,
        start_x=args.x,
        start_y=args.y,
        start_z=args.z,
        start_room=args.room_id,
        character=normalize_character(args.character),
        rate_hz=max(0.1, args.rate),
    )
    controller = StressController(config, args.clients, args.follow)
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        with contextlib.suppress(NotImplementedError):
            loop.add_signal_handler(sig, lambda: setattr(controller, "running", False))
    await controller.start()
    await controller.command_loop()


if __name__ == "__main__":
    asyncio.run(amain())
