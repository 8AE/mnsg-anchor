/* File62's placed 0x3D6 container driven through the production hooks with
 * native 32-bit pointer slots. Every substitute below reproduces an audited
 * File_62 body (file62-doll-native-audit-2026-09-19.txt); the nested File_26
 * Doll itself belongs to the dynamic-world API and is only accounted here. */
#include "impact_test_pointers.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define WORLD_DOLL_HOST_TEST 1
#define DP(p,o) TP(p,o)
/* Use an otherwise unused pointer bit on the host; native uses 0x00800000. */
#define WDC_DISABLED (1ul << (sizeof(unsigned long)*8-1))
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
#include "../src/anchor_world_doll.c"

unsigned short D_800C7AB2;
void *D_8016DAB4_16E6B4;
void *D_80236984_5F1E54[1026];

static unsigned char doll_saves[2048], doll_temps[800];
static int missing_file, spawn_result, spawn_calls, sounds, last_sound;
static int blinks, checks, failures;
static const char *scenario;

#define DOLL_SLOTS 4
static unsigned int task_mem[DOLL_SLOTS][0x100/4];
static unsigned int object_mem[DOLL_SLOTS][0x100/4];
static void *tasks[DOLL_SLOTS];
static int task_count;
static void *root, *hitter, *foreign_task;

static unsigned short model_files[] = {0x1fa, 0x16f};
static unsigned int model_slots[] = {0x08000058u};
static void *model[] = {model_files, model_slots};

static void check(int cond, const char *what) {
  ++checks;
  if (!cond) { ++failures; printf("  FAIL [%s] %s\n", scenario, what); }
}
static int near(float a, float b) { return fabsf(a-b) < .02f; }
static unsigned int root_pitch(void) {
  return (unsigned int)DH(DP(root,0x18),0x14)&1023u;
}
static void strike(void) {
  DW(root,0x68) |= 0x80u;
  DP(root,0x38) = hitter;
}

int func_800141C4_14DC4(unsigned int f) {
  return (int)f == missing_file ? -1 : 0x1000;
}
int func_800240DC_24CDC(int f) { assert(f >= 0 && f < 2048); return doll_saves[f]; }
int func_80023E94_24A94(int f) { assert(f >= 0 && f < 800); return doll_temps[f]; }
void func_80023DF0_249F0(int f) { assert(f >= 0 && f < 800); doll_temps[f] = 1; }
void func_80038BC8_397C8(unsigned int s) { ++sounds; last_sound = (int)s; }
/* The parent API: 1 = known or already collected, 0 = retry. */
int anchor_world_dynamic_doll_spawn(void *a, unsigned int slot) {
  assert(a == root);
  assert(slot == 8 || slot == 3);
  ++spawn_calls;
  return spawn_result;
}

/* Audited File_62 callbacks. The engine consumes the contact latch after the
 * dispatch, so the substitutes clear it exactly as the collision system does. */
static void blink(void *a) { (void)a; ++blinks; }
void func_080025A8_723BC8(void *a, void *o) {
  (void)o;
  if (!(DW(a,0x68)&0x80u) || !DP(a,0x38)) { blink(a); return; }
  DW(a,0x68) &= ~0x80u;
  DB(a,0x8d) = 100;
  func_80038BC8_397C8(0x24f);
  DP(a,0xc) = (void *)func_08002618_723C38;
}
void func_08002618_723C38(void *a, void *o) {
  unsigned int v = ((unsigned int)DH(o,0x14)+8u)&1023u;
  if (v > 69u) { DH(o,0x14) = 70; DP(a,0xc) = (void *)func_0800266C_723C8C; }
  else DH(o,0x14) = (unsigned short)v;
}
void func_0800266C_723C8C(void *a, void *o) {
  (void)o;
  /* Save 0xEE first, then the volatile temp bit 2; only both clear births. */
  if (!func_800240DC_24CDC(0xee) && !func_80023E94_24A94(2)) {
    if (spawn_result) { spawn_calls++; func_80023DF0_249F0(2); }
  }
  DP(a,0xc) = (void *)func_08002740_723D60;
}
void func_08002740_723D60(void *a, void *o) {
  unsigned int v = ((unsigned int)DH(o,0x14)-16u)&1023u;
  if (v > 0x300u) {
    DH(o,0x14) = 0;
    func_80038BC8_397C8(0x273);
    DP(a,0xc) = (void *)func_080025A8_723BC8;
  } else DH(o,0x14) = (unsigned short)v;
}

static void *make_task(int i, void (*cb)(void *,void *)) {
  tasks[i] = task_mem[i];
  memset(task_mem[i],0,0x100);
  memset(object_mem[i],0,0x100);
  TP(task_mem[i],0x0c) = (void *)cb;
  TP(task_mem[i],0x18) = object_mem[i];
  DB(task_mem[i],0x74) = 1;
  return task_mem[i];
}
static void step_task(void *a) {
  void (*cb)(void *,void *);
  if (!a || DB(a,0x74) == 0 || (DW(a,0x68)&2u)) return;
  D_8016DAB4_16E6B4 = a;
  cb = (void (*)(void *,void *))DP(a,0xc);
  if (cb && !((unsigned long)cb & WDC_DISABLED)) cb(a,DP(a,0x18));
  anchor_world_doll_post(a);
  /* The generic per-actor integration func_80218F30 would run here. */
  anchor_world_doll_post_return();
}
static void tick(void) {
  int i;
  anchor_world_doll_begin();
  for (i = 0; i < task_count; ++i) step_task(tasks[i]);
}
static void settle(void) {
  int i;
  for (i = 0; i < 4 && raw(root) != doll_callback; ++i) tick();
}
static void setup(void) {
  anchor_world_doll_reset(1);
  memset(task_mem,0,sizeof(task_mem));
  memset(object_mem,0,sizeof(object_mem));
  memset(tasks,0,sizeof(tasks));
  memset(test_ptrs,0,sizeof(test_ptrs)); test_ptr_count = 0;
  memset(doll_saves,0,sizeof(doll_saves));
  memset(doll_temps,0,sizeof(doll_temps));
  memset(D_80236984_5F1E54,0,sizeof(D_80236984_5F1E54));
  D_80236984_5F1E54[WORLD_DOLL_ENTITY] = model;
  D_800C7AB2 = WORLD_DOLL_ROOM_A;
  missing_file = -1; spawn_result = 1; spawn_calls = 0;
  sounds = last_sound = blinks = 0; task_count = 0;
  hitter = object_mem[3];
  foreign_task = make_task(2,func_080025A8_723BC8);
  root = make_task(0,func_080025A8_723BC8);
  DH(root,0x5c) = WORLD_DOLL_ENTITY;
  task_count = 1;
  anchor_world_doll_register(root,8);
  anchor_world_doll_control(root,0,0,0);
  tick(); /* The first pass observes the native initializer and marks ready. */
}
static void row_idle(int *r) {
  memset(r,0,sizeof(int)*ANCHOR_WORLD_WORDS);
  r[1] = WORLD_DOLL_ENTITY; r[2] = WORLD_DOLL_CONTAINER;
}

/* ---- scenarios --------------------------------------------------------- */

static void identity_and_parent_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  scenario = "identity";
  setup(); settle();
  check(raw(root) == doll_callback, "the wrapper is installed once ready");
  check(cur_phase() == DOLL_IDLE, "the fresh container starts idle");
  check(anchor_world_doll_parent_valid(8), "room 0x16A slot 8 is the parent");
  check(anchor_world_doll_parent() == 8, "the parent identity is the slot");
  check(!anchor_world_doll_parent_valid(3), "the other room's slot is rejected");
  row_idle(r);
  check(anchor_world_doll_capture(root,r), "an idle container captures");
  check(r[WDC_PHASE] == 0 && r[WDC_PITCH] == 0 && r[WDC_CYCLE] == 0,
        "an unstarted idle row carries cycle 0");
  /* A recycled task slot must detach the live binding but keep the roster
   * identity: the nested Doll outlives the placed root. */
  DB(root,0x74)++;
  check(!same(), "a recycled generation detaches the binding");
  check(anchor_world_doll_parent_valid(8) && anchor_world_doll_parent() == 8,
        "the parent roster identity survives root culling");
  check(!anchor_world_doll_capture(root,r), "a stale generation cannot capture");
  /* Pool reuse keeps the identity until the room resets. */
  anchor_world_doll_reuse(root);
  check(anchor_world_doll_parent_valid(8), "reuse keeps the parent mapping");
  anchor_world_doll_reset(1);
  check(!anchor_world_doll_parent_valid(8) && !anchor_world_doll_parent(),
        "a room reset clears the parent mapping");
  /* Room 0x182 names the same container by slot 3. */
  setup();
  D_800C7AB2 = WORLD_DOLL_ROOM_B;
  anchor_world_doll_register(root,3);
  check(anchor_world_doll_parent_valid(3) && anchor_world_doll_parent() == 3,
        "room 0x182 uses slot 3");
}

static void room_and_resource_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  scenario = "room and resource";
  setup(); settle(); row_idle(r);
  D_800C7AB2 = 0x161;
  check(!anchor_world_doll_parent_valid(8) && !anchor_world_doll_parent(),
        "a wrong room has no parent identity");
  check(!anchor_world_doll_capture(root,r), "a wrong room refuses capture");
  check(!anchor_world_doll_apply(root,r), "a wrong room refuses apply");
  D_800C7AB2 = WORLD_DOLL_ROOM_A;
  check(anchor_world_doll_capture(root,r), "the right room captures again");
  missing_file = 62;
  check(!anchor_world_doll_capture(root,r), "a missing overlay refuses capture");
  check(!anchor_world_doll_apply(root,r), "a missing overlay refuses apply");
  tick();
  check(doll_suspend, "begin suspends the cycle while a resource is missing");
  missing_file = 0x152;
  check(!anchor_world_doll_capture(root,r), "a missing common file refuses");
  missing_file = -1;
  tick();
  check(!doll_suspend, "the cycle resumes once resources return");
  check(anchor_world_doll_capture(root,r), "the row publishes after recovery");
}

static void owner_cycle_test(void) {
  int r[ANCHOR_WORLD_WORDS], i;
  scenario = "owner cycle";
  setup(); settle();
  anchor_world_doll_control(root,0,0,1); /* confirmed owner */
  strike();
  tick();
  check(cur_phase() == DOLL_OPENING, "an idle contact opens the cycle");
  check(doll_cycle == 1, "the contact starts cycle 1");
  check(root_pitch() == 0, "the opening starts from pitch 0");
  check(last_sound == 0x24f, "the trigger plays the native 0x24F cue");
  for (i = 0; i < 9 && cur_phase() == DOLL_OPENING; ++i) tick();
  check(cur_phase() == DOLL_BIRTH && root_pitch() == 70,
        "the opening walks eighths and clamps to 70");
  tick();
  check(cur_phase() == DOLL_CLOSING && spawn_calls == 1,
        "the birth asks the dynamic API once, then advances");
  for (i = 0; i < 40 && cur_phase() != DOLL_IDLE; ++i) tick();
  check(cur_phase() == DOLL_IDLE && root_pitch() == 0,
        "the closing returns to idle at pitch 0");
  check(last_sound == 0x273, "the closure plays the native 0x273 cue");
  check(sounds == 2, "one cycle emits exactly the two native cues");
  /* The consumed latch must not re-arm the next cycle by itself. */
  for (i = 0; i < 5; ++i) tick();
  check(cur_phase() == DOLL_IDLE && doll_cycle == 1,
        "a consumed contact cannot restart the container");
  row_idle(r);
  check(anchor_world_doll_capture(root,r), "a finished cycle captures");
  check(r[WDC_CYCLE] == 1 && r[WDC_PHASE] == 0,
        "the finished cycle keeps its counter at idle");
  check(anchor_world_row_valid(r), "the completed idle cycle is wire-valid");
  /* A second contact must outweigh the previous close. */
  strike();
  tick();
  check(cur_phase() == DOLL_OPENING && doll_cycle == 2,
        "the next contact opens cycle 2");
  row_idle(r);
  for (i = 0; i < 9 && cur_phase() == DOLL_OPENING; ++i) tick();
  check(anchor_world_doll_capture(root,r), "cycle 2 captures mid-flight");
  check(r[WDC_CYCLE]*4 + r[WDC_PHASE] > 1*4 + 2,
        "a new cycle outranks the previous close");
}

static void unconfirmed_and_peer_test(void) {
  int r[ANCHOR_WORLD_WORDS], i;
  scenario = "unconfirmed and peer";
  /* Enabled but not yet confirmed: the idle contact publishes a proposal and
   * then freezes. It must never run the native birth on its own. */
  setup(); settle();
  anchor_world_doll_control(root,0,0,0);
  strike();
  tick();
  check(cur_phase() == DOLL_OPENING && doll_cycle == 1,
        "an unconfirmed authority proposes the opening");
  check(spawn_calls == 0, "an unconfirmed authority never births a Doll");
  for (i = 0; i < 5; ++i) tick();
  check(cur_phase() == DOLL_OPENING && root_pitch() == 0,
        "the proposal freezes until the owner is confirmed");
  check(spawn_calls == 0, "the freeze keeps the birth unrun");
  /* The self-echo confirms this simulator and the cycle resumes. */
  anchor_world_doll_control(root,0,0,1);
  tick();
  check(root_pitch() == 8, "confirmation resumes the frozen opening");
  /* A peer-owned record turns the container into a waiting replica. */
  setup(); settle();
  anchor_world_doll_control(root,1,0,1);
  strike();
  tick();
  check(cur_phase() == DOLL_OPENING && doll_cycle == 1,
        "a replica accepts its local idle contact as a proposal");
  check(spawn_calls == 0, "a replica never births the peer's Doll");
  row_idle(r);
  r[WDC_CYCLE] = 4; r[WDC_PHASE] = 2; r[WDC_PITCH] = 70; r[WDC_SPAWNED] = 1;
  check(anchor_world_row_valid(r), "the peer birth row is well formed");
  check(anchor_world_doll_apply(root,r), "the peer row applies to the replica");
  check(cur_phase() == DOLL_BIRTH && root_pitch() == 70 && doll_cycle == 4,
        "the replica adopts the owner's cycle and pose");
  check(doll_temps[2], "an applied birth marks the volatile room bit");
  tick();
  check(cur_phase() == DOLL_BIRTH && spawn_calls == 0,
        "a replica waits at the owner's phase and never births");
  row_idle(r);
  r[WDC_CYCLE] = 4; r[WDC_PHASE] = 3; r[WDC_PITCH] = 70;
  check(anchor_world_doll_apply(root,r), "the owner's closing row applies");
  check(cur_phase() == DOLL_CLOSING && root_pitch() == 70,
        "the replica follows the owner into the close");
}

static void paused_freeze_test(void) {
  unsigned int flags;
  float vel;
  int i;
  scenario = "paused freeze";
  setup(); settle();
  anchor_world_doll_control(root,1,1,1);
  strike();
  tick();
  check(cur_phase() == DOLL_IDLE && doll_cycle == 0,
        "a paused container ignores a local contact");
  /* The freeze mask must touch only the tracked root. */
  DW(root,0x60) = 0x016002a1u;
  DF(root,0x78) = 5.0f;
  flags = DW(root,0x60); vel = DF(root,0x78);
  DW(foreign_task,0x60) = 0x12345678u;
  DF(foreign_task,0x78) = 7.0f;
  anchor_world_doll_post(foreign_task);
  anchor_world_doll_post_return();
  check(DW(foreign_task,0x60) == 0x12345678u,
        "a foreign update is not frozen");
  check(DF(foreign_task,0x78) == 7.0f, "a foreign velocity is not touched");
  check(DW(root,0x60) == flags && DF(root,0x78) == vel,
        "the root is left intact by a foreign update");
  anchor_world_doll_post(root);
  check(DW(root,0x60) == (flags & ~0x800301u),
        "the root update masks the integration flags");
  check(DF(root,0x78) == 0.0f, "the root velocity is zeroed while paused");
  anchor_world_doll_post_return();
  check(DW(root,0x60) == flags && DF(root,0x78) == vel,
        "the root flags and velocity are restored");
  /* Unpause resumes the cycle. */
  anchor_world_doll_control(root,0,0,1);
  for (i = 0; i < 20 && cur_phase() == DOLL_IDLE; ++i) {
    strike(); tick();
  }
  check(cur_phase() != DOLL_IDLE, "unpausing resumes the container");
}

static void late_entry_test(void) {
  int r[ANCHOR_WORLD_WORDS];
  scenario = "late entry";
  setup();
  /* The vanilla callbacks advanced the root before the mod was active: no
   * wrapper is installed yet, so the native substitute runs the opening. */
  check(raw(root) == func_080025A8_723BC8,
        "the root starts on its native continuation");
  strike();
  func_080025A8_723BC8(root,DP(root,0x18));
  DH(DP(root,0x18),0x14) = 24;
  check(cur_phase() == DOLL_OPENING, "the offline opening is observable");
  check(doll_cycle == 0, "the mod counter is still unprimed");
  /* Going online observes the native phase before the first capture. */
  settle();
  row_idle(r);
  check(anchor_world_doll_capture(root,r),
        "the online capture primes the observed cycle");
  check(r[WDC_CYCLE] >= 1 && r[WDC_PHASE] == 1, "the observed cycle is legal");
  check(anchor_world_row_valid(r), "the observed row passes the codec");
  /* An unconfirmed authority freezes the observed opening. */
  anchor_world_doll_control(root,0,0,0);
  tick();
  check(root_pitch() == 24, "an unconfirmed authority does not advance it");
}

static void retry_and_apply_test(void) {
  int r[ANCHOR_WORLD_WORDS], i;
  scenario = "retry and apply";
  setup(); settle();
  anchor_world_doll_control(root,0,0,1);
  strike();
  tick();
  for (i = 0; i < 9 && cur_phase() == DOLL_OPENING; ++i) tick();
  check(cur_phase() == DOLL_BIRTH, "the cycle reaches the birth phase");
  /* A refused birth stays pending on its own phase and retries. */
  spawn_result = 0;
  spawn_calls = 0;
  tick(); tick();
  check(cur_phase() == DOLL_BIRTH, "a refused birth does not advance");
  check(spawn_calls == 2, "the refused birth retries every update");
  check(root_pitch() == 70, "the pending birth keeps its pose");
  spawn_result = 1;
  tick();
  check(cur_phase() == DOLL_CLOSING, "the birth proceeds once accepted");
  check(spawn_calls == 3, "the retry stops after success");
  /* A closing row replays the exact native ramp. */
  setup(); settle();
  row_idle(r);
  r[WDC_CYCLE] = 2; r[WDC_PHASE] = 3; r[WDC_PITCH] = 54;
  r[WDC_X] = -600; r[WDC_Y] = 3400; r[WDC_Z] = -10200;
  check(anchor_world_row_valid(r), "the closing row is well formed");
  check(anchor_world_doll_apply(root,r), "the closing row applies");
  check(cur_phase() == DOLL_CLOSING && root_pitch() == 54 && doll_cycle == 2,
        "the closing pose and cycle are adopted");
  check(near(DF(DP(root,0x18),8),-6.0f) && near(DF(DP(root,0x18),0xc),34.0f) &&
        near(DF(DP(root,0x18),0x10),-102.0f),
        "the container transform is hundredths on the wire");
  anchor_world_doll_control(root,0,0,1);
  for (i = 0; i < 6; ++i) tick();
  check(cur_phase() == DOLL_IDLE && root_pitch() == 0,
        "the applied close walks back to idle");
  /* The spawned boolean is monotone and never rebuilds by itself. */
  setup(); settle();
  row_idle(r);
  r[WDC_CYCLE] = 1; r[WDC_PHASE] = 1; r[WDC_PITCH] = 8; r[WDC_SPAWNED] = 1;
  check(anchor_world_doll_apply(root,r), "a spawned row applies");
  check(doll_temps[2], "the applied spawned flag sets the volatile bit");
  row_idle(r);
  check(anchor_world_doll_capture(root,r), "the container captures");
  check(r[WDC_SPAWNED] == 1, "the spawned fact stays reported");
  /* Malformed phase/pitch pairs are refused without touching the object. */
  row_idle(r);
  r[WDC_CYCLE] = 1; r[WDC_PHASE] = 1; r[WDC_PITCH] = 7;
  check(!anchor_world_doll_apply(root,r), "a non-eighth opening pose is refused");
  r[WDC_PITCH] = 8; r[WDC_PHASE] = 0;
  check(!anchor_world_doll_apply(root,r), "idle at a raised pitch is refused");
  r[WDC_CYCLE] = 0; r[WDC_PITCH] = 0; r[WDC_PHASE] = 0;
  check(anchor_world_doll_apply(root,r), "a legal idle row applies");
}

int main(void) {
  identity_and_parent_test();
  room_and_resource_test();
  owner_cycle_test();
  unconfirmed_and_peer_test();
  paused_freeze_test();
  late_entry_test();
  retry_and_apply_test();
  printf("world doll: %d checks, %d failures\n", checks, failures);
  return failures != 0;
}
