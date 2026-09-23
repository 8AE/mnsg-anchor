/* Host harness for the File_40 room 0x6B six-piece counterweight module.
 * Native task storage and the 32-bit descriptor tables are emulated with the
 * shared pointer slots so the module sees its real 4-byte field offsets. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#define WORLD_COUNTERWEIGHT_HOST_TEST 1
#define CP(p, o) TP(p, o)
#define CW_DISABLED (1ul << (sizeof(unsigned long) * 8 - 1))
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#include "../src/world/anchor_world_counterweight.c"

unsigned short D_800C7AB2;
void *D_801FC604_5B8514, *D_8016DAB4_16E6B4;
void *D_80236984_5F1E54[1026];

#define TASKS 26
#define ROOT(i) (i)
#define CHILD(root, slot) (3 + (root) * 6 + (slot))
#define PLAYER 21
#define SPARE 22

static unsigned int tasks[TASKS][64], objects[TASKS][64];
static unsigned short files[] = {0x1eb, 0x160};
static unsigned int slots[] = {0x08000198u, 0x080004a8u, 0x08000798u,
                               0x08000a88u, 0x08000d78u, 0x080010a8u};
static void *model[2];
static int missing = -1, births = 0, fail_birth = 0, native_runs = 0;
static unsigned int next_free = SPARE;
static const float dc_off[6] = {100.0f, 60.0f, 20.0f, 0.0f, 0.0f, 0.0f};
static const float e0_off[6] = {0.0f, 0.0f, 0.0f, -20.0f, -60.0f, -100.0f};

int func_800141C4_14DC4(unsigned int file) {
  return (int)file == missing ? -1 : 0x1234;
}
static void init_task(unsigned int i, CwCallback cb) {
  memset(tasks[i], 0, sizeof(tasks[i]));
  memset(objects[i], 0, sizeof(objects[i]));
  CP(tasks[i], 0x18) = objects[i];
  CP(tasks[i], 0xc) = (void *)cb;
  CB(tasks[i], 0x74) = 1;
}
/* Born exactly as func_802171A8 leaves it: the initializer owns +0xc. */
static void child_birth(unsigned int i, unsigned int slot, unsigned int root) {
  void *o = objects[i];
  init_task(i, cw_inits[slot]);
  CH(tasks[i], 0x5c) = WORLD_COUNTERWEIGHT_ENTITY;
  CW(tasks[i], 0x60) = 0x80000020u;
  CB(tasks[i], 0x6c) = (unsigned char)slot;
  CP(tasks[i], 0xe8) = tasks[root];
  CF(o, 8) = CF(objects[root], 8);
  CF(o, 0xc) = CF(objects[root], 0xc);
  CF(o, 0x10) = CF(objects[root], 0x10);
  for (unsigned int j = 0; j < 3; ++j)
    CF(o, 0x1c + j * 4) = 1.0f;
}
static void root_task(unsigned int i, float x, float z, unsigned short yaw) {
  init_task(i, cw_inits[0]);
  CH(tasks[i], 0x5c) = WORLD_COUNTERWEIGHT_ENTITY;
  CF(objects[i], 8) = x;
  CF(objects[i], 0xc) = -80.0f;
  CF(objects[i], 0x10) = z;
  CH(objects[i], 0x16) = yaw;
}
/* Route a native birth through the module, then let the scheduled initializer
 * run once and be observed by the common post. */
static void birth_child(unsigned int r, unsigned int s) {
  unsigned int i = CHILD(r, s);
  anchor_world_counterweight_child(tasks[ROOT(r)], tasks[i]);
  cw_inits[s](tasks[i], objects[i]);
  anchor_world_counterweight_post(tasks[i]);
}
static void fixture(void) {
  memset(test_ptrs, 0, sizeof(test_ptrs));
  test_ptr_count = 0;
  memset(D_80236984_5F1E54, 0, sizeof(D_80236984_5F1E54));
  D_800C7AB2 = WORLD_COUNTERWEIGHT_ROOM;
  missing = -1;
  births = fail_birth = native_runs = 0;
  next_free = SPARE;
  slots[2] = 0x08000798u;
  files[0] = 0x1eb;
  files[1] = 0x160;
  anchor_world_counterweight_reset(1);
  addresses(); /* Native callback tables are populated before any birth. */
  CP(model, 0) = files;
  CP(model, 4) = slots;
  D_80236984_5F1E54[WORLD_COUNTERWEIGHT_ENTITY] = model;
  root_task(ROOT(0), 0.0f, -20.0f, 256);
  root_task(ROOT(1), -300.0f, 0.0f, 512);
  root_task(ROOT(2), 300.0f, 0.0f, 0);
  for (unsigned int r = 0; r < 3; ++r)
    for (unsigned int s = 0; s < 6; ++s)
      child_birth(CHILD(r, s), s, ROOT(r));
  init_task(PLAYER, 0);
  CH(tasks[PLAYER], 0x5c) = 1;
  D_801FC604_5B8514 = tasks[PLAYER];
  for (unsigned int r = 0; r < 3; ++r) {
    anchor_world_counterweight_register(tasks[ROOT(r)]);
    anchor_world_counterweight_post(tasks[ROOT(r)]);
    for (unsigned int s = 0; s < 6; ++s)
      birth_child(r, s);
  }
  for (unsigned int i = 0; i < 3; ++i) {
    anchor_world_counterweight_control(tasks[ROOT(i)], 0, 0, 1, 0);
    anchor_world_counterweight_begin();
  }
}
static const int pose[3][6] = {{0, -8000, -2000, 0, 256, 0},
                               {-30000, -8000, 0, 0, 512, 0},
                               {30000, -8000, 0, 0, 0, 0}};
/* A real transport row carries only the header words. */
static void header_row(unsigned int index, int *r) {
  memset(r, 0, ANCHOR_WORLD_WORDS * sizeof(*r));
  r[0] = (int)(13 + index);
  r[1] = WORLD_COUNTERWEIGHT_ENTITY;
  r[2] = WORLD_COUNTERWEIGHT;
}
static void full_row(unsigned int index, int *r) {
  header_row(index, r);
  for (unsigned int i = 0; i < 6; ++i)
    r[WC_X + i] = pose[index][i];
  for (unsigned int i = 0; i < 6; ++i)
    r[WC_CHILD_Y + i] = -8000;
  r[WC_PRESENT] = 63;
}
static void step(unsigned int i) {
  CwCallback c = (CwCallback)((unsigned long)CP(tasks[i], 0xc) & ~CW_DISABLED);
  c(tasks[i], objects[i]);
}
static int close_to(float a, float b) { return fabsf(a - b) < 1e-3f; }

/* Native child initializers, in slot order. */
#define CW_INIT_BODY(slot)                                                    \
  do {                                                                        \
    (void)a;                                                                  \
    CW(a, 0x60) = 0x80000020u;                                                \
    CB(a, 0x6c) = (unsigned char)(slot);                                      \
    for (unsigned int j = 0; j < 3; ++j)                                      \
      CF(o, 0x1c + j * 4) = 1.0f;                                             \
    CF(a, 0xdc) = CF(o, 0xc) + dc_off[slot];                                  \
    CF(a, 0xe0) = CF(o, 0xc) + e0_off[slot];                                  \
    CP(a, 0xc) = (void *)cw_updates[slot];                                    \
  } while (0)
void func_08001ECC_6EB01C(void *a, void *o) { CW_INIT_BODY(0); }
void func_080020CC_6EB21C(void *a, void *o) { CW_INIT_BODY(1); }
void func_08002328_6EB478(void *a, void *o) { CW_INIT_BODY(2); }
void func_08002564_6EB6B4(void *a, void *o) { CW_INIT_BODY(3); }
void func_080027A0_6EB8F0(void *a, void *o) { CW_INIT_BODY(4); }
void func_080029FC_6EBB4C(void *a, void *o) { CW_INIT_BODY(5); }
/* Native child updates. They must never run while the module owns the child. */
void func_08001F5C_6EB0AC(void *a, void *o) { (void)a; (void)o; ++native_runs; }
void func_08002160_6EB2B0(void *a, void *o) { (void)a; (void)o; ++native_runs; }
void func_080023BC_6EB50C(void *a, void *o) { (void)a; (void)o; ++native_runs; }
void func_080025F8_6EB748(void *a, void *o) { (void)a; (void)o; ++native_runs; }
void func_08002834_6EB984(void *a, void *o) { (void)a; (void)o; ++native_runs; }
void func_08002A90_6EBBE0(void *a, void *o) { (void)a; (void)o; ++native_runs; }

void *func_802171A8_5D2678(void *parent, CwCallback cb, unsigned char kind) {
  (void)kind;
  assert(parent);
  ++births;
  if (fail_birth)
    return 0;
  unsigned int i = next_free++;
  assert(i < TASKS);
  init_task(i, cb);
  CH(tasks[i], 0x5c) = WORLD_COUNTERWEIGHT_ENTITY;
  CF(objects[i], 8) = CF(CP(parent, 0x18), 8);
  CF(objects[i], 0xc) = CF(CP(parent, 0x18), 0xc);
  CF(objects[i], 0x10) = CF(CP(parent, 0x18), 0x10);
  return tasks[i];
}

static void real_zero_header_capture(void) {
  fixture();
  for (unsigned int r = 0; r < 3; ++r) {
    int row[ANCHOR_WORLD_WORDS];
    header_row(r, row);
    assert(anchor_world_counterweight_capture(tasks[ROOT(r)], row));
    /* The capture fills the pose, not the caller. */
    for (unsigned int i = 0; i < 6; ++i)
      assert(row[WC_X + i] == pose[r][i]);
    for (unsigned int i = 0; i < 6; ++i)
      assert(row[WC_CHILD_Y + i] == -8000);
    assert(row[WC_PRESENT] == 63 && row[WC_BUSY] == 0);
    assert(row[WC_SELECTED] == 0 && row[WC_SPEED] == 0);
    assert(anchor_world_row_valid(row));
    assert(anchor_world_counterweight_valid(row));
    assert(anchor_world_counterweight_apply(tasks[ROOT(r)], row));
  }
}
static void six_roles_move_opposed(void) {
  static const float mid[6] = {-30.0f, -50.0f, -70.0f, -90.0f, -110.0f,
                               -130.0f};
  static const double scale[6] = {1.0, 0.6, 0.2, 0.2, 0.6, 1.0};
  fixture();
  int row[ANCHOR_WORLD_WORDS];
  /* Rest is the retracted state, so every piece starts mid-range. The ridden
   * piece supplies the shared speed and each moving piece applies its own
   * scale: 1, 3/5, 1/5, 1/5, 3/5, 1. */
  for (unsigned int s = 0; s < 6; ++s)
    CF(objects[CHILD(0, s)], 0xc) = mid[s];
  anchor_world_counterweight_control(tasks[ROOT(0)], 0, 0, 1, 8);
  anchor_world_counterweight_begin();
  for (unsigned int s = 0; s < 6; ++s)
    step(CHILD(0, s));
  for (unsigned int s = 0; s < 6; ++s) {
    double d = 0.8 * scale[s];
    float want = (float)(s < 3 ? (double)mid[s] + d : (double)mid[s] - d);
    assert(close_to(CF(objects[CHILD(0, s)], 0xc), want));
  }
  /* The opposite half inverts every direction and moves faster. */
  for (unsigned int s = 0; s < 6; ++s)
    CF(objects[CHILD(0, s)], 0xc) = mid[s];
  anchor_world_counterweight_control(tasks[ROOT(0)], 0, 0, 1, 1);
  anchor_world_counterweight_begin();
  for (unsigned int s = 0; s < 6; ++s)
    step(CHILD(0, s));
  for (unsigned int s = 0; s < 6; ++s) {
    double d = 2.4 * scale[s];
    float want = (float)(s < 3 ? (double)mid[s] - d : (double)mid[s] + d);
    assert(close_to(CF(objects[CHILD(0, s)], 0xc), want));
  }
  /* No rider relaxes toward rest at the slot's own double step. */
  for (unsigned int s = 0; s < 6; ++s)
    CF(objects[CHILD(0, s)], 0xc) = mid[s];
  anchor_world_counterweight_control(tasks[ROOT(0)], 0, 0, 1, 0);
  anchor_world_counterweight_begin();
  step(CHILD(0, 0));
  assert(close_to(CF(objects[CHILD(0, 0)], 0xc), -30.4f));
  step(CHILD(0, 3));
  assert(close_to(CF(objects[CHILD(0, 3)], 0xc), -89.92f));
  /* The upper clamp is the child's own extend offset. */
  CF(objects[CHILD(0, 0)], 0xc) = 19.9f;
  anchor_world_counterweight_control(tasks[ROOT(0)], 0, 0, 1, 8);
  anchor_world_counterweight_begin();
  step(CHILD(0, 0));
  assert(close_to(CF(objects[CHILD(0, 0)], 0xc), 20.0f));
  /* Opposed riders collapse to the highest set bit, deterministically. */
  anchor_world_counterweight_control(tasks[ROOT(0)], 0, 0, 1, 1u | 8u);
  anchor_world_counterweight_begin();
  header_row(0, row);
  assert(anchor_world_counterweight_capture(tasks[ROOT(0)], row));
  assert(row[WC_SELECTED] == 8 && row[WC_SPEED] == 800);
  assert(anchor_world_row_valid(row));
  /* All three roots keep separate aggregates. */
  anchor_world_counterweight_control(tasks[ROOT(1)], 0, 0, 1, 32);
  anchor_world_counterweight_control(tasks[ROOT(2)], 0, 0, 1, 2);
  anchor_world_counterweight_begin();
  for (unsigned int r = 0; r < 3; ++r) {
    header_row(r, row);
    assert(anchor_world_counterweight_capture(tasks[ROOT(r)], row));
    assert(row[WC_SELECTED] == (r == 0 ? 8 : r == 1 ? 32 : 2));
  }
}
static void paused_and_pending_freeze(void) {
  fixture();
  int row[ANCHOR_WORLD_WORDS];
  float before = CF(objects[CHILD(0, 0)], 0xc);
  anchor_world_counterweight_control(tasks[ROOT(0)], 0, 1, 1, 8);
  anchor_world_counterweight_begin();
  for (unsigned int s = 0; s < 6; ++s)
    step(CHILD(0, s));
  assert(CF(objects[CHILD(0, 0)], 0xc) == before);
  /* Pending: no authoritative row has landed yet. */
  anchor_world_counterweight_control(tasks[ROOT(0)], 1, 0, 0, 8);
  anchor_world_counterweight_begin();
  step(CHILD(0, 0));
  assert(CF(objects[CHILD(0, 0)], 0xc) == before);
  /* The local X-distance cull reads task +0x84 as the resolved object. */
  CW(tasks[CHILD(0, 0)], 0x60) = 0;
  CP(tasks[CHILD(0, 0)], 0x84) = objects[PLAYER];
  CF(objects[PLAYER], 8) = 500.0f;
  step(CHILD(0, 0));
  assert(!(CW(tasks[CHILD(0, 0)], 0x60) & 0x80000000u));
  CF(objects[PLAYER], 8) = 0.0f;
  step(CHILD(0, 0));
  assert(CW(tasks[CHILD(0, 0)], 0x60) & 0x80000000u);
  /* An applied checkpoint lands the exact height before prediction resumes. */
  full_row(0, row);
  row[WC_CHILD_Y + 0] = -3000;
  assert(anchor_world_counterweight_apply(tasks[ROOT(0)], row));
  assert(close_to(CF(objects[CHILD(0, 0)], 0xc), -30.0f));
}
static void reconstruction_partial_and_reuse(void) {
  fixture();
  int row[ANCHOR_WORLD_WORDS];
  /* Retiring a piece must drop the parent link so it can never reach a
   * released root through +0xE8. */
  retire_child(&cw_roots[1], 4);
  assert(CP(tasks[CHILD(1, 4)], 0xe8) == 0);
  assert(CW(tasks[CHILD(1, 4)], 0x68) & 2u);
  full_row(1, row);
  /* Slot 4's own range is root Y -60..0, i.e. -14000..-8000 hundredths. */
  row[WC_CHILD_Y + 4] = -10000;
  /* The checkpoint is not confirmed until all six pieces are live. */
  assert(!anchor_world_counterweight_apply(tasks[ROOT(1)], row));
  assert(births == 1);
  unsigned int fresh = SPARE;
  assert(CP(tasks[fresh], 0xc) == (void *)func_080027A0_6EB8F0);
  assert(CH(tasks[fresh], 0x28) == 0x28);
  assert(CP(tasks[fresh], 0xe8) == tasks[ROOT(1)]);
  /* The scheduled initializer runs once, then the pending height lands before
   * the child's first prediction tick. */
  cw_inits[4](tasks[fresh], objects[fresh]);
  anchor_world_counterweight_post(tasks[fresh]);
  assert(close_to(CF(objects[fresh], 0xc), -100.0f));
  assert(native_runs == 0);
  assert(anchor_world_counterweight_apply(tasks[ROOT(1)], row));
  step(fresh);
  assert(native_runs == 0);
  /* No rider: a bottom piece relaxes upward at its own 0.24 step. */
  assert(close_to(CF(objects[fresh], 0xc), -99.76f));
  /* A partial allocation keeps the checkpoint pending and is retried. */
  fixture();
  full_row(2, row);
  retire_child(&cw_roots[2], 5);
  fail_birth = 1;
  assert(!anchor_world_counterweight_apply(tasks[ROOT(2)], row));
  assert(births == 1);
  fail_birth = 0;
  assert(!anchor_world_counterweight_apply(tasks[ROOT(2)], row));
  assert(births == 2);
  assert(CP(tasks[SPARE], 0xc) == (void *)func_080029FC_6EBB4C);
  cw_inits[5](tasks[SPARE], objects[SPARE]);
  anchor_world_counterweight_post(tasks[SPARE]);
  assert(anchor_world_counterweight_apply(tasks[ROOT(2)], row));
  /* Missing resources reject the row without touching the live children. */
  fixture();
  full_row(2, row);
  float keep = CF(objects[CHILD(2, 0)], 0xc);
  missing = files[0];
  assert(!anchor_world_counterweight_apply(tasks[ROOT(2)], row));
  assert(CF(objects[CHILD(2, 0)], 0xc) == keep);
  missing = -1;
  /* The pool reset hook drops the binding without a dangling parent link. */
  anchor_world_counterweight_reuse(tasks[CHILD(2, 3)]);
  assert(find(tasks[CHILD(2, 3)]) == 0);
  assert(!anchor_world_counterweight_apply(tasks[ROOT(2)], row));
  assert(births == 1);
}
static void resident_pins_descriptor(void) {
  fixture();
  int row[ANCHOR_WORLD_WORDS];
  full_row(0, row);
  slots[2] = 0x08000799u; /* one wrong binder command */
  assert(!anchor_world_counterweight_apply(tasks[ROOT(0)], row));
  slots[2] = 0x08000798u;
  files[1] = 0x161;
  assert(!anchor_world_counterweight_apply(tasks[ROOT(0)], row));
  files[1] = 0x160;
  assert(anchor_world_counterweight_apply(tasks[ROOT(0)], row));
  missing = 0x1eb;
  assert(!anchor_world_counterweight_apply(tasks[ROOT(0)], row));
  missing = -1;
  assert(anchor_world_counterweight_apply(tasks[ROOT(0)], row));
}
static void per_root_restore_is_independent(void) {
  fixture();
  int row[ANCHOR_WORLD_WORDS];
  header_row(0, row);
  assert(anchor_world_counterweight_capture(tasks[ROOT(0)], row));
  /* Cull and re-place root 0 in the same pool slot: only that root needs a
   * restore, and its children are gone with it. */
  anchor_world_counterweight_reuse(tasks[ROOT(0)]);
  root_task(ROOT(0), 0.0f, -20.0f, 256);
  anchor_world_counterweight_register(tasks[ROOT(0)]);
  assert(anchor_world_counterweight_needs_restore(tasks[ROOT(0)]));
  assert(!anchor_world_counterweight_needs_restore(tasks[ROOT(1)]));
  assert(!anchor_world_counterweight_needs_restore(tasks[ROOT(2)]));
  /* The other two roots still capture and apply while root 0 restores. */
  header_row(1, row);
  assert(anchor_world_counterweight_capture(tasks[ROOT(1)], row));
  full_row(2, row);
  assert(anchor_world_counterweight_apply(tasks[ROOT(2)], row));
  /* A restoring root does not publish a checkpoint. */
  header_row(0, row);
  assert(!anchor_world_counterweight_capture(tasks[ROOT(0)], row));
}
static void disable_restores_native(void) {
  fixture();
  for (unsigned int s = 0; s < 6; ++s)
    assert(raw(tasks[CHILD(0, s)]) == cw_callback);
  anchor_world_counterweight_control(0, 0, 0, 0, 0);
  anchor_world_counterweight_end();
  for (unsigned int s = 0; s < 6; ++s) {
    assert(raw(tasks[CHILD(0, s)]) == cw_updates[s]);
    step(CHILD(0, s));
  }
  assert(native_runs == 6);
  /* A reset returns every remaining root to its native callback too. */
  anchor_world_counterweight_reset(0);
  for (unsigned int r = 0; r < 3; ++r)
    for (unsigned int s = 0; s < 6; ++s)
      assert(raw(tasks[CHILD(r, s)]) == cw_updates[s]);
}
static void waiting_roots_are_wrapped_not_native(void) {
  fixture();
  /* While the sync waits for a row the native local-only update must not run. */
  for (unsigned int s = 0; s < 6; ++s)
    assert(raw(tasks[CHILD(0, s)]) == cw_callback);
  anchor_world_counterweight_control(tasks[ROOT(0)], 1, 0, 0, 0);
  anchor_world_counterweight_begin();
  for (unsigned int s = 0; s < 6; ++s) {
    assert(raw(tasks[CHILD(0, s)]) == cw_callback);
    step(CHILD(0, s));
  }
  assert(native_runs == 0);
  /* Offline (no begin) leaves the native callbacks untouched. */
  anchor_world_counterweight_control(0, 0, 0, 0, 0);
  anchor_world_counterweight_end();
  for (unsigned int s = 0; s < 6; ++s)
    assert(raw(tasks[CHILD(0, s)]) == cw_updates[s]);
}
static void validator_rejects_bad_rows(void) {
  static const int rel_lo[6] = {0, 0, 0, -2000, -6000, -10000};
  static const int rel_hi[6] = {10000, 6000, 2000, 0, 0, 0};
  static const int bits[7] = {0, 1, 2, 4, 8, 16, 32};
  static const int speeds[7] = {0, 2400, 1600, 800, 800, 1600, 2400};
  int row[ANCHOR_WORLD_WORDS], bad[ANCHOR_WORLD_WORDS];
  struct { int field, value; } cases[] = {
      {0, 12}, {0, 16}, {1, 0x3ca}, {2, 11}, {3, 2}, {4, 1}, {8, 17},
      {16, 31}, {17, 3}, {17, 64}, {18, 1}, {19, 1}, {37, 1}, {39, 1},
      {45, 1}, {46, 64}, {47, -1}, {48, -1}};
  full_row(0, row);
  assert(anchor_world_row_valid(row));
  for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    memcpy(bad, row, sizeof(bad));
    bad[cases[i].field] = cases[i].value;
    assert(!anchor_world_row_valid(bad));
  }
  /* Every child height is bounded by its own native range. */
  for (unsigned int s = 0; s < 6; ++s) {
    memcpy(bad, row, sizeof(bad));
    bad[WC_CHILD_Y + s] = row[WC_Y] + rel_lo[s];
    assert(anchor_world_row_valid(bad));
    bad[WC_CHILD_Y + s] = row[WC_Y] + rel_hi[s];
    assert(anchor_world_row_valid(bad));
    bad[WC_CHILD_Y + s] = row[WC_Y] + rel_lo[s] - 1;
    assert(!anchor_world_row_valid(bad));
    bad[WC_CHILD_Y + s] = row[WC_Y] + rel_hi[s] + 1;
    assert(!anchor_world_row_valid(bad));
  }
  /* Each selected bit maps to exactly one speed. */
  for (unsigned int k = 0; k < 7; ++k) {
    memcpy(bad, row, sizeof(bad));
    bad[WC_SELECTED] = bits[k];
    bad[WC_SPEED] = speeds[k];
    assert(anchor_world_row_valid(bad));
    bad[WC_SPEED] = speeds[k] + 1;
    assert(!anchor_world_row_valid(bad));
  }
  /* A row for another placement index is refused by the live pose check. */
  fixture();
  full_row(0, row);
  assert(!anchor_world_counterweight_apply(tasks[ROOT(1)], row));
  full_row(1, row);
  assert(anchor_world_counterweight_apply(tasks[ROOT(1)], row));
  header_row(1, row);
  assert(anchor_world_counterweight_capture(tasks[ROOT(1)], row));
  assert(!anchor_world_counterweight_capture(tasks[ROOT(0)], row));
}
static void local_support_bits(void) {
  fixture();
  int row[ANCHOR_WORLD_WORDS];
  CP(tasks[PLAYER], 0xa0) = objects[CHILD(0, 2)];
  assert(anchor_world_counterweight_local_inputs(tasks[ROOT(0)]) == 4u);
  CP(tasks[PLAYER], 0xa0) = objects[CHILD(2, 5)];
  assert(anchor_world_counterweight_local_inputs(tasks[ROOT(2)]) == 32u);
  assert(anchor_world_counterweight_local_inputs(tasks[ROOT(0)]) == 0u);
  CP(tasks[PLAYER], 0xa0) = objects[CHILD(1, 0)];
  header_row(1, row);
  assert(anchor_world_counterweight_capture(tasks[ROOT(1)], row));
  assert(row[WC_BUSY] == 1 && row[WC_LOCAL] == 1);
  /* The authoritative aggregate, not the local mask, drives the selection. */
  assert(row[WC_SELECTED] == 0 && row[WC_SPEED] == 0);
  assert(anchor_world_row_valid(row));
  anchor_world_counterweight_control(tasks[ROOT(1)], 0, 0, 1, 1);
  anchor_world_counterweight_begin();
  header_row(1, row);
  assert(anchor_world_counterweight_capture(tasks[ROOT(1)], row));
  assert(row[WC_SELECTED] == 1 && row[WC_SPEED] == 2400);
  assert(anchor_world_row_valid(row));
}
int main(void) {
  real_zero_header_capture();
  six_roles_move_opposed();
  paused_and_pending_freeze();
  reconstruction_partial_and_reuse();
  resident_pins_descriptor();
  per_root_restore_is_independent();
  disable_restores_native();
  waiting_roots_are_wrapped_not_native();
  validator_rejects_bad_rows();
  local_support_bits();
  puts("world counterweight native tests passed");
  return 0;
}
