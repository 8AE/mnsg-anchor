"""Occupied-room NPC/platform checkpoints. No pointers or offline event queue.

Presence is separate from simulation ownership: proximity culling on one client
must not stop an actor still loaded by another. Complete, bounded batches replace
peer state atomically. Call under anchor_mnsg's player-state lock.
"""
import json

PACKET_TYPE = 'MNSG_WORLD'
METADATA_KEY = 'worldSync'
VERSION = 3
WORDS = 39
MAX_ACTORS = 256
ROWS_PER_PACKET = 24
MAX_PARTS = 11
PACKET_BYTES = 8192
STATE_BYTES = 131072
INTERVAL = .2
TTL = 1.5


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


def row_valid(row):
    if (not isinstance(row, list) or len(row) != WORDS or
            not all(integer(v, -0x80000000, 0x7fffffff) for v in row)):
        return False
    bounds = [(0,255),(1,0x7ff),(1,5),(0,1)] + [(-3276800,3276700)]*3
    bounds += [(0,1023)]*3 + [(0,255),(0,1000000),(0,65535),(0,7)]
    bounds += [(-100000,100000)]*3 + [(-32768,32767),(0,0x7fffffff)]
    bounds += [(-32768,32767)]*7 + [(0,64000)]*3 + [(0,31)]
    bounds += [(0,163),(-32768,32767),(0,7),(0,255)]
    bounds += [(-32768,32767)]*3 + [(0,255),(0,1)]
    return all(lo <= v <= hi for v, (lo, hi) in zip(row, bounds))


class WorldTransport:
    def __init__(self):
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
        self.last_rows = ()
        self.force = False

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
        rows = packet.get('a')
        if (not integer(q,1,0x7fffffff) or not integer(parts,1,MAX_PARTS) or
                not integer(part,0,parts-1) or presence is None or dead is None or busy is None or
                any(b & ~p for b,p in zip(busy,presence)) or
                not isinstance(rows,list) or len(rows)>ROWS_PER_PACKET or
                not all(row_valid(r) and bit(presence,r[0]) for r in rows) or
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
            pending = dict(q=q,meta=meta,time=now,parts=parts,p=presence,d=dead,b=busy,chunks={})
            self.pending[cid] = pending
        if (pending['parts'] != parts or pending['p'] != presence or
                pending['d'] != dead or pending['b'] != busy or part in pending['chunks']):
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
        self.peers[cid] = dict(q=q,meta=meta,time=now,p=presence,b=busy,
                              rows=retained)
        for i in range(32):
            self.dead[i] |= dead[i]
        del self.pending[cid]
        return True

    def update(self, ctx, room, signature, visit, rows, removed, now):
        active = (ctx.get('connected') and ctx.get('loaded') and ctx.get('cid',0)>0
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
                len({r[0] for r in rows}) != len(rows)):
            return {'a':[], 'd':self.dead.hex()}, []
        removed = bitmap(removed)
        if removed is None:
            return {'a':[], 'd':self.dead.hex()}, []
        for i in range(32):
            self.dead[i] |= removed[i]
        self._prune(ctx,now)
        presence=bytearray(32)
        for row in rows:
            presence[row[0]//8] |= 1 << (row[0]%8)
        output, publish = [], []
        self.owners = {}
        busy = bytearray(32)
        for row in rows:
            if row[3]:busy[row[0]//8] |= 1 << (row[0]%8)
        for row in rows:
            i=row[0]
            candidates = [(ctx['cid'], bool(row[3]), bool(row[38]))]
            for cid,p in self.peers.items():
                if bit(p['p'],i):
                    remote=p['rows'].get(i)
                    candidates.append((cid,bit(p['b'],i),bool(remote and remote[38])))
            owner,_,_=min(candidates,key=lambda p:(p[2] if not p[1] else False,-p[1],p[0]))
            self.owners[i] = owner
            if owner != ctx['cid']:
                peer=self.peers[owner]
                state=peer['rows'].get(i)
                if state and state[1:3]==row[1:3]:
                    output.append([owner, int(max(0,now-peer['time'])*1000)] + state)
            else:
                publish.append(row)
        # Door reversal/stop changes must arrive promptly even though the
        # remaining simulator is not a player interacting with the door.
        door_motion = lambda rs: {r[0]:r[29]&18 for r in rs if r[2]==5}
        edge=(presence.hex()!=self.last_presence or self.dead.hex()!=self.last_dead or
              busy.hex()!=self.last_busy or door_motion(publish)!=door_motion(self.last_rows))
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
                    'd':self.dead.hex(),'b':busy.hex(),'a':chunk})
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
            self.last_rows=tuple(tuple(r) for p in packets for r in p['a'])
            self.force=False
