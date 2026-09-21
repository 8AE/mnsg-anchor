/* File_46 0x1B0 Koryuta wave graph: an invisible child of the placed body
 * that rolls the native wave emitter, plus the 0x12D flight and 0xFA
 * pursue/coast children it births. Only the owning client runs the emitter and
 * the child state machines; every child is a typed checkpoint, so a late
 * entrant reconstructs the same world state without replaying a local roll or
 * a local damage step.
 *
 * func_080037B0_7048B0 picks spawn positions around the LOCAL player object
 * and gates on the local live-wave counter D_8015CDB4. Both are local, so the
 * owner refreshes that counter from the tracked census before it emits, and the
 * pursuit destination is published so replicas agree on the encounter. */
extern void func_0800370C_70480C(void *, void *);
extern void func_0800376C_70486C(void *, void *);
extern void func_080037B0_7048B0(void *, void *);
extern void func_08003B4C_704C4C(void *, void *);
extern void func_08003C90_704D90(void *, void *);
extern void func_08003D64_704E64(void *, void *);
extern void func_08003E60_704F60(void *, void *);
extern void func_08003F68_705068(void *, void *);
extern void func_08004034_705134(void *, void *);
extern void func_80212088_5CD558(void *, void *);
/* Both are u16 in the shipped tables: the live-wave census and the encounter
 * stop latch. The census is local and is refreshed from the tracked family; the
 * stop latch is the one producer word that is shared between clients. */
extern unsigned short D_8015CDB4, D_8015CDC0;
extern void *D_801FC60C_5B851C;

static DynamicActor *wave_register_root(void *);
static void wave_child_birth(DynamicActor *, DynamicActor *);
static void wave_apply(DynamicActor *);
static int wave_capture(DynamicActor *);
static int wave_callback(DynamicActor *, void *, void *);
static void wave_hold(DynamicActor *);
static void wave_native(void *, void *);
static int wave_gate(const int *, const DynamicActor *);
static int wave_resource(const int *);

static DynamicCallback volatile d_wave_phases[6];
static DynamicCallback wave_phase(int phase) {
  /* Volatile stores prevent LLVM from converting this selection into a static
   * pointer table. Overlay callbacks require runtime relocations. */
  d_wave_phases[0]=func_0800376C_70486C;
  d_wave_phases[1]=func_080037B0_7048B0;
  d_wave_phases[2]=func_08003C90_704D90;
  d_wave_phases[3]=func_08003E60_704F60;
  d_wave_phases[4]=func_08003F68_705068;
  d_wave_phases[5]=func_08004034_705134;
  return d_wave_phases[phase>=WW_WAIT && phase<=WW_PURSUE2 ? phase-WW_WAIT : 0];
}

/* The one producer record of a given placed parent. It is a scheduled child of
 * the placed File_46 body, so it is registered from its own initializer rather
 * than from the placed roster. */
static DynamicActor *wave_root_for(unsigned int parent) {
  unsigned int i;
  for (i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *p=&d_actors[i];
    if (p->used && p->kind==WD_WAVE && !p->row[WW_ROLE] &&
        p->parent==(unsigned short)parent) return p;
  }
  return 0;
}
static DynamicActor *wave_root(void) { return wave_root_for(WW_ROOT_PARENT); }

/* Role selector for a freshly allocated wave child. Zero means the callback is
 * not one of the two File_46 wave initializers. */
static unsigned int wave_role_of(void *a) {
  unsigned long callback=(unsigned long)DPTR(a,0xc)&~DDISABLED;
  if (callback==(unsigned long)func_08003B4C_704C4C) return WW_MODEL_12D;
  if (callback==(unsigned long)func_08003D64_704E64) return WW_MODEL_FA;
  return 0;
}

/* Exactly one birth ordinal per successful allocation. func_080037B0 only
 * reaches the child initializer after func_802171A8 returned a task, so a
 * failed allocation consumes nothing, and at the published identity limit the
 * producer holds instead of reusing an ordinal. Idempotent: the child registry
 * and the initializer hook may both offer the same child. */
static void wave_child_birth(DynamicActor *p, DynamicActor *d) {
  if (!p || !d || p->kind!=WD_WAVE || p->row[WW_ROLE] || d->wave_birth)
    return;
  if (p->row[WD_ORDINAL] < 0x7fffffff) ++p->row[WD_ORDINAL];
  d->parent=p->parent;
  d->stable_ordinal=d->wave_birth=1;
  d->row[WD_ORDINAL]=p->row[WD_ORDINAL];
}

/* Typed child identity. The child inherits the producer's entity number and
 * takes its own model, role and entry phase; the real native continuation stays
 * the owner-side callback. Ready stays clear so the common post still
 * classifies the initialized task. */
static void wave_establish(DynamicActor *d, void *a, unsigned int role) {
  d->kind=WD_WAVE;d->wave_birth=1;
  d->generation=B(a,0x74);
  d->row[WD_KIND]=WD_WAVE;d->row[WD_ENTITY]=WW_ENTITY;
  d->row[WD_MODEL]=role==WW_MODEL_12D ? WW_12D_MODEL : WW_FA_MODEL;
  d->row[WD_PHASE]=role==WW_MODEL_12D ? WW_FLIGHT : WW_PURSUE;
  d->row[WW_ROLE]=(int)role;
  d->native=(void *)(role==WW_MODEL_12D ? func_08003C90_704D90
                                        : func_08003E60_704F60);
}

/* The producer is never a roster entry, so anchor_world_actor_placed must not
 * gate it. The common child registry has already derived its one-based parent
 * from the placed body and its eligible flag from the body's authority; this
 * only fills the slot in, keeping the single-writer rule the body establishes.
 * A reload reuses the retained slot exactly like the Slicer and RNG roots. */
static DynamicActor *wave_register_root(void *a) {
  unsigned long callback;
  DynamicActor *d;
  if (!a || !DPTR(a,0x18) || H(a,0x5c)!=WW_ENTITY) return 0;
  callback=(unsigned long)DPTR(a,0xc)&~DDISABLED;
  if (callback!=(unsigned long)func_0800370C_70480C &&
      callback!=(unsigned long)func_0800376C_70486C &&
      callback!=(unsigned long)func_080037B0_7048B0) return 0;
  d=lookup(a);
  if (!d) {
    d=wave_root();
    if (d) {
      if (alive(d)) return 0;
      d->actor=a;
      if (++d_serial>0x7fffffffu) d_serial=1;
      d->serial=d_serial;
      d->dirty=d->have=0;d->saved=0;
      d->owner=0;d->proxy=0;d->wave_frozen=1;
      clear(d->net,sizeof(d->net));
      /* Claims belong to the room identity: a fresh native incarnation must
       * not undo a hit accepted before proximity unloading. */
      d->claimed=d->row[WD_LIFE]==WD_CLAIM;
    } else d=allocate(a);
  }
  if (!d) return 0;
  d->kind=WD_WAVE;d->placed=0;d->ready=1;d->eligible=1;
  d->wave_initialized=1;
  d->parent=(unsigned short)WW_ROOT_PARENT;
  d->generation=B(a,0x74);
  d->native=(void *)(callback==(unsigned long)func_080037B0_7048B0
                       ? func_080037B0_7048B0 : func_0800376C_70486C);
  d->row[WD_KIND]=WD_WAVE;d->row[WD_ENTITY]=WW_ENTITY;
  d->row[WD_MODEL]=H(a,0x5e);d->row[WD_PARENT]=WW_ROOT_PARENT;
  d->row[WW_ROLE]=WW_ROOT;
  d->row[WW_INSTANCE]=(int)d->serial;
  d->row[WW_RECEIPT]=d->row[WW_ESTABLISHED]=0;d->row[WW_PRESENT]=1;
  d->row[WD_PHASE]=callback==(unsigned long)func_080037B0_7048B0
                     ? WW_PRODUCER : WW_WAIT;
  if (d->row[WD_LIFE]==WD_REMOVED) W(a,0x68)|=2u;
  return d;
}

/* func_08001A28 spawns the producer with func_802171A8(body, 370C, 0) and only
 * ever writes +0x28/+0x2C, so the producer object keeps the allocator default.
 * The shared anchor is pinned to zero instead of inventing the body position,
 * and the family keeps its own exact birth record for the same reason. */
static void wave_root_anchor(DynamicActor *d) {
  int *r=d->row;
  r[WD_X]=r[WD_Y]=r[WD_Z]=0;
  r[WD_BIRTH_X]=r[WD_BIRTH_Y]=r[WD_BIRTH_Z]=0;
}

/* The local player object: the flight phase's pursuit target and the base the
 * FA phases measure their forward pursuit point from. */
static void *wave_local_target(void) {
  if (D_801FC60C_5B851C) return D_801FC60C_5B851C;
  return D_801FC604_5B8514 ? DPTR(D_801FC604_5B8514,0x18) : 0;
}

/* Live tracked wave children. The native max-8 gate reads a local counter that
 * never advances on a client that only applies remote checkpoints, so the owner
 * refreshes it from this census before it emits. */
static unsigned short wave_live_count(void) {
  unsigned int i;
  unsigned short n=0;
  for (i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *d=&d_actors[i];
    if (d->used && d->kind==WD_WAVE && d->row[WW_ROLE] &&
        d->row[WD_LIFE]!=WD_REMOVED && alive(d)) ++n;
  }
  return n;
}

/* A stranded native task still owns one census slot. */
static void wave_census_release(void) {
  if (D_8015CDB4) --D_8015CDB4;
}

/* The owner publishes the pursuit destination so a replica's presentation
 * agrees with the authority instead of its own local player position. */
static void wave_target(DynamicActor *d) {
  int *r=d->row;
  void *target=DPTR(d->actor,0x84);
  int x, y, z;
  if (!target) target=wave_local_target();
  if (!target) return;
  if (quantize(F(target,8),WW_TARGET_SCALE,-WW_TARGET_LIMIT,WW_TARGET_LIMIT,&x) &&
      quantize(F(target,0xc),WW_TARGET_SCALE,-WW_TARGET_LIMIT,WW_TARGET_LIMIT,&y) &&
      quantize(F(target,0x10),WW_TARGET_SCALE,-WW_TARGET_LIMIT,WW_TARGET_LIMIT,&z)) {
    r[WW_TARGET_X]=x;r[WW_TARGET_Y]=y;r[WW_TARGET_Z]=z;
  }
}

/* Refresh only the bridge words while a frozen wave keeps its last physical
 * checkpoint. The velocity the freeze zeroed must never reach the row. */
static void wave_hold(DynamicActor *d) {
  int *r=d->row;
  r[WW_INSTANCE]=(int)d->serial;
  r[WW_ESTABLISHED]=0;
  r[WW_PRESENT]=alive(d);
  if (!r[WW_ROLE]) r[WW_STOP]=(D_8015CDC0!=0)||r[WW_STOP];
}

static int wave_capture(DynamicActor *d) {
  int *r=d->row;
  void *a=d->actor;
  unsigned long callback=(unsigned long)(d->saved ? d->saved : DPTR(a,0xc))&~DDISABLED;
  r[WW_INSTANCE]=(int)d->serial;r[WW_ESTABLISHED]=0;r[WW_PRESENT]=1;
  r[WD_ROUTE]=163;
  if (!r[WW_ROLE]) {
    /* The invisible producer: no motion, no animation, no collision and no
     * target. Its published state is the phase, the shared stop latch and the
     * monotone birth ordinal. */
    if (callback!=(unsigned long)func_0800370C_70480C &&
        callback!=(unsigned long)func_0800376C_70486C &&
        callback!=(unsigned long)func_080037B0_7048B0 &&
        callback!=(unsigned long)dynamic_callback &&
        callback!=(unsigned long)wave_native) return 0;
    r[WD_PHASE]=callback==(unsigned long)func_080037B0_7048B0
                  ? WW_PRODUCER : WW_WAIT;
    r[WD_CLIP]=r[WD_ANIMATED]=0;
    r[WD_FRAME]=r[WD_RATE]=r[WD_ANIM_FLAGS]=0;
    r[WD_VX]=r[WD_VY]=r[WD_VZ]=0;
    r[WD_FLAGS_LO]=r[WD_FLAGS_HI]=0;
    r[WD_AUX_LO]=r[WD_AUX_HI]=0;
    r[WD_SPHERE]=0;
    r[WW_TARGET_X]=r[WW_TARGET_Y]=r[WW_TARGET_Z]=0;
    /* The encounter stop is a shared latch: once any client has reached the
     * body the encounter is over for everyone, so the published value is
     * monotone and the apply side ORs it into the local global. */
    r[WW_STOP]=(D_8015CDC0!=0)||r[WW_STOP];
    wave_root_anchor(d);
    return anchor_world_wave_scope(r,D_800C7AB2);
  }
  if (callback!=(unsigned long)func_08003C90_704D90 &&
      callback!=(unsigned long)func_08003E60_704F60 &&
      callback!=(unsigned long)func_08003F68_705068 &&
      callback!=(unsigned long)func_08004034_705134 &&
      callback!=(unsigned long)dynamic_callback &&
      callback!=(unsigned long)wave_native) return 0;
  r[WD_PHASE]= callback==(unsigned long)func_08003E60_704F60 ? WW_PURSUE
             : callback==(unsigned long)func_08003F68_705068 ? WW_COAST
             : callback==(unsigned long)func_08004034_705134 ? WW_PURSUE2
             : WW_FLIGHT;
  r[WD_CLIP]=0;r[WD_ANIMATED]=1;
  r[WD_RATE]=WW_CHILD_RATE;r[WD_ANIM_FLAGS]=WW_CHILD_ANIM_FLAGS;
  r[WD_SPHERE]=WW_SPHERE;
  r[WD_AUX_LO]=WW_CHILD_AUX_LO;r[WD_AUX_HI]=WW_CHILD_AUX_HI;
  r[WW_STOP]=0;
  /* The live target object is the authoritative steering input, and only the
   * owning client may publish it: a replica keeps the published destination, so
   * a stale local player position can never overwrite a remote checkpoint. */
  if (d->owner==d_self || (!d->have && !d->proxy)) wave_target(d);
  return anchor_world_wave_scope(r,D_800C7AB2);
}

/* The native flight expiry spawns one category-8 presentation particle. A
 * committed shared kill plays the same local effect on every peer; it is never
 * a loot roll and never runs the native death pipeline. */
static void wave_death_visual(void *a) {
  void *child;
  d_building=1;
  child=func_802171A8_5D2678(a,func_80212088_5CD558,8);
  d_building=0;
  if (child && DPTR(child,0x18)) W(child,0x64)|=0x20000u;
}

/* Bounded replica continuation. The checkpoint is authoritative for phase and
 * timer, so this only mirrors the two scalar writes the native phases perform
 * every tick: the constant downward Z step of the 0x12D flight and the shared
 * lifetime's decrement toward, never below, the published value. It never
 * allocates, deletes, rolls RNG, spawns a visual, replays a temp flag or calls
 * an overlay helper, so a lagging copy can neither expire nor duplicate a
 * wave. */
static void wave_step(void *a, const int *r) {
  short timer;
  if (r[WW_ROLE]==WW_MODEL_12D && r[WD_PHASE]==WW_FLIGHT)
    F(a,0x80)=WW_FLIGHT_VZ;
  timer=S(a,0x8a);
  if (timer>0) S(a,0x8a)=timer-1;
}

/* Bounded replacement for the child continuations. A reconstructed child uses
 * this so a replica never calls the native pursuit helpers, and the owner runs
 * it only when the task outlived its tracking record. */
static void wave_native(void *a, void *o) {
  DynamicActor *d=lookup(a);
  if (!d) {
    /* Disconnect, reset or a room change can drop the tracking record while the
     * native task is still scheduled. Its +0x84 is locally valid, so hand it
     * back to its own role continuation; when that cannot run, keep a
     * self-contained lifetime and release the shared census so a stranded copy
     * can never hold a birth slot. */
    if (!DPTR(a,0x84)) DPTR(a,0x84)=wave_local_target();
    if (D_8015CDC0 || (W(a,0x68)&0x80u) || !DPTR(a,0x84)) {
      wave_census_release();
      W(a,0x68)|=2u;
      return;
    }
    if (H(a,0x5e)==WW_12D_MODEL) { func_08003C90_704D90(a,o);return; }
    if (H(a,0x5e)==WW_FA_MODEL) { func_08003E60_704F60(a,o);return; }
    short old=S(a,0x8a);
    if (old>0) { S(a,0x8a)=old-1;return; }
    S(a,0x8a)=0;
    wave_census_release();
    W(a,0x68)|=2u;
    return;
  }
  if (d->kind!=WD_WAVE) return;
  if (!d->row[WW_ROLE]) {
    /* Only the owner may run the emitter; a replica root never rolls. */
    if (d->owner!=d_self) return;
    DynamicCallback real=wave_phase(d->row[WD_PHASE]);
    DPTR(a,0xc)=(void *)real;
    real(a,o);
    if (alive(d)) {
      d->native=d->saved=DPTR(a,0xc);
      DPTR(a,0xc)=(void *)dynamic_callback;
      capture(d);
    }
    return;
  }
  if (d->owner!=d_self) { wave_step(a,d->net);return; }
  {
    DynamicCallback real=(DynamicCallback)((unsigned long)(d->saved ? d->saved : d->native)&
                                           ~DDISABLED);
    if (!real) return;
    DPTR(a,0xc)=(void *)real;
    real(a,o);
    if (alive(d)) {
      d->native=d->saved=DPTR(a,0xc);
      DPTR(a,0xc)=(void *)dynamic_callback;
      capture(d);
    }
  }
}

static int wave_callback(DynamicActor *d, void *a, void *o) {
  if (d->row[WD_LIFE]==WD_REMOVED) {
    /* Timer expiry is a silent native removal with no presentation. A committed
     * shared kill plays the local effect on every peer and then retires the
     * task; it never reaches the native death pipeline, so no peer rolls loot. */
    if (!d->row[WD_LANDED]) { W(a,0x68)|=2u;return 1; }
    if (!d->death_started) {
      d->death_started=1;
      W(a,0x64)|=0x8000u;
      wave_death_visual(a);
      W(a,0x68)|=2u;
    }
    return 1;
  }
  if (!d->wave_initialized) {
    /* A late entrant reconstructs the child, not the emitting AI: run the
     * verified native initializer once in this task's own scheduler slot, then
     * let the checkpoint supply every scalar. The 0x12D initializer reads the
     * local player Z, which apply restores from the checkpoint immediately
     * after the bind. */
    if (D_8016DAB4_16E6B4!=a || !resource_valid(d->net)) return 0;
    if (!d->proxy && !d->wave_birth) {
      /* A freshly born child can reach the adapter before its own initializer
       * slot. Consume the birth and establish the typed identity here so no
       * ordinal or identity is lost. */
      DynamicActor *p=wave_root_for(d->parent);
      if (p) { wave_child_birth(p,d);wave_establish(d,a,wave_role_of(a)); }
    }
    void *wrapped=DPTR(a,0xc);
    d_building=d_binding=1;
    if (H(a,0x5e)==WW_FA_MODEL || d->row[WW_ROLE]==WW_MODEL_FA)
      func_08003D64_704E64(a,o);
    else func_08003B4C_704C4C(a,o);
    d_building=d_binding=0;
    d->wave_initialized=1;d->animated=1;d->clip=0;d->sphere=WW_SPHERE;
    if (d->proxy) d->native=(void *)wave_native;
    d->saved=d->native;DPTR(a,0xc)=wrapped;d->dirty=1;
  }
  /* A frozen tick zeroed the local velocity to suppress the post. Force one
   * full checkpoint write on the first confirmed tick so an unchanged bridge
   * row still restores the published motion. */
  if (d->have && d->wave_frozen) d->dirty=1;
  if (!d->have || !apply(d) || !d->row[WW_RECEIPT] || d->claimed || !d->owner ||
      d->net[WD_PAUSED]) {
    /* Freeze motion and animation, but keep the last physical checkpoint. */
    d->wave_frozen=1;
    F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    W(a,0x60)=0;
    return 1;
  }
  d->wave_frozen=0;
  if (!d->row[WW_ROLE]) {
    /* The invisible producer only ever rolls on the owning client. The native
     * max-8 gate is a local counter, so refresh it from the shared census
     * before the emitter reads it. */
    if (d->owner!=d_self) return 1;
    if (d->row[WD_PHASE]==WW_PRODUCER) D_8015CDB4=wave_live_count();
  } else {
    /* A copy we own after a handoff re-binds an absent inherited pursuit target
     * to the local player object before the real continuation dereferences
     * +0x84. */
    if (!DPTR(a,0x84)) DPTR(a,0x84)=wave_local_target();
    if (!DPTR(a,0x84)) return 1;
    if (d->owner!=d_self) {
      /* A replica predicts velocity, animation and lifetime from the checkpoint
       * and never calls the native AI, so it can neither expire nor duplicate a
       * wave; the authoritative phase and timer are applied every tick. */
      wave_step(a,d->net);
      return 1;
    }
  }
  {
    /* The owner runs its real continuation: the native overlay callback for a
     * locally born child, the bounded replacement for a reconstructed copy. */
    DynamicCallback real=(DynamicCallback)((unsigned long)(d->saved ? d->saved : d->native)&
                                           ~DDISABLED);
    if (!real) return 1;
    DPTR(a,0xc)=(void *)real;
    real(a,o);
    if (alive(d)) {
      d->native=d->saved=DPTR(a,0xc);
      DPTR(a,0xc)=(void *)dynamic_callback;
      capture(d);
    }
    return 1;
  }
}

/* func_0800370C is the producer's own initializer entry: it is the only point
 * where the producer is identifiable before it schedules func_0800376C. */
RECOMP_HOOK("func_0800370C_70480C")
void world_dynamic_wave_root(void *a) {
  if (d_building || !a) return;
  wave_register_root(a);
}

/* func_080037B0 passes one of the two File_46 initializers to func_802171A8, so
 * this hook runs exactly once per successful wave birth. */
RECOMP_HOOK("func_08003B4C_704C4C")
void world_dynamic_wave_child_12d(void *a) {
  DynamicActor *d, *p;
  unsigned int role;
  if (d_building || !a) return;
  role=wave_role_of(a);
  if (!role) return;
  d=lookup(a);
  if (!d) {
    p=wave_root();
    if (!p) return;
    d=allocate(a);
    if (!d) return;
    d->parent=p->parent;
  }
  if (d->wave_birth) return;
  p=wave_root_for(d->parent);
  if (!p) p=wave_root();
  if (!p) return;
  wave_child_birth(p,d);
  wave_establish(d,a,role);
  /* The native initializer runs immediately after this hook. */
  d->wave_initialized=1;
}

RECOMP_HOOK("func_08003D64_704E64")
void world_dynamic_wave_child_fa(void *a) {
  DynamicActor *d, *p;
  unsigned int role;
  if (d_building || !a) return;
  role=wave_role_of(a);
  if (!role) return;
  d=lookup(a);
  if (!d) {
    p=wave_root();
    if (!p) return;
    d=allocate(a);
    if (!d) return;
    d->parent=p->parent;
  }
  if (d->wave_birth) return;
  p=wave_root_for(d->parent);
  if (!p) p=wave_root();
  if (!p) return;
  wave_child_birth(p,d);
  wave_establish(d,a,role);
  d->wave_initialized=1;
}

/* The receipt-gated union copy. The common apply has already written every
 * shared scalar word; this only owns the family words, the +0x60 texture-loop
 * bit the common mask clears, the native selection and the freeze rule. */
static void wave_apply(DynamicActor *d) {
  int *r=d->net;
  void *a=d->actor;
  unsigned int j;
  d->row[WD_COMMITTER]=r[WD_COMMITTER];
  for (j=WW_ROLE;j<=WW_PRESENT;++j) d->row[j]=r[j];
  d->row[WD_PHASE]=r[WD_PHASE];d->row[WD_ORDINAL]=r[WD_ORDINAL];
  for (j=WD_X;j<=WD_SZ;++j) d->row[j]=r[j];
  d->row[WD_TIMER]=r[WD_TIMER];
  d->row[WW_RECEIPT]=r[WW_RECEIPT];d->row[WW_PRESENT]=1;
  if (!r[WW_ROLE]) {
    /* The producer's only shared state beyond its ordinal is the stop latch,
     * transported through the body's parent identity and OR'd across players
     * so an encounter that ended for one client ends for all. */
    if (r[WW_STOP]) D_8015CDC0=1;
    wave_root_anchor(d);
  }
  W(a,0x60)=(((unsigned int)r[WD_FLAGS_LO] |
              ((unsigned int)r[WD_FLAGS_HI]<<16)) & 0x16e00febu) |
            ((unsigned int)r[WD_FLAGS_LO] & WW_TEXTURE_LOOP);
  W(a,0x64)=((unsigned int)r[WD_AUX_LO] |
             ((unsigned int)r[WD_AUX_HI]<<16)) & 0x9022u;
  d->native=(void *)(r[WW_ROLE] && d->proxy ? wave_native : wave_phase(r[WD_PHASE]));
  if (d->saved) d->saved=d->native;
  else DPTR(a,0xc)=d->native;
  if (!r[WD_OWNER] || r[WD_PAUSED] || d->claimed) {
    d->wave_frozen=1;
    W(a,0x60)=0;
    F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
  }
}

/* Complete residency check for the family, including both child descriptors.
 * The common model path cannot serve it: the child descriptors carry a zero
 * second file, and the 0xFFFFFFFF attack word makes the generic clip sweep
 * walk 18 slots. */
static int wave_resource(const int *r) {
  void *model;
  const unsigned short *files;
  const unsigned int *clips;
  if (!anchor_world_wave_scope(r,D_800C7AB2) ||
      func_800141C4_14DC4(WW_FILE_CODE)==-1) return 0;
  /* The producer binds no appearance and no clip. */
  if (!r[WW_ROLE]) return 1;
  if (r[WD_MODEL]>1025 || r[WD_MODEL]<0) return 0;
  model=D_80236984_5F1E54[r[WD_MODEL]];
  if (!model) return 0;
  files=(const unsigned short *)DPTR(model,0);
  clips=(const unsigned int *)DPTR(model,4);
  if (!files || !clips || !clips[0] ||
      func_800141C4_14DC4(0x152)==-1) return 0;
  if (r[WW_ROLE]==WW_MODEL_12D)
    return files[0]==0x19e && !files[1] && clips[0]==0x08000130u;
  return files[0]==0x1b6 && !files[1] && clips[0]==0x080003e4u;
}

/* The receipt/instance/role gate the common apply runs before wave_apply. */
static int wave_gate(const int *r, const DynamicActor *d) {
  return d->wave_initialized && r[WW_RECEIPT] &&
         r[WW_INSTANCE]==(int)d->serial && r[WW_ROLE]==d->row[WW_ROLE] &&
         anchor_world_wave_scope(r,D_800C7AB2) &&
         (r[WW_ROLE] || d->row[WD_ORDINAL]<=r[WD_ORDINAL]);
}
