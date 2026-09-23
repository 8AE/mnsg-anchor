/* File30 placed one-HP objects. Only the common native hit/death path changes
 * shared state; native tint, spin, scroll and support callbacks stay local. */
static int fragile_native_ok(unsigned int entity,unsigned int role,void *callback) {
  unsigned long cb=(unsigned long)callback&~DDISABLED;
  if (role) return entity==0x331 ? cb==(unsigned long)func_08004AB4_6C4204 :
                    entity==0x332 && cb==(unsigned long)func_0800676C_6C5EBC;
  switch (entity) {
    case 0x196: return cb==(unsigned long)func_0800028C_6BF9DC;
    case 0x330: return cb==(unsigned long)func_08004CEC_6C443C ||
                       cb==(unsigned long)func_08004D64_6C44B4;
    case 0x331: return cb==(unsigned long)func_08004AA0_6C41F0;
    case 0x332: return cb==(unsigned long)func_0800664C_6C5D9C;
    case 0x339: return cb==(unsigned long)func_08004AE8_6C4238;
    case 0x3ec: return cb==(unsigned long)func_08007984_6C70D4;
  }
  return 0;
}
static void fragile_root_begin(void *a,unsigned int entity) {
  unsigned int parent=0;
  DynamicActor *d;
  if (!a || !DPTR(a,0x18) || H(a,0x5c)!=entity ||
      !anchor_world_actor_placed(a) || func_800141C4_14DC4(30)==-1)
    return;
  anchor_world_actor_authority(a,&parent);
  if (!anchor_world_fragile_scope(D_800C7AB2,entity,parent)) return;
  d=allocate(a);
  if (!d) return;
  d->kind=WD_FRAGILE;d->placed=d->eligible=1;
  d->parent=(unsigned short)parent;
  d->row[WD_KIND]=WD_FRAGILE;d->row[WD_ENTITY]=(int)entity;
  d->row[WD_PARENT]=(int)parent;d->row[WF_VARIANT]=B(a,0xd0);
}
RECOMP_HOOK("func_080049A4_6C40F4")
void world_dynamic_fragile331_begin(void *a) { fragile_root_begin(a,0x331); }
RECOMP_HOOK("func_0800624C_6C599C")
void world_dynamic_fragile332_begin(void *a) { fragile_root_begin(a,0x332); }
static DynamicActor *fragile_register_root(void *a) {
  unsigned int parent=0,entity;
  DynamicActor *d=lookup(a);
  if (d && d->kind!=WD_FRAGILE) return 0;
  if (!a || !DPTR(a,0x18) || !anchor_world_actor_placed(a) ||
      func_800141C4_14DC4(30)==-1 || B(a,0x8d)!=1) return 0;
  entity=H(a,0x5c);
  anchor_world_actor_authority(a,&parent);
  if (!anchor_world_fragile_scope(D_800C7AB2,entity,parent) ||
      !fragile_native_ok(entity,0,DPTR(a,0xc)) ||
      H(a,0x5e)!=(entity==0x339 ? 0x24f : entity) ||
      (entity==0x339 && func_800240DC_24CDC(0x32))) return 0;
  for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *old=&d_actors[i];
    if (old!=d && old->used && old->kind==WD_FRAGILE && !old->row[WF_ROLE] &&
        old->parent==parent) {
      if (alive(old)) return 0;
      if (d) {
        for (unsigned int j=0;j<WORLD_DYNAMIC_WORDS;++j) d->row[j]=old->row[j];
        clear(old,sizeof(*old));
      } else d=old;
      break;
    }
  }
  if (!d) d=allocate(a);
  else {
    d->actor=a;d->saved=0;d->native=DPTR(a,0xc);
    if (++d_serial>0x7fffffffu) d_serial=1;
    d->serial=d_serial;d->have=d->dirty=0;d->owner=0;d->proxy=0;
    clear(d->net,sizeof(d->net));
    d->claimed=d->row[WD_LIFE]==WD_CLAIM;
  }
  if (!d) return 0;
  d->kind=WD_FRAGILE;d->placed=d->ready=d->eligible=d->fragile_initialized=1;
  d->parent=(unsigned short)parent;d->generation=B(a,0x74);
  d->native=DPTR(a,0xc);
  d->clip=(unsigned char)(entity==0x339 ? 4 : entity==0x331 ? 1 :
                          entity==0x330 && B(a,0xd0)==2 ? 1 : 0);
  d->animated=0;
  d->row[WD_SERIAL]=(int)d->serial;d->row[WD_KIND]=WD_FRAGILE;
  d->row[WD_ENTITY]=(int)entity;d->row[WD_MODEL]=H(a,0x5e);
  d->row[WD_PARENT]=(int)parent;d->row[WD_ORDINAL]=0;
  d->row[WF_ROLE]=0;d->row[WF_VARIANT]=B(a,0xd0);
  d->row[WF_INSTANCE]=(int)d->serial;
  d->row[WF_RECEIPT]=d->row[WF_ESTABLISHED]=0;d->row[WF_PRESENT]=1;
  d->row[WD_PHASE]=WF_PHASE;
  if (d->row[WD_LIFE]==WD_REMOVED) W(a,0x68)|=2u;
  return d;
}
static void fragile_child_birth(DynamicActor *p,DynamicActor *d) {
  unsigned long cb=(unsigned long)DPTR(d->actor,0xc)&~DDISABLED;
  unsigned int entity=(unsigned int)p->row[WD_ENTITY];
  if (!((entity==0x331 && cb==(unsigned long)func_08004AB4_6C4204) ||
        (entity==0x332 && cb==(unsigned long)func_08006778_6C5EC8))) return;
  for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *old=&d_actors[i];
    if (old!=d && old->used && old->kind==WD_FRAGILE &&
        old->row[WF_ROLE]==1 && old->parent==p->parent && !alive(old)) {
      for (unsigned int j=0;j<WORLD_DYNAMIC_WORDS;++j) d->row[j]=old->row[j];
      clear(old,sizeof(*old));break;
    }
  }
  d->parent=p->parent;d->fragile_birth=d->stable_ordinal=1;
  d->row[WD_KIND]=WD_FRAGILE;d->row[WD_PARENT]=p->parent;
  d->row[WD_SERIAL]=(int)d->serial;
  d->row[WD_ENTITY]=(int)entity;d->row[WD_MODEL]=(int)entity;
  d->row[WD_ORDINAL]=d->row[WF_ROLE]=1;
  d->row[WF_VARIANT]=p->row[WF_VARIANT];d->row[WD_PHASE]=WF_PHASE;
  d->row[WF_INSTANCE]=(int)d->serial;d->row[WF_RECEIPT]=0;
}
static int fragile_capture(DynamicActor *d) {
  int *r=d->row;
  if (r[WD_ENTITY]==0x339 && func_800240DC_24CDC(0x32))
    return 0; /* Save-completed disappearance is not a native kill. */
  if (!anchor_world_fragile_scope(D_800C7AB2,(unsigned int)r[WD_ENTITY],
                                   d->parent)) return 0;
  r[WD_PHASE]=WF_PHASE;r[WD_ORDINAL]=r[WF_ROLE];
  r[WD_ROUTE]=163;r[WD_TALKABLE]=r[WD_DIALOG]=r[WD_BUSY]=0;
  r[WD_LANDED]=d->claimed!=0;
  r[WF_VARIANT]=r[WF_ROLE] ? r[WF_VARIANT] : B(d->actor,0xd0);
  r[WF_RESERVED0]=r[WF_RESERVED1]=r[WF_RESERVED2]=0;
  r[WF_INSTANCE]=(int)d->serial;r[WF_ESTABLISHED]=0;r[WF_PRESENT]=1;
  return anchor_world_fragile_valid(r);
}
static int fragile_apply(DynamicActor *d) {
  int *r=d->net;
  if (!alive(d) || !r[WF_RECEIPT] || r[WF_INSTANCE]!=(int)d->serial ||
      r[WF_ROLE]!=d->row[WF_ROLE] ||
      r[WD_ENTITY]!=d->row[WD_ENTITY] ||
      !anchor_world_fragile_scope(D_800C7AB2,(unsigned int)r[WD_ENTITY],d->parent))
    return 0;
  if (r[WD_ENTITY]==0x339 && func_800240DC_24CDC(0x32)) return 0;
  d->row[WF_RECEIPT]=r[WF_RECEIPT];
  d->row[WD_COMMITTER]=r[WD_COMMITTER];
  d->row[WF_PRESENT]=1;
  d->dirty=0;
  return 1;
}
static int fragile_callback(DynamicActor *d,void *a,void *o) {
  DynamicCallback real;
  if (d->row[WD_ENTITY]==0x339 && func_800240DC_24CDC(0x32))
    return 1;
  if (d->row[WD_LIFE]==WD_REMOVED) {
    if (!d->row[WD_LANDED]) W(a,0x68)|=2u;
    return 1;
  }
  if (d->have) apply(d);
  real=(DynamicCallback)((unsigned long)(d->saved ? d->saved : d->native)&~DDISABLED);
  if (!real || !fragile_native_ok((unsigned int)d->row[WD_ENTITY],
                                 (unsigned int)d->row[WF_ROLE],(void *)real))
    return 1;
  /* These File30 continuations perform local visuals/support only. Native
   * contact and post still run once, including for a replica. */
  DPTR(a,0xc)=(void *)real;
  real(a,o);
  if (alive(d)) {
    d->native=d->saved=DPTR(a,0xc);
    DPTR(a,0xc)=(void *)dynamic_callback;
  }
  return 1;
}
static void fragile_damage(void *a) {
  DynamicActor *d=lookup(a);
  unsigned int status;
  if (!d_active || d_room!=D_800C7AB2 || !d || d->kind!=WD_FRAGILE ||
      !d->fragile_initialized || !alive(d) || d->death_started ||
      d->row[WD_LIFE]==WD_REMOVED ||
      (d->row[WD_ENTITY]==0x339 && func_800240DC_24CDC(0x32)) ||
      B(a,0x8d)!=1) return;
  status=W(a,0x68);
  if (!(status&0x40000u) &&
      !((status&0x80u) && !(status&1u) && DPTR(a,0x38))) return;
  d->claimed=1;
  W(a,0x68)=(status&~0x40080u)|1u;
  B(a,0x8c)=60;
  capture(d);
}
