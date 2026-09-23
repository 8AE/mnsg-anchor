/* File30 19D: invisible placed emitter plus independently scheduled blades.
 * Included in the dynamic lifecycle so roots, blades and committed drops use
 * one snapshot and no second scheduler wrapper. */
extern int func_8021B988_5D6E58(void *, float, float, float, float);

static DynamicCallback volatile d_slicer_phases[4];
static DynamicCallback slicer_phase(int phase) {
  /* Volatile stores prevent LLVM from converting this selection into a
   * static pointer table. Overlay callbacks require runtime relocations. */
  d_slicer_phases[0]=func_08004550_6C3CA0;
  d_slicer_phases[1]=func_08004594_6C3CE4;
  d_slicer_phases[2]=func_08004654_6C3DA4;
  d_slicer_phases[3]=func_0800488C_6C3FDC;
  return d_slicer_phases[phase>=WS_INITIAL_WAIT && phase<=WS_FLIGHT ? phase-WS_INITIAL_WAIT : 3];
}
static DynamicActor *slicer_register_root(void *a) {
  int params[4];
  unsigned int parent=0;
  if (!a || !DPTR(a,0x18) || H(a,0x5c)!=0x19d ||
      !anchor_world_actor_placed(a) ||
      ((unsigned long)DPTR(a,0xc)&~DDISABLED)!=(unsigned long)func_08004550_6C3CA0)
    return 0;
  anchor_world_actor_authority(a,&parent);
  if (!anchor_world_slicer_params(D_800C7AB2,parent,params) ||
      B(a,0xd0)!=params[0] || B(a,0xd3)!=params[1] ||
      B(a,0xd1)!=params[2] || B(a,0xd2)!=params[3]) return 0;
  DynamicActor *d=0;
  for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *old=&d_actors[i];
    if (old->used && old->kind==WD_SLICER && !old->row[WS_ROLE] && old->parent==parent) {
      if (alive(old)) return 0;
      d=old;break;
    }
  }
  if (!d) d=allocate(a);
  else {
    d->actor=a;
    if (++d_serial>0x7fffffffu) d_serial=1;
    d->serial=d_serial;
    d->dirty=d->have=0;d->saved=0;
    d->owner=0;d->proxy=0;d->slicer_frozen=1;
    clear(d->net,sizeof(d->net));
    /* Claims and tombstones belong to the room identity. A fresh native
     * incarnation must not undo a hit accepted before proximity unloading. */
    d->claimed=d->row[WD_LIFE]==WD_CLAIM;
  }
  if (!d) return 0;
  d->kind=WD_SLICER;d->placed=d->ready=d->eligible=d->slicer_initialized=1;
  d->parent=(unsigned short)parent;d->generation=B(a,0x74);
  d->native=(void *)func_08004550_6C3CA0;
  d->row[WD_KIND]=WD_SLICER;d->row[WD_ENTITY]=d->row[WD_MODEL]=0x19d;
  d->row[WD_PARENT]=(int)parent;d->row[WS_INSTANCE]=(int)d->serial;
  d->row[WS_RECEIPT]=d->row[WS_ESTABLISHED]=0;d->row[WS_PRESENT]=1;
  for (unsigned int j=0;j<4;++j) d->row[WS_SUBTYPE+j]=params[j];
  if (!d->row[WD_PHASE]) d->row[WD_PHASE]=WS_INITIAL_WAIT;
  if (d->row[WD_LIFE]==WD_REMOVED) W(a,0x68)|=2u;
  /* A reload keeps the checkpoint, but cannot advance before a receipt for
   * this incarnation. The bridge will offer the retained/remote checkpoint. */
  return d;
}

RECOMP_HOOK("func_08004594_6C3CE4")
void world_dynamic_slicer_birth(void *a) {
  DynamicActor *d=lookup(a);
  if (d && d->kind==WD_SLICER && !d->row[WS_ROLE] &&
      d->row[WD_ORDINAL]<0x7fffffff) ++d->row[WD_ORDINAL];
}
RECOMP_HOOK("func_08004694_6C3DE4")
void world_dynamic_slicer_traits(void *a) {
  DynamicActor *d=lookup(a);
  if (!d || !d->slicer_birth) return;
  /* Native init/flight only read DC->D3/D0. Give the blade its own immutable
   * trait record, so culling or killing the emitter cannot leave a dangling
   * pointer. This remains valid even when network synchronization is disabled. */
  B(a,0xd0)=(unsigned char)d->row[WS_SUBTYPE];
  B(a,0xd1)=(unsigned char)d->row[WS_REPEAT];
  B(a,0xd2)=(unsigned char)d->row[WS_INITIAL];
  B(a,0xd3)=(unsigned char)d->row[WS_SPEED];
  DPTR(a,0xdc)=a;
}

static int slicer_capture(DynamicActor *d) {
  int *r=d->row;
  unsigned long callback=(unsigned long)(d->saved ? d->saved : DPTR(d->actor,0xc))&~DDISABLED;
  if (!r[WS_ROLE]) {
    if (callback==(unsigned long)func_08004550_6C3CA0) r[WD_PHASE]=WS_INITIAL_WAIT;
    else if (callback==(unsigned long)func_08004594_6C3CE4) r[WD_PHASE]=WS_BIRTH;
    else if (callback==(unsigned long)func_08004654_6C3DA4) r[WD_PHASE]=WS_REPEAT_WAIT;
    else if (callback!=(unsigned long)dynamic_callback && callback!=(unsigned long)slicer_native) return 0;
    r[WD_FRAME]=r[WD_RATE]=r[WD_ANIM_FLAGS]=0;
    r[WD_VX]=r[WD_VY]=r[WD_VZ]=0;
    r[WD_SPHERE]=17;
  } else {
    r[WD_PHASE]=WS_FLIGHT;r[WD_RATE]=256;r[WD_ANIM_FLAGS]=0;r[WD_SPHERE]=1;
  }
  r[WD_FLAGS_LO]=0x6e1;r[WD_FLAGS_HI]=0x20;
  r[WD_ANIMATED]=r[WS_ROLE];r[WD_CLIP]=0;
  r[WD_SX]=r[WD_SY]=r[WD_SZ]=100;
  r[WD_ROUTE]=163;
  for (unsigned int j=WD_PATH_TIMER;j<=WD_DIALOG;++j) r[j]=0;
  r[WD_BUSY]=0;r[WD_BOUNCE]=B(d->actor,0x8c);
  r[WD_LANDED]=d->claimed!=0;
  r[WS_INSTANCE]=(int)d->serial;r[WS_ESTABLISHED]=0;r[WS_PRESENT]=1;
  return anchor_world_slicer_scope(r,D_800C7AB2);
}

/* This callback can survive disconnect/reset: its source is native task
 * scalars, not the dynamic tracking allocation. */
static void slicer_native(void *a, void *o) {
  func_0800488C_6C3FDC(a,o);
}
static int slicer_callback(DynamicActor *d, void *a, void *o) {
  if (d->row[WD_LIFE]==WD_REMOVED) {
    if (!d->row[WD_LANDED]) W(a,0x68)|=2u;
    return 1;
  }
  if (!d->slicer_initialized) {
    if (D_8016DAB4_16E6B4!=a || !resource_valid(d->net)) return 0;
    world_dynamic_slicer_traits(a);
    void *wrapped=DPTR(a,0xc);
    d_building=d_binding=1;
    func_08004694_6C3DE4(a,o);
    d_building=d_binding=0;
    d->slicer_initialized=1;d->animated=1;d->clip=0;d->sphere=1;
    d->native=(void *)func_0800488C_6C3FDC;
    d->saved=d->native;DPTR(a,0xc)=wrapped;d->dirty=1;
  }
  /* A bridge interruption can freeze an unchanged checkpoint. Reapply its
   * velocity on resume even when none of the received words changed. */
  if (d->have && d->slicer_frozen) d->dirty=1;
  if (!d->have || !apply(d) || !d->row[WS_RECEIPT] || d->claimed ||
      !d->owner || d->net[WD_PAUSED]) {
    d->slicer_frozen=1;
    F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    W(a,0x60)=0; /* no inherited collision/animation before a valid receipt */
    return 1;
  }
  d->slicer_frozen=0;
  W(a,0x60)=0x002006e1u;
  if (d->row[WS_ROLE]) H(o,0x7e)=256;
  if (d->owner!=d_self) {
    if (!d->row[WS_ROLE]) return 1;
    /* Replicas predict motion, but only the owner may publish expiry. */
    int type=d->row[WS_SUBTYPE];
    if (type<2) {
      H(o,0x14)=75;
      func_8021B988_5D6E58(a,-40.f,type ? -167.f : -154.f,160.f,type ? 4.f : 2.f);
    } else {
      H(o,0x14)=0;
      if (S(a,0x8a)>0) --S(a,0x8a);
      else F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    }
    return 1;
  }
  if (!d->row[WS_ROLE] && d->row[WD_PHASE]==WS_BIRTH && d->row[WD_ORDINAL]==0x7fffffff)
    return 1; /* Never reuse a published emission identity. */
  DynamicCallback real=slicer_phase(d->row[WD_PHASE]);
  DPTR(a,0xc)=(void *)real;
  real(a,o);
  if (alive(d)) {
    d->native=d->saved=DPTR(a,0xc);
    DPTR(a,0xc)=(void *)dynamic_callback;
    capture(d);
  }
  return 1;
}
