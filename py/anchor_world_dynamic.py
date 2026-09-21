"""Live child actors: complete snapshots, late entry, handoff and pickup claims.

Uses WorldTransport's exact occupied-room/session checks. Packets are transient,
team routed, coalesced off the event FIFO, and never queued by Anchor. Origin
identity survives ownership changes. Claims are decided by the current owner;
only the winning client runs a native award callback.
"""
import json
import anchor_world as world
import anchor_world_slicer as slicer
import anchor_world_random as random_spawner
import anchor_world_bomb as bomb
import anchor_world_wave as wave

PACKET_TYPE = 'MNSG_WORLD_ACTORS'
WORDS = 83
MAX_ACTORS = 128
ROWS_PER_PACKET = 12
MAX_PARTS = 11
PACKET_BYTES = 8192
STATE_BYTES = 131072
MAX_DEAD = 4096
CID, SESSION, VISIT, SERIAL, OWNER, LIFE, KIND = range(7)
LIVE, CLAIM, REMOVED = range(3)
NPC = 1
# A placed shutter emits one-hit native enemies. They carry no health word, so
# a single claim decides the kill and word 44 records that it was lethal.
SHUTTER_ENEMY = 6
LANDED = 44
COMMITTER = 72
# The File62 container spawns the nested File_26 Doll: one typed falling/rest
# checkpoint per container. The room fixes which container may exist, so the
# parent index is part of the binding, and its canonical origin is the room
# plus that parent rather than a client id.
DOLL = 7
DOLL_ORIGIN = 0x7ffffffa
DOLL_PARENT = {0x16a: 8, 0x182: 3}
DOLL_FALLING, DOLL_REST = 16, 17
# Rooms 0xAB/0xAC slicer emitters and their flying blades (entity 0x19D). The
# kind and phase windows widen for it, and its own recipe pins both before the
# removal early return, so no other row becomes valid.
SLICER = slicer.KIND
SLICER_FLIGHT = slicer.FLIGHT
RANDOM = random_spawner.KIND
BOMB = bomb.KIND
WAVE = wave.KIND
FAMILY_RECIPES = {SLICER:slicer, RANDOM:random_spawner, BOMB:bomb, WAVE:wave}
FAMILY_ORIGINS = {origin:recipe for recipe in FAMILY_RECIPES.values()
                  for origin in (recipe.ROOT_ORIGIN, recipe.BLADE_ORIGIN, recipe.LOOT_ORIGIN)
                  if origin is not None}

# Kept in the same order as anchor_world_dynamic.h and the native codec's lo/hi
# arrays. The shared window widens for the slicer kind and phase, and every kind
# still pins its own recipe below, so no other row becomes valid.
BOUNDS = [(0,0x7fffffff)]*5 + [(0,2),(1,WAVE),(0,256),(0,1025),(0,1025),(0,255),(0,1)]
BOUNDS += [(-3276800,3276700)]*3 + [(0,1023)]*3
BOUNDS += [(0,1000000),(0,65535),(0,7)] + [(-100000,100000)]*3
BOUNDS += [(0,64000)]*3 + [(0,65535)]*4
BOUNDS += [(-32768,32767),(0,163),(-32768,32767),(0,7),(0,255)]
BOUNDS += [(-32768,32767)]*3 + [(0,255),(0,1),(0,65535),(0,wave.PURSUE2),(-100000,100000)]
BOUNDS += [(0,1),(-32768,32767),(0,1023),(0,65535),(0,65535),(-32768,32767)]
BOUNDS += [(0,65535),(0,65535),(-32768,32767),(0,255),(0,65535)]
BOUNDS += [(0,255)]*4 + [(0,100000)]*2 + [(-3276800,3276700)]*3
BOUNDS += [(0,0x7fffffff),(0,1),(0,1),(0,17)]
BOUNDS += [(0,255),(0,255),(0,65535),(-32768,32767)]
BOUNDS += [(0,0x7fffffff)]
BOUNDS += [(0,255)]
BOUNDS += [(0,22)] + [(-32768,32767)]*7 + [(0,6)]
assert len(BOUNDS) == WORDS
# The shared table keeps the NPC continuation's signed 16-bit state. A slicer
# row overrides only the columns it reuses, which is how the native codec reads
# the same union without widening it for everyone else.
SLICER_BOUNDS = slicer.bounds(BOUNDS)
FAMILY_BOUNDS = {kind:recipe.bounds(BOUNDS) for kind,recipe in FAMILY_RECIPES.items()}


def valid(row):
    if not isinstance(row,list) or len(row)!=WORDS:
        return False
    if not all(world.integer(v,lo,hi) for v,(lo,hi) in
               zip(row, FAMILY_BOUNDS.get(row[KIND],BOUNDS))):
        return False
    if (not row[SERIAL] or
            row[KIND] not in (2,3,4,SHUTTER_ENEMY,DOLL,*FAMILY_RECIPES) and row[LIFE] == CLAIM):
        return False
    if row[KIND] in FAMILY_RECIPES:
        return FAMILY_RECIPES[row[KIND]].valid(row)
    if not world.npc_valid(row[8],row[9],row[74:]) or row[6]!=NPC and row[74]:
        return False
    if row[KIND] == DOLL:
        # The nested File_26 Doll is pinned before the removal early return, so
        # a tombstone still carries the typed container identity: fixed entity
        # and model, clip 2, no animation, the one-based container parent, the
        # single spawn ordinal and the fixed birth X/Z. It falls in hundreds of
        # a unit and rests at 3400.
        if (row[8] or row[9] or row[10] != 2 or row[11] or
                not 1 <= row[7] <= 256 or row[64] != 1 or
                row[12] != -600 or row[14] != -10200 or
                any(row[i] for i in (21,22,23)) or
                any(row[i] for i in (40,41,65)) or row[32] != 163 or
                any(row[74:])):
            return False
        if row[42] == DOLL_FALLING:
            return 3500 <= row[13] <= 12500 and row[13] % 100 == 0
        if row[42] == DOLL_REST:
            return row[13] == 3400
        return False
    if row[LIFE] == REMOVED:
        return True
    phase = row[42]
    kind,model,clip,animated=row[6],row[9],row[10],row[11]
    if kind==SHUTTER_ENEMY:
        # A one-hit native child of a placed shutter: no health word, so the
        # recipe pins the only appearance it can have. The table already caps
        # the ordinal at INTMAX; the recipe only has to exclude zero. Base Y is
        # a boolean here and its set value carries native route status 0x400.
        return (row[8]==0xFC and model==0xFB and clip==0 and animated==1 and
                phase==15 and row[32]==57 and 1 <= row[7] <= 256 and
                row[64] >= 1 and not row[40] and not row[41] and not row[65] and
                0 <= row[43] <= 255 and row[45] in (0,1))
    if kind==1 and not ((model==0x8b and not animated) or (0x2bd<=model<=0x401 and animated)):
        return False
    if kind==2 and (model,clip,animated)!=(1,4,0):return False
    if kind==3 and (model,clip,animated)!=(1,3,0):return False
    if kind==4 and (model,clip,animated)!=(0x85,0,0):return False
    if kind==5:
        appearance=(0x191,0,1) if phase<=6 else (0x1a4,0,1) if phase<=8 else (0x1a4,3,1) if phase<=10 else (0x19b,0,1)
        if (model,clip,animated)!=appearance:return False
    return ((row[KIND] == 1 and phase == 0) or
            (row[KIND] == 2 and phase in (1,2,13)) or
            (row[KIND] == 3 and phase in (3,14)) or
            (row[KIND] == 4 and phase == 4) or
            (row[KIND] == 5 and 5 <= phase <= 12))


def producer(row):
    """Resolve a typed actor/drop, including invalid uses of reserved origins."""
    recipe=FAMILY_RECIPES.get(row[KIND]) or FAMILY_ORIGINS.get(row[0])
    if recipe is not None:
        return recipe
    if row[KIND] in (2,3,4) and row[45] in (2,3):
        return next((r for r in FAMILY_RECIPES.values() if row[8]==r.ENTITY),None)
    return None


def producer_scope(row, placed, wire=False):
    recipe=producer(row)
    if recipe is None:
        return True
    identity=recipe.identity(row,placed)
    if identity is None or (any(row[:3]) and row[:4]!=identity):
        return False
    return not (wire and row[KIND] in FAMILY_RECIPES and
                (row[slicer.INSTANCE] or row[slicer.RECEIPT]))


def key(row):
    return tuple(row[:4])


def npc_identity(row, placed):
    # Native scripts can instantiate the same NPC independently on both peers.
    # Its immutable birth descriptor, not task allocation order, identifies it.
    h=2166136261
    for v in [row[7],row[8],*row[61:65]]:
        for shift in (0,8,16,24):
            h=((h ^ ((v >> shift)&255))*16777619)&0xffffffff
    return [0x7fffffff,placed.signature,placed.room+1,(h&0x7fffffff) or 1]


def loot_identity(row, placed):
    # A placed parent can produce several same-kind items. Its per-kind birth
    # ordinal distinguishes them while coalescing simultaneous local copies.
    # Only the 16-bit placed window is packed in, so this stays injective.
    serial=((row[7]-1)*3 + row[KIND]-2)*65536 + row[64]+1
    return [0x7ffffffd,placed.signature,placed.room+1,serial]


def drop_identity(row, placed):
    # A robot drops at most one item, so its emitting shutter plus the robot's
    # emission ordinal identify it. That ordinal leaves the 16-bit placed
    # window, so this key uses its own sentinel, packs parent/kind/room into one
    # word and carries the raw ordinal: injective for every combination.
    return [0x7ffffffb,placed.signature,
            (row[7]<<12)|((row[KIND]-2)<<10)|(placed.room+1),row[64]]


def enemy_identity(row, placed):
    # A placed shutter emits the same enemy on both clients, so the key is
    # canonical and never carries a client id. The roster index keeps two
    # shutters apart and the emission ordinal keeps overlapping cycles apart.
    return [0x7ffffffc,placed.signature,(row[7]<<10)|(placed.room+1),row[64]]


def doll_identity(row, placed):
    # One container per room owns exactly one Doll slot, so the canonical origin
    # is the room visit plus that container's roster index. The cycle is not
    # part of the identity: the container reuses the same Doll every time.
    return [DOLL_ORIGIN,placed.signature,placed.room+1,row[7]]


class DynamicTransport:
    def __init__(self, placed):
        self.placed = placed
        self.reset()

    def reset(self):
        self.scope = None
        self.peers = {}
        self.pending = {}
        self.dead = {}
        self.dead_dirty = set()
        self.last = ()
        self.sequence = 0
        self.last_attempt = self.last_send = -100.
        self.force = False
        self.owners = {}
        self.claim_owners = {}
        # Slicer kill arbiters are stable per root family until the holder
        # leaves the roster, unlike the simulation owner which may hand off.
        self.arbiters = {}
        self.checkpoints = {}
        self.receipts = slicer.Tracker()

    def _prune(self, ctx, now):
        for cache in (self.peers,self.pending):
            for cid,p in list(cache.items()):
                if self.placed._peer(ctx,cid) != p['meta'] or now-p['time'] > world.TTL:
                    del cache[cid]

    def receive(self, ctx, packet, now):
        if not self.scope or self.scope != self.placed.scope or not ctx.get('connected'):
            return False
        cid = packet.get('clientId')
        if not world.integer(cid,1,0x7fffffff):
            return False
        meta = self.placed._peer(ctx,cid)
        q,part,parts = (packet.get(k) for k in ('q','part','parts'))
        rows = packet.get('a')
        if (packet.get('type') != PACKET_TYPE or not meta or packet.get('m') != meta or
                packet.get('targetTeamId') != ctx['team'] or 'targetClientId' in packet or
                packet.get('addToQueue',False) or not world.integer(q,1,0x7fffffff) or
                not world.integer(parts,1,MAX_PARTS) or not world.integer(part,0,parts-1) or
                not isinstance(rows,list) or len(rows)>ROWS_PER_PACKET or
                not all(valid(r) and all(r[:4]) and
                        not (r[KIND]==SHUTTER_ENEMY and self.placed.room != world.SHUTTER_ROOM)
                        and not (r[KIND]==DOLL and
                                 (DOLL_PARENT.get(self.placed.room) != r[7] or
                                  r[:4] != doll_identity(r,self.placed)))
                        and producer_scope(r,self.placed,wire=True)
                        for r in rows) or
                len(json.dumps(packet,separators=(',',':')).encode())+1>PACKET_BYTES):
            return False
        self._prune(ctx,now)
        old = self.peers.get(cid)
        pending = self.pending.get(cid)
        if old and q<=old['q'] or pending and q<pending['q']:
            return False
        if not pending or pending['q'] != q:
            if len(self.pending)>=32 and cid not in self.pending:
                return False
            pending=dict(q=q,meta=meta,time=now,parts=parts,chunks={})
            self.pending[cid]=pending
        if pending['parts']!=parts or part in pending['chunks']:
            return False
        pending['chunks'][part]=[list(r) for r in rows]
        if len(pending['chunks'])!=parts:
            return True
        combined=[r for i in range(parts) for r in pending['chunks'][i]]
        del self.pending[cid]
        if len(combined)>MAX_ACTORS or len({key(r) for r in combined})!=len(combined):
            return False
        if len(self.peers)>=32 and cid not in self.peers:
            return False
        self.force |= old is None or set(old['rows']) != {key(r) for r in combined}
        self.peers[cid]=dict(q=q,meta=meta,time=now,rows={key(r):r for r in combined})
        for row in combined:
            if row[KIND] in (2,3,4,SHUTTER_ENEMY,DOLL) and row[LIFE]!=REMOVED and row[OWNER]==cid:
                # The holder registers a claim lease so a silent peer cannot
                # hand the same reward, or the same kill, to a second client.
                self.claim_owners.setdefault(key(row),cid)
            if row[KIND] in FAMILY_RECIPES and row[LIFE]!=REMOVED and row[COMMITTER]:
                # A family has one kill arbiter even when its root and children
                # have different simulation owners. Carry that lease explicitly;
                # OWNER alone cannot identify it after a presence handoff.
                fam=producer(row).family(key(row))
                holder=self.arbiters.get(fam)
                if holder is None or row[COMMITTER]<holder:
                    self.arbiters[fam]=row[COMMITTER]
        return True

    def update(self, ctx, rows, now):
        if self.scope != self.placed.scope:
            self.reset()
            self.scope = self.placed.scope
        if not self.scope:
            return {'a':[], 'l':0}, []
        self._prune(ctx,now)
        eligible=[ctx['cid']] + [cid for cid in ctx['players'] if self.placed._peer(ctx,cid)]
        # The roster can announce a room entrant before its world signature.
        # That interval is not an empty room: wait for its complete snapshot
        # before bootstrapping a new emitter or assigning a new kill lease.
        members={ctx['cid']} | {cid for cid,p in ctx['players'].items()
            if p.get('online') and p.get('isSaveLoaded') and
            p.get('teamId')==ctx['team'] and p.get('roomId')==self.placed.room}
        leader=min(eligible)
        if (not isinstance(rows,list) or len(rows)>MAX_ACTORS or
                not all(valid(r) for r in rows) or len({key(r) for r in rows})!=len(rows) or
                (self.placed.room != world.SHUTTER_ROOM and
                 any(r[KIND]==SHUTTER_ENEMY for r in rows)) or
                any(r[KIND]==DOLL and
                    (DOLL_PARENT.get(self.placed.room) != r[7] or
                     # A bound Doll row must carry the canonical container
                     # origin, so a forged signature, visit, serial or a
                     # mixed-zero prefix rejects the whole snapshot instead of
                     # reconstructing a second Doll in this room. Unbound
                     # native rows are canonicalized below.
                     (any(r[:3]) and r[:4] != doll_identity(r,self.placed)))
                    for r in rows) or
                any(not producer_scope(r,self.placed) for r in rows)):
            return {'a':[], 'l':leader}, []
        local={}
        discarded=[]
        for supplied in rows:
            r=list(supplied)
            shutter_birth=False
            if not any(r[:3]):
                if r[KIND]==NPC:
                    r[:4]=npc_identity(r,self.placed)
                elif r[KIND]==DOLL and r[7]:
                    # One container per room spawns one Doll, so both clients
                    # coalesce onto the same canonical origin instead of
                    # advertising a second copy of the same descent.
                    r[:4]=doll_identity(r,self.placed)
                elif r[KIND]==SHUTTER_ENEMY and r[7] and r[64]:
                    # Both clients watch their local shutter emit, so the key is
                    # canonical: the two births coalesce into one enemy and the
                    # ordinal keeps overlapping emission cycles apart.
                    r[:4]=enemy_identity(r,self.placed)
                    shutter_birth=True
                    if r[LIFE]==REMOVED and not r[COMMITTER]:
                        # A route exit seen while this client was offline arrives
                        # unbound with no committer. This client is the one that
                        # can see it now, so it authorises the removal.
                        r[COMMITTER]=ctx['cid']
                elif producer(r) is not None:
                    r[:4]=producer(r).identity(r,self.placed)
                elif r[KIND] in (2,3,4) and r[7] and r[64]<65536:
                    # The placed item window is unchanged.
                    r[:4]=loot_identity(r,self.placed)
                elif r[KIND] in (2,3,4) and r[7] and r[8]==0xFC:
                    # A robot drops at most one item and its emission ordinal
                    # leaves the packed window, so the drop gets the extended
                    # key. This must precede the placed INTMAX branch or an
                    # INT_MAX ordinal would be read as a placed unique child.
                    r[:4]=drop_identity(r,self.placed)
                elif r[64]==0x7fffffff and r[7]:
                    r[:4]=[0x7ffffffe,self.placed.signature,self.placed.room+1,r[7]]
                else:
                    if r[KIND]==5:
                        # A late entrant may run a local emitter before the
                        # first placed snapshot arrives. Do not publish those
                        # speculative births as a second set of hazards.
                        others=[cid for cid in eligible if cid!=ctx['cid']]
                        if any(cid not in self.placed.peers for cid in others):
                            continue
                        parent_owner=self.placed.owners.get(r[7]-1,leader)
                        if parent_owner!=ctx['cid']:
                            r[:3]=[ctx['cid'],ctx['session'],self.placed.visit]
                            r[OWNER],r[LIFE],r[COMMITTER]=0,REMOVED,0
                            discarded.append(r)
                            continue
                    r[:3]=[ctx['cid'],ctx['session'],self.placed.visit]
            elif not all(r[:3]):
                continue
            if shutter_birth and r[LIFE]!=REMOVED:
                # Only the client that owns the placed shutter advertises the
                # birth. Everyone else coalesces onto that canonical key instead
                # of publishing a second copy of the same enemy. A removal is
                # still relayed: it is authoritative whoever saw it.
                others=[cid for cid in eligible if cid!=ctx['cid']]
                if any(cid not in self.placed.peers for cid in others):
                    continue
                if self.placed.owners.get(r[7]-1,leader)!=ctx['cid']:
                    continue
            if r[KIND] in FAMILY_RECIPES:
                # The native instance serial and its apply echo are bridge
                # words, never wire words, and a local row only becomes eligible
                # to own once the native side confirmed the apply.
                entry=self.receipts.observe(key(r),r[slicer.INSTANCE],r[slicer.RECEIPT])
                r[slicer.INSTANCE]=r[slicer.RECEIPT]=0
                r[slicer.ESTABLISHED]=1 if entry['established'] else 0
            local[key(r)]=r
        sources={ctx['cid']:local, **{cid:p['rows'] for cid,p in self.peers.items()}}
        keys=set().union(*(set(a) for a in sources.values()))
        keys.update(self.dead_dirty)
        retained=keys | self.dead.keys()
        for cache in (self.claim_owners,self.checkpoints,self.receipts.entries):
            for stale in cache.keys()-retained:
                del cache[stale]
        # A fresh slicer may only bootstrap once every other eligible client has
        # supplied a complete snapshot. An empty snapshot is a legitimate
        # absence, so waiting on a row would deadlock the room.
        ready=all(self.placed._peer(ctx,cid) and cid in self.peers
                  for cid in members if cid!=ctx['cid'])
        result=[]
        publish=[]
        owners={}
        for k in sorted(keys):
            candidates={cid:a[k] for cid,a in sources.items() if k in a and a[k][LIFE]!=REMOVED}
            previous=self.owners.get(k)
            # A deletion remains authoritative even though its actor no longer
            # contributes presence. Initial late-entry snapshots carry the owner.
            deaths=[(cid,a[k]) for cid,a in sources.items() if k in a and a[k][LIFE]==REMOVED
                    and (cid == previous or cid == a[k][COMMITTER] or
                         previous is None and cid == k[0] or
                          # A peer holding a committed removal may relay it to
                          # a late entrant after the original arbiter has left.
                          # Require the same room/session eligibility as live
                          # checkpoints; the server does not authenticate rows.
                          a[k][COMMITTER] and a[k][COMMITTER]!=ctx['cid'] and
                          self.placed._peer(ctx,a[k][COMMITTER]) is None)]
            if k not in self.dead and deaths:
                source,removed=min(deaths,key=lambda x:x[0])
                removed=list(removed)
                if not removed[COMMITTER]:removed[COMMITTER]=source
                self._dead(k,removed,source==ctx['cid'])
            if k in self.dead:
                result.append(self.dead[k])
                # Repeat a removal while any client still has the actor/claim.
                # New entrants also receive occupied-room deletion checkpoints.
                if k in local or candidates or k in self.dead_dirty:
                    publish.append(self.dead[k])
                continue
            if not candidates:
                continue
            # Prefer a native conversation, then an unpaused peer, then the
            # original simulator while present. The key never changes on handoff.
            # A canonical key has no client id in its origin slot. Only the
            # shutter enemy opts into holder stickiness, so a fresh lower-id copy
            # cannot rewind its established simulator; NPC and loot keys keep the
            # plain client-id tie break.
            if any(c[KIND] in FAMILY_RECIPES for c in candidates.values()):
                # A slicer is owned by an established, unpaused, present client;
                # the incumbent keeps it across a handoff, and a fresh lower id
                # yields to an established remote. With nobody established the
                # lowest present client bootstraps, but only once the room is
                # complete. Anything else holds the last checkpoint.
                live=[cid for cid,c in candidates.items()
                      if c[slicer.PRESENT] and not c[66] and c[slicer.ESTABLISHED]]
                present=[cid for cid,c in candidates.items()
                         if c[slicer.PRESENT] and not c[66]]
                fresh=not any(c[slicer.ESTABLISHED] for c in candidates.values())
                sample=min(candidates.items(),key=lambda x:(
                    not x[1][slicer.ESTABLISHED],not x[1][slicer.PRESENT],x[0]))[1]
                fam=producer(sample).family(k)
                holder=self.arbiters.get(fam)
                if holder not in members:
                    holder=None
                advertised={c[COMMITTER] for c in candidates.values()
                            if c[COMMITTER] in members}
                if advertised:
                    holder=min(advertised | ({holder} if holder else set()))
                if holder is None and ready:
                    holder=min(members)
                if holder is not None:
                    self.arbiters[fam]=holder
                claims=[cid for cid,c in candidates.items() if c[LIFE]==CLAIM]
                if claims and holder==ctx['cid']:
                    # A culled or paused claimant still owns a valid hit. Commit
                    # before simulation-presence selection, which can hold when
                    # every native copy has disappeared.
                    committed=list(candidates[min(claims)])
                    committed[LIFE]=REMOVED
                    committed[OWNER]=min(claims)
                    committed[COMMITTER]=holder
                    committed[LANDED]=1
                    if self._dead(k,committed,True):
                        result.append(committed)
                        publish.append(committed)
                        owners[k]=holder
                        continue
                # Incumbency is advertised, not private transport history. A
                # newly established lower-id replica must not independently
                # preempt an owner which still advertises a live incarnation.
                incumbents={c[OWNER] for c in candidates.values() if c[OWNER] in live}
                if live:
                    owner=min(incumbents or live)
                elif fresh and present and ready:
                    owner=min(present)
                else:
                    # Nobody may own this row yet. Hold the last checkpoint, or
                    # the retained row itself when this client has not emitted
                    # one, so a paused or culled actor is never dropped. The
                    # local presence is still advertised, unestablished when
                    # that is the truth, so the room can settle instead of
                    # deadlocking on a row nobody publishes.
                    held=self.checkpoints.get(k)
                    if held is None:
                        held=list(min(candidates.items(),key=lambda x:(
                            not x[1][slicer.ESTABLISHED],not x[1][slicer.PRESENT],
                            x[1][66],x[0]))[1])
                    held=list(held)
                    held[OWNER]=0
                    held[COMMITTER]=holder or 0
                    held[66]=1
                    result.append(held)
                    owners[k]=0
                    if k in local:
                        outgoing=list(local[k])
                        outgoing[COMMITTER]=holder or 0
                        outgoing[slicer.INSTANCE]=outgoing[slicer.RECEIPT]=0
                        outgoing[slicer.ESTABLISHED]=(
                            1 if self.receipts.entry(k)['established'] else 0)
                        publish.append(outgoing)
                    continue
                sample=candidates[owner]
            else:
                anchor=k[0]
                if k[0]>=0x7ffffffc and any(candidates[cid][KIND]==SHUTTER_ENEMY
                                            for cid in candidates):
                    anchor=previous
                owner=min(candidates,key=lambda cid:(not candidates[cid][65],
                    bool(candidates[cid][66]) and not candidates[cid][65],cid!=anchor,cid))
                sample=candidates[owner]
            if sample[KIND] in (2,3,4,SHUTTER_ENEMY,DOLL):
                # A silent TCP peer is not proof that it relinquished the
                # pickup. Preserve the award arbiter until roster metadata says
                # it left; TTL-based dual ownership could grant the same item
                # twice during a transient stall.
                lease=self.claim_owners.get(k,k[0])
                eligible=(lease==ctx['cid'] or self.placed._peer(ctx,lease) is not None)
                if not eligible:
                    lease=owner
                self.claim_owners[k]=lease
                owner=lease
            owners[k]=owner
            canonical=list(candidates.get(owner,sample))
            if sample[KIND] in FAMILY_RECIPES and sample[slicer.ROLE]==0:
                # Keep the phase and timer paired with the emission counter.
                # Merging only the counter into an older phase can emit twice.
                newest=max(c[64] for c in candidates.values())
                if canonical[64]<newest:
                    canonical=list(min((cid,c) for cid,c in candidates.items()
                                       if c[64]==newest)[1])
            if sample[KIND] in FAMILY_RECIPES:
                canonical[COMMITTER]=holder or 0
            if sample[KIND]==WAVE and not sample[wave.ROLE]:
                canonical[wave.STOP]=max(row[wave.STOP] for row in candidates.values())
            if sample[KIND]==BOMB and not sample[bomb.ROLE]:
                canonical[45]=max(c[45] for c in candidates.values())
            if sample[KIND]==DOLL:
                # A Doll keeps its furthest descent: a fresh local birth on a
                # lower client id must not rewind a checkpoint whose Y has
                # already fallen, and a settled Y stays settled. The phase must
                # follow the chosen Y or the row would describe a resting Y at
                # the falling phase.
                deepest=min(c[13] for c in candidates.values())
                canonical[13]=deepest
                canonical[42]=DOLL_REST if deepest==3400 else DOLL_FALLING
            canonical[OWNER]=owner
            claims=[cid for cid,r in candidates.items() if r[LIFE]==CLAIM]
            if claims and owner==ctx['cid'] and sample[KIND] not in FAMILY_RECIPES:
                committed=list(canonical)
                committed[LIFE]=REMOVED
                committed[OWNER]=min(claims)
                committed[COMMITTER]=ctx['cid']
                if committed[KIND] in (SHUTTER_ENEMY,*FAMILY_RECIPES):
                    # Only a committed claim is a kill. The native side arms its
                    # fast death path from this flag, so a route exit relayed
                    # with the flag clear stays non-lethal.
                    committed[LANDED]=1
                if self._dead(k,committed,True):
                    canonical=committed
                else:
                    canonical[LIFE]=LIVE
            else:
                canonical[LIFE]=LIVE
            result.append(canonical)
            if k in local:
                outgoing=list(local[k])
                outgoing[OWNER]=owner
                if k in self.dead:
                    outgoing=list(self.dead[k])
                if outgoing[KIND] in FAMILY_RECIPES:
                    outgoing[COMMITTER]=holder or 0
                    # The wire never carries the instance or its receipt, and
                    # presence is advertised even before the local apply so a
                    # fresh room can settle instead of deadlocking.
                    outgoing[slicer.INSTANCE]=outgoing[slicer.RECEIPT]=0
                    outgoing[slicer.ESTABLISHED]=1 if self.receipts.entry(k)['established'] else 0
                publish.append(outgoing)
        # Even a held checkpoint needs a current-instance apply offer. Without
        # it, a reloaded root cannot become established while the old holders
        # are culled/paused, and the room would wait forever for its own echo.
        for index, supplied in enumerate(result):
            if supplied[KIND] not in FAMILY_RECIPES or supplied[LIFE] == REMOVED:
                continue
            k=key(supplied)
            wire=list(supplied)
            wire[slicer.INSTANCE]=wire[slicer.RECEIPT]=0
            entry=self.receipts.entry(k)
            wire[slicer.ESTABLISHED]=int(entry['established'])
            self.checkpoints[k]=wire
            delivered=list(wire)
            delivered[slicer.INSTANCE]=entry['instance']
            delivered[slicer.RECEIPT]=self.receipts.offer(k,entry['instance'])
            result[index]=delivered
        result.sort(key=key)
        self.owners=owners
        # Bounded live set. Refuse a partial snapshot instead of accidentally
        # declaring omitted live actors dead at the receiver.
        if len(result)>MAX_ACTORS or len(publish)>MAX_ACTORS:
            return {'a':[], 'l':leader}, []
        snapshot=tuple(tuple(r) for r in publish)
        def edge_key(r):
            # A slicer's established/present pair is part of its checkpoint, so
            # a fresh room sees it settle instead of waiting on the presence
            # cadence alone.
            return (r[:4]+(r[OWNER],r[LIFE],r[65],r[66],
                    r[74] if r[OWNER]==ctx['cid'] else 0) +
                    ((r[slicer.ESTABLISHED],r[slicer.PRESENT]) if r[KIND] in FAMILY_RECIPES else ()))
        edge={edge_key(r) for r in snapshot}!={edge_key(r) for r in self.last}
        # Replicas advertise presence at 1 Hz, not every predicted transform.
        # An owner, interaction, claim or handoff still publishes immediately
        # at its normal bounded cadence. Full presence rows retain a usable
        # continuation if the previous simulator disconnects.
        def hot(rows):
            return tuple(r if r[OWNER]==ctx['cid'] or r[65] or r[LIFE]!=LIVE
                         else r[:12]+(r[65],r[66]) for r in rows)
        changed=hot(snapshot)!=hot(self.last)
        packets=[]
        if (now-self.last_attempt>=.05 and
                now-self.last_send >= (.05 if edge else .2 if changed or self.force else 1.)):
            chunks=[publish[i:i+ROWS_PER_PACKET] for i in range(0,len(publish),ROWS_PER_PACKET)] or [[]]
            packets=[{'type':PACKET_TYPE,'clientId':ctx['cid'],'targetTeamId':ctx['team'],
                'quiet':True,'m':self.placed.advertisement(ctx),'q':self.sequence+1,
                'part':i,'parts':len(chunks),'a':chunk} for i,chunk in enumerate(chunks)]
        return {'a':(discarded+result)[:MAX_ACTORS],'l':leader},packets

    def _dead(self,k,row,local=False):
        if len(self.dead)<MAX_DEAD or k in self.dead:
            self.dead[k]=list(row)
            if local:
                self.dead_dirty.add(k)
            return True
        return False

    def native_result(self,result):
        # Native awards/deallocation wait until a locally decided commit made
        # it onto the TCP connection. A throttle or failed write must not erase
        # the native record that is keeping its retry alive.
        return {**result,'a':[r for r in result['a']
            if r[LIFE]!=REMOVED or key(r) not in self.dead_dirty]}

    def sent(self,packets,success,now):
        if not packets:
            return
        self.sequence=packets[0]['q']
        self.last_attempt=now
        if success:
            self.last_send=now
            self.last=tuple(tuple(r) for p in packets for r in p['a'])
            self.force=False
            self.dead_dirty.difference_update(key(r) for p in packets for r in p['a'] if r[LIFE]==REMOVED)
