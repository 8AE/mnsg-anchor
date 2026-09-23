"""Room-scoped snapshots for the visible actors of scripted quest scenes.

Local dialogue, camera and temporary-flag scripts remain native. This bridge
chooses one complete visible graph per family and offers its scalar rows to the
native render-only proxy adapter. Pointer-sized words never enter a packet.
"""
import json
import anchor_world as world

PACKET_TYPE = 'MNSG_WORLD_QUEST'
ABI = 2
WORDS = 64
MAX_ROWS = 32
ROWS_PER_PACKET = 8
MAX_PARTS = 4
PACKET_BYTES = 8192
STATE_BYTES = 98304

FAMILY, ROLE, SELF, OWNER, SERIAL, LIFE = 1, 2, 3, 4, 5, 6
PARENT, ORDINAL, ENTITY, MODEL, SLOT, PART = 8, 9, 10, 11, 12, 13
INSTANCE, RECEIPT = 62, 63
LIVE, CLAIM, REMOVED = 0, 1, 2
FAMILY_ROOM = {1:(0x153,), 2:(0x155,), 3:(0x16a,0x182),
               4:(0xc1,), 5:(0xc1,)}
FAMILY_PARENT = {(1,0x153):5,(2,0x155):5,(3,0x16a):7,
                 (3,0x182):2,(4,0xc1):4,(5,0xc1):5}
ROLE_FAMILY = {**dict.fromkeys(range(1,6),1),
               **dict.fromkeys(range(6,8),2),
               **dict.fromkeys(range(8,11),3),
               **dict.fromkeys(range(11,13),4),
               **dict.fromkeys(range(13,15),5)}
ROLE_MODEL = {1:0x31b,2:0x24e,3:0x24e,4:0x33f,5:0x24e,
              6:0x1b0,7:0x1b0,8:0x315,9:0x10d,10:1}
FAMILY_ENTITY = {1:0x316,2:0x1b0,3:0x315,4:0x35c,5:0x35d}


def role_recipe(role, part):
    if role in ROLE_MODEL and (part == 0 or role == 7 and 1 <= part <= 11):
        slot = {2:1,3:2,5:2}.get(role,0)
        if role == 7:
            slot = 0 if part == 5 else 3 if part <= 7 else part - 4
        clip = 4 if role == 1 else 2 if role == 7 and part == 5 else 0
        return ROLE_MODEL[role],slot,clip
    if role == 12 and 0 <= part < 22:
        model = (0x36a if part <= 1 else 0x351 if part == 2 else
                 0x318 if part == 3 else 0x352 if part == 4 else
                 0x353 if part == 5 else 0xfb if 6 <= part <= 13 else
                 0x359 if part == 17 else
                 0x35a if part == 18 else 0x35b if part == 19 else 0x367)
        slot = {16:1,20:3,21:2}.get(part,0)
        clip = {0:10,1:11,2:5,3:3,4:2,5:2}.get(part,0)
        return model,slot,clip
    if role == 14 and 0 <= part < 3:
        return (0x2da,0x32c,0x2d6)[part],0,(5,6,2)[part]
    # The GMC roots are local camera/dialogue controllers, not visual rows.
    return None


def key(row):
    return (row[FAMILY],row[ROLE],row[PARENT],row[ORDINAL],row[PART])


def family_key(row):
    return (row[FAMILY],row[PARENT])


def valid(row, room):
    if (not isinstance(row,list) or len(row)!=WORDS or
            any(type(v) is not int or v < -0x80000000 or v > 0x7fffffff
                for v in row)):
        return False
    family, role = row[FAMILY], row[ROLE]
    if (row[0]!=ABI or role not in ROLE_FAMILY or
            ROLE_FAMILY[role]!=family or room not in FAMILY_ROOM[family] or
            row[PARENT]!=FAMILY_PARENT.get((family,room)) or
            not 1<=row[ORDINAL]<=255 or not 1<=row[SERIAL]<=0x7fffffff or
            not 0<=row[SELF]<=0x7fffffff or not 0<=row[OWNER]<=0x7fffffff or
            row[LIFE] not in (LIVE,REMOVED) or row[7] or row[31] or
            row[ENTITY]!=(0x1b4 if role==7 else 0 if role==12 else
                           FAMILY_ENTITY[family]) or
            not 0<=row[MODEL]<=1025 or
            not 0<=row[SLOT]<=7 or not 0<=row[PART]<=31 or
            not 0<=row[14]<=255 or
            not -32768<=row[15]<=32767 or not 0<=row[16]<=255 or
            any(not -3276800<=v<=3276700 for v in row[17:20]) or
            any(not 0<=v<=1023 for v in row[20:23]) or
            any(not 0<=v<=64000 for v in row[23:26]) or
            not 0<=row[26]<=15 or not 0<=row[27]<=65535 or
            not 0<=row[28]<=1000000 or not 0<=row[29]<=7 or
            not 0<=row[30]<=255 or
            any(not 0<=v<=65535 for v in row[32:36]) or
            not 0<=row[36]<=3 or not 0<=row[37]<=65535 or
            row[38] not in (0,1) or not 0<=row[39]<=255 or
            any(not 0<=v<=255 for v in row[40:44]) or
            row[44] not in (0,1) or any(row[45:62]) or
            not 0<=row[INSTANCE]<=0x7fffffff or
            not 0<=row[RECEIPT]<=0x7fffffff):
        return False
    recipe=role_recipe(role,row[PART])
    if (recipe is None or
            (row[MODEL],row[SLOT])!=recipe[:2] or row[26]>recipe[2]):
        return False
    if role!=5 and (row[16] or row[37] or row[38]):
        return False
    if role==5 and row[37] not in (0,0x2d41):
        return False
    if row[44] and role!=10:
        return False
    if not row[44] and any(row[40:44]):
        return False
    return True


class QuestTransport:
    def __init__(self, placed):
        self.placed=placed
        self.next_receipt=0
        self.reset()

    def reset(self):
        self.scope=None
        self.peers={}
        self.pending={}
        self.owners={}
        self.offers={}
        self.sequence=0
        self.last=()
        self.last_send=self.last_attempt=-100.
        self.force=True

    def _prune(self,ctx,now):
        for cache in (self.peers,self.pending):
            for cid,p in list(cache.items()):
                if self.placed._peer(ctx,cid)!=p['meta'] or now-p['time']>world.TTL:
                    del cache[cid]

    def receive(self,ctx,packet,now):
        if (not self.scope or self.scope!=self.placed.scope or
                not ctx.get('connected') or not ctx.get('loaded') or
                not isinstance(packet,dict) or packet.get('type')!=PACKET_TYPE):
            return False
        cid=packet.get('clientId')
        if not world.integer(cid,1,0x7fffffff):
            return False
        meta=self.placed._peer(ctx,cid)
        q,part,parts=(packet.get(k) for k in ('q','part','parts'))
        rows=packet.get('a')
        if (not meta or packet.get('m')!=meta or
                packet.get('targetTeamId')!=ctx['team'] or
                'targetClientId' in packet or packet.get('addToQueue',False) or
                not world.integer(q,1,0x7fffffff) or
                not world.integer(parts,1,MAX_PARTS) or
                not world.integer(part,0,parts-1) or
                not isinstance(rows,list) or len(rows)>ROWS_PER_PACKET or
                len(json.dumps(packet,separators=(',',':')).encode())+1>PACKET_BYTES or
                any(not valid(r,self.placed.room) or r[SELF]!=cid or
                    r[INSTANCE] or r[RECEIPT] for r in rows)):
            return False
        self._prune(ctx,now)
        previous=self.peers.get(cid)
        pending=self.pending.get(cid)
        if previous and q<=previous['q'] or pending and q<pending['q']:
            return False
        if not pending or pending['q']!=q:
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
        if len(combined)>MAX_ROWS or len({key(r) for r in combined})!=len(combined):
            return False
        if len(self.peers)>=32 and cid not in self.peers:
            return False
        self.force |= previous is None or set(previous['rows'])!={key(r) for r in combined}
        self.peers[cid]=dict(q=q,meta=meta,time=now,
                             rows={key(r):r for r in combined})
        return True

    def _offer(self,key_,source,local):
        instance=local[INSTANCE] if local else 0
        signature=(source[SELF],source[SERIAL],source[LIFE],instance)
        entry=self.offers.get(key_)
        if (not entry or entry['signature'][:3]!=signature[:3] or
                instance and entry['signature'][3] not in (0,instance)):
            if self.next_receipt>=0x7fffffff:
                return None
            self.next_receipt+=1
            entry=dict(signature=signature,receipt=self.next_receipt)
            self.offers[key_]=entry
        elif instance and not entry['signature'][3]:
            entry['signature']=signature
        row=list(source)
        row[INSTANCE]=instance
        row[RECEIPT]=entry['receipt']
        return row

    def update(self,ctx,sources,status,now):
        if self.scope!=self.placed.scope:
            self.reset();self.scope=self.placed.scope
        if not self.scope:
            return {'a':[],'ready':False,'l':0},[]
        self._prune(ctx,now)
        leader=min([ctx['cid']]+[cid for cid in ctx['players']
                                 if self.placed._peer(ctx,cid)])
        if (not isinstance(sources,list) or not isinstance(status,list) or
                len(sources)>MAX_ROWS or len(status)>MAX_ROWS):
            return {'a':[],'ready':False,'l':leader},[]
        local={};local_status={}
        for supplied in sources:
            if not isinstance(supplied,list) or len(supplied)!=WORDS:
                return {'a':[],'ready':False,'l':leader},[]
            row=list(supplied);row[SELF]=ctx['cid']
            if (not valid(row,self.placed.room) or row[INSTANCE]<=0 or
                    row[RECEIPT] or key(row) in local):
                return {'a':[],'ready':False,'l':leader},[]
            local[key(row)]=row
        for supplied in status:
            if not isinstance(supplied,list) or len(supplied)!=WORDS:
                return {'a':[],'ready':False,'l':leader},[]
            row=list(supplied);row[SELF]=ctx['cid']
            if (not valid(row,self.placed.room) or row[INSTANCE]<=0 or
                    key(row) in local_status):
                return {'a':[],'ready':False,'l':leader},[]
            local_status[key(row)]=row
        members={ctx['cid']} | {cid for cid,p in ctx['players'].items()
            if p.get('online') and p.get('isSaveLoaded') and
            p.get('teamId')==ctx['team'] and p.get('roomId')==self.placed.room}
        ready=all(self.placed._peer(ctx,cid) and cid in self.peers
                  for cid in members if cid!=ctx['cid'])
        sources_by_client={ctx['cid']:local,
                           **{cid:p['rows'] for cid,p in self.peers.items()}}
        families=set(family_key(r) for rows in sources_by_client.values()
                     for r in rows.values())
        selected=[]
        new_owners={}
        for family in sorted(families):
            candidates={cid for cid,rows in sources_by_client.items()
                        if any(family_key(r)==family for r in rows.values())}
            live={cid for cid in candidates if any(
                family_key(r)==family and r[LIFE]==LIVE
                for r in sources_by_client[cid].values())}
            # Every peer must elect from the same observed candidate set.
            # A retained local incumbent would split the room if both peers
            # started a family after their last exchanged empty snapshot.
            # A retired graph must not blank a still-staged graph on another
            # client. Once all copies retire, a tombstone remains selectable.
            owner=min(live or candidates)
            new_owners[family]=owner
            selected.extend(list(r) for r in sources_by_client[owner].values()
                            if family_key(r)==family)
        if len(selected)>MAX_ROWS:
            return {'a':[],'ready':False,'l':leader},[]
        selected.sort(key=key)
        result=[]
        if ready:
            selected_keys=set()
            for source in selected:
                source[OWNER]=new_owners[family_key(source)]
                k=key(source)
                selected_keys.add(k)
                # An original scene actor is never a render proxy. Only the
                # adapter's status rows may bind a previously offered proxy.
                offered=self._offer(k,source,local_status.get(k))
                if offered is None:
                    return {'a':[],'ready':False,'l':leader},[]
                result.append(offered)
            self.offers={k:v for k,v in self.offers.items() if k in selected_keys}
            self.owners=new_owners
        publish=[]
        for r in local.values():
            outgoing=list(r)
            outgoing[OWNER]=new_owners.get(family_key(r),0) if ready else 0
            outgoing[INSTANCE]=outgoing[RECEIPT]=0
            publish.append(outgoing)
        publish.sort(key=key)
        snapshot=tuple(tuple(r) for r in publish)
        edge=lambda rows:tuple((r[FAMILY],r[ROLE],r[PARENT],r[ORDINAL],r[PART],
                                r[OWNER],r[LIFE]) for r in rows)
        changed=snapshot!=self.last
        fast=edge(snapshot)!=edge(self.last)
        packets=[]
        if (now-self.last_attempt>=.05 and
                now-self.last_send>=(.05 if fast else .2 if changed or self.force else 1.)):
            chunks=[publish[i:i+ROWS_PER_PACKET]
                    for i in range(0,len(publish),ROWS_PER_PACKET)] or [[]]
            packets=[{'type':PACKET_TYPE,'clientId':ctx['cid'],
                      'targetTeamId':ctx['team'],'quiet':True,
                      'm':self.placed.advertisement(ctx),'q':self.sequence+1,
                      'part':i,'parts':len(chunks),'a':chunk}
                     for i,chunk in enumerate(chunks)]
            if any(len(json.dumps(p,separators=(',',':')).encode())+1>PACKET_BYTES
                   for p in packets):
                packets=[]
        return {'a':result,'ready':ready,'l':leader},packets

    def sent(self,packets,success,now):
        if not packets:
            return
        self.sequence=packets[0]['q']
        self.last_attempt=now
        if success:
            self.last_send=now
            self.last=tuple(tuple(r) for p in packets for r in p['a'])
            self.force=False
