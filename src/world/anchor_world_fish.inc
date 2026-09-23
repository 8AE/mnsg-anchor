/* File68's private fish object is created only by its placed constructor.
 * Keep native movement local and arbitrate the contact before the active
 * callback can increment a capped quest counter. */
static DynamicActor *fish_register_root(void *a) {
  DynamicActor *d=lookup(a);
  unsigned int parent=0,flag,variant;
  DynamicCallback callback;
  if (!a || !DPTR(a,0x18) || !anchor_world_actor_placed(a) ||
      H(a,0x5c)!=WFISH_ENTITY || H(a,0x5e)!=WFISH_ENTITY ||
      !DPTR(a,0xd0)) return 0;
  callback=(DynamicCallback)((unsigned long)DPTR(a,0xc)&~DDISABLED);
  /* A continuation beyond the active callback already crossed the native
   * count increment. Do not retroactively claim it or interrupt its scene. */
  if (callback!=func_080002C0_72ACF0) return 0;
  anchor_world_actor_authority(a,&parent);
  if (!anchor_world_fish_placement(D_800C7AB2,parent,&flag,&variant) ||
      W(a,0xd4)!=flag || W(a,0xd8)!=variant || W(a,0xdc)!=0)
    return 0;
  if (d && d->kind && d->kind!=WD_FISH) return 0;
  if (d && d->ready) return d;
  if (!d) {
    for (unsigned int i=0;i<WORLD_DYNAMIC_MAX;++i) {
      DynamicActor *old=&d_actors[i];
      if (old->used && old->kind==WD_FISH && !alive(old) &&
          old->parent==parent && old->row[WFISH_FLAG]==(int)flag) {
        d=old;
        d->actor=a;d->saved=0;d->native=DPTR(a,0xc);
        d->have=d->dirty=0;d->proxy=0;
        if (++d_serial>0x7fffffffu) d_serial=1;
        d->serial=d_serial;
        clear(d->net,sizeof(d->net));
        break;
      }
    }
  }
  if (!d) d=allocate(a);
  if (!d) return 0;
  d->kind=WD_FISH;d->placed=d->ready=d->eligible=1;
  d->parent=(unsigned short)parent;d->generation=B(a,0x74);
  d->native=DPTR(a,0xc);
  d->row[WD_KIND]=WD_FISH;d->row[WD_ENTITY]=WFISH_ENTITY;
  d->row[WD_MODEL]=WFISH_ENTITY;d->row[WD_PARENT]=(int)parent;
  d->row[WD_ROUTE]=163;d->row[WD_ORDINAL]=0;
  d->row[WFISH_FLAG]=(int)flag;d->row[WFISH_VARIANT]=(int)variant;
  d->row[WFISH_INSTANCE]=(int)d->serial;d->row[WFISH_PRESENT]=1;
  d->row[WFISH_RECEIPT]=d->row[WFISH_ESTABLISHED]=0;
  d->row[WD_PAUSED]=0;
  d->row[WD_BIRTH_X]=d->row[WD_BIRTH_Y]=d->row[WD_BIRTH_Z]=0;
  return d;
}

static int fish_capture(DynamicActor *d) {
  int *r=d->row;
  unsigned int flag,variant;
  if (!d->eligible ||
      !anchor_world_fish_placement(D_800C7AB2,d->parent,&flag,&variant))
    return 0;
  if (r[WD_LIFE]==WD_REMOVED) {
    r[WFISH_PRESENT]=alive(d);
    return anchor_world_fish_valid(r);
  }
  if (!alive(d) || W(d->actor,0xd4)!=flag ||
      W(d->actor,0xd8)!=variant || W(d->actor,0xdc)!=0)
    return 0;
  r[WD_LIFE]=d->claimed ? WD_CLAIM : WD_LIVE;
  r[WD_LANDED]=d->claimed!=0;
  r[WFISH_INSTANCE]=(int)d->serial;
  r[WFISH_PRESENT]=1;
  return anchor_world_fish_valid(r);
}

static void fish_callback(DynamicActor *d,void *a,void *o) {
  DynamicCallback real;
  void *wrapped=DPTR(a,0xc);
  unsigned int touch=W(a,0x68)&0x200u;
  if (!alive(d) || !anchor_world_fish_placement(D_800C7AB2,d->parent,0,0))
    return;
  real=(DynamicCallback)((unsigned long)(d->saved ? d->saved : d->native)&~DDISABLED);
  if (real!=func_080002C0_72ACF0 &&
      real!=func_08000668_72B098 && real!=func_08000928_72B358)
    return;
  if (!d_active) {
    DPTR(a,0xc)=(void *)real;real(a,o);
  } else if (d->fish_awarded) {
    /* The winner must finish the native award, flag and music cleanup. */
    if (real==func_080002C0_72ACF0) W(a,0x68)&=~0x200u;
    DPTR(a,0xc)=(void *)real;real(a,o);
  } else if (d->row[WD_LIFE]==WD_REMOVED) {
    if (d->claimed && d->row[WD_LANDED] &&
        (unsigned int)d->row[WD_OWNER]==d_self &&
        real==func_080002C0_72ACF0 &&
        !func_800240DC_24CDC(d->row[WFISH_FLAG])) {
      d->fish_awarded=1;
      d->fish_admit=1;
      W(a,0x68)|=0x200u;
      DPTR(a,0xc)=(void *)real;real(a,o);
      d->fish_admit=0;
    } else {
      /* Let the native save-bit check clean up the private object after the
       * shared flag arrives. A losing client never enters the award path. */
      W(a,0x68)&=~0x200u;
      if (real==func_080002C0_72ACF0) {
        DPTR(a,0xc)=(void *)real;real(a,o);
      }
    }
  } else {
    if (touch && real==func_080002C0_72ACF0)
      d->claimed=1;
    W(a,0x68)&=~0x200u;
    DPTR(a,0xc)=(void *)real;real(a,o);
  }
  if (alive(d)) {
    d->native=DPTR(a,0xc);
    if (d->saved)
      d->saved=(void *)((unsigned long)d->native |
                       ((unsigned long)d->saved&DDISABLED));
    DPTR(a,0xc)=wrapped;
  }
}
