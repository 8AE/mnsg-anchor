/* File34 0x3DA: the native placed task owns collision and its local particle
 * presentation. The shared checkpoint decides when the height trigger fires,
 * where the boulder is, and when the 150-frame descent expires. */
static DynamicActor *boulder_register_root(void *a) {
  DynamicActor *d=lookup(a);
  unsigned int parent=0,ordinal=0;
  short birth[3];
  unsigned int j;
  if (!a || !DPTR(a,0x18) || H(a,0x5c)!=WBO_ENTITY ||
      func_800141C4_14DC4(WBO_FILE)==-1 ||
      ((unsigned long)DPTR(a,0xc)&~DDISABLED)!=
          (unsigned long)func_080014B4_6D57F4)
    return 0;
  if (anchor_world_actor_placed(a))
    anchor_world_actor_authority(a,&parent);
  else if (!anchor_race_boulder_duplicate(a,&parent,&ordinal))
    return 0;
  if (!anchor_world_boulder_placement(D_800C7AB2,parent,0) ||
      ordinal>65535 || !anchor_world_source_position(parent,birth))
    return 0;
  if (d && d->kind && d->kind!=WD_BOULDER) return 0;
  for (j=0;j<WORLD_DYNAMIC_MAX;++j) {
    DynamicActor *old=&d_actors[j];
    if (old!=d && old->used && old->kind==WD_BOULDER &&
        old->parent==parent && old->row[WD_ORDINAL]==(int)ordinal) {
      if (alive(old)) return 0;
      if (d) {
        for (unsigned int k=0;k<WORLD_DYNAMIC_WORDS;++k) d->row[k]=old->row[k];
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
  }
  if (!d) return 0;
  d->kind=WD_BOULDER;d->placed=d->ready=d->eligible=d->boulder_initialized=1;
  d->parent=(unsigned short)parent;d->generation=B(a,0x74);
  d->native=DPTR(a,0xc);d->row[WD_SERIAL]=(int)d->serial;
  d->row[WD_KIND]=WD_BOULDER;d->row[WD_ENTITY]=WBO_ENTITY;
  d->row[WD_PARENT]=(int)parent;d->row[WD_ORDINAL]=(int)ordinal;
  d->row[WD_ROUTE]=163;d->row[WBO_ROLE]=0;
  d->row[WBO_INSTANCE]=(int)d->serial;
  d->row[WBO_RECEIPT]=d->row[WBO_ESTABLISHED]=0;
  d->row[WBO_PRESENT]=1;
  for (j=0;j<3;++j) d->row[WD_BIRTH_X+j]=(int)birth[j];
  if (d->row[WD_LIFE]==WD_REMOVED) W(a,0x68)|=2u;
  return d;
}

static int boulder_capture(DynamicActor *d) {
  void *a=d->actor;
  int *r=d->row;
  if (!alive(d) || !d->boulder_initialized ||
      !anchor_world_boulder_scope(r,D_800C7AB2)) return 0;
  r[WD_PHASE]=B(a,0xd4);
  r[WD_TIMER]=S(a,0xd6);
  r[WBO_INSTANCE]=(int)d->serial;r[WBO_PRESENT]=1;
  if (r[WD_PHASE]==4 && r[WD_TIMER]>=150 && (W(a,0x68)&2u)) {
    r[WD_LIFE]=WD_REMOVED;r[WD_OWNER]=0;
    if (!r[WD_COMMITTER]) r[WD_COMMITTER]=(int)d_self;
  }
  return anchor_world_boulder_valid(r);
}

static int boulder_apply(DynamicActor *d) {
  const int *r=d->net;
  void *a=d->actor,*o;
  unsigned int j;
  if (!d->dirty) return 1;
  if (!alive(d) || !d->boulder_initialized ||
      !r[WBO_RECEIPT] || r[WBO_INSTANCE]!=(int)d->serial ||
      r[WD_ORDINAL]!=d->row[WD_ORDINAL] ||
      !anchor_world_boulder_scope(r,D_800C7AB2) ||
      !resource_valid(r)) return 0;
  for (j=0;j<WORLD_DYNAMIC_WORDS;++j) d->row[j]=r[j];
  d->row[WBO_INSTANCE]=(int)d->serial;
  d->row[WBO_PRESENT]=1;
  /* Phase three is the only native collision/model initializer. It must run
   * inside this actor's scheduled callback before any phase-four checkpoint
   * is applied. A remote height trigger may never consult our local player. */
  if (r[WD_PHASE]>=3 && B(a,0xd4)<4) {
    B(a,0xd4)=3;
    d->boulder_frozen=1;
    d->dirty=0;
    return 1;
  }
  if (r[WD_PHASE]<3) {
    B(a,0xd4)=(unsigned char)r[WD_PHASE];
    S(a,0xd6)=(short)r[WD_TIMER];
    F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
  } else if (B(a,0xd4)==4) {
    o=DPTR(a,0x18);
    for (j=0;j<3;++j) {
      F(o,8+j*4)=(float)r[WD_X+j]/100.f;
      H(o,0x14+j*2)=(unsigned short)r[WD_PITCH+j];
      F(a,0x78+j*4)=(float)r[WD_VX+j]/1000.f;
    }
    S(a,0xd6)=(short)r[WD_TIMER];
  }
  d->boulder_frozen=(unsigned int)r[WD_OWNER]!=d_self;
  d->dirty=0;
  return 1;
}

static void boulder_callback(DynamicActor *d,void *a,void *o) {
  DynamicCallback real;
  void *wrapped=DPTR(a,0xc);
  if (d->row[WD_LIFE]==WD_REMOVED) { W(a,0x68)|=2u;return; }
  if (d->have) boulder_apply(d);
  real=(DynamicCallback)((unsigned long)(d->saved ? d->saved : d->native)&~DDISABLED);
  if ((void *)real!=(void *)func_080014B4_6D57F4) return;
  if (d_active && (!d->have || !d->row[WBO_RECEIPT])) {
    /* Let the native first invocation establish its local visibility state,
     * then wait for a shared owner before allowing a height trigger. */
    if (B(a,0xd4)!=0) return;
  } else if (d->owner!=d_self && B(a,0xd4)==2) return;
  DPTR(a,0xc)=(void *)real;
  real(a,o);
  if (alive(d)) {
    d->native=DPTR(a,0xc);
    DPTR(a,0xc)=wrapped;
    if (d->have && d->owner!=d_self && B(a,0xd4)==4) {
      d->dirty=1;
      boulder_apply(d);
      if (d->net[WD_LIFE]==WD_LIVE) W(a,0x68)&=~2u;
    }
  }
}
