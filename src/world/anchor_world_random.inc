/* File30 0x3EF: invisible placed RNG spawner plus independently scheduled
 * 0x12F children. Only the owning client rolls the native period and birth
 * position; every child is a typed checkpoint, so a late entrant reconstructs
 * the same world state without replaying a local roll.
 *
 * The native child continuation func_08007BDC_6C732C dereferences the target
 * object pointer at +0x84 before it steers. Tracked children therefore run the
 * verified pure yaw helper func_802196FC_5D4BCC against the checkpointed target
 * X/Z, and a replica never touches +0x84 at all. */
extern void func_080079FC_6C714C(void *, void *);
extern void func_08007B28_6C7278(void *, void *);
extern void func_08007BDC_6C732C(void *, void *);
extern int func_802196FC_5D4BCC(void *, float, float, float, int);
extern void func_80034ED4_35AD4(void);

static DynamicCallback volatile d_random_phases[2];
static DynamicCallback random_phase(int phase) {
  /* Volatile stores prevent LLVM from converting this selection into a
   * static pointer table. Overlay callbacks require runtime relocations. */
  d_random_phases[0]=func_080079FC_6C714C;
  d_random_phases[1]=func_08007BDC_6C732C;
  return d_random_phases[phase==WR_ROOT ? 0 : 1];
}

static DynamicActor *random_root_for(unsigned int parent) {
  unsigned int i;
  for (i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *p=&d_actors[i];
    if (p->used && p->kind==WD_RANDOM && !p->row[WR_ROLE] &&
        p->parent==(unsigned short)parent) return p;
  }
  return 0;
}

/* Exactly one birth ordinal per successful allocation. The child initializer
 * only runs after func_802171A8_5D2678 returned a task, so a failed allocation
 * never reaches either call site and consumes nothing. At the published
 * identity limit the root holds instead of reusing an ordinal. Idempotent: the
 * registry parent path and the initializer hook may both offer the same child. */
static void random_child_birth(DynamicActor *p, DynamicActor *d) {
  if (!p || !d || p->kind!=WD_RANDOM || p->row[WR_ROLE] || d->random_birth)
    return;
  if (p->row[WD_ORDINAL] < 0x7fffffff) ++p->row[WD_ORDINAL];
  d->parent=p->parent;
  d->stable_ordinal=d->random_birth=1;
  d->row[WD_ORDINAL]=p->row[WD_ORDINAL];
}

/* Typed child identity. The child inherits the root's entity number and its own
 * model, and keeps the real native continuation as its owner-side callback.
 * Ready stays clear: the common post must still classify the initialized task. */
static void random_establish(DynamicActor *d, void *a) {
  d->kind=WD_RANDOM;d->random_birth=1;
  d->generation=B(a,0x74);
  d->row[WD_KIND]=WD_RANDOM;d->row[WD_ENTITY]=WR_ENTITY;
  d->row[WD_MODEL]=WR_CHILD_MODEL;d->row[WD_PHASE]=WR_CHILD;
  d->row[WR_ROLE]=1;
  d->native=(void *)func_08007BDC_6C732C;
}

static DynamicActor *random_register_root(void *a) {
  unsigned int parent=0, i;
  if (!a || !DPTR(a,0x18) || H(a,0x5c)!=WR_ENTITY ||
      !anchor_world_actor_placed(a) ||
      ((unsigned long)DPTR(a,0xc)&~DDISABLED)!=(unsigned long)func_080079FC_6C714C)
    return 0;
  anchor_world_actor_authority(a,&parent);
  if (parent!=WR_ROOT_PARENT || D_800C7AB2!=WR_ROOM) return 0;
  DynamicActor *d=0;
  for (i=0;i<WORLD_DYNAMIC_MAX;++i) {
    DynamicActor *old=&d_actors[i];
    if (old->used && old->kind==WD_RANDOM && !old->row[WR_ROLE] &&
        old->parent==(unsigned short)parent) {
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
  }
  if (!d) return 0;
  d->kind=WD_RANDOM;d->placed=d->ready=d->eligible=d->random_initialized=1;
  d->parent=(unsigned short)parent;d->generation=B(a,0x74);
  d->native=(void *)func_080079FC_6C714C;
  d->row[WD_KIND]=WD_RANDOM;d->row[WD_ENTITY]=d->row[WD_MODEL]=WR_ENTITY;
  d->row[WD_PARENT]=(int)parent;d->row[WR_ROLE]=0;
  d->row[WR_INSTANCE]=(int)d->serial;
  d->row[WR_RECEIPT]=d->row[WR_ESTABLISHED]=0;d->row[WR_PRESENT]=1;
  if (!d->row[WD_PHASE]) d->row[WD_PHASE]=WR_ROOT;
  if (d->row[WD_LIFE]==WD_REMOVED) W(a,0x68)|=2u;
  /* A reload keeps the checkpoint, but cannot spawn before a receipt for this
   * incarnation. The bridge offers the retained/remote checkpoint. */
  return d;
}

/* func_08007B28_6C7278 is the initializer the native spawner passes to
 * func_802171A8_5D2678, so this hook runs exactly once per successful birth.
 * The child registry has already recorded the one-based placed parent, which
 * is what identifies the unique 0x3EF birth counter. */
RECOMP_HOOK("func_08007B28_6C7278")
void world_dynamic_random_child_traits(void *a) {
  DynamicActor *d, *p;
  if (d_building || !a) return;
  d=lookup(a);
  if (!d || d->random_birth) return;
  p=random_root_for(d->parent);
  if (!p) return;
  random_child_birth(p,d);
  random_establish(d,a);
  /* The native initializer runs immediately after this hook. */
  d->random_initialized=1;
}

/* The player's current display object: the local pursuit target the native
 * wrapper reads through a child's +0x84. */
static void *random_local_target(void) {
  void *player=D_801FC604_5B8514;
  if (player && DPTR(player,0x18)) return DPTR(player,0x18);
  return 0;
}

static int random_capture(DynamicActor *d) {
  int *r=d->row;
  void *a=d->actor;
  unsigned long callback=(unsigned long)(d->saved ? d->saved : DPTR(a,0xc))&~DDISABLED;
  r[WR_RESERVED0]=r[WR_RESERVED1]=0;
  r[WR_INSTANCE]=(int)d->serial;r[WR_ESTABLISHED]=0;r[WR_PRESENT]=1;
  r[WD_ROUTE]=163;
  if (!r[WR_ROLE]) {
    if (callback!=(unsigned long)func_080079FC_6C714C &&
        callback!=(unsigned long)dynamic_callback &&
        callback!=(unsigned long)random_native) return 0;
    /* File30 never writes +0x60/+0x64 on the placed root and the root carries
     * no motion or animation, so the checkpoint holds none of them. */
    r[WD_PHASE]=WR_ROOT;
    r[WD_CLIP]=r[WD_ANIMATED]=0;
    r[WD_FRAME]=r[WD_RATE]=r[WD_ANIM_FLAGS]=0;
    r[WD_VX]=r[WD_VY]=r[WD_VZ]=0;
    r[WD_FLAGS_LO]=r[WD_FLAGS_HI]=0;
    r[WD_AUX_LO]=r[WD_AUX_HI]=0;
    r[WD_SPHERE]=WR_SPHERE;
    r[WR_TARGET_X]=r[WR_TARGET_Z]=0;
    return anchor_world_random_scope(r,D_800C7AB2);
  }
  if (callback!=(unsigned long)func_08007BDC_6C732C &&
      callback!=(unsigned long)dynamic_callback &&
      callback!=(unsigned long)random_native) return 0;
  r[WD_PHASE]=WR_CHILD;
  r[WD_CLIP]=0;r[WD_ANIMATED]=1;
  r[WD_RATE]=WR_CHILD_RATE;r[WD_ANIM_FLAGS]=WR_CHILD_ANIM_FLAGS;
  r[WD_SPHERE]=WR_SPHERE;
  r[WD_FLAGS_LO]=WR_CHILD_FLAGS_LO;r[WD_FLAGS_HI]=WR_CHILD_FLAGS_HI;
  /* The live target object is the authoritative steering input, and only the
   * owning client may publish it: every replica keeps the published X/Z, so a
   * stale local +0x84 can never overwrite a remote checkpoint. The one other
   * publisher is a locally born child that is not bound to a remote owner yet,
   * whose +0x84 was inherited from the root. A copy we own after an ownership
   * handoff re-binds an absent inherited target to the local player object. */
  if (d->owner==d_self || (!d->have && !d->proxy)) {
    void *target=DPTR(a,0x84);
    int x, z;
    if (!target) {
      target=random_local_target();
      if (target) DPTR(a,0x84)=target;
    }
    if (target &&
        quantize(F(target,8),WR_TARGET_SCALE,-WR_TARGET_LIMIT,WR_TARGET_LIMIT,&x) &&
        quantize(F(target,0x10),WR_TARGET_SCALE,-WR_TARGET_LIMIT,WR_TARGET_LIMIT,&z)) {
      r[WR_TARGET_X]=x;r[WR_TARGET_Z]=z;
    }
  }
  return anchor_world_random_scope(r,D_800C7AB2);
}

/* Verified helper ABI: (actor, targetX, unusedY, targetZ, speed), stepping at
 * most twice speed in the 10-bit yaw space. The native wrapper's +0x84 object
 * pointer is never dereferenced here. */
static void random_steer(void *a, const int *r) {
  func_802196FC_5D4BCC(a,(float)r[WR_TARGET_X]/(float)WR_TARGET_SCALE,0.0f,
                       (float)r[WR_TARGET_Z]/(float)WR_TARGET_SCALE,WR_STEER_STEP);
}

/* One bounded continuation step: the verified pure yaw turn plus the native
 * lifetime arithmetic. The native continuation retires the child when the old
 * timer value was zero; a replica clamps at zero and never retires, so a
 * lagging or duplicated copy can neither expire a child the owner still
 * publishes nor walk its own timer into an invalid row. */
static void random_step(void *a, const int *r) {
  DynamicActor *d=lookup(a);
  short old;
  random_steer(a,r);
  old=S(a,0x8a);
  if (old>0) { S(a,0x8a)=old-1;return; }
  S(a,0x8a)=0;
  if (old==0 && d && d->owner==d_self) {
    /* A silent expiry is a shared removal with no loot, so the retiring client
     * is the committer and the tombstone must never look like a committed kill. */
    d->row[WD_LANDED]=0;
    d->row[WD_COMMITTER]=(int)d_self;
    func_80034ED4_35AD4();
  } else F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
}

/* Bounded replacement for func_08007BDC_6C732C, which dereferences +0x84. A
 * reconstructed child uses this instead, so a replica never touches that
 * pointer. Silent expiry: no common death, no loot.
 *
 * This callback can outlive the tracking record across a disconnect or reset.
 * Without one it binds the local player object and hands the task back to the
 * real continuation, so the child still steers and still ends. */
static void random_native(void *a, void *o) {
  DynamicActor *d=lookup(a);
  if (!d) {
    if (!DPTR(a,0x84)) DPTR(a,0x84)=random_local_target();
    if (DPTR(a,0x84)) {
      DPTR(a,0xc)=(void *)func_08007BDC_6C732C;
      func_08007BDC_6C732C(a,o);
      return;
    }
    /* No local target at all: keep the native lifetime so the task still ends. */
    short old=S(a,0x8a);
    if (old>0) { S(a,0x8a)=old-1;return; }
    S(a,0x8a)=0;
    func_80034ED4_35AD4();
    return;
  }
  if (d->kind!=WD_RANDOM) return;
  if (!d->row[WR_ROLE]) { func_080079FC_6C714C(a,o);return; }
  random_step(a,d->row);
}

static int random_callback(DynamicActor *d, void *a, void *o) {
  if (d->row[WD_LIFE]==WD_REMOVED) {
    /* Timer expiry is a silent native removal; a committed kill retires through
     * the deferred shared one-HP path instead. */
    if (!d->row[WD_LANDED]) W(a,0x68)|=2u;
    return 1;
  }
  if (!d->random_initialized) {
    /* A late entrant reconstructs the child, not the emitting AI: bind the
     * verified resource set once, in this task's own scheduler slot, then let
     * the checkpoint supply every scalar. */
    if (D_8016DAB4_16E6B4!=a || !resource_valid(d->net)) return 0;
    if (!d->proxy && !d->random_birth) {
      /* A freshly born child can reach the adapter before its own initializer
       * slot. Consume the birth and establish the typed identity here so no
       * ordinal or identity is lost. */
      random_child_birth(random_root_for(d->parent),d);
      random_establish(d,a);
    }
    void *wrapped=DPTR(a,0xc);
    d_building=d_binding=1;
    func_08007B28_6C7278(a,o);
    d_building=d_binding=0;
    d->random_initialized=1;d->animated=1;d->clip=0;d->sphere=WR_SPHERE;
    if (d->proxy) d->native=(void *)random_native;
    d->saved=d->native;DPTR(a,0xc)=wrapped;d->dirty=1;
  }
  /* A frozen tick zeroed the local velocity to suppress the post. Force one
   * full checkpoint write on the first confirmed tick so an unchanged bridge
   * row still restores the published motion. */
  if (d->random_frozen) d->dirty=1;
  if (!d->have || !apply(d) || !d->row[WR_RECEIPT] || d->claimed || !d->owner ||
      d->net[WD_PAUSED]) {
    /* Freeze motion and animation, but keep the last physical checkpoint. */
    d->random_frozen=1;
    F(a,0x78)=F(a,0x7c)=F(a,0x80)=0;
    W(a,0x60)=0;
    return 1;
  }
  d->random_frozen=0;
  if (!d->row[WR_ROLE]) {
    /* The placed root only ever rolls and allocates on the owning client. */
    if (d->owner!=d_self || d->row[WD_ORDINAL]>=0x7fffffff) return 1;
    DynamicCallback real=random_phase(WR_ROOT);
    DPTR(a,0xc)=(void *)real;
    real(a,o);
    if (alive(d)) {
      d->native=d->saved=DPTR(a,0xc);
      DPTR(a,0xc)=(void *)dynamic_callback;
      capture(d);
    }
    return 1;
  }
  if (d->owner!=d_self) {
    /* A replica predicts the same turn from the checkpointed target X/Z and
     * advances the shared lifetime, but only the owner may retire it. */
    random_step(a,d->net);
    return 1;
  }
  {
    /* The owner runs its real continuation: the native one for a locally born
     * child, the bounded replacement for a reconstructed copy. */
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
