/* File30 0x3EF RNG spawner: host native substitutes plus lifecycle coverage.
 *
 * This file is an include fragment, not a translation unit: it is included
 * near the end of tests/test_world_dynamic_native.c, so it inherits that
 * harness' includes, helpers and native substitutes and defines only the File30
 * 0x3EF names the harness does not already own. The harness' main calls
 * random_lifecycle_test(). */

static int random_period = 1, random_ticks, random_spawns, random_deletes;
static int random_alloc_fail, random_roll;
static unsigned int random_target[16], random_other_target[16];
static unsigned short random_files[2];
static unsigned int random_clips[8], random_model[2];

/* --- File30 0x3EF native substitutes ------------------------------------- */

void func_8003521C_35E1C(void *fn) {
  if (D_8016DAB4_16E6B4) DPTR(D_8016DAB4_16E6B4, 0xc) = fn;
}
void func_80218DA8_5D4278(void *a, int radius_times_ten, unsigned short height,
                          short offset) {
  W(a, 0x48) = 0xffffffffu;
  H(a, 0x4e) = (unsigned short)(radius_times_ten / 10);
  H(a, 0x50) = height;
  S(a, 0x52) = offset;
}
/* Verified ABI: (actor, targetX, unusedY, targetZ, speed); a step of twice
 * speed in the 10-bit yaw space, snapping when the shortest difference is
 * strictly smaller. Y is ignored. */
int func_802196FC_5D4BCC(void *a, float x, float unused_y, float z, int speed) {
  (void)unused_y;
  void *o = DPTR(a, 0x18);
  float dx = x - F(o, 8), dz = z - F(o, 0x10);
  int target = (int)(atan2f(dx, dz) * (1024.0f / 6.28318530718f)) & 0x3ff;
  int current = H(o, 0x16) & 0x3ff;
  int delta = ((target - current + 0x200) & 0x3ff) - 0x200;
  int step = speed * 2;
  if (delta > -step && delta < step) {
    H(o, 0x16) = (unsigned short)target;
    return 0;
  }
  H(o, 0x16) = (unsigned short)((current + (delta > 0 ? step : -step)) & 0x3ff);
  return delta > 0 ? 1 : 2;
}
int func_802197D8_5D4CA8(void *a, int turn_step) {
  void *t = a ? DPTR(a, 0x84) : 0;
  if (!t) return -1; /* the native wrapper dereferences it unconditionally */
  return func_802196FC_5D4BCC(a, F(t, 8), F(t, 0xc), F(t, 0x10), turn_step);
}
void func_80034ED4_35AD4(void) {
  ++random_deletes;
  world_dynamic_delete();
}
void func_08007BDC_6C732C(void *a, void *o) {
  (void)o;
  func_802197D8_5D4CA8(a, 0x10);
  short old = S(a, 0x8a);
  S(a, 0x8a) = old - 1;
  if (!old) func_80034ED4_35AD4();
}
void func_08007B28_6C7278(void *a, void *o) {
  (void)o;
  H(a, 0x5e) = 0x12f;
  W(a, 0x60) = 0x2006e3u;
  world_dynamic_animation(a, 0);
  func_8021664C_5D1B1C(a, 0, 0.25f, 1);
  func_80224ABC_5DFF8C(a, 0, 1.0f, 1);
  func_80218DA8_5D4278(a, 400, 0x28, 0);
  S(a, 0x8a) = 0x5a;
  F(a, 0x78) = 7.5f;
  func_8003521C_35E1C((void *)func_08007BDC_6C732C);
}
/* The placed root: roll a period, then attempt exactly one child allocation
 * when the system tick lands on it. */
void func_080079FC_6C714C(void *a, void *o) {
  (void)o;
  ++random_ticks;
  if (random_ticks % random_period) return;
  if (random_alloc_fail) return;
  void *child = func_802171A8_5D2678(a, func_08007B28_6C7278, 1);
  if (!child) return;
  world_dynamic_child(a, child);
  /* func_80218C28_5D40F8 inherits the parent's entity identity and target. */
  H(child, 0x5c) = WR_ENTITY;
  DPTR(child, 0x84) = DPTR(a, 0x84);
  H(child, 0x28) = 0x1e;
  W(child, 0x2c) = (unsigned int)func_800141C4_14DC4(0x1e);
  void *co = DPTR(child, 0x18);
  F(co, 8) = 280.0f;
  F(co, 0xc) = (float)((random_roll % 50) - 780);
  F(co, 0x10) = (float)((random_roll % 40) - 260);
  ++random_spawns;
}

/* --- Fixtures ------------------------------------------------------------ */

static DynamicActor *random_root_fixture(void) {
  fixture();
  D_800C7AB2 = d_room = WR_ROOM;
  placed_actor = 1;
  placed_index = WR_ROOT_PARENT;
  parent_authority = 1;
  random_period = 1;
  random_ticks = random_spawns = random_deletes = random_alloc_fail = 0;
  random_roll = 7;
  /* File30 0x12F binds its own File 0x18F/0x19F pair; the shared harness
   * descriptor would fail the typed resource guard. */
  DPTR(random_model, 0) = random_files;
  DPTR(random_model, 4) = random_clips;
  random_files[0] = 0x18f;
  random_files[1] = 0x19f;
  random_clips[0] = 0x080009f8u;
  D_80236984_5F1E54[WR_CHILD_MODEL] = random_model;
  F(random_target, 8) = 100.0f;
  F(random_target, 0xc) = 0.0f;
  F(random_target, 0x10) = -200.0f;
  F(random_other_target, 8) = -900.0f;
  F(random_other_target, 0xc) = 0.0f;
  F(random_other_target, 0x10) = 700.0f;
  void *a = actors[7], *o = objects[7];
  DPTR(a, 0x18) = o;
  DPTR(a, 0xc) = (void *)func_080079FC_6C714C;
  DPTR(a, 0x84) = random_target;
  H(a, 0x5c) = H(a, 0x5e) = WR_ENTITY;
  B(a, 0x74) = 3;
  B(a, 0x8d) = 1;
  W(a, 0x48) = 0xffffffffu;
  W(a, 0x60) = 0;
  DynamicActor *d = random_register_root(a);
  assert(d && d->kind == WD_RANDOM && !d->row[WR_ROLE]);
  assert(d->row[WD_PHASE] == WR_ROOT && d->row[WD_PARENT] == WR_ROOT_PARENT);
  assert(!d->row[WD_ORDINAL]);
  return d;
}

/* Publish the local checkpoint as the bridge row for this actor. */
static void random_offer(DynamicActor *d, int owner, int token) {
  assert(capture(d));
  memcpy(d->net, d->row, sizeof(d->row));
  d->net[WR_INSTANCE] = (int)d->serial;
  d->net[WR_RECEIPT] = token;
  d->net[WD_OWNER] = owner;
  d->owner = (unsigned int)owner;
  d->dirty = d->have = 1;
}

/* Run the native initializer the way the native scheduler would, then let the
 * common post classify the initialized task. */
static DynamicActor *random_born_child(DynamicActor *root, unsigned int slot) {
  void *a = actors[slot];
  DynamicActor *d = lookup(a);
  (void)root;
  assert(d && d->random_birth && d->row[WR_ROLE] == 1);
  D_8016DAB4_16E6B4 = a;
  func_08007B28_6C7278(a, DPTR(a, 0x18));
  world_dynamic_post(a);
  assert(d->kind == WD_RANDOM && d->random_initialized && d->ready);
  assert(d->row[WD_PHASE] == WR_CHILD && d->row[WD_MODEL] == WR_CHILD_MODEL);
  assert(capture(d));
  return d;
}

/* --- Lifecycle coverage -------------------------------------------------- */

void random_lifecycle_test(void) {
  DynamicActor *root = random_root_fixture();
  DynamicActor *child, *second, *copy;
  int r[WORLD_DYNAMIC_WORDS];
  unsigned int yaw;
  short timer;
  int i;

  /* The two native continuations map to the two phases. */
  assert(random_phase(WR_ROOT) == func_080079FC_6C714C);
  assert(random_phase(WR_CHILD) == func_08007BDC_6C732C);
  assert(capture(root) && anchor_world_random_valid(root->row));
  /* File30 never writes +0x60/+0x64 on the placed root: no motion, no flags. */
  assert(!root->row[WD_FLAGS_LO] && !root->row[WD_FLAGS_HI]);
  assert(!root->row[WD_ANIMATED] && !root->row[WD_FRAME] && !root->row[WD_RATE]);
  assert(!root->row[WR_TARGET_X] && !root->row[WR_TARGET_Z]);

  /* The root rolls and allocates only while the owning client holds it. */
  random_offer(root, (int)d_self, 1);
  tick(root);
  assert(random_spawns == 1);
  child = random_born_child(root, 0);
  assert(root->row[WD_ORDINAL] == 1 && child->row[WD_ORDINAL] == 1);
  assert(child->row[WD_TIMER] == WR_CHILD_TIMER);
  assert(child->row[WD_VX] == 7500 && child->row[WD_SPHERE] == WR_SPHERE);
  assert(child->row[WD_FLAGS_LO] == WR_CHILD_FLAGS_LO);
  assert(child->row[WD_FLAGS_HI] == WR_CHILD_FLAGS_HI);
  assert(child->row[WD_ANIMATED] && child->row[WD_RATE] == WR_CHILD_RATE);
  assert(anchor_world_random_valid(child->row));
  /* The live target object is the published steering input. */
  assert(child->row[WR_TARGET_X] == 10000 && child->row[WR_TARGET_Z] == -20000);

  /* A failed allocation consumes no birth ordinal. */
  random_alloc_fail = 1;
  random_offer(root, (int)d_self, 2);
  tick(root);
  assert(random_spawns == 1 && root->row[WD_ORDINAL] == 1);
  random_alloc_fail = 0;
  random_offer(root, (int)d_self, 3);
  tick(root);
  assert(random_spawns == 2 && root->row[WD_ORDINAL] == 2);
  second = random_born_child(root, 1);
  assert(second->row[WD_ORDINAL] == 2 && child->row[WD_ORDINAL] == 1);

  /* A replica must not overwrite the authority's target with its own +0x84. */
  /* A fresh replica with only a stale local +0x84 must not publish it either. */
  child->owner = 0;
  child->have = 0;
  child->proxy = 1;
  DPTR(child->actor, 0x84) = random_other_target;
  assert(capture(child));
  assert(child->row[WR_TARGET_X] == 10000 && child->row[WR_TARGET_Z] == -20000);
  child->proxy = 0;
  /* With a fresh row from another owner the local +0x84 is not authoritative. */
  child->row[WR_TARGET_X] = 10000;
  child->row[WR_TARGET_Z] = -20000;
  child->owner = 1;
  child->have = 1;
  DPTR(child->actor, 0x84) = random_other_target;
  assert(capture(child));
  assert(child->row[WR_TARGET_X] == 10000 && child->row[WR_TARGET_Z] == -20000);
  /* The owning client republishes its own live target object. */
  child->owner = (unsigned int)d_self;
  DPTR(child->actor, 0x84) = random_target;
  assert(capture(child));
  assert(child->row[WR_TARGET_X] == 10000 && child->row[WR_TARGET_Z] == -20000);
  child->have = 0;

  /* Owner and replica must agree exactly while the lifetime runs down. */
  F(child->actor, 0x78) = F(child->actor, 0x7c) = F(child->actor, 0x80) = 0;
  random_offer(child, (int)d_self, 11);
  memcpy(r, child->row, sizeof(r));
  r[WD_VX] = r[WD_VY] = r[WD_VZ] = 0;
  copy = reconstruct(r);
  assert(copy && copy->proxy && copy->random_birth);
  copy->have = 1;
  copy->owner = 1;
  memcpy(copy->net, r, sizeof(r));
  copy->net[WR_INSTANCE] = (int)copy->serial;
  copy->net[WR_RECEIPT] = 12;
  copy->net[WD_OWNER] = 1;
  copy->dirty = 1;
  tick(copy); /* binds the resource set once, then applies the checkpoint */
  assert(copy->random_initialized && copy->native == (void *)random_native);
  for (i = 0; i < 12; ++i) {
    random_offer(child, (int)d_self, 13 + i);
    memcpy(copy->net, child->row, sizeof(copy->net));
    copy->net[WR_INSTANCE] = (int)copy->serial;
    copy->net[WR_RECEIPT] = 13 + i;
    copy->net[WD_OWNER] = 1;
    copy->owner = 1;
    copy->dirty = copy->have = 1;
    tick(child);
    tick(copy);
  }
  yaw = H(DPTR(child->actor, 0x18), 0x16);
  timer = S(child->actor, 0x8a);
  assert(yaw == H(DPTR(copy->actor, 0x18), 0x16));
  assert(timer == S(copy->actor, 0x8a));
  assert(timer < WR_CHILD_TIMER);

  /* A replica clamps at zero instead of walking its timer negative. */
  S(copy->actor, 0x8a) = 0;
  copy->row[WD_TIMER] = 0;
  copy->net[WD_TIMER] = 0;
  copy->dirty = 0;
  tick(copy);
  assert(S(copy->actor, 0x8a) == 0);
  assert(!F(copy->actor, 0x78) && !F(copy->actor, 0x7c) && !F(copy->actor, 0x80));
  assert(alive(copy) && copy->row[WD_LIFE] != WD_REMOVED);

  /* Freeze without a receipt, then re-arm the checkpointed motion on resume. */
  random_offer(child, (int)d_self, 40);
  child->net[WD_PAUSED] = 1;
  child->dirty = 1;
  tick(child);
  assert(child->random_frozen);
  assert(!W(child->actor, 0x60));
  assert(!F(child->actor, 0x78) && !F(child->actor, 0x7c) && !F(child->actor, 0x80));
  assert(S(child->actor, 0x8a) == child->row[WD_TIMER]);
  /* The bridge row is unchanged: only the frozen edge forces the rewrite. */
  child->net[WD_PAUSED] = 0;
  child->dirty = 0;
  tick(child);
  assert(!child->random_frozen);
  assert(W(child->actor, 0x60) == 0x002006e7u);
  assert(F(child->actor, 0x78) == (float)child->row[WD_VX] / 1000.0f);

  /* Owner expiry: a silent shared removal, no loot, committer is the retirer. */
  S(child->actor, 0x8a) = 1;
  random_offer(child, (int)d_self, 50);
  tick(child);
  assert(S(child->actor, 0x8a) == 0 && !random_deletes);
  S(child->actor, 0x8a) = 0;
  random_offer(child, (int)d_self, 51);
  tick(child);
  assert(random_deletes == 1);
  assert(child->row[WD_LIFE] == WD_REMOVED);
  assert((unsigned int)child->row[WD_COMMITTER] == d_self);
  assert(!child->row[WD_LANDED] && !child->row[WD_OWNER]);

  /* A corrupted checkpoint is rejected outright. */
  memcpy(r, second->row, sizeof(r));
  assert(anchor_world_random_valid(r));
  r[WD_PARENT] = WR_ROOT_PARENT + 1;
  assert(!anchor_world_random_valid(r));
  r[WD_PARENT] = WR_ROOT_PARENT;
  r[WD_MODEL] = WR_ENTITY;
  assert(!anchor_world_random_valid(r));
  r[WD_MODEL] = WR_CHILD_MODEL;
  r[WD_TIMER] = WR_CHILD_TIMER + 1;
  assert(!anchor_world_random_valid(r));
  r[WD_TIMER] = 0;
  r[WD_BUSY] = 1;
  assert(!anchor_world_random_valid(r));
  r[WD_BUSY] = 0;
  r[WD_PATH_TIMER] = 1;
  assert(!anchor_world_random_valid(r));
  r[WD_PATH_TIMER] = 0;
  r[WD_FLAGS_LO] = 0x06e3;
  assert(!anchor_world_random_valid(r));

  /* Disconnect: the tracking record is gone but the live task still ends. */
  void *orphan = second->actor;
  DPTR(orphan, 0x84) = 0;
  S(orphan, 0x8a) = 2;
  D_8016DAB4_16E6B4 = orphan;
  anchor_world_dynamic_room();
  assert(!lookup(orphan));
  random_native(orphan, DPTR(orphan, 0x18));
  assert(DPTR(orphan, 0x84) == DPTR(actors[7], 0x18));
  assert(S(orphan, 0x8a) == 1);
  random_native(orphan, DPTR(orphan, 0x18));
  assert(S(orphan, 0x8a) == 0);
  random_native(orphan, DPTR(orphan, 0x18));
  assert(random_deletes == 2);
}
