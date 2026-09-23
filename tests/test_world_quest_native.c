/* Packet-free quest adapter harness. The actor and object are host buffers;
 * native calls are stubs so this tests identity, guards and lifecycle only. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WORLD_QUEST_HOST_TEST 1
#define RECOMP_HOOK(name)
#define RECOMP_HOOK_RETURN(name)
typedef struct __attribute__((packed)) { void *value; } QuestTestPointer;
#define QP(p, o) (((QuestTestPointer *)((char *)(p) + (o)))->value)
#include "../src/world/anchor_world_quest.c"

unsigned short D_800C7AB2;
void *D_80236984_5F1E54[1026];
void *D_801FC604_5B8514;
void *D_8016DAB4_16E6B4;

static unsigned short model_files[2] = {0x200, 0};
static unsigned int model_clips[16] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static unsigned char actor_source[512], object_source[512];
static unsigned char actor_new[512], object_new[512];
static unsigned char actor_proxy[512], object_proxy[512];
static void (*scheduled)(void *, void *);
static unsigned int deletes;

int world_quest_test_model_tables(unsigned int model,
                                  const unsigned short **files,
                                  const unsigned int **clips) {
  if (!model || model > 1025)
    return 0;
  *files = model_files;
  *clips = model_clips;
  return 1;
}

void *func_800141C4_14DC4(unsigned int file) {
  return file ? model_files : 0;
}
int func_800240DC_24CDC(int flag) { (void)flag; return 0; }
void func_8021664C_5D1B1C(void *a, unsigned int clip, float rate,
                            unsigned int flags) {
  void *o = QP(a, 0x18);
  (void)clip;
  QB(o, 0x7c) = (unsigned char)flags;
  QH(o, 0x7e) = (unsigned short)(rate * 256.0f);
}
void func_80216E1C_5D22EC(void *a, int slot) {
  void *o = QP(a, 0x18);
  (void)slot;
  QF(o, 0x1c) = QF(o, 0x20) = QF(o, 0x24) = 1.0f;
}
void func_80216DF8_5D22C8(void *a, int slot) {
  (void)a;
  assert(slot == 1);
}
void func_80224ABC_5DFF8C(void *a, int clip, float rate, int mode) {
  (void)a; (void)clip; (void)rate; (void)mode;
}
void *func_802171A8_5D2678(void *parent,
                             void (*callback)(void *, void *),
                             unsigned char kind) {
  (void)parent;
  assert(kind == 8);
  memset(actor_proxy, 0, sizeof(actor_proxy));
  memset(object_proxy, 0, sizeof(object_proxy));
  QP(actor_proxy, 0x18) = object_proxy;
  QH(actor_proxy, 0x5c) = 0x316;
  scheduled = callback;
  return actor_proxy;
}
void func_80035020_35C20(void) {}
void func_80034EF8_35AF8(void *a) {
  assert(a == actor_proxy);
  ++deletes;
  QW(a, 0x68) |= 2u;
}
void *func_08002A1C_72403C(void *a, unsigned int alpha,
                             unsigned int r, unsigned int g,
                             unsigned int b) {
  (void)alpha;
  (void)r;
  (void)g;
  (void)b;
  return (char *)QP(a, 0x18) + 0x80;
}
void *func_8021A26C_5D573C(void *a, unsigned int alpha,
                             unsigned int r, unsigned int g,
                             unsigned int b) {
  return func_08002A1C_72403C(a, alpha, r, g, b);
}
void func_08001BD0_70FC80(void *a, void *o) { (void)a; (void)o; }
void func_08001E60_70FF10(void *a, void *o) { (void)a; (void)o; }
void mnsg_string_write_s32(char *out, signed int value) {
  sprintf(out, "%d", value);
}

static void source_actor(void *a, void *o) {
  memset(a, 0, 512);
  memset(o, 0, 512);
  QP(a, 0x18) = o;
  QH(a, 0x5c) = 0x316;
  QH(a, 0x5e) = 0x24e;
  QW(a, 0x60) = 0x220u;
  QF(o, 8) = -3.0f;
  QF(o, 0xc) = 153.0f;
  QF(o, 0x10) = -3.0f;
  QF(o, 0x1c) = QF(o, 0x20) = QF(o, 0x24) = 1.0f;
  QH(o, 0x7e) = 13;
  /* A tagged local pointer, intentionally never published. */
  QW(o, 0x30) = 0xa0123456u;
}

int main(void) {
  int row[WORLD_QUEST_WORDS], offer[1][WORLD_QUEST_WORDS];
  int status[2][WORLD_QUEST_WORDS], decoded[2][WORLD_QUEST_WORDS];
  int parts[2][WORLD_QUEST_WORDS];
  unsigned int count, i;
  char json[WORLD_QUEST_JSON];

  D_800C7AB2 = WORLD_QUEST_ROOM_GATEWAY;
  D_801FC604_5B8514 = actor_source;
  anchor_world_quest_set_self(7);
  source_actor(actor_source, object_source);
  D_8016DAB4_16E6B4 = actor_source;
  quest_gateway_a();
  anchor_world_quest_frame(D_800C7AB2, 1, 1, 1);
  assert(anchor_world_quest_row_count() == 1);
  for (i = 0; i < WORLD_QUEST_WORDS; ++i)
    row[i] = anchor_world_quest_rows()[i];
  assert(row[WQ_GEN] == 0 && row[WQ_COLOUR] == 0 &&
         row[WQ_INSTANCE] > 0 && row[WQ_RECEIPT] == 0);
  assert(row[WQ_SLOT] == 1 && row[WQ_MODEL] == 0x24e);
  assert(!anchor_world_quest_apply(actor_source, row));

  for (i = 0; i < WORLD_QUEST_WORDS; ++i)
    offer[0][i] = row[i];
  offer[0][WQ_SELF] = 11;
  offer[0][WQ_OWNER] = 11;
  offer[0][WQ_INSTANCE] = 0;
  offer[0][WQ_RECEIPT] = 51;
  assert(anchor_world_quest_row_valid(offer[0]));
  assert(anchor_world_quest_receive(offer, 1, 7) == 0);
  assert(!anchor_world_quest_hidden(actor_source));
  assert(scheduled);
  D_8016DAB4_16E6B4 = actor_proxy;
  scheduled(actor_proxy, object_proxy);
  assert(anchor_world_quest_receive(offer, 1, 7) == 1);
  assert(anchor_world_quest_hidden(actor_source));
  assert(QW(actor_source, 0x60) == 0x220u && QB(actor_source, 0xd0) == 0);
  assert((QB(object_source, 0x64) & 1u) != 0);
  assert(anchor_world_quest_status(status, 2) == 1);
  assert(status[0][WQ_INSTANCE] > row[WQ_INSTANCE] &&
         status[0][WQ_RECEIPT] == 51);
  anchor_world_quest_scheduler_begin();
  assert((QB(object_source, 0x64) & 1u) == 0);
  anchor_world_quest_scheduler_end();
  assert((QB(object_source, 0x64) & 1u) != 0);

  offer[0][WQ_COLOUR] = 0x12345678;
  assert(!anchor_world_quest_row_valid(offer[0]));
  assert(anchor_world_quest_receive(offer, 1, 7) == 0);
  assert(anchor_world_quest_hidden(actor_source));
  offer[0][WQ_COLOUR] = 0;
  offer[0][WQ_SLOT] = 2;
  assert(!anchor_world_quest_row_valid(offer[0]));
  offer[0][WQ_SLOT] = 1;

  offer[0][WQ_LIFE] = WQ_REMOVED;
  assert(anchor_world_quest_receive(offer, 1, 7) == 0);
  assert(deletes == 1 && anchor_world_quest_hidden(actor_source));
  assert((QB(object_source, 0x64) & 1u) != 0);

  D_8016DAB4_16E6B4 = actor_source;
  quest_retire_explicit(actor_source);
  source_actor(actor_new, object_new);
  D_8016DAB4_16E6B4 = actor_new;
  quest_gateway_a();
  anchor_world_quest_frame(D_800C7AB2, 1, 1, 1);
  assert(anchor_world_quest_row_count() == 1);
  assert(anchor_world_quest_rows()[WQ_ORDINAL] == 2);
  anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
  assert(anchor_world_quest_row_count() == 1);
  assert(anchor_world_quest_rows()[WQ_ORDINAL] == 2);
  anchor_world_quest_frame(0, 0, 0, 0);
  assert(anchor_world_quest_row_count() == 0);
  anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
  assert(anchor_world_quest_row_count() == 1);

  /* Expired visual births are represented by absence in the complete room
   * snapshot; their slots must be reusable throughout a long scene. */
  for (i = 0; i < 60; ++i) {
    D_8016DAB4_16E6B4 = actor_new;
    quest_retire_explicit(actor_new);
    anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
    assert(anchor_world_quest_row_count() == 0);
    source_actor(actor_new, object_new);
    quest_gateway_a();
    anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
    assert(anchor_world_quest_row_count() == 1);
  }
  assert(anchor_world_quest_rows()[WQ_ORDINAL] == 62);

  /* A complete remote graph with only child A suppresses a local child B
   * without touching either local controller. An empty next snapshot restores
   * both originals. */
  source_actor(actor_source, object_source);
  D_8016DAB4_16E6B4 = actor_source;
  quest_gateway_b();
  anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
  assert(anchor_world_quest_row_count() == 2);
  memcpy(offer[0], anchor_world_quest_rows(), sizeof(row));
  assert(offer[0][WQ_ROLE] == WQ_ROLE_GATEWAY_CHILD_A);
  offer[0][WQ_SELF] = 11;
  offer[0][WQ_OWNER] = 11;
  offer[0][WQ_INSTANCE] = 0;
  offer[0][WQ_RECEIPT] = 53;
  assert(anchor_world_quest_receive(offer, 1, 7) == 0);
  assert(!anchor_world_quest_hidden(actor_source));
  D_8016DAB4_16E6B4 = actor_proxy;
  scheduled(actor_proxy, object_proxy);
  assert(anchor_world_quest_receive(offer, 1, 7) == 1);
  assert(anchor_world_quest_hidden(actor_new));
  assert(anchor_world_quest_hidden(actor_source));
  assert(QW(actor_source, 0x60) == 0x220u);
  assert(anchor_world_quest_receive(0, 0, 7) == 0);
  assert(!anchor_world_quest_hidden(actor_new));
  assert(!anchor_world_quest_hidden(actor_source));

  /* Wire ordinals are one byte. After 255 activations of one role/part,
   * discovery fails closed instead of wrapping onto an old identity. */
  quest_retire_explicit(actor_new);
  anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
  q_activation[WQ_ROLE_GATEWAY_CHILD_A][0] = 255;
  source_actor(actor_new, object_new);
  D_8016DAB4_16E6B4 = actor_new;
  quest_gateway_a();
  anchor_world_quest_frame(D_800C7AB2, 2, 2, 1);
  assert(anchor_world_quest_row_count() == 1);
  assert(anchor_world_quest_rows()[WQ_ROLE] == WQ_ROLE_GATEWAY_CHILD_B);
  assert(q_activation[WQ_ROLE_GATEWAY_CHILD_A][0] == 255);

  /* Distinct part identities decode together; duplicates of the same part
   * cannot enter a complete snapshot. */
  for (i = 0; i < 2; ++i) {
    memcpy(parts[i], row, sizeof(row));
    parts[i][WQ_FAMILY] = WQ_FAMILY_KORYUTA;
    parts[i][WQ_ROLE] = WQ_ROLE_KORYUTA_PART;
    parts[i][WQ_ENTITY] = 0x1b4;
    parts[i][WQ_MODEL] = 0x1b0;
    parts[i][WQ_SLOT] = 3;
    parts[i][WQ_PART] = (int)i + 1;
  }
  assert(anchor_world_quest_encode((const int (*)[WORLD_QUEST_WORDS])parts,
                                   2, json, sizeof(json)));
  assert(anchor_world_quest_decode(json, decoded, &count) && count == 2);
  memcpy(parts[1], parts[0], sizeof(row));
  assert(anchor_world_quest_encode((const int (*)[WORLD_QUEST_WORDS])parts,
                                   2, json, sizeof(json)));
  assert(!anchor_world_quest_decode(json, decoded, &count));

  {
    static const int gmc74_models[22] = {
        0x36a,0x36a,0x351,0x318,0x352,0x353,0xfb,0xfb,0xfb,0xfb,0xfb,
        0xfb,0xfb,0xfb,0x367,0x367,0x367,0x359,0x35a,0x35b,0x367,0x367};
    static const int gmc74_slots[22] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,3,2};
    static const int gmc75_models[3] = {0x2da,0x32c,0x2d6};
    for (i = 0; i < 22; ++i) {
      memcpy(parts[0], row, sizeof(row));
      parts[0][WQ_FAMILY] = WQ_FAMILY_GMC74;
      parts[0][WQ_ROLE] = WQ_ROLE_GMC74_CHILD;
      parts[0][WQ_PARENT] = 4;
      parts[0][WQ_ENTITY] = 0;
      parts[0][WQ_MODEL] = gmc74_models[i];
      parts[0][WQ_SLOT] = gmc74_slots[i];
      parts[0][WQ_PART] = (int)i;
      assert(anchor_world_quest_row_valid(parts[0]));
      if (i == 4 || i == 5) {
        parts[0][WQ_CLIP] = 2;
        assert(anchor_world_quest_row_valid(parts[0]));
        parts[0][WQ_CLIP] = 0;
      }
      parts[0][WQ_MODEL] ^= 1;
      assert(!anchor_world_quest_row_valid(parts[0]));
    }
    for (i = 0; i < 3; ++i) {
      memcpy(parts[0], row, sizeof(row));
      parts[0][WQ_FAMILY] = WQ_FAMILY_GMC75;
      parts[0][WQ_ROLE] = WQ_ROLE_GMC75_CHILD;
      parts[0][WQ_PARENT] = 5;
      parts[0][WQ_ENTITY] = 0x35d;
      parts[0][WQ_MODEL] = gmc75_models[i];
      parts[0][WQ_SLOT] = 0;
      parts[0][WQ_PART] = (int)i;
      assert(anchor_world_quest_row_valid(parts[0]));
    }
  }

  /* Follower can reconstruct a room with no local quest presentation. */
  anchor_world_quest_reset(1);
  anchor_world_quest_frame(D_800C7AB2, 3, 3, 1);
  offer[0][WQ_LIFE] = WQ_LIVE;
  assert(anchor_world_quest_receive(offer, 1, 7) == 0);
  D_8016DAB4_16E6B4 = actor_proxy;
  scheduled(actor_proxy, object_proxy);
  assert(anchor_world_quest_receive(offer, 1, 7) == 1);
  assert(anchor_world_quest_status(status, 2) == 1);
  anchor_world_quest_frame(D_800C7AB2, 3, 3, 0);
  assert(deletes == 3);
  assert(anchor_world_quest_status(status, 2) == 0);
  anchor_world_quest_frame(D_800C7AB2, 3, 3, 1);
  assert(anchor_world_quest_row_count() == 0);

  /* Animated quest child constructors select nonzero initial clips before
   * their discovery hook runs. Capture must not mislabel those first frames. */
  anchor_world_quest_reset(1);
  D_800C7AB2 = WORLD_QUEST_ROOM_GMC;
  anchor_world_quest_frame(D_800C7AB2, 4, 4, 1);
  source_actor(actor_source, object_source);
  QH(actor_source, 0x5c) = 0;
  QH(actor_source, 0x5e) = 0x352;
  QB(object_source, 0x7c) = 1;
  q_source(actor_source, WQ_ROLE_GMC74_CHILD, 1, 4);
  anchor_world_quest_frame(D_800C7AB2, 4, 4, 1);
  assert(anchor_world_quest_row_count() == 1);
  assert(anchor_world_quest_rows()[WQ_CLIP] == 2);
  assert(anchor_world_quest_rows()[WQ_PART] == 4);
  assert(anchor_world_quest_row_valid(anchor_world_quest_rows()));
  anchor_world_quest_reset(1);
  D_800C7AB2 = WORLD_QUEST_ROOM_KORYUTA;
  anchor_world_quest_frame(0, 0, 0, 0);
  source_actor(actor_source, object_source);
  QH(actor_source, 0x5c) = 0x1b4;
  QH(actor_source, 0x5e) = 0x1b0;
  QB(object_source, 0x7c) = 1;
  q_source(actor_source, WQ_ROLE_KORYUTA_PART, 1, 5);
  anchor_world_quest_frame(0, 0, 0, 0);
  assert(anchor_world_quest_row_count() == 0);
  anchor_world_quest_frame(D_800C7AB2, 5, 5, 1);
  assert(anchor_world_quest_row_count() == 1);
  assert(anchor_world_quest_rows()[WQ_ENTITY] == 0x1b4);
  assert(anchor_world_quest_rows()[WQ_CLIP] == 2);
  anchor_world_quest_frame(0, 0, 0, 0);
  anchor_world_quest_frame(D_800C7AB2, 5, 5, 1);
  assert(anchor_world_quest_row_count() == 1);
  anchor_world_quest_reset(1);
  D_800C7AB2 = WORLD_QUEST_ROOM_GATEWAY;
  anchor_world_quest_frame(D_800C7AB2, 6, 6, 1);
  source_actor(actor_source, object_source);
  QH(actor_source, 0x5e) = 0x31b;
  QB(object_source, 0x7c) = 1;
  QW(actor_source, 0xd0) = 4;
  QW(actor_source, 0xd4) = 37;
  q_source(actor_source, WQ_ROLE_GATEWAY_NPC, 1, 0);
  quest_animation_select(actor_source, 4, 0.3f, 1);
  q_source(actor_source, WQ_ROLE_GATEWAY_NPC, 1, 0);
  anchor_world_quest_frame(D_800C7AB2, 6, 6, 1);
  assert(anchor_world_quest_row_count() == 1);
  assert(anchor_world_quest_rows()[WQ_CLIP] == 4);
  assert(anchor_world_quest_rows()[WQ_PHASE] == 4);
  assert(anchor_world_quest_rows()[WQ_TIMER] == 37);
  puts("quest native harness: PASS");
  return 0;
}
