"""Occupied-room NPC/platform checkpoints. No pointers or offline event queue.

Presence is separate from simulation ownership: proximity culling on one client
must not stop an actor still loaded by another. Complete, bounded batches replace
peer state atomically. Call under anchor_mnsg's player-state lock.
"""
import json
from anchor_world_npc import valid as npc_valid
import anchor_world_counterweight as counterweight

PACKET_TYPE = 'MNSG_WORLD'
METADATA_KEY = 'worldSync'
VERSION = 16
WORDS = 50
INSTANCE = 48
RECEIPT = 49
BOOTSTRAP = 0x40000000
APPLY_FAILED = BOOTSTRAP-1
MAX_ACTORS = 256
ROWS_PER_PACKET = 24
MAX_PARTS = 11
PACKET_BYTES = 8192
STATE_BYTES = 131072
INTERVAL = .2
TTL = 1.5
# Room 0x31 crane (placed entity 0x1b9). Pads aggregate per client, so the
# checkpoint carries one local input mask and one shared aggregate mask.
CRANE = 7
CRANE_ENTITY = 0x1b9
CRANE_ROOM = 0x31
CRANE_TEMP_MASK = 0x4bcf
CRANE_INPUT = 46
CRANE_AGGREGATE = 47
CRANE_INPUT_MAX = 3
# Room 0xB2 timed shutter (placed entity 0x354). One typed phase/timer/animation
# checkpoint per placed shutter plus its retained emission ordinal.
SHUTTER = 8
SHUTTER_ENTITY = 0x354
SHUTTER_ROOM = 0xb2
# Room 0x15E File51 Super Pass bridge (placed root entity 0x240). One root
# checkpoint owns both guard routes, the gate presentation and the blocker.
BRIDGE = 9
BRIDGE_ENTITY = 0x240
BRIDGE_ROOM = 0x15e
WB_FLAGS = 4
WB_FRESH = 5
WB_GATE_PHASE = 6
WB_GATE_FRAME = 7
WB_GATE_ANIMATION = 8
WB_BLOCKER_REMOVED = 9
WB_GUARD_0 = 10
WB_GUARD_1 = 24
WB_PAUSED = 38
WB_RESERVED = 45
WB_INPUT = 46
WB_AGGREGATE = 47
WB_INPUT_MAX = 3
# Room 0x14B gate64 (placed entity 0x325). One typed phase/timer row per gate;
# the native adapter owns the phase-specific model and resource binding.
GATE64 = 10
GATE64_ENTITY = 0x325
GATE64_ROOM = 0x14b
WG64_TIMER = 11
WG64_PHASE = 12
WG64_COMPLETE = 25
# Rooms 0x16A / 0x182 File62 doll container (placed entity 0x3D6). One typed
# cycle/phase/pitch checkpoint per container; the native adapter owns the
# spawned doll and the closing presentation.
DOLL = 11
DOLL_ENTITY = 0x3d6
DOLL_ROOMS = (0x16a, 0x182)
DOLL_CYCLE = 11
DOLL_PHASE = 12
DOLL_SPAWNED = 13
# Delivery rows are [source clientId, staleness ms] + the 50 checkpoint words.
DELIVERY_ROW = 2


def integer(n, lo, hi):
    return type(n) is int and lo <= n <= hi


def metadata(value, session=None):
    if (not isinstance(value, list) or len(value) != 5 or
            value[0] != VERSION or
            not all(integer(v, 0, 0x7fffffff) for v in value) or
            (session is not None and value[1] != session) or
            value[3] >= 800 or value[4] > 65535):
        return None
    return list(value)


def merge_metadata(old, new, session=None):
    new = metadata(new, session)
    old = metadata(old, session)
    if old and (not new or new[2] < old[2]):
        return old
    return new


def bitmap(value):
    if not isinstance(value, str) or len(value) != 64:
        return None
    try:
        result = bytes.fromhex(value)
    except ValueError:
        return None
    return result if len(result) == 32 else None


def bit(bits, index):
    return bool(bits[index // 8] & (1 << (index % 8)))


def pad_inputs(value, room=None):
    """Bounded per-root rider/latch inputs for the current room."""
    coupled = room == counterweight.ROOM
    if not isinstance(value, list) or len(value) > (3 if coupled else 1):
        return None
    seen = set()
    entries = []
    for entry in value:
        if (not isinstance(entry, list) or len(entry) != 2 or
                not integer(entry[0], 0, MAX_ACTORS-1) or
                not integer(entry[1], 0, 63 if coupled else CRANE_INPUT_MAX) or
                (coupled and entry[0] not in counterweight.POSES)):
            return None
        key = entry[0]
        if key in seen:
            return None
        seen.add(key)
        entries.append(list(entry))
    return entries


def inputs_key(value):
    return tuple(tuple(entry) for entry in value)


CRANE_BOUNDS = [(0,255),(CRANE_ENTITY,CRANE_ENTITY),(CRANE,CRANE),(0,0)]
CRANE_BOUNDS += [(-3276800,3276700)]*3 + [(0,1023)]*3 + [(1,1)]
CRANE_BOUNDS += [(0,1000000),(0,65535),(0,7)] + [(-100000,100000)]*3
CRANE_BOUNDS += [(-1,60),(1,19),(1,5),(-1,10),(-32768,32767),(1,5),(-1,10),(-32768,32767)]
CRANE_BOUNDS += [(0,CRANE_TEMP_MASK)] + [(0,64000)]*3
CRANE_BOUNDS += [(0,3),(0,1),(0,0),(0,1),(0,3)]
CRANE_BOUNDS += [(-32768,32767)]*3 + [(1,13),(0,1),(0,1023)] + [(-32768,32767)]*3
CRANE_BOUNDS += [(-1,12),(0,2),(0,1),(0,CRANE_INPUT_MAX),(0,CRANE_INPUT_MAX)]
CRANE_BOUNDS += [(0,0x7fffffff)]*2


# Every reserved shutter word is a fixed zero bound, so this table is the whole
# checkpoint contract: pose/clip/frame/rate/animflags with clip 14 and no
# velocity, signed timer, four phases, immutable emitter, emission ordinal,
# playback/reverse bits and the pause flag.
SHUTTER_BOUNDS = [(0,255),(SHUTTER_ENTITY,SHUTTER_ENTITY),(SHUTTER,SHUTTER),(0,0)]
SHUTTER_BOUNDS += [(-3276800,3276700)]*3 + [(0,1023)]*3 + [(14,14)]
SHUTTER_BOUNDS += [(0,1000000),(0,65535),(0,7)] + [(0,0)]*3
SHUTTER_BOUNDS += [(-1,90),(1,4),(0,1),(0,0x7fffffff)] + [(0,0)]*5
SHUTTER_BOUNDS += [(0,64000)]*3 + [(0,3)] + [(0,0)]*8 + [(0,1)] + [(0,0)]*9
SHUTTER_BOUNDS += [(0,0x7fffffff)]*2


# One bridge root carries two 14-word guard blocks: packed phase/playback/
# status400, XYZ hundredths, yaw with AA packed above it, clip, frame, rate with
# animation flags packed above it, route timer, three origin halfwords, route
# substate and the local route PC. Guard phase is 1 pre-open, 2 route, 3
# post-open, so the low two bits must never be zero. The route ID is local and
# immutable (guard0 0x44, guard1 0x43) and never crosses this row.
BRIDGE_GUARD = [(1,15),(-3276800,3276700),(-3276800,3276700),(-3276800,3276700),
                (0,262143),(0,2),(0,1000000),(0,0x7ffff),(-32768,32767),
                (-32768,32767),(-32768,32767),(-32768,32767),(0,7),(0,255)]
BRIDGE_BOUNDS = [(0,255),(BRIDGE_ENTITY,BRIDGE_ENTITY),(BRIDGE,BRIDGE),(0,1),
                 (0,3),(0,1),(1,3),(0,1000000),(0,0xfffff),(0,1)]
BRIDGE_BOUNDS += BRIDGE_GUARD*2
BRIDGE_BOUNDS += [(0,1)] + [(-100000,100000)]*6 + [(0,0),(0,WB_INPUT_MAX),(0,WB_INPUT_MAX)]
BRIDGE_BOUNDS += [(0,0x7fffffff)]*2


# Room 0x14B gate64. Bounds are the whole wire contract apart from the
# completion cross-check: the native adapter owns the phase-specific model,
# resource and collision binding. Body and child transforms are hundredths,
# the child scale is the fixed 150 build value, and the timer never feeds
# authority.
GATE64_BOUNDS = [(0,255),(GATE64_ENTITY,GATE64_ENTITY),(GATE64,GATE64),(0,1)]
GATE64_BOUNDS += [(-3276800,3276700)]*3 + [(0,1023)]*3
GATE64_BOUNDS += [(1,2),(-1,32767),(1,10),(0,1)]
GATE64_BOUNDS += [(0,0)]*3
GATE64_BOUNDS += [(-3276800,3276700)]*3 + [(0,1023)]*3
GATE64_BOUNDS += [(150,150),(0,1),(0,1)]
GATE64_BOUNDS += [(0,0)]*12
GATE64_BOUNDS += [(0,1)] + [(0,0)]*9
GATE64_BOUNDS += [(0,0x7fffffff)]*2


# Rooms 0x16A / 0x182 File62 doll container. Bounds plus the cycle/phase/pitch
# contract: the idle pose is the only one with an unstarted cycle, the opening
# slides in eighths up to 64, the birth pose is fully raised, and the closing
# steps back down the same ramp.
DOLL_BOUNDS = [(0,255),(DOLL_ENTITY,DOLL_ENTITY),(DOLL,DOLL),(0,0)]
DOLL_BOUNDS += [(-3276800,3276700)]*3
DOLL_BOUNDS += [(0,70),(0,1023),(0,1023),(0,0)]
DOLL_BOUNDS += [(0,1000000),(0,3),(0,1)]
DOLL_BOUNDS += [(0,0)]*24
DOLL_BOUNDS += [(0,1)] + [(0,0)]*9
DOLL_BOUNDS += [(0,0x7fffffff)]*2


def row_valid(row):
    if (not isinstance(row, list) or len(row) != WORDS or
            not all(integer(v, -0x80000000, 0x7fffffff) for v in row)):
        return False
    if row[2] == counterweight.KIND:
        return counterweight.valid(row)
    if row[2] == CRANE:
        # Early return keeps the dedicated crane words out of the actor/NPC
        # bounds below; kind word 7 also fails that table's kind range.
        return (all(lo <= v <= hi for v, (lo, hi) in zip(row, CRANE_BOUNDS)) and
                row[1] == CRANE_ENTITY and not row[3] and row[10] == 1 and
                not (row[25] & ~CRANE_TEMP_MASK) and
                (row[37] == 13 or 1 <= row[37] <= 11) and
                row[32] == (1 if row[37] == 13 else 0) and
                (row[44] == 2) == (row[32] == 1))
    if row[2] == SHUTTER:
        # Same shape as the crane case: kind 8 never reaches the actor/NPC table
        # below, and the shutter needs no cross-word checks.
        return all(lo <= v <= hi for v, (lo, hi) in zip(row, SHUTTER_BOUNDS))
    if row[2] == BRIDGE:
        # Both guard blocks repeat the same 14-word pattern. The native adapter
        # also validates the PC against the local route's opcode boundaries.
        return (all(lo <= v <= hi for v, (lo, hi) in zip(row, BRIDGE_BOUNDS)) and
                (row[WB_GUARD_0] & 3) != 0 and (row[WB_GUARD_1] & 3) != 0)
    if row[2] == GATE64:
        # Bounds plus one cross-check: the completion flag mirrors the final
        # phase, so a replica cannot hold an ended gate at a live phase.
        return (all(lo <= v <= hi for v, (lo, hi) in zip(row, GATE64_BOUNDS)) and
                row[WG64_COMPLETE] == (1 if row[WG64_PHASE] == 10 else 0))
    if row[2] == DOLL:
        pitch, phase = row[7], row[DOLL_PHASE]
        live = row[DOLL_CYCLE] >= 1
        if phase == 0:
            # A zero cycle only ever means idle, but a finished idle container
            # legitimately holds a live cycle, so the pitch alone is pinned.
            pose = pitch == 0
        elif phase == 1:
            pose = live and pitch <= 64 and pitch % 8 == 0
        elif phase == 2:
            pose = live and pitch == 70
        else:
            pose = live and pitch in (70,54,38,22,6)
        return (all(lo <= v <= hi for v, (lo, hi) in zip(row, DOLL_BOUNDS)) and
                pose)
    bounds = [(0,255),(1,0x7ff),(1,6),(0,1)] + [(-3276800,3276700)]*3
    bounds += [(0,1023)]*3 + [(0,255),(0,1000000),(0,65535),(0,7)]
    bounds += [(-100000,100000)]*3 + [(-32768,32767),(0,0x7fffffff)]
    bounds += [(-32768,32767)]*7 + [(0,64000)]*3 + [(0,31)]
    bounds += [(0,163),(-32768,32767),(0,7),(0,255)]
    bounds += [(-32768,32767)]*3 + [(0,255),(0,1)]
    bounds += [(0,22)] + [(-32768,32767)]*8
    bounds += [(0,0x7fffffff)]*2
    if row[2] == 2 and row[1] == 0x3d0:
        bounds[23] = (0,8388607)
    if not all(lo <= v <= hi for v, (lo, hi) in zip(row, bounds)):
        return False
    if row[2] == 2 and row[1] in (0x365,0x366):
        top = row[1] == 0x365
        if (any(row[i] for i in (3,7,9,*range(10,18),25,*range(30,38),*range(39,48))) or
                row[18] != (78 if top else 79) or row[26:29] != [100]*3 or row[29] != 5):
            return False
        if top:
            return (0 <= row[19] <= 1023 and
                    ((row[20] in (2,3) and row[21:23] == [160,1]) or
                     (row[20] == 0 and row[21:23] in ([130,2],[80,4]))))
        return row[19] in (0,1) and not any(row[20:25])
    if row[2] == 2 and row[1] == 0x1aa:
        return (not row[3] and row[18] == 77 and row[20] == 10 and not row[17] and
                row[7] == (row[19]&1023) and not row[10] and not row[11] and
                row[12] == 512 and row[13] == 1 and row[26:29] == [1000]*3 and row[29] == 1 and
                not any(row[i] for i in (*range(14,17),*range(21,26),
                                        *range(30,38),*range(39,48))))
    if row[2] == 2 and row[1] == 0x3ca:
        subtype,phase = row[19:21]
        return (not row[3] and row[18] == 76 and 0 <= subtype <= 2 and
                phase in (1,3,4) and
                0 <= row[21] <= (20 if subtype == 2 else 23 if subtype == 1 else 24) and
                row[22] == (90 if subtype else 155) and row[23] == int(phase == 4) and
                row[10] == (2 if subtype == 1 else 0) and row[11] <= 299 and
                row[12] == 12 and row[13] in (0,2) and
                (0 <= row[17] <= 155 if phase == 1 else row[17] == -1) and
                row[29] == (1 if phase == 1 else 17) and
                row[26:29] == [1000,1000,1000 if subtype else 1200] and
                not any(row[i] for i in (*range(14,17),24,25,*range(30,38),*range(39,48))))
    if row[2] == 2 and row[1] == 0x3d0:
        p = row[18]
        animated = 71 <= p <= 74
        flags = (31,27,1,11,15,27,17)
        return (69 <= p <= 75 and 0 <= row[21] <= 5 and 0 <= row[22] <= 31 and
                0 <= row[24] <= 255 and 0 <= row[25] <= 255 and row[30] <= 6 and
                row[29] == flags[row[30]] and row[10] == (1 if p == 71 else 2 if animated else 0) and
                (p not in (69,75) or row[30] <= 4) and (p != 70 or row[30] == 5) and
                (not animated or row[30] == 6) and (p == 69 or not row[3]) and
                (0 <= row[17] <= 200 if p == 73 else row[17] == 0) and
                not any(row[i] for i in range(31,INSTANCE) if i != 38))
    if row[2] == 6 and (
            row[1] != 0x226 or not 1 <= row[18] <= 5 or
            (row[18] > 1 and row[3] != 1) or not 0 <= row[19] <= 1 or
            not 0 <= row[20] < (2048 if row[19] else 800) or
            not 0 <= row[21] <= 1 or not 0 <= row[22] <= 1 or
            any(row[i] for i in (10,17,23,24,25)) or row[29] & ~19):
        return False
    if row[2] == 2 and row[1] in (0x324,0x326):
        temporary = row[1] == 0x324
        first, last = (53,55) if temporary else (56,59)
        if (not first <= row[18] <= last or (row[18] != first and row[3] != 1) or
                not 0 <= row[19] <= (11 if temporary else 7) or
                not 0 <= row[20] < (800 if temporary else 2048) or
                not 0 <= row[21] <= 1 or not 0 <= row[24] <= 1 or
                (row[24] and (temporary or row[18] != 59))):
            return False
    if row[2] == 2 and row[1] in (0x228,0x1fe):
        fire = row[1] == 0x1fe
        first,last,active = (64,68,65) if fire else (60,63,62)
        if (not first <= row[18] <= last or row[23] != int(row[18] >= active)
                or row[39]):
            return False
        if not fire:
            if (any(row[i] for i in (3,17,30,32,33,37)) or
                    not 0 <= row[24] <= 255 or not 0 <= row[25] <= 4 or
                    not 0 <= row[31] <= 255 or
                    row[10] != (2 if row[23] and row[25] == 2 else 0)):
                return False
        else:
            if (row[30] > 3 or row[10] != (2 if row[23] else 0) or not 0 <= row[24] <= 5 or
                    not 0 <= row[25] <= 10 or (row[18] != 67 and row[25]) or
                    (row[18] == 67 and (row[24] != 1 or not row[25])) or
                    (row[18] in (65,66) and row[24] not in (0,5)) or
                    (row[18] == 68 and not 1 <= row[24] <= 4) or
                    not 0 <= row[31] <= 2047 or row[32] > 1 or row[33] > 1 or
                    not 0 <= row[40] <= 25500 or not 0 <= row[41] <= 8000 or
                    row[42] not in (0,0x200,0x240)):
                return False
            if 1 <= row[24] <= 4:
                if not 1 <= row[17] <= (256 if row[24] == 3 else 40):
                    return False
            elif any(row[i] for i in (17,40,41)):
                return False
            if not row[33] and any(row[i] for i in (32,37,45,46,47)):
                return False
            if not row[23] and any(row[i] for i in (3,24,25,31,33,42,43,44)):
                return False
            return True
    model = {0x2c3:0x2c2,0x2d8:0x2bf,0x2d9:0x2c1}.get(row[1],row[1])
    return (all(lo <= v <= hi for v, (lo, hi) in zip(row, bounds)) and
            (row[2] == 1 or not row[39]) and npc_valid(row[1],model,row[39:48]))


def controller_progress(row):
    if row[2] == counterweight.KIND:
        return 0
    if row[2] == 2 and row[1] in (0x3ca,0x1aa,0x365,0x366):
        return 0  # Wait/open/retract repeats; established receipts choose authority.
    # Doll container progress is the emission cycle plus the phase step, so a
    # later cycle always outranks an earlier one and the idle pose scores just
    # above its own cycle.
    if row[2] == DOLL:
        phase = row[DOLL_PHASE]
        return row[DOLL_CYCLE]*4 + (3 if phase == 0 else phase-1)
    # Gate64 authority is the phase alone, so a completed gate outranks every
    # running one. The idle and wrong-hit-cooldown phases score zero, and the
    # native timer never feeds authority.
    if row[2] == GATE64:
        return row[WG64_PHASE] if row[WG64_PHASE] >= 3 else 0
    # Bridge progress is temp flags*16 plus both guard phases, so a further
    # route or a started opening ranks above an untouched root. The caller keeps
    # established native receipt ahead of this value, so a fresh save-1
    # constructor pose cannot outrank a live route or an active dialogue.
    if row[2] == BRIDGE:
        return row[WB_FLAGS]*16 + (row[WB_GUARD_0] & 3) + (row[WB_GUARD_1] & 3)
    # The shutter's retained emission ordinal is monotone for the whole visit,
    # so it alone decides which checkpoint is current.
    if row[2] == SHUTTER:
        return row[20]
    if row[2] == 2 and row[1] == 0x3d0:
        if 70 <= row[18] <= 74:
            p = row[18]-69 if row[18] <= 72 else 204-row[17] if row[18] == 73 else 205
            return 0x40000000+p
        return row[23]*128+int(row[18]==75)
    if row[2] == 2 and row[1] in (0x228,0x1fe):
        return row[23]*2 + int(row[1] == 0x228 and row[18] == 61)
    # A constructor reading an already-set save flag chooses the endpoint.
    # Prefer an actual in-progress transition over that fresh endpoint pose.
    return row[18]*2 - (5 if row[1]==0x326 and row[18]==59 and row[24] else 0)


class WorldTransport:
    def __init__(self):
        # Kept across scope/reset changes so an old native acknowledgment
        # cannot accidentally acknowledge a new room's first offer.
        self.receipt_serial = 0
        self.reset()

    def reset(self):
        self.scope = None
        self.owners = {}
        self.visit = 0
        self.room = 0
        self.signature = 0
        self.peers = {}
        self.pending = {}
        self.dead = bytearray(32)
        self.last_send = -100.
        self.last_attempt = -100.
        self.sequence = 0
        self.dirty = False
        self.last_presence = ''
        self.last_dead = ''
        self.last_busy = ''
        self.last_paused = ''
        self.last_established = ''
        self.last_rows = ()
        self.last_inputs = ()
        self.force = False
        self.instances = {}
        self.established = set()
        self.offers = {}
        self.members_seen = {}

    @staticmethod
    def needs_bootstrap(row):
        return row[2] in (1,2,4,5,CRANE,SHUTTER,BRIDGE,GATE64,DOLL,counterweight.KIND) and row[1] not in (0x324,0x326)

    def _pad_mask(self, index, row):
        """OR of live same-room pad inputs for one crane, paused rows excluded.

        Peers that do not own the crane never publish its row, so presence and
        the paused bit come from their bitmaps rather than the checkpoint."""
        mask = 0 if row[38] else row[CRANE_INPUT]
        for peer in self.peers.values():
            for placed, pad in peer.get('u') or []:
                if placed != index:
                    continue
                if not bit(peer['p'],index) or bit(peer['z'],index):
                    continue
                mask |= pad
        return mask

    def _bridge_mask(self, index, row):
        """OR of live same-room bridge latches for one root, paused included.

        A paused bridge row still carries a latch that its own native event
        already accepted, so unlike the crane pads it keeps contributing, and a
        peer entry published for another kind at this index is ignored."""
        mask=row[WB_INPUT]
        for peer in self.peers.values():
            for placed, pad in peer.get('u') or []:
                if placed != index or not bit(peer['p'],index):
                    continue
                state=peer['rows'].get(index)
                if state is not None and state[2] != BRIDGE:
                    continue
                mask |= pad
        return mask

    def _waiting_for_members(self, ctx, now):
        current = set()
        waiting = False
        for cid, player in ctx['players'].items():
            if (cid == ctx['cid'] or not player.get('online') or
                    not player.get('isSaveLoaded') or player.get('teamId') != ctx['team'] or
                    player.get('roomId') != self.room):
                continue
            raw = player.get(METADATA_KEY)
            if isinstance(raw,list) and raw and raw[0] != VERSION:
                continue
            meta = self._peer(ctx,cid)
            key = (player.get('interactionSession'),tuple(meta) if meta else None)
            current.add(cid)
            previous = self.members_seen.get(cid)
            if not previous or previous[0] != key:
                self.members_seen[cid] = (key,now)
                self.force = True
            if cid not in self.peers and now-self.members_seen[cid][1] <= TTL:
                waiting = True
        self.members_seen = {cid:v for cid,v in self.members_seen.items() if cid in current}
        return waiting

    def _offer_key(self, cid, state, instance):
        return (cid,tuple(self.peers[cid]['meta']),tuple(state[:INSTANCE]),instance)

    def _acknowledged(self, row):
        offer = self.offers.get(row[0])
        if not offer or not offer['token'] or row[RECEIPT] != offer['token']:
            return False
        cid = offer['key'][0]
        peer = self.peers.get(cid)
        state = peer['rows'].get(row[0]) if peer and row[0] in peer['published'] else None
        return bool(state and self._offer_key(cid,state,row[INSTANCE]) == offer['key'])

    def _native_offer(self, row, cid, now, bootstrap):
        peer = self.peers[cid]
        state = peer['rows'].get(row[0])
        if not state or row[0] not in peer['published'] or state[1:3] != row[1:3]:
            return None
        key = self._offer_key(cid,state,row[INSTANCE])
        offer = self.offers.get(row[0])
        if not offer or offer['key'] != key or offer['bootstrap'] != bootstrap:
            # Exhaustion keeps this instance a replica; it never wraps into
            # an old receipt. Reaching the bound requires over a billion offers.
            if self.receipt_serial < APPLY_FAILED-1:
                self.receipt_serial += 1
                token = self.receipt_serial | (BOOTSTRAP if bootstrap else 0)
            else:
                token = 0
            offer = dict(key=key,token=token,bootstrap=bootstrap)
            self.offers[row[0]] = offer
        delivered = list(state)
        delivered[INSTANCE] = row[INSTANCE]
        delivered[RECEIPT] = offer['token']
        return [cid,int(max(0,now-peer['time'])*1000)] + delivered

    def advertisement(self, ctx):
        return [VERSION, ctx.get('session', 0), self.visit,
                self.room, self.signature]

    def _peer(self, ctx, cid):
        peer = ctx['players'].get(cid, {})
        meta = metadata(peer.get(METADATA_KEY), peer.get('interactionSession'))
        if (cid == ctx['cid'] or not peer.get('online') or
                not peer.get('isSaveLoaded') or peer.get('teamId') != ctx['team'] or
                peer.get('roomId') != self.room or not meta or
                not meta[2] or meta[3:] != [self.room, self.signature]):
            return None
        return meta

    def _prune(self, ctx, now):
        for cid in list(self.peers):
            p = self.peers[cid]
            if self._peer(ctx, cid) != p['meta'] or now - p['time'] > TTL:
                del self.peers[cid]
        for cid in list(self.pending):
            p = self.pending[cid]
            if self._peer(ctx, cid) != p['meta'] or now - p['time'] > TTL:
                del self.pending[cid]

    def receive(self, ctx, packet, now):
        if not self.scope or not ctx.get('connected') or not ctx.get('loaded'):
            return False
        if not isinstance(packet, dict) or packet.get('type') != PACKET_TYPE:
            return False
        cid = packet.get('clientId')
        if not integer(cid, 1, 0x7fffffff):
            return False
        meta = self._peer(ctx, cid)
        if (not meta or packet.get('targetTeamId') != ctx['team'] or
                'targetClientId' in packet or packet.get('addToQueue', False) or
                packet.get('m') != meta):
            return False
        q, part, parts = (packet.get(k) for k in ('q','part','parts'))
        presence, dead = bitmap(packet.get('p')), bitmap(packet.get('d'))
        busy = bitmap(packet.get('b'))
        paused = bitmap(packet.get('z'))
        established = bitmap(packet.get('h'))
        rows = packet.get('a')
        inputs = pad_inputs(packet.get('u'), self.room)
        if (not integer(q,1,0x7fffffff) or not integer(parts,1,MAX_PARTS) or
                not integer(part,0,parts-1) or presence is None or dead is None or busy is None or paused is None or established is None or
                inputs is None or (inputs and self.room not in (CRANE_ROOM,BRIDGE_ROOM,counterweight.ROOM)) or
                any((b|z|h) & ~p for b,z,h,p in zip(busy,paused,established,presence)) or
                not isinstance(rows,list) or len(rows)>ROWS_PER_PACKET or
                not all(row_valid(r) and not r[INSTANCE] and not r[RECEIPT] and
                        not (r[2]==CRANE and (r[CRANE_INPUT] or r[CRANE_AGGREGATE])) and
                        not (r[2]==BRIDGE and (r[WB_INPUT] or r[WB_AGGREGATE])) and
                        not (r[2]==counterweight.KIND and
                             (self.room != counterweight.ROOM or r[46] or r[47])) and
                        not (r[2]==SHUTTER and self.room != SHUTTER_ROOM) and
                        not (r[2]==BRIDGE and self.room != BRIDGE_ROOM) and
                        not (r[2]==GATE64 and self.room != GATE64_ROOM) and
                        not (r[2]==DOLL and self.room not in DOLL_ROOMS) and
                        bit(presence,r[0]) for r in rows) or
                len(json.dumps(packet,separators=(',',':')).encode())+1>PACKET_BYTES):
            return False
        self._prune(ctx,now)
        old = self.peers.get(cid)
        if old and q <= old['q']:
            return False
        pending = self.pending.get(cid)
        if pending and q < pending['q']:
            return False
        if not pending or pending['q'] != q:
            if len(self.pending) >= 32 and cid not in self.pending:
                return False
            pending = dict(q=q,meta=meta,time=now,parts=parts,p=presence,d=dead,b=busy,z=paused,h=established,u=inputs,chunks={})
            self.pending[cid] = pending
        if (pending['parts'] != parts or pending['p'] != presence or
                pending['d'] != dead or pending['b'] != busy or pending['z'] != paused or pending['h'] != established or pending['u'] != inputs or part in pending['chunks']):
            return False
        pending['chunks'][part] = rows
        if len(pending['chunks']) != parts:
            return True
        combined = [row for n in range(parts) for row in pending['chunks'][n]]
        if len({row[0] for row in combined}) != len(combined):
            del self.pending[cid]
            return False
        if len(self.peers) >= 32 and cid not in self.peers:
            del self.pending[cid]
            return False
        retained = {i:r for i,r in old['rows'].items() if bit(presence,i)} if old else {}
        retained.update({row[0]:row for row in combined})
        self.force = self.force or not old or old['p'] != presence
        self.peers[cid] = dict(q=q,meta=meta,time=now,p=presence,b=busy,z=paused,h=established,
                              rows=retained,published={r[0] for r in combined},u=inputs)
        for i in range(32):
            self.dead[i] |= dead[i]
        del self.pending[cid]
        return True

    def update(self, ctx, room, signature, visit, rows, removed, now):
        active = (ctx.get('connected') and ctx.get('loaded') and ctx.get('cid',0)>0 and ctx.get('worldRosterReady',True)
                  and integer(room,0,799) and integer(signature,1,65535)
                  and integer(visit,1,0x7fffffff) and ctx.get('room')==room)
        scope = (ctx.get('session'),ctx.get('team'),room,signature,visit) if active else None
        if scope != self.scope:
            self.reset()
            self.scope = scope
            self.visit, self.room, self.signature = (visit,room,signature) if active else (0,0,0)
            self.dirty = True
        if not active:
            return {'a':[], 'd':'00'*32}, []
        if (not isinstance(rows,list) or len(rows)>MAX_ACTORS or
                not all(row_valid(row) for row in rows) or
                len({r[0] for r in rows}) != len(rows) or
                (self.room != SHUTTER_ROOM and any(r[2]==SHUTTER for r in rows)) or
                (self.room != BRIDGE_ROOM and any(r[2]==BRIDGE for r in rows)) or
                (self.room != GATE64_ROOM and any(r[2]==GATE64 for r in rows)) or
                (self.room not in DOLL_ROOMS and any(r[2]==DOLL for r in rows)) or
                (self.room != counterweight.ROOM and any(r[2]==counterweight.KIND for r in rows))):
            return {'a':[], 'd':self.dead.hex()}, []
        removed = bitmap(removed)
        if removed is None:
            return {'a':[], 'd':self.dead.hex()}, []
        for i in range(32):
            self.dead[i] |= removed[i]
        self._prune(ctx,now)
        waiting = self._waiting_for_members(ctx,now)
        presence=bytearray(32)
        for row in rows:
            presence[row[0]//8] |= 1 << (row[0]%8)
        # Only room 0x31 publishes pad input; paused or culled rows contribute
        # nothing even when the local actor is still resident.
        inputs=[]
        if self.room == CRANE_ROOM:
            best=None
            for row in rows:
                if row[2]==CRANE and not row[38] and bit(presence,row[0]):
                    if best is None or row[0] < best[0]:
                        best=(row[0],row[CRANE_INPUT])
            if best:
                inputs=[[best[0],best[1]]]
        elif self.room == BRIDGE_ROOM:
            # The bridge latch keeps propagating while the row is paused: the
            # local native event that set it was already accepted.
            best=None
            for row in rows:
                if row[2]==BRIDGE and bit(presence,row[0]):
                    if best is None or row[0] < best[0]:
                        best=(row[0],row[WB_INPUT])
            if best:
                inputs=[[best[0],best[1]]]
        elif self.room == counterweight.ROOM:
            inputs = [[r[0],r[46]] for r in rows
                      if r[2]==counterweight.KIND and not r[38]]
        output, publish = [], []
        indices = {r[0] for r in rows}
        self.established.intersection_update(indices)
        self.instances = {i:v for i,v in self.instances.items() if i in indices}
        self.offers = {i:v for i,v in self.offers.items() if i in indices}
        self.owners = {}
        busy = bytearray(32)
        paused = bytearray(32)
        established = bytearray(32)
        for row in rows:
            if row[3]:busy[row[0]//8] |= 1 << (row[0]%8)
            if row[38]:paused[row[0]//8] |= 1 << (row[0]%8)
        for row in rows:
            i=row[0]
            bootstrap = self.needs_bootstrap(row)
            if self.instances.get(i) != row[INSTANCE]:
                self.established.discard(i)
                self.offers.pop(i,None)
                self.instances[i] = row[INSTANCE]
            if row[RECEIPT] == APPLY_FAILED:
                self.established.discard(i)
            if self._acknowledged(row):
                self.established.add(i)
            controller = row[2] in (6,SHUTTER,BRIDGE,GATE64,DOLL) or (row[2]==2 and row[1] in (0x324,0x326,0x228,0x1fe,0x3d0))
            candidates = [(ctx['cid'], bool(row[3]), bool(row[38]), controller_progress(row) if controller else 0,
                           bootstrap and i in self.established)]
            for cid,p in self.peers.items():
                if bit(p['p'],i):
                    remote=p['rows'].get(i)
                    progress = controller_progress(remote) if controller and remote and remote[1:3]==row[1:3] else 0
                    candidates.append((cid,bit(p['b'],i),bit(p['z'],i),progress,
                                       bootstrap and bit(p['h'],i)))
            # These native continuations advance once per room visit. Prefer
            # the furthest checkpoint so an older lower-ID observer cannot
            # erase an activation during the ownership round trip.
            # Door busy is a real native travel action, not a rider standing
            # on a fresh platform. Preserve a traveller reopening a closing door.
            if row[2] == GATE64:
                # An active camera must finish its own local cleanup. Retain
                # its ownership across pause instead of starting a second
                # camera-free simulation that it cannot safely adopt.
                order=lambda p:(-p[1],-p[3],-p[4],p[2],p[0])
            elif row[2] == counterweight.KIND:
                order=lambda p:(-p[4],p[2],p[0])
            elif row[2] == BRIDGE:
                # Established native receipt outranks progress here, so a fresh
                # save-1 constructor pose on a lower-ID client cannot outrank a
                # live guard route or an already accepted dialogue.
                order=lambda p:(-p[4],-p[3],-p[1],p[2],p[0])
            else:
                order=lambda p:(-p[1] if row[2]==5 else 0,-p[3],
                    -p[1] if row[1]==0x3d0 else 0,-p[4],
                    p[2] if controller or not p[1] else False,-p[1],p[0])
            owner,_,_,_,_=min(candidates,key=order)
            self.owners[i] = owner
            if owner != ctx['cid']:
                delivered = self._native_offer(row,owner,now,bootstrap and i not in self.established)
                if delivered:
                    if row[2] in (CRANE,counterweight.KIND):
                        # The receipt key stays bound to the real authoritative
                        # row; the aggregate goes only onto this delivery copy.
                        delivered[DELIVERY_ROW+CRANE_INPUT] = row[CRANE_INPUT]
                        delivered[DELIVERY_ROW+CRANE_AGGREGATE] = self._pad_mask(i,row)
                    elif row[2] == BRIDGE:
                        delivered[DELIVERY_ROW+WB_INPUT] = row[WB_INPUT]
                        delivered[DELIVERY_ROW+WB_AGGREGATE] = self._bridge_mask(i,row)
                    output.append(delivered)
            else:
                if not waiting:
                    self.established.add(i)
                wire = list(row)
                wire[INSTANCE] = wire[RECEIPT] = 0
                if row[2] == counterweight.KIND:
                    wire[46] = wire[47] = 0
                    if not waiting:
                        echo = list(row)
                        echo[47] = self._pad_mask(i,row)
                        echo[RECEIPT] = 0
                        output.append([ctx['cid'],0]+echo)
                elif row[2] == CRANE:
                    wire[CRANE_INPUT] = wire[CRANE_AGGREGATE] = 0
                    # The owner also needs its own row back so remote pad input
                    # reaches the local actor; zero receipt claims no bootstrap.
                    echo = list(row)
                    echo[CRANE_AGGREGATE] = self._pad_mask(i,row)
                    echo[RECEIPT] = 0
                    output.append([ctx['cid'],0]+echo)
                elif row[2] == BRIDGE:
                    wire[WB_INPUT] = wire[WB_AGGREGATE] = 0
                    # Same shape as the crane: the owner echoes its own row with
                    # the aggregate latch, and a zero receipt claims nothing.
                    echo = list(row)
                    echo[WB_AGGREGATE] = self._bridge_mask(i,row)
                    echo[RECEIPT] = 0
                    output.append([ctx['cid'],0]+echo)
                elif row[2] == GATE64 and not waiting:
                    # Native hit handling waits for this confirmation before
                    # acquiring its local camera/control sequence.
                    echo = list(row)
                    echo[RECEIPT] = 0
                    output.append([ctx['cid'],0]+echo)
                elif row[2] == DOLL and not waiting:
                    # The container native adapter only starts its cycle once
                    # the transport confirms self ownership, so the local owner
                    # needs its own row back with a zero receipt.
                    echo = list(row)
                    echo[RECEIPT] = 0
                    output.append([ctx['cid'],0]+echo)
                publish.append(wire)
            if i in self.established:
                established[i//8] |= 1 << (i%8)
        # Door reversal/stop changes must arrive promptly even though the
        # remaining simulator is not a player interacting with the door.
        door_motion = lambda rs: {r[0]:r[29]&18 for r in rs if r[2]==5}
        npc_phase = lambda rs: {r[0]:r[39] for r in rs if r[2]==1}
        controller_phase = lambda rs: {r[0]:(r[18],r[21],r[23],r[24],r[33]) for r in rs
                                      if r[2]==6 or r[1] in (0x324,0x326,0x228,0x1fe,0x3d0)}
        crane_phase = lambda rs: {r[0]:(r[18],r[19],r[22],r[37],r[30],r[32]) for r in rs if r[2]==CRANE}
        shutter_phase = lambda rs: {r[0]:(r[18],r[20]) for r in rs if r[2]==SHUTTER}
        # Spike timers and clip frames use the normal checkpoint cadence;
        # wait/extend/retract transitions use the bounded edge cadence.
        spike_phase = lambda rs: {r[0]:r[20] for r in rs if r[2]==2 and r[1]==0x3ca}
        # Bridge edges: temp flags, gate phase, both guard phases and route PCs,
        # and the blocker. Input changes are covered by the latch comparison.
        bridge_phase = lambda rs: {r[0]:(r[WB_FLAGS],r[WB_GATE_PHASE],r[WB_GUARD_0]&3,
                                   r[WB_GUARD_1]&3,r[WB_GUARD_0+13],r[WB_GUARD_1+13],
                                   r[WB_BLOCKER_REMOVED])
                                  for r in rs if r[2]==BRIDGE}
        # Gate64 phase boundaries and the completion/body-slot switches are the
        # semantic edges. The native timer and the transforms ride the ordinary
        # cadence; the busy bitmap already has its own edge term below.
        gate64_phase = lambda rs: {r[0]:(r[WG64_PHASE],r[WG64_COMPLETE],r[10],r[24])
                                  for r in rs if r[2]==GATE64}
        # Doll container edges: the cycle/phase/spawn triple is the semantic
        # state; the pitch and the transforms ride the ordinary cadence.
        doll_phase = lambda rs: {r[0]:(r[DOLL_CYCLE],r[DOLL_PHASE],r[DOLL_SPAWNED])
                                for r in rs if r[2]==DOLL}
        edge=(presence.hex()!=self.last_presence or self.dead.hex()!=self.last_dead or
              busy.hex()!=self.last_busy or paused.hex()!=self.last_paused or established.hex()!=self.last_established or
              inputs_key(inputs)!=self.last_inputs or
              door_motion(publish)!=door_motion(self.last_rows) or
              npc_phase(publish)!=npc_phase(self.last_rows) or
              controller_phase(publish)!=controller_phase(self.last_rows) or
              crane_phase(publish)!=crane_phase(self.last_rows) or
              shutter_phase(publish)!=shutter_phase(self.last_rows) or
              spike_phase(publish)!=spike_phase(self.last_rows) or
              gate64_phase(publish)!=gate64_phase(self.last_rows) or
              doll_phase(publish)!=doll_phase(self.last_rows) or
              bridge_phase(publish)!=bridge_phase(self.last_rows))
        changed = tuple(tuple(r) for r in publish) != self.last_rows
        packets=[]
        # Edge bypass has its own hard 20 Hz floor; steady checkpoints are 5 Hz.
        if (now-self.last_attempt >= .05 and
                now-self.last_send >= (.05 if edge else INTERVAL if changed or self.force else 1.)):
            q=self.sequence+1
            chunks=[publish[i:i+ROWS_PER_PACKET] for i in range(0,len(publish),ROWS_PER_PACKET)] or [[]]
            for part,chunk in enumerate(chunks):
                packets.append({'type':PACKET_TYPE,'clientId':ctx['cid'],
                    'targetTeamId':ctx['team'],'quiet':True,'m':self.advertisement(ctx),
                    'q':q,'part':part,'parts':len(chunks),'p':presence.hex(),
                    'd':self.dead.hex(),'b':busy.hex(),'z':paused.hex(),'h':established.hex(),
                    'u':[list(entry) for entry in inputs],'a':chunk})
        return {'a':output,'d':self.dead.hex()},packets

    def sent(self, packets, success, now):
        if not packets:
            return
        # Even a partial batch consumed this sequence. Do not reuse it on retry.
        self.sequence=packets[0]['q']
        self.last_attempt=now
        if success:
            self.last_send=now
            self.last_presence=packets[0]['p']
            self.last_dead=packets[0]['d']
            self.last_busy=packets[0]['b']
            self.last_paused=packets[0]['z']
            self.last_established=packets[0]['h']
            self.last_rows=tuple(tuple(r) for p in packets for r in p['a'])
            self.last_inputs=inputs_key(packets[0]['u'])
            self.force=False
