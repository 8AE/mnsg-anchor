/* Shared-presentation adapter for the custom quest scenes.
 *
 * The four families (Gateway Viewpoint File_53, Koryuta's flight File_46,
 * Kihachi's scene File_62, Gorgeous Music Castle File_74/75) each interleave
 * portable visual scalars with a local scene handshake inside the same task
 * fields. This module keeps every local coordinator, player/dialogue script,
 * camera, fade and temporary bit untouched. It reads the visible scalars,
 * hides the original render of a tracked actor only while a reconstructed
 * proxy is active, and writes the same scalars back onto a render-only proxy
 * that binds native models and resources with a pure initializer.
 *
 * The row schema, the ABI enumeration and the API contract live in
 * include/world/anchor_world_quest.h. JSON transport is owned by the parent. */
#ifndef WORLD_QUEST_HOST_TEST
#include "platform/modding.h"
#endif
#include "world/anchor_world_quest.h"
#include "utils/string_utils.h"

#define QB(p, o) (*(unsigned char *)((char *)(p) + (o)))
#define QH(p, o) (*(unsigned short *)((char *)(p) + (o)))
#define QS(p, o) (*(short *)((char *)(p) + (o)))
#define QW(p, o) (*(unsigned int *)((char *)(p) + (o)))
#define QF(p, o) (*(float *)((char *)(p) + (o)))
#ifndef QP
#define QP(p, o) (*(void **)((char *)(p) + (o)))
#endif

/* Runtime stores keep relocatable overlay imports out of a static table,
 * which RecompModTool cannot resolve. */
extern unsigned short D_800C7AB2;
extern void *D_80236984_5F1E54[];
extern void *func_800141C4_14DC4(unsigned int);
extern int func_800240DC_24CDC(int);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
extern void func_80216E1C_5D22EC(void *, int);
extern void func_80216DF8_5D22C8(void *, int);
extern void func_80224ABC_5DFF8C(void *, int, float, int);
extern void *func_802171A8_5D2678(void *, void (*)(void *, void *),
                                    unsigned char);
extern void func_80035020_35C20(void);
extern void func_80034EF8_35AF8(void *);
extern void *func_08002A1C_72403C(void *, unsigned int, unsigned int,
                                    unsigned int, unsigned int);
extern void *func_8021A26C_5D573C(void *, unsigned int, unsigned int,
                                    unsigned int, unsigned int);
extern void *D_801FC604_5B8514;
extern void *D_8016DAB4_16E6B4;
extern void func_08001BD0_70FC80(void *, void *);
extern void func_08001E60_70FF10(void *, void *);

/* The native render-hide contract: object+0x64 bit 0 short-circuits the model
 * draw path func_80016C44_17844 (lbu $t6,0x64($a0) / andi $t7,$t6,0x1 /
 * beql $t7,$zero,<draw>). It must never retire the object, unbind the model,
 * alter scale or touch the separate collision body. */
#define QUEST_HIDE_BIT 0x01u
#define QUEST_DELETED_BIT 0x02u
/* The mesh fade driver func_08001E60_70FF10 is reached through private+0x20
 * by the mesh render callback func_80025B38. Replacing that slot keeps
 * temporary bits 8/0xC/0xD/0xF/0x10 and the native teardown local. */
#define QUEST_MESH_FADE_SLOT 0x20
#define QUEST_MESH_FIRST_SLOT 0x1C
#define QUEST_MESH_FLAGS_WORD 0x2D41u
/* The native shadow allocation result lives in actor+0x60 bit 0x08000000 and
 * belongs to the local scene, so a peer row must never overwrite it. */
#define QUEST_LOCAL_FLAGS 0x08000000u
#define QUEST_EMPTY_RECEIPT 0u

typedef struct {
  void *actor, *object;
  unsigned int self, owner, room, serial, instance, receipt;
  unsigned int clip, rate, anim;
  unsigned char family, role, ordinal, part;
  unsigned char generation, ready, proxy, hidden, saved;
  unsigned char saved64, mesh_constructed;
  unsigned char scheduled, primed, retired;
  int offered[WORLD_QUEST_WORDS];
} QuestNode;

static QuestNode q_nodes[WORLD_QUEST_MAX];
static int q_rows[WORLD_QUEST_MAX][WORLD_QUEST_WORDS];
static unsigned int q_row_count;
static unsigned int q_room = 0xffffu;
static unsigned int q_visit;
static unsigned int q_signature;
static unsigned int q_serial;
static unsigned int q_instance;
static unsigned int q_self;
static void *q_roots[6];
static unsigned char q_root_gen[6];
static unsigned char q_activation[WQ_ROLE_GMC75_CHILD + 1][32];
static unsigned int q_hidden;
static int q_active;
static void *q_scope_actor;
static void *volatile q_native_fade;

static void q_clear(void *p, unsigned int size) {
  unsigned int i;
  for (i = 0; i < size; ++i)
    ((volatile unsigned char *)p)[i] = 0;
}

/* ---- recipes ----------------------------------------------------------- */

static unsigned int role_family(unsigned int role) {
  switch (role) {
    case WQ_ROLE_GATEWAY_NPC:
    case WQ_ROLE_GATEWAY_CHILD_A:
    case WQ_ROLE_GATEWAY_CHILD_B:
    case WQ_ROLE_GATEWAY_DECOR:
    case WQ_ROLE_GATEWAY_MESH:
      return WQ_FAMILY_GATEWAY;
    case WQ_ROLE_KORYUTA_BODY:
    case WQ_ROLE_KORYUTA_PART:
      return WQ_FAMILY_KORYUTA;
    case WQ_ROLE_KIHACHI_NPC:
    case WQ_ROLE_KIHACHI_RISE:
    case WQ_ROLE_KIHACHI_COLOUR:
      return WQ_FAMILY_KIHACHI;
    case WQ_ROLE_GMC74_ROOT:
    case WQ_ROLE_GMC74_CHILD:
      return WQ_FAMILY_GMC74;
    case WQ_ROLE_GMC75_ROOT:
    case WQ_ROLE_GMC75_CHILD:
      return WQ_FAMILY_GMC75;
  }
  return WQ_FAMILY_NONE;
}

static int family_room(unsigned int family, unsigned int room) {
  switch (family) {
    case WQ_FAMILY_GATEWAY:
      return room == WORLD_QUEST_ROOM_GATEWAY;
    case WQ_FAMILY_KORYUTA:
      return room == WORLD_QUEST_ROOM_KORYUTA;
    case WQ_FAMILY_KIHACHI:
      return room == WORLD_QUEST_ROOM_KIHACHI_A ||
             room == WORLD_QUEST_ROOM_KIHACHI_B;
    case WQ_FAMILY_GMC74:
    case WQ_FAMILY_GMC75:
      return room == WORLD_QUEST_ROOM_GMC;
  }
  return 0;
}

/* Exact per-part rendered model. The invisible File_74/75 roots are not rows. */
static unsigned int role_model(unsigned int role, unsigned int part) {
  switch (role) {
    case WQ_ROLE_GATEWAY_NPC: return 0x31b;
    case WQ_ROLE_GATEWAY_CHILD_A: return 0x24e;
    case WQ_ROLE_GATEWAY_CHILD_B: return 0x24e;
    case WQ_ROLE_GATEWAY_DECOR: return 0x33f;
    case WQ_ROLE_GATEWAY_MESH: return 0x24e;
    case WQ_ROLE_KORYUTA_BODY: return 0x1b0;
    /* Every 0x1B4 part is re-tagged to render as model 0x1B0. */
    case WQ_ROLE_KORYUTA_PART: return 0x1b0;
    case WQ_ROLE_KIHACHI_NPC: return 0x315;
    case WQ_ROLE_KIHACHI_RISE: return 0x10d;
    case WQ_ROLE_KIHACHI_COLOUR: return 1;
    case WQ_ROLE_GMC74_CHILD:
      /* File_74 private pointers +0x04..+0x58, part 0..21. */
      if (part <= 1) return 0x36a;
      if (part == 2) return 0x351;
      if (part == 3) return 0x318;
      if (part == 4) return 0x352;
      if (part == 5) return 0x353;
      if (part >= 6 && part <= 13) return 0xfb;
      if (part == 17) return 0x359;
      if (part == 18) return 0x35a;
      if (part == 19) return 0x35b;
      if (part < WORLD_QUEST_GMC74_CHILDREN) return 0x367;
      return 0;
    case WQ_ROLE_GMC75_CHILD:
      if (part == 0) return 0x2da;
      if (part == 1) return 0x32c;
      if (part == 2) return 0x2d6;
      return 0;
  }
  return 0;
}

/* Highest clip index the native callbacks of this role ever select. */
static unsigned int role_clip_max(unsigned int role, unsigned int part) {
  switch (role) {
    case WQ_ROLE_GATEWAY_NPC: return 4;
    case WQ_ROLE_KORYUTA_PART: return part == 5 ? 2 : 0;
    case WQ_ROLE_GMC74_CHILD:
      if (part == 0) return 10;
      if (part == 1) return 11;
      if (part == 2) return 5;
      if (part == 3) return 3;
      if (part == 4 || part == 5) return 2;
      return 0;
    case WQ_ROLE_GMC75_CHILD:
      if (part == 0) return 5;
      if (part == 1) return 6;
      if (part == 2) return 2;
      return 0;
  }
  return 0;
}

static unsigned int role_slot(unsigned int role, unsigned int part) {
  switch (role) {
    case WQ_ROLE_GATEWAY_CHILD_A: return 1;
    case WQ_ROLE_GATEWAY_CHILD_B: return 2;
    case WQ_ROLE_GATEWAY_MESH: return 2;
    case WQ_ROLE_KORYUTA_PART:
      if (part == 5) return 0;
      if (part <= 7) return 3;
      return part - 4; /* 8..11 -> static slots 4..7 */
    case WQ_ROLE_GMC74_CHILD:
      if (part == 16) return 1;
      if (part == 20) return 3;
      if (part == 21) return 2;
      return 0;
    default: return 0;
  }
}

static unsigned int role_file(unsigned int role) {
  switch (role_family(role)) {
    case WQ_FAMILY_GATEWAY: return 0x35;
    case WQ_FAMILY_KORYUTA: return 0x2e;
    case WQ_FAMILY_KIHACHI: return 0x3e;
    case WQ_FAMILY_GMC74: return 0x4a;
    case WQ_FAMILY_GMC75: return 0x4b;
  }
  return 0;
}

/* Verified default clip and rate (1/256ths) of a role. A role with no native
 * animation binding stays at clip 0 with animation disabled. */
static void role_anim(unsigned int role, unsigned int part, unsigned int *clip,
                      unsigned int *rate, unsigned int *anim) {
  *clip = 0;
  *rate = 0;
  *anim = 0;
  switch (role) {
    case WQ_ROLE_GATEWAY_NPC:
      *clip = 3;
      *rate = 128; /* File_53's first visible phase selects 0.5 */
      *anim = 1;
      return;
    case WQ_ROLE_GATEWAY_CHILD_A:
      *rate = 13; /* 0.05 */
      *anim = 1;
      return;
    case WQ_ROLE_KORYUTA_BODY:
      *rate = 26; /* 0.1 */
      *anim = 2;
      return;
    case WQ_ROLE_KORYUTA_PART:
      if (part == 5) {
        *clip = 2;
        *rate = 64;
        *anim = 1;
      }
      return;
    case WQ_ROLE_KIHACHI_NPC:
      *rate = 85; /* 0.3333 */
      *anim = 1;
      return;
    case WQ_ROLE_GMC74_CHILD:
      if (part <= 13) {
        *anim = 1;
        *clip = part == 1 ? 1 : part == 2 ? 4 :
                (part >= 3 && part <= 5 ? 2 : 0);
        *rate = part <= 1 ? 64 : part <= 5 ? 102 : 85;
      }
      return;
    case WQ_ROLE_GMC75_CHILD:
      *rate = 64;
      *anim = 1;
      return;
  }
}

/* Highest part/child index a role stages, or 0 when it stages none. */
static unsigned int role_parts(unsigned int role) {
  switch (role) {
    case WQ_ROLE_KORYUTA_PART: return WORLD_QUEST_KORYUTA_PARTS;
    case WQ_ROLE_GMC74_CHILD: return WORLD_QUEST_GMC74_CHILDREN - 1u;
    case WQ_ROLE_GMC75_CHILD: return WORLD_QUEST_GMC75_CHILDREN - 1u;
  }
  return 0;
}

static unsigned int role_part_min(unsigned int role) {
  return role == WQ_ROLE_KORYUTA_PART ? 1u : 0u;
}

/* Only the File_53 roles keep a callback phase and timer at actor+0xD0/+0xD4.
 * For every other role those words are a private allocation pointer or an
 * unrelated scalar, so they are published as zero rather than as garbage. */
static int role_phase(unsigned int role) {
  return role == WQ_ROLE_GATEWAY_NPC || role == WQ_ROLE_GATEWAY_CHILD_A ||
         role == WQ_ROLE_GATEWAY_CHILD_B;
}

unsigned int anchor_world_quest_root_slot(unsigned int room,
                                          unsigned int family) {
  switch (family) {
    case WQ_FAMILY_GATEWAY:
      return room == WORLD_QUEST_ROOM_GATEWAY ? WORLD_QUEST_SLOT_GATEWAY : 0;
    case WQ_FAMILY_KORYUTA:
      return room == WORLD_QUEST_ROOM_KORYUTA ? WORLD_QUEST_SLOT_KORYUTA : 0;
    case WQ_FAMILY_KIHACHI:
      if (room == WORLD_QUEST_ROOM_KIHACHI_A)
        return WORLD_QUEST_SLOT_KIHACHI_A;
      if (room == WORLD_QUEST_ROOM_KIHACHI_B)
        return WORLD_QUEST_SLOT_KIHACHI_B;
      return 0;
    case WQ_FAMILY_GMC74:
      return room == WORLD_QUEST_ROOM_GMC ? WORLD_QUEST_SLOT_GMC74 : 0;
    case WQ_FAMILY_GMC75:
      return room == WORLD_QUEST_ROOM_GMC ? WORLD_QUEST_SLOT_GMC75 : 0;
  }
  return 0;
}

/* ---- resources --------------------------------------------------------- */

static int model_tables(unsigned int model, const unsigned short **files,
                        const unsigned int **clips) {
#ifdef WORLD_QUEST_HOST_TEST
  extern int world_quest_test_model_tables(unsigned int,
                                           const unsigned short **,
                                           const unsigned int **);
  return world_quest_test_model_tables(model, files, clips);
#else
  void *m;
  if (model > 1025u)
    return 0;
  m = D_80236984_5F1E54[model];
  if (!m || !QP(m, 0) || !QP(m, 4))
    return 0;
  *files = (const unsigned short *)QP(m, 0);
  *clips = (const unsigned int *)QP(m, 4);
  return *files && *clips;
#endif
}

/* A missing resource must never reach the native binder. */
static int model_resident(unsigned int model) {
  const unsigned short *files;
  const unsigned int *clips;
  void *resource;
  if (!model_tables(model, &files, &clips))
    return 0;
  resource = func_800141C4_14DC4(files[0]);
  if (!resource || resource == (void *)(unsigned long)0xffffffffu)
    return 0;
  resource = func_800141C4_14DC4(files[1]);
  if (files[1] && (!resource ||
                   resource == (void *)(unsigned long)0xffffffffu))
    return 0;
  return 1;
}

/* The model's clip table is zero-terminated, so an absent slot is refused
 * rather than trusted. Clip indices are bounded by the role recipe. */
static int clip_resident(unsigned int model, unsigned int clip) {
  const unsigned short *files;
  const unsigned int *clips;
  if (clip > 15u)
    return 0;
  if (!model_tables(model, &files, &clips))
    return 0;
  return clips[clip] != 0;
}

/* ---- node tracking ----------------------------------------------------- */

static QuestNode *q_lookup(void *actor) {
  unsigned int i;
  if (!actor)
    return 0;
  for (i = 0; i < WORLD_QUEST_MAX; ++i)
    if (q_nodes[i].actor == actor && !q_nodes[i].retired)
      return &q_nodes[i];
  return 0;
}

static QuestNode *q_alloc(void *actor) {
  QuestNode *n = q_lookup(actor);
  unsigned int i;
  if (n && n->object &&
      (QP(actor, 0x18) != n->object ||
       QB(actor, 0x74) != n->generation ||
       (QW(actor, 0x68) & QUEST_DELETED_BIT))) {
    /* Native recycled the task slot. The old incarnation and receipt must
     * never authorize a replacement in the new scene. */
    if (n->hidden && q_hidden)
      --q_hidden;
    q_clear(n, sizeof(*n));
    n = 0;
  }
  if (n) {
    /* A task whose object had not been installed yet becomes live as soon as
     * the engine publishes it. */
    if (!n->object) {
      n->object = QP(actor, 0x18);
      n->generation = QB(actor, 0x74);
      n->ready = n->object ? 1 : 0;
    }
    return n;
  }
  for (i = 0; i < WORLD_QUEST_MAX; ++i)
    if (!q_nodes[i].actor)
      break;
  if (i == WORLD_QUEST_MAX)
    return 0;
  n = &q_nodes[i];
  q_clear(n, sizeof(*n));
  n->actor = actor;
  n->object = QP(actor, 0x18);
  n->generation = QB(actor, 0x74);
  n->room = D_800C7AB2;
  n->serial = ++q_serial;
  if (!n->serial)
    n->serial = ++q_serial;
  n->instance = ++q_instance;
  if (!n->instance)
    n->instance = ++q_instance;
  n->self = q_self;
  n->ready = n->object ? 1 : 0;
  return n;
}

static void q_unbind(QuestNode *n) {
  if (!n)
    return;
  if (n->hidden && q_hidden)
    --q_hidden;
  q_clear(n, sizeof(*n));
}

/* A recycled task slot keeps the pointer but changes generation. The object
 * pointer and the generation byte together prove this is still the same live
 * instance; the +0x68 bit 1 is the native removal flag. */
static int q_live(QuestNode *n) {
  return !n->retired && n->actor && n->object && n->ready &&
         QP(n->actor, 0x18) == n->object &&
         QB(n->actor, 0x74) == n->generation &&
         !(QW(n->actor, 0x68) & QUEST_DELETED_BIT);
}

/* ---- render hide ------------------------------------------------------- */

int anchor_world_quest_hidden(void *actor) {
  QuestNode *n = q_lookup(actor);
  return n && n->hidden ? 1 : 0;
}

int anchor_world_quest_hide(void *actor, int hidden) {
  QuestNode *n = q_lookup(actor);
  if (!n || !n->object)
    return 0;
  if (hidden) {
    if (!n->hidden) {
      n->saved64 = QB(n->object, 0x64);
      n->saved = 1;
    }
    QB(n->object, 0x64) = (unsigned char)(n->saved64 | QUEST_HIDE_BIT);
    if (!n->hidden)
      ++q_hidden;
    n->hidden = 1;
    return 1;
  }
  if (n->hidden) {
    QB(n->object, 0x64) = n->saved64;
    n->hidden = 0;
    if (q_hidden)
      --q_hidden;
  }
  return 1;
}

void anchor_world_quest_release(void *actor) {
  QuestNode *n = q_lookup(actor);
  int proxy;
  if (!n)
    return;
  proxy = n->proxy;
  if (!proxy && D_800C7AB2 == n->room && q_live(n))
    anchor_world_quest_hide(actor, 0);
  q_unbind(n);
  if (proxy && D_800C7AB2 == q_room)
    func_80034EF8_35AF8(actor);
}

/* The native per-actor integration must observe the original render byte, so
 * the hide is lifted for the duration of the update and re-applied afterwards.
 * The render, which runs later in the frame, therefore still sees the hide. */
void anchor_world_quest_scope_begin(void *actor) {
  QuestNode *n = q_lookup(actor);
  if (!n || !n->hidden || !n->object)
    return;
  QB(n->object, 0x64) = n->saved64;
}

void anchor_world_quest_scope_end(void *actor) {
  QuestNode *n = q_lookup(actor);
  if (!n || !n->hidden || !n->object)
    return;
  QB(n->object, 0x64) = (unsigned char)(n->saved64 | QUEST_HIDE_BIT);
}

/* ---- lifecycle --------------------------------------------------------- */

static void q_release_all(void) {
  unsigned int i;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    void *actor;
    int proxy, retired;
    if (!n->actor)
      continue;
    actor = n->actor;
    proxy = n->proxy;
    retired = n->retired;
    if (!proxy && D_800C7AB2 == n->room && q_live(n))
      anchor_world_quest_hide(actor, 0);
    q_unbind(n);
    if (proxy && !retired && D_800C7AB2 == q_room)
      func_80034EF8_35AF8(actor);
  }
  q_hidden = 0;
  q_row_count = 0;
  for (i = 0; i < 6; ++i) {
    q_roots[i] = 0;
    q_root_gen[i] = 0;
  }
  q_clear(q_activation, sizeof(q_activation));
}

static void q_drop_proxies(void) {
  unsigned int i;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (n->actor && !n->proxy && !n->retired && n->hidden && q_live(n))
      anchor_world_quest_hide(n->actor, 0);
  }
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (n->actor && n->proxy && !n->retired)
      anchor_world_quest_release(n->actor);
  }
}

static void q_new_scope(void) {
  unsigned int i;
  q_drop_proxies();
  q_row_count = 0;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (n->retired || (n->actor && !q_live(n))) {
      q_unbind(n);
      continue;
    }
    if (n->actor) {
      n->receipt = QUEST_EMPTY_RECEIPT;
      n->owner = 0;
    }
  }
  for (i = 1; i <= WQ_FAMILY_GMC75; ++i)
    if (q_roots[i] &&
        (QB(q_roots[i], 0x74) != q_root_gen[i] ||
         (QW(q_roots[i], 0x68) & QUEST_DELETED_BIT)))
      q_roots[i] = 0;
}

void anchor_world_quest_room(unsigned int room) {
  if (q_room == room)
    return;
  q_release_all();
  q_room = room;
  q_active = 0;
  q_visit = 0;
  q_signature = 0;
}

void anchor_world_quest_reset(int room_changed) {
  if (!room_changed) {
    q_drop_proxies();
    return;
  }
  q_release_all();
  q_room = 0xffffu;
  q_active = 0;
  q_visit = 0;
  q_signature = 0;
  q_serial = 0;
}

unsigned int anchor_world_quest_room_id(void) { return q_room; }

void anchor_world_quest_set_self(unsigned int self_id) {
  unsigned int i;
  q_self = self_id;
  for (i = 0; i < WORLD_QUEST_MAX; ++i)
    if (q_nodes[i].actor && !q_nodes[i].proxy)
      q_nodes[i].self = self_id;
}

const int *anchor_world_quest_rows(void) { return &q_rows[0][0]; }

unsigned int anchor_world_quest_row_count(void) { return q_row_count; }

/* ---- capture ----------------------------------------------------------- */

static int q_quantize(float value, float scale, int lo, int hi, int *out) {
  volatile union {
    float f;
    unsigned int u;
  } v;
  v.f = value;
  if ((v.u & 0x7f800000u) == 0x7f800000u)
    return 0;
  value *= scale;
  if (!(value >= (float)lo && value <= (float)hi))
    return 0;
  *out = (int)value;
  return 1;
}

static int q_durable(unsigned int family) {
  switch (family) {
    /* Dialogue 0x2F4 owns the Gateway durable commit; the checkpoint only
     * reports it and never manufactures it. */
    case WQ_FAMILY_GATEWAY: return func_800240DC_24CDC(0x17) ? 1 : 0;
    /* Terminal Koryuta departure, set by File_46 and File_58. */
    case WQ_FAMILY_KORYUTA: return func_800240DC_24CDC(0xc2) ? 1 : 0;
    /* The Kihachi controller is enabled by save 0x23. */
    case WQ_FAMILY_KIHACHI: return func_800240DC_24CDC(0x23) ? 0 : 1;
  }
  return 0;
}

static int q_capture_common(QuestNode *n, unsigned int role,
                            unsigned int ordinal, int *row) {
  void *a = n->actor, *o = n->object;
  unsigned int family = role_family(role);
  unsigned int model = QH(a, 0x5e), expected = role_model(role, n->part);
  unsigned int clip = n->clip, rate = QH(o, 0x7e);
  unsigned int anim = QB(o, 0x7c) & 7u;
  unsigned int i;
  if (!family || !family_room(family, D_800C7AB2))
    return 0;
  /* A task this module constructed is a proxy, never a native source. */
  if (n->proxy)
    return 0;
  if (n->role && n->role != role)
    return 0;
  if (!expected || model != expected)
    return 0;
  if (!model_resident(model))
    return 0;
  if (clip > role_clip_max(role, n->part))
    return 0;
  if (!clip_resident(model, clip))
    return 0;
  if (ordinal < 1u || ordinal > 255u)
    return 0;
  q_clear(row, sizeof(int) * WORLD_QUEST_WORDS);
  row[WQ_ABI] = WORLD_QUEST_ABI;
  row[WQ_FAMILY] = (int)family;
  row[WQ_ROLE] = (int)role;
  row[WQ_SELF] = (int)n->self;
  row[WQ_OWNER] = (int)n->owner;
  row[WQ_SERIAL] = (int)n->serial;
  row[WQ_LIFE] = WQ_LIVE;
  row[WQ_GEN] = 0;
  row[WQ_PARENT] = (int)anchor_world_quest_root_slot(D_800C7AB2, family);
  row[WQ_ORDINAL] = (int)ordinal;
  row[WQ_ENTITY] = (int)QH(a, 0x5c);
  row[WQ_MODEL] = (int)model;
  row[WQ_SLOT] = (int)role_slot(role, n->part);
  row[WQ_PART] = (int)n->part;
  if (role_phase(role)) {
    /* File_53 stores these as full big-endian words, not byte/halfword
     * fields. Reading +0xD0 as a byte would publish phase zero forever. */
    row[WQ_PHASE] = (int)QW(a, 0xd0);
    row[WQ_TIMER] = (int)QW(a, 0xd4);
  }
  row[WQ_ALPHA] = 0;
  if (!q_quantize(QF(o, 8), 100.0f, -3276800, 3276700, &row[WQ_X]) ||
      !q_quantize(QF(o, 0xc), 100.0f, -3276800, 3276700, &row[WQ_Y]) ||
      !q_quantize(QF(o, 0x10), 100.0f, -3276800, 3276700, &row[WQ_Z]))
    return 0;
  row[WQ_PITCH] = (int)(QH(o, 0x14) & 1023u);
  row[WQ_YAW] = (int)(QH(o, 0x16) & 1023u);
  row[WQ_ROLL] = (int)(QH(o, 0x18) & 1023u);
  for (i = 0; i < 3; ++i) {
    int v;
    if (!q_quantize(QF(o, 0x1c + i * 4), 100.0f, 0, 64000, &v))
      return 0;
    row[WQ_SX + (int)i] = v;
  }
  row[WQ_CLIP] = (int)clip;
  row[WQ_RATE] = (int)rate;
  /* The animation frame is best effort: a role that never binds a clip leaves
   * the field undefined, so a refused sample publishes zero instead of failing
   * an otherwise valid checkpoint. */
  if (!q_quantize(QF(o, 0x28), 100.0f, 0, 1000000, &row[WQ_FRAME]))
    row[WQ_FRAME] = 0;
  row[WQ_ANIM] = (int)anim;
  row[WQ_BYTE5] = (int)QB(o, 5);
  /* +0x30 is a tagged local display-list pointer, never portable. */
  row[WQ_COLOUR] = 0;
  if (role == WQ_ROLE_KIHACHI_COLOUR &&
      QW(o, 0x80) == 0x06000000u &&
      QW(o, 0x88) == 0xfb000000u &&
      QW(o, 0x90) == 0xb8000000u) {
    unsigned int rgba = QW(o, 0x8c);
    row[WQ_RED] = (int)((rgba >> 24) & 255u);
    row[WQ_GREEN] = (int)((rgba >> 16) & 255u);
    row[WQ_BLUE] = (int)((rgba >> 8) & 255u);
    row[WQ_COLOUR_ALPHA] = (int)(rgba & 255u);
    row[WQ_COLOUR_MODE] = 1;
  }
  row[WQ_FLAGS_LO] = (int)(QW(a, 0x60) & 0xffffu);
  row[WQ_FLAGS_HI] = (int)(QW(a, 0x60) >> 16);
  row[WQ_AUX_LO] = (int)(QW(a, 0x64) & 0xffffu);
  row[WQ_AUX_HI] = (int)(QW(a, 0x64) >> 16);
  row[WQ_DURABLE] = q_durable(family);
  row[WQ_TEXTURE] = (int)QB(o, 0x7c);
  row[WQ_INSTANCE] = (int)n->instance;
  row[WQ_RECEIPT] = (int)n->receipt;
  n->family = (unsigned char)family;
  n->role = (unsigned char)role;
  n->ordinal = (unsigned char)ordinal;
  return anchor_world_quest_row_valid(row);
}

int anchor_world_quest_capture(void *actor, unsigned int role,
                               unsigned int ordinal, int *row) {
  QuestNode *n;
  unsigned int family = role_family(role);
  if (!actor || !row || !family)
    return 0;
  if (role == WQ_ROLE_GATEWAY_MESH)
    return anchor_world_quest_mesh_capture(actor, row);
  n = q_lookup(actor);
  /* Explicit self/owner registration is required: an unowned row would let a
   * proxy re-advertise itself as a native source. */
  if (!n || !n->self || !q_live(n))
    return 0;
  return q_capture_common(n, role, ordinal, row);
}

int anchor_world_quest_register(void *actor, unsigned int role,
                                unsigned int ordinal, unsigned int part) {
  QuestNode *n;
  unsigned int family = role_family(role);
  unsigned int clip, rate, anim;
  if (!actor || !family)
    return 0;
  if (!family_room(family, D_800C7AB2))
    return 0;
  if (ordinal < 1u || ordinal > 255u)
    return 0;
  if (part < role_part_min(role) || part > role_parts(role))
    return 0;
  n = q_alloc(actor);
  if (!n || !n->object)
    return 0;
  if (n->role && n->role != role)
    return 0;
  n->family = (unsigned char)family;
  n->role = (unsigned char)role;
  n->ordinal = (unsigned char)ordinal;
  n->part = (unsigned char)part;
  role_anim(role, part, &clip, &rate, &anim);
  n->clip = clip;
  n->rate = rate;
  n->anim = anim;
  return 1;
}

/* ---- apply ------------------------------------------------------------- */

static int q_apply_common(QuestNode *n, const int *row) {
  void *a = n->actor, *o = n->object;
  unsigned int model = (unsigned int)row[WQ_MODEL];
  unsigned int clip = (unsigned int)row[WQ_CLIP];
  unsigned int rate = (unsigned int)row[WQ_RATE];
  unsigned int anim = (unsigned int)row[WQ_ANIM];
  unsigned int i;
  if (!n->proxy || !q_live(n))
    return 0;
  if (QH(a, 0x5e) != model || !model_resident(model))
    return 0;
  if (!clip_resident(model, clip))
    return 0;
  if (anim && (n->clip != clip || n->anim != anim))
    func_8021664C_5D1B1C(a, clip, (float)rate / 256.0f, anim);
  QF(o, 8) = (float)row[WQ_X] / 100.0f;
  QF(o, 0xc) = (float)row[WQ_Y] / 100.0f;
  QF(o, 0x10) = (float)row[WQ_Z] / 100.0f;
  QH(o, 0x14) = (unsigned short)row[WQ_PITCH];
  QH(o, 0x16) = (unsigned short)row[WQ_YAW];
  QH(o, 0x18) = (unsigned short)row[WQ_ROLL];
  for (i = 0; i < 3; ++i)
    QF(o, 0x1c + i * 4) = (float)row[WQ_SX + (int)i] / 100.0f;
  QF(o, 0x28) = (float)row[WQ_FRAME] / 100.0f;
  if (row[WQ_COLOUR_MODE] == 1 &&
      row[WQ_ROLE] == WQ_ROLE_KIHACHI_COLOUR) {
    void *prim = func_08002A1C_72403C(a,
        (unsigned int)row[WQ_COLOUR_ALPHA], (unsigned int)row[WQ_RED],
        (unsigned int)row[WQ_GREEN], (unsigned int)row[WQ_BLUE]);
    if (!prim)
      return 0;
    QW(o, 0x30) = ((unsigned int)(unsigned long)prim) | 0x20000000u;
  }
  QB(o, 5) = (unsigned char)row[WQ_BYTE5];
  /* A visual carrier cannot acquire the peer's collision or scene-control
   * flags. The native model binder's local render and resource flags stay. */
  QB(o, 0x7c) = (unsigned char)row[WQ_TEXTURE];
  QH(o, 0x7e) = (unsigned short)rate;
  n->clip = clip;
  n->rate = rate;
  n->anim = anim;
  n->family = (unsigned char)row[WQ_FAMILY];
  n->role = (unsigned char)row[WQ_ROLE];
  n->ordinal = (unsigned char)row[WQ_ORDINAL];
  n->part = (unsigned char)row[WQ_PART];
  n->proxy = 1;
  n->receipt = (unsigned int)row[WQ_RECEIPT];
  return 1;
}

int anchor_world_quest_apply(void *actor, const int *row) {
  QuestNode *n;
  unsigned int family, role;
  if (!actor || !row || !anchor_world_quest_row_valid(row))
    return 0;
  if (!anchor_world_quest_identity(row, &family, &role, 0, 0))
    return 0;
  if (!family_room(family, D_800C7AB2))
    return 0;
  if (role == WQ_ROLE_GATEWAY_MESH)
    return anchor_world_quest_mesh_apply(actor, row);
  n = q_lookup(actor);
  /* Only the module's scheduled carrier is eligible. A local quest actor is
   * never turned into a proxy by a caller-provided pointer. */
  if (!n || !n->proxy || !n->primed || !q_live(n))
    return 0;
  if (n->role && n->role != role)
    return 0;
  return q_apply_common(n, row);
}

/* ---- proxy decision ---------------------------------------------------- */

int anchor_world_quest_update(void *actor, unsigned int self_id,
                              unsigned int owner) {
  QuestNode *n;
  unsigned int i;
  int engaged;
  if (!actor || !self_id)
    return 0;
  n = q_lookup(actor);
  if (!n)
    return 0;
  if (n->actor != actor || !n->object)
    return 0;
  n->self = self_id;
  n->owner = owner;
  /* Hide only after a typed replacement for this exact role and part has
   * finished native initialization and applied its receipt. */
  engaged = 0;
  if (owner && owner != self_id && n->role && !n->proxy)
    for (i = 0; i < WORLD_QUEST_MAX; ++i) {
      QuestNode *p = &q_nodes[i];
      if (p->proxy && p->primed && p->receipt && q_live(p) &&
          p->family == n->family && p->role == n->role &&
          p->ordinal == n->ordinal && p->part == n->part &&
          p->owner == owner) {
        engaged = 1;
        break;
      }
    }
  if (engaged)
    anchor_world_quest_hide(actor, 1);
  else
    anchor_world_quest_hide(actor, 0);
  return engaged && n->hidden;
}

int anchor_world_quest_bridge(const int *row, unsigned int self_id,
                              unsigned int owner, int *out) {
  unsigned int i;
  if (!row || !out || !self_id)
    return 0;
  if (!anchor_world_quest_row_valid(row))
    return 0;
  for (i = 0; i < WORLD_QUEST_WORDS; ++i)
    out[i] = row[i];
  out[WQ_SELF] = (int)self_id;
  out[WQ_OWNER] = (int)owner;
  out[WQ_INSTANCE] = 0;
  out[WQ_RECEIPT] = 0;
  return 1;
}

int anchor_world_quest_identity(const int *row, unsigned int *family,
                                unsigned int *role, unsigned int *parent,
                                unsigned int *ordinal) {
  if (!row || row[WQ_ABI] != WORLD_QUEST_ABI)
    return 0;
  if (!role_family((unsigned int)row[WQ_ROLE]) &&
      row[WQ_ROLE] != WQ_ROLE_NONE)
    return 0;
  if (family)
    *family = (unsigned int)row[WQ_FAMILY];
  if (role)
    *role = (unsigned int)row[WQ_ROLE];
  if (parent)
    *parent = (unsigned int)row[WQ_PARENT];
  if (ordinal)
    *ordinal = (unsigned int)row[WQ_ORDINAL];
  return 1;
}

/* ---- frame ------------------------------------------------------------- */

void anchor_world_quest_frame(unsigned int room, unsigned int signature,
                              unsigned int visit, int active) {
  unsigned int i;
  if (!active) {
    /* The bridge passes room=0 while disconnected. That is not a native room
     * transition: constructor-only children (notably Koryuta's eleven parts)
     * must remain registered for a reconnect in the same loaded room. Use
     * the actual game room to distinguish a real transition. */
    if (q_room != D_800C7AB2)
      anchor_world_quest_room(D_800C7AB2);
    if (q_active) {
      q_active = 0;
      q_drop_proxies();
      for (i = 0; i < WORLD_QUEST_MAX; ++i) {
        if (q_nodes[i].actor && !q_nodes[i].proxy) {
          q_nodes[i].owner = 0;
          q_nodes[i].receipt = QUEST_EMPTY_RECEIPT;
        }
      }
    }
    q_row_count = 0;
    return;
  }
  if (q_room != room)
    anchor_world_quest_room(room);
  if ((q_visit && q_visit != visit) ||
      (q_signature && q_signature != signature))
    q_new_scope();
  q_signature = signature;
  q_active = 1;
  q_visit = visit;
  q_row_count = 0;
  for (i = 0; i < WORLD_QUEST_MAX && q_row_count < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (!n->actor || n->proxy || !n->role || !n->self)
      continue;
    if (n->retired || !q_live(n)) {
      q_unbind(n);
      continue;
    }
    if (!(n->role == WQ_ROLE_GATEWAY_MESH ?
          anchor_world_quest_mesh_capture(n->actor, q_rows[q_row_count]) :
          q_capture_common(n, n->role, n->ordinal,
                           q_rows[q_row_count])))
      continue;
    ++q_row_count;
  }
}

/* ---- Gateway mesh ------------------------------------------------------ */

/* The mesh private block is a task-owned 0x80 allocation. Its +0x1C slot is
 * the one-shot grid writer func_08001CE4_70FD94 and its +0x20 slot is the fade
 * driver func_08001E60_70FF10, which the mesh render callback func_80025B38
 * invokes every update. Replacing +0x20 with this no-op keeps temporary bits
 * 8/0xC/0xD/0xF/0x10 and the native teardown off a replica. */
static void q_mesh_fade_noop(void *a, void *o) {
  (void)a;
  (void)o;
}

static void *q_mesh_private(void *task) { return QP(task, 0xd0); }

static int q_mesh_arrays_ready(void *task) {
  void *priv = task ? q_mesh_private(task) : 0;
  return priv && QW(priv, 0) == QUEST_MESH_FLAGS_WORD &&
         QP(priv, 0x40) && QP(priv, 0x48) && QP(priv, 0x50) &&
         QP(priv, 0x54) && QP(priv, 0x58) && QP(priv, 0x5c) &&
         QP(priv, 0x60);
}

int anchor_world_quest_mesh_neutralize(void *task) {
  void *priv;
  if (!task)
    return 0;
  priv = q_mesh_private(task);
  if (!priv)
    return 0;
  if (QP(priv, QUEST_MESH_FIRST_SLOT) == 0)
    return 0;
  QP(priv, QUEST_MESH_FADE_SLOT) = (void *)q_mesh_fade_noop;
  return 1;
}

int anchor_world_quest_mesh_ready(void *task) {
  QuestNode *n = q_lookup(task);
  void *priv;
  if (!task)
    return 0;
  priv = q_mesh_private(task);
  if (!priv)
    return 0;
  return q_mesh_arrays_ready(task) &&
         (!n || !n->proxy ||
          QP(priv, QUEST_MESH_FADE_SLOT) == (void *)q_mesh_fade_noop);
}

int anchor_world_quest_mesh_construct(void *task) {
  QuestNode *n;
  void *object, *priv;
  if (!task)
    return 0;
  if (D_800C7AB2 != WORLD_QUEST_ROOM_GATEWAY)
    return 0;
  n = q_lookup(task);
  if (!n || !n->proxy || n->role != WQ_ROLE_GATEWAY_MESH ||
      D_8016DAB4_16E6B4 != task)
    return 0;
  /* Allocation and scheduler wiring are once-only: a second call would
   * double-allocate the private block and the seven sub-arrays. */
  if (n->mesh_constructed || q_mesh_private(task) != 0)
    return 0;
  if (!model_resident(0x24e))
    return 0;
  object = QP(task, 0x18);
  if (!object)
    return 0;
  q_native_fade = func_08001E60_70FF10;
  /* Runs the native pure constructor in this task: it allocates the private
   * 0x80 block, writes the typed defaults and the object transform, installs
   * the callback pair and schedules func_80024160_24D60. */
  func_08001BD0_70FC80(task, object);
  priv = q_mesh_private(task);
  if (!priv)
    return 0;
  /* Typed check: the constructor must have installed its own first-frame
   * writer and its own fade driver before the fade driver is replaced. */
  if (QP(priv, QUEST_MESH_FIRST_SLOT) == 0 ||
      QP(priv, QUEST_MESH_FADE_SLOT) != q_native_fade)
    return 0;
  n->mesh_constructed = 1;
  n->proxy = 1;
  if (!anchor_world_quest_mesh_neutralize(task))
    return 0;
  /* Native func_80024160 allocates its mesh arrays on a later scheduler
   * update. The proxy becomes ready only after that return hook observes all
   * arrays. */
  return 1;
}

int anchor_world_quest_mesh_capture(void *task, int *row) {
  QuestNode *n;
  void *priv;
  if (!task || !row)
    return 0;
  if (D_800C7AB2 != WORLD_QUEST_ROOM_GATEWAY)
    return 0;
  n = q_lookup(task);
  /* A proxy this module constructed never re-advertises itself as native. */
  if (!n || n->proxy || !n->self || !n->role || n->role != WQ_ROLE_GATEWAY_MESH)
    return 0;
  if (!q_live(n) || !model_resident(0x24e))
    return 0;
  priv = q_mesh_private(task);
  if (!priv)
    return 0;
  if (!q_capture_common(n, WQ_ROLE_GATEWAY_MESH, n->ordinal, row))
    return 0;
  row[WQ_PHASE] = (int)QW(task, 0xe0);
  row[WQ_TIMER] = (int)(QW(task, 0xe4) & 0xffffu);
  row[WQ_ALPHA] = (int)QB(priv, 0x27);
  row[WQ_MESH_FLAGS] = (int)(QW(priv, 0) & 0xffffu);
  row[WQ_MESH_READY] = q_mesh_arrays_ready(task);
  return anchor_world_quest_row_valid(row);
}

int anchor_world_quest_mesh_apply(void *task, const int *row) {
  QuestNode *n;
  void *priv;
  if (!task || !row || !anchor_world_quest_row_valid(row))
    return 0;
  if (D_800C7AB2 != WORLD_QUEST_ROOM_GATEWAY)
    return 0;
  n = q_lookup(task);
  if (!n || !n->proxy || !n->primed || !q_live(n))
    return 0;
  if (n->role && n->role != WQ_ROLE_GATEWAY_MESH)
    return 0;
  if (!n->mesh_constructed || !anchor_world_quest_mesh_ready(task))
    return 0;
  priv = q_mesh_private(task);
  if (!priv)
    return 0;
  if (!q_apply_common(n, row))
    return 0;
  /* Portable mesh scalars only: the grid geometry and the seven sub-arrays
   * stay exactly as the native constructor left them. */
  QB(priv, 0x27) = (unsigned char)row[WQ_ALPHA];
  QW(task, 0xe0) = (unsigned int)row[WQ_PHASE];
  QW(task, 0xe4) = (QW(task, 0xe4) & 0xffff0000u) |
                   ((unsigned int)row[WQ_TIMER] & 0xffffu);
  return 1;
}

/* ---- owned render proxies --------------------------------------------- */

static int q_same_key(const int *a, const int *b) {
  return a[WQ_FAMILY] == b[WQ_FAMILY] &&
         a[WQ_ROLE] == b[WQ_ROLE] &&
         a[WQ_PARENT] == b[WQ_PARENT] &&
         a[WQ_ORDINAL] == b[WQ_ORDINAL] &&
         a[WQ_PART] == b[WQ_PART];
}

static QuestNode *q_find_proxy(const int *row) {
  unsigned int i;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (n->proxy && !n->retired && n->actor &&
        q_same_key(n->offered, row))
      return n;
  }
  return 0;
}

static void q_proxy_idle(void *actor, void *object) {
  (void)actor;
  (void)object;
}

/* Scheduled by native child allocation. Never invoke a scene constructor
 * here: it may own the camera, dialogue, temporary flags or damage. */
static void q_proxy_init(void *actor, void *object) {
  QuestNode *n = q_lookup(actor);
  unsigned int role, part, model, slot;
  if (!n || !n->proxy || !object || !q_live(n) ||
      !anchor_world_quest_row_valid(n->offered))
    return;
  role = (unsigned int)n->offered[WQ_ROLE];
  part = (unsigned int)n->offered[WQ_PART];
  model = (unsigned int)n->offered[WQ_MODEL];
  slot = role_slot(role, part);
  if (model != role_model(role, part) ||
      !model_resident(model))
    return;
  QH(actor, 0x5c) = (unsigned short)n->offered[WQ_ENTITY];
  QH(actor, 0x5e) = (unsigned short)model;
  QW(actor, 0x60) = 0x20u; /* render only, no hit/contact flags */
  QW(actor, 0x64) = 0;
  if (role == WQ_ROLE_GATEWAY_MESH) {
    if (!anchor_world_quest_mesh_construct(actor))
      return;
    n->scheduled = 1;
    return;
  }
  /* File_74's 0x367 variants select modes of the model initializer itself;
   * Gateway/Koryuta instead use the distinct static-slot binder. */
  if (role == WQ_ROLE_KIHACHI_COLOUR)
    func_80216E1C_5D22EC(actor, 8);
  else if (role == WQ_ROLE_GMC74_CHILD)
    func_80216E1C_5D22EC(actor, (int)slot);
  else if (slot)
    func_80216DF8_5D22C8(actor, (int)slot);
  else
    func_80216E1C_5D22EC(actor, 0);
  /* File_74 part 17 uses a second, independent texture channel. Its native
   * initializer calls this pure model bind with clip 0; call it once on our
   * own carrier so the local texture cycles without replaying scene AI. */
  if (role == WQ_ROLE_GMC74_CHILD && part == 17)
    func_80224ABC_5DFF8C(actor, 0, 3.0f, 1);
  if (role == WQ_ROLE_KIHACHI_COLOUR)
    func_80224ABC_5DFF8C(actor, 4, 0.001f, 1);
  if (role == WQ_ROLE_KIHACHI_COLOUR) {
    void *prim = func_08002A1C_72403C(actor, 0x80, 255, 255, 255);
    if (!prim)
      return;
    QW(object, 0x30) = ((unsigned int)(unsigned long)prim) | 0x20000000u;
  } else if (role == WQ_ROLE_KIHACHI_RISE) {
    void *prim = func_8021A26C_5D573C(actor, 0x80, 255, 255, 255);
    if (!prim)
      return;
    QW(object, 0x30) = ((unsigned int)(unsigned long)prim) | 0x20000000u;
  }
  QP(actor, 0xc) = (void *)q_proxy_idle;
  n->primed = 1;
}

RECOMP_HOOK_RETURN("func_80024160_24D60")
void anchor_world_quest_mesh_ready_hook(void) {
  QuestNode *n = q_lookup(D_8016DAB4_16E6B4);
  if (n && n->proxy && n->role == WQ_ROLE_GATEWAY_MESH &&
      n->mesh_constructed && anchor_world_quest_mesh_ready(n->actor))
    n->primed = 1;
}

unsigned int anchor_world_quest_status(
    int out[][WORLD_QUEST_WORDS], unsigned int capacity) {
  unsigned int i, j, count = 0;
  if (!out || !capacity)
    return 0;
  for (i = 0; i < WORLD_QUEST_MAX && count < capacity; ++i) {
    QuestNode *n = &q_nodes[i];
    if (!n->proxy || !q_live(n) || !n->scheduled)
      continue;
    for (j = 0; j < WORLD_QUEST_WORDS; ++j)
      out[count][j] = n->offered[j];
    out[count][WQ_SELF] = (int)q_self;
    out[count][WQ_INSTANCE] = (int)n->instance;
    out[count][WQ_RECEIPT] = (int)n->receipt;
    ++count;
  }
  return count;
}

static void q_show_original(const int *row) {
  unsigned int i;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (!n->proxy && !n->retired && n->actor && n->role &&
        n->family == (unsigned int)row[WQ_FAMILY] &&
        n->role == (unsigned int)row[WQ_ROLE] &&
        n->ordinal == (unsigned int)row[WQ_ORDINAL] &&
        n->part == (unsigned int)row[WQ_PART] && q_live(n))
      anchor_world_quest_hide(n->actor, 0);
  }
}

static void q_hide_original(const int *row, unsigned int self_id) {
  unsigned int i;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (!n->proxy && !n->retired && n->actor && n->role &&
        n->family == (unsigned int)row[WQ_FAMILY] &&
        n->role == (unsigned int)row[WQ_ROLE] &&
        n->ordinal == (unsigned int)row[WQ_ORDINAL] &&
        n->part == (unsigned int)row[WQ_PART] && q_live(n))
      anchor_world_quest_update(n->actor, self_id,
                                (unsigned int)row[WQ_OWNER]);
  }
}

/* A selected family is a complete visual graph. Once every live remote row
 * has a fully applied replacement, suppress even local children omitted from
 * that graph. Their native task and callbacks continue to run. If any child
 * is still pending, keep the entire local graph visible until reconstruction
 * is ready rather than presenting a mixture of two stages. */
static void q_reconcile_originals(
    const int rows[][WORLD_QUEST_WORDS], unsigned int count,
    unsigned int self_id) {
  unsigned int family, i, j;
  for (family = WQ_FAMILY_GATEWAY; family <= WQ_FAMILY_GMC75; ++family) {
    unsigned int owner = 0;
    int ready = 1;
    for (i = 0; i < count; ++i) {
      const int *row = rows[i];
      QuestNode *p;
      if ((unsigned int)row[WQ_FAMILY] != family)
        continue;
      if (!row[WQ_OWNER] || (unsigned int)row[WQ_OWNER] == self_id) {
        ready = 0;
        break;
      }
      if (owner && owner != (unsigned int)row[WQ_OWNER]) {
        ready = 0;
        break;
      }
      owner = (unsigned int)row[WQ_OWNER];
      if (row[WQ_LIFE] != WQ_LIVE)
        continue;
      p = q_find_proxy(row);
      if (!p || !p->primed || !p->receipt ||
          p->receipt != (unsigned int)row[WQ_RECEIPT] || !q_live(p)) {
        ready = 0;
        break;
      }
    }
    for (j = 0; j < WORLD_QUEST_MAX; ++j) {
      QuestNode *n = &q_nodes[j];
      if (n->actor && !n->proxy && !n->retired && n->family == family &&
          n->role && q_live(n)) {
        n->self = self_id;
        n->owner = owner;
        anchor_world_quest_hide(n->actor, owner && ready);
      }
    }
  }
}

unsigned int anchor_world_quest_receive(
    const int rows[][WORLD_QUEST_WORDS], unsigned int count,
    unsigned int self_id) {
  unsigned int i, j, applied = 0;
  if ((!rows && count) || count > WORLD_QUEST_MAX || !self_id ||
      !q_active || D_800C7AB2 != q_room)
    return 0;
  /* Snapshot validation is atomic. A malformed or duplicate row must not
   * trigger cleanup of otherwise valid live proxies. */
  for (i = 0; i < count; ++i) {
    if (!anchor_world_quest_row_valid(rows[i]) ||
        !family_room((unsigned int)rows[i][WQ_FAMILY], q_room) ||
        anchor_world_quest_root_slot(q_room,
            (unsigned int)rows[i][WQ_FAMILY]) !=
            (unsigned int)rows[i][WQ_PARENT])
      return 0;
    for (j = 0; j < i; ++j)
      if (q_same_key(rows[i], rows[j]))
        return 0;
  }
  anchor_world_quest_set_self(self_id);
  for (i = 0; i < count; ++i) {
    const int *row = rows[i];
    QuestNode *n;
    unsigned int file;
    void *resource, *actor;
    n = q_find_proxy(row);
    if (row[WQ_LIFE] == WQ_REMOVED ||
        row[WQ_OWNER] == (int)self_id) {
      if (n)
        anchor_world_quest_release(n->actor);
      q_show_original(row);
      continue;
    }
    if (row[WQ_LIFE] != WQ_LIVE || !row[WQ_OWNER] ||
        row[WQ_RECEIPT] <= 0 ||
        (n && row[WQ_INSTANCE] &&
         (unsigned int)row[WQ_INSTANCE] != n->instance))
      continue;
    if (!n) {
      if (row[WQ_INSTANCE] || !D_801FC604_5B8514 ||
          !model_resident((unsigned int)row[WQ_MODEL]) ||
          !clip_resident((unsigned int)row[WQ_MODEL],
                         (unsigned int)row[WQ_CLIP]))
        continue;
      file = role_file((unsigned int)row[WQ_ROLE]);
      resource = file ? func_800141C4_14DC4(file) : 0;
      if (!resource || resource == (void *)(unsigned long)0xffffffffu)
        continue;
      actor = func_802171A8_5D2678(D_801FC604_5B8514,
                                      q_proxy_init, 8);
      if (!actor)
        continue;
      QH(actor, 0x28) = (unsigned short)file;
      QP(actor, 0x2c) = resource;
      n = q_alloc(actor);
      if (!n) {
        func_80034EF8_35AF8(actor);
        continue;
      }
      n->proxy = 1;
      n->scheduled = 1;
      n->family = (unsigned char)row[WQ_FAMILY];
      n->role = (unsigned char)row[WQ_ROLE];
      n->ordinal = (unsigned char)row[WQ_ORDINAL];
      n->part = (unsigned char)row[WQ_PART];
    }
    if (!q_live(n))
      continue;
    n->owner = (unsigned int)row[WQ_OWNER];
    for (j = 0; j < WORLD_QUEST_WORDS; ++j)
      n->offered[j] = row[j];
    if (!n->primed)
      continue;
    if (!anchor_world_quest_apply(n->actor, row))
      continue;
    ++applied;
    q_hide_original(row, self_id);
  }
  /* The parent passes only a complete selected-room snapshot. Proxies absent
   * from it are no longer authoritative and must release their originals. */
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    int present = 0;
    if (!n->proxy || !n->actor || n->retired)
      continue;
    for (j = 0; j < count; ++j)
      if (q_same_key(n->offered, rows[j]) &&
          rows[j][WQ_LIFE] == WQ_LIVE &&
          rows[j][WQ_OWNER] != (int)self_id) {
        present = 1;
        break;
      }
    if (!present) {
      int key[WORLD_QUEST_WORDS];
      unsigned int k;
      void *old_actor = n->actor;
      for (k = 0; k < WORLD_QUEST_WORDS; ++k)
        key[k] = n->offered[k];
      anchor_world_quest_release(old_actor);
      q_show_original(key);
    }
  }
  q_reconcile_originals(rows, count, self_id);
  return applied;
}

/* ---- native scope hook ------------------------------------------------- */

/* Only tracked actors with an engaged proxy are touched, and the body is
 * skipped entirely while nothing is hidden. */
RECOMP_HOOK("func_80218F30_5D4400")
void anchor_world_quest_scope_hook(void *actor) {
  if (!q_hidden)
    return;
  if (!anchor_world_quest_hidden(actor))
    return;
  q_scope_actor = actor;
  anchor_world_quest_scope_begin(actor);
}

RECOMP_HOOK_RETURN("func_80218F30_5D4400")
void anchor_world_quest_scope_hook_return(void) {
  void *actor = q_scope_actor;
  q_scope_actor = 0;
  if (actor)
    anchor_world_quest_scope_end(actor);
}

/* The native scheduler can run quest AI before the common actor post. Lift
 * every render-only hide at scheduler entry and restore it after all callbacks
 * complete, so local dialogue, cull, camera and scene handshakes read the
 * original byte. */
RECOMP_HOOK("func_80034734_35334")
void anchor_world_quest_scheduler_begin(void) {
  unsigned int i;
  if (!q_hidden || D_800C7AB2 != q_room)
    return;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (n->hidden && !n->retired && !n->proxy && q_live(n))
      QB(n->object, 0x64) = n->saved64;
  }
}
RECOMP_HOOK_RETURN("func_80034734_35334")
void anchor_world_quest_scheduler_end(void) {
  unsigned int i;
  if (!q_hidden || D_800C7AB2 != q_room)
    return;
  for (i = 0; i < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (n->hidden && !n->retired && !n->proxy && q_live(n))
      QB(n->object, 0x64) = (unsigned char)(n->saved64 | QUEST_HIDE_BIT);
  }
}

/* ---- native scene discovery and retirement ---------------------------- */

static void q_source(void *actor, unsigned int role, unsigned int ordinal,
                     unsigned int part) {
  unsigned int family = role_family(role);
  QuestNode *n;
  if (!actor || !family || role > WQ_ROLE_GMC75_CHILD || part >= 32u ||
      !family_room(family, D_800C7AB2))
    return;
  if (q_room != D_800C7AB2)
    anchor_world_quest_room(D_800C7AB2);
  n = q_lookup(actor);
  if (n && n->proxy)
    return;
  if (n && n->role) {
    /* Scheduler callbacks such as Gateway A/B run every frame. Registration
     * seeds the constructor's initial clip only once; repeated calls must not
     * reset a later native animation selection. */
    if (n->role == role && n->part == part)
      n->self = q_self;
    return;
  }
  if (ordinal <= q_activation[role][part])
    ordinal = (unsigned int)q_activation[role][part] + 1u;
  if (ordinal > 255u)
    return;
  if (anchor_world_quest_register(actor, role, ordinal, part)) {
    q_activation[role][part] = (unsigned char)ordinal;
    n = q_lookup(actor);
    if (n)
      n->self = q_self;
  }
}

static void q_set_root(unsigned int family, void *actor) {
  if (!actor || !family_room(family, D_800C7AB2))
    return;
  if (q_room != D_800C7AB2)
    anchor_world_quest_room(D_800C7AB2);
  q_roots[family] = actor;
  q_root_gen[family] = QB(actor, 0x74);
}

static int q_root_live(unsigned int family) {
  void *root = q_roots[family];
  return root && QB(root, 0x74) == q_root_gen[family] &&
         !(QW(root, 0x68) & QUEST_DELETED_BIT) &&
         D_800C7AB2 == q_room;
}

static void q_gmc74_child(void *actor) {
  unsigned int i;
  void *priv;
  if (!q_root_live(WQ_FAMILY_GMC74))
    return;
  priv = QP(q_roots[WQ_FAMILY_GMC74], 0xd0);
  if (!priv)
    return;
  for (i = 0; i < WORLD_QUEST_GMC74_CHILDREN; ++i)
    if (QP(priv, 4u + i * 4u) == actor) {
      q_source(actor, WQ_ROLE_GMC74_CHILD, 1, i);
      return;
    }
}

RECOMP_HOOK_RETURN("func_08001680_70F730")
void quest_gateway_a(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_GATEWAY_CHILD_A, 1, 0);
}
RECOMP_HOOK_RETURN("func_08001868_70F918")
void quest_gateway_b(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_GATEWAY_CHILD_B, 1, 0);
}
RECOMP_HOOK_RETURN("func_0800220C_7102BC")
void quest_gateway_decor(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_GATEWAY_DECOR, 1, 0);
}
RECOMP_HOOK_RETURN("func_080007F4_70E8A4")
void quest_gateway_npc(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_GATEWAY_NPC, 1, 0);
}
RECOMP_HOOK_RETURN("func_08001BD0_70FC80")
void quest_gateway_mesh(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_GATEWAY_MESH, 1, 0);
}

RECOMP_HOOK_RETURN("func_08001A28_702B28")
void quest_koryuta_body(void) {
  void *root = D_8016DAB4_16E6B4;
  if (!root || D_800C7AB2 != WORLD_QUEST_ROOM_KORYUTA)
    return;
  q_set_root(WQ_FAMILY_KORYUTA, root);
  q_source(root, WQ_ROLE_KORYUTA_BODY, 1, 0);
}
RECOMP_HOOK("func_080011FC_7022FC")
void quest_koryuta_part(void *part, int index) {
  if (index >= 1 && index <= WORLD_QUEST_KORYUTA_PARTS)
    q_source(part, WQ_ROLE_KORYUTA_PART, 1, (unsigned int)index);
}

RECOMP_HOOK_RETURN("func_08001C20_723240")
void quest_kihachi_npc(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_KIHACHI_NPC, 1, 0);
}
RECOMP_HOOK_RETURN("func_08001D50_723370")
void quest_kihachi_rise(void) {
  void *a = D_8016DAB4_16E6B4;
  q_source(a, WQ_ROLE_KIHACHI_RISE, 1, 0);
}
RECOMP_HOOK_RETURN("func_08001FB4_7235D4")
void quest_kihachi_colour(void) {
  q_source(D_8016DAB4_16E6B4, WQ_ROLE_KIHACHI_COLOUR, 1, 0);
}

RECOMP_HOOK_RETURN("func_08000000_734790")
void quest_gmc74_root(void) {
  q_set_root(WQ_FAMILY_GMC74, D_8016DAB4_16E6B4);
}
RECOMP_HOOK_RETURN("func_08000000_7386D0")
void quest_gmc75_root(void) {
  q_set_root(WQ_FAMILY_GMC75, D_8016DAB4_16E6B4);
}
RECOMP_HOOK_RETURN("func_08000064_738734")
void quest_gmc75_children(void) {
  void *root = D_8016DAB4_16E6B4;
  void *priv;
  unsigned int part;
  if (!root || D_800C7AB2 != WORLD_QUEST_ROOM_GMC ||
      QH(root, 0x5c) != 0x35d)
    return;
  if (!q_root_live(WQ_FAMILY_GMC75))
    q_set_root(WQ_FAMILY_GMC75, root);
  priv = QP(root, 0xd0);
  if (!priv)
    return;
  /* Native phase 6 stores three visual child tasks in private words 3..5.
   * The camera and other local scene handles use different private slots. */
  for (part = 0; part < WORLD_QUEST_GMC75_CHILDREN; ++part) {
    void *child = QP(priv, 0x0c + part * 4u);
    if (child && QH(child, 0x5e) == role_model(WQ_ROLE_GMC75_CHILD, part))
      q_source(child, WQ_ROLE_GMC75_CHILD, 1, part);
  }
}

RECOMP_HOOK("func_8021664C_5D1B1C")
void quest_animation_select(void *actor, unsigned int clip, float rate,
                            unsigned int flags) {
  QuestNode *n = q_lookup(actor);
  (void)rate;
  (void)flags;
  if (n && !n->proxy && clip <= 15u)
    n->clip = clip;
}

#define Q_GMC74_HOOK(suffix, name) \
  RECOMP_HOOK_RETURN(suffix) void name(void) { \
    q_gmc74_child(D_8016DAB4_16E6B4); \
  }
Q_GMC74_HOOK("func_08002BBC_73734C", quest_gmc_2bbc)
Q_GMC74_HOOK("func_08002CDC_73746C", quest_gmc_2cdc)
Q_GMC74_HOOK("func_08002E04_737594", quest_gmc_2e04)
Q_GMC74_HOOK("func_08002F2C_7376BC", quest_gmc_2f2c)
Q_GMC74_HOOK("func_08003028_7377B8", quest_gmc_3028)
Q_GMC74_HOOK("func_080030F8_737888", quest_gmc_30f8)
Q_GMC74_HOOK("func_080031C8_737958", quest_gmc_31c8)
Q_GMC74_HOOK("func_080032B0_737A40", quest_gmc_32b0)
Q_GMC74_HOOK("func_08003398_737B28", quest_gmc_3398)
Q_GMC74_HOOK("func_08003470_737C00", quest_gmc_3470)
Q_GMC74_HOOK("func_08003548_737CD8", quest_gmc_3548)
Q_GMC74_HOOK("func_08003620_737DB0", quest_gmc_3620)
Q_GMC74_HOOK("func_080036F8_737E88", quest_gmc_36f8)
#undef Q_GMC74_HOOK

static void q_retire(void *actor) {
  QuestNode *n = q_lookup(actor);
  unsigned int i;
  if (!n)
    return;
  if (n->hidden && n->object) {
    QB(n->object, 0x64) = n->saved64;
    if (q_hidden)
      --q_hidden;
    n->hidden = 0;
  }
  n->retired = 1;
  for (i = 1; i <= WQ_FAMILY_GMC75; ++i)
    if (q_roots[i] == actor)
      q_roots[i] = 0;
}

RECOMP_HOOK("func_80034EF8_35AF8")
void quest_retire_explicit(void *actor) { q_retire(actor); }
RECOMP_HOOK("func_80035020_35C20")
void quest_retire_current(void) { q_retire(D_8016DAB4_16E6B4); }

/* ---- row validation ---------------------------------------------------- */

static int q_role_row_ok(const int *r) {
  unsigned int role = (unsigned int)r[WQ_ROLE];
  unsigned int family = (unsigned int)r[WQ_FAMILY];
  unsigned int part = (unsigned int)r[WQ_PART];
  unsigned int expected = role_model(role, part);
  unsigned int entity;
  unsigned int part_min, part_max;
  if (!role || family != role_family(role) || !expected)
    return 0;
  if ((unsigned int)r[WQ_MODEL] != expected ||
      (unsigned int)r[WQ_SLOT] != role_slot(role, part))
    return 0;
  if ((unsigned int)r[WQ_CLIP] > role_clip_max(role, part))
    return 0;
  part_min = role_part_min(role);
  part_max = role_parts(role);
  if ((unsigned int)r[WQ_PART] < part_min ||
      (unsigned int)r[WQ_PART] > part_max)
    return 0;
  switch (family) {
    case WQ_FAMILY_GATEWAY: entity = 0x316; break;
    case WQ_FAMILY_KORYUTA:
      /* func_80217360 births the eleven display parts as entity 0x1B4;
       * func_080011FC only retags their render model to 0x1B0. */
      entity = role == WQ_ROLE_KORYUTA_PART ? 0x1b4 : 0x1b0;
      break;
    case WQ_FAMILY_KIHACHI: entity = 0x315; break;
    /* File_74 uses func_80220410, whose new task starts with +0x5C=0;
     * unlike func_802171A8 it does not inherit the root's entity ID. */
    case WQ_FAMILY_GMC74: entity = 0; break;
    case WQ_FAMILY_GMC75: entity = 0x35d; break;
    default: return 0;
  }
  if ((unsigned int)r[WQ_ENTITY] != entity ||
      (family == WQ_FAMILY_GATEWAY && r[WQ_PARENT] != 5) ||
      (family == WQ_FAMILY_KORYUTA && r[WQ_PARENT] != 5) ||
      (family == WQ_FAMILY_KIHACHI && r[WQ_PARENT] != 2 &&
       r[WQ_PARENT] != 7) ||
      (family == WQ_FAMILY_GMC74 && r[WQ_PARENT] != 4) ||
      (family == WQ_FAMILY_GMC75 && r[WQ_PARENT] != 5))
    return 0;
  if (r[WQ_GEN] || r[WQ_COLOUR] ||
      (r[WQ_COLOUR_MODE] && role != WQ_ROLE_KIHACHI_COLOUR))
    return 0;
  if (role == WQ_ROLE_GATEWAY_MESH) {
    if (r[WQ_MESH_FLAGS] != 0 && r[WQ_MESH_FLAGS] != (int)QUEST_MESH_FLAGS_WORD)
      return 0;
    return r[WQ_MESH_READY] == 0 || r[WQ_MESH_READY] == 1;
  }
  /* Only the mesh carries an alpha and a mesh flag word. */
  return r[WQ_ALPHA] == 0 && r[WQ_MESH_FLAGS] == 0 && r[WQ_MESH_READY] == 0;
}

int anchor_world_quest_row_valid(const int *r) {
  unsigned int i;
  if (!r || r[WQ_ABI] != WORLD_QUEST_ABI ||
      r[WQ_FAMILY] < 1 || r[WQ_FAMILY] > 5 ||
      r[WQ_ROLE] < 1 || r[WQ_ROLE] > WQ_ROLE_GMC75_CHILD ||
      r[WQ_SELF] < 0 || r[WQ_OWNER] < 0 || r[WQ_SERIAL] < 1 ||
      r[WQ_LIFE] < WQ_LIVE || r[WQ_LIFE] > WQ_REMOVED ||
      r[WQ_GEN] || r[WQ_PARENT] < 1 || r[WQ_PARENT] > 7 ||
      r[WQ_ORDINAL] < 1 || r[WQ_ORDINAL] > 255 ||
      r[WQ_ENTITY] < 0 || r[WQ_ENTITY] > 1025 ||
      r[WQ_MODEL] < 0 || r[WQ_MODEL] > 1025 ||
      r[WQ_SLOT] < 0 || r[WQ_SLOT] > 7 ||
      r[WQ_PART] < 0 || r[WQ_PART] > 31 ||
      r[WQ_PHASE] < 0 || r[WQ_PHASE] > 255 ||
      r[WQ_TIMER] < -32768 || r[WQ_TIMER] > 32767 ||
      r[WQ_ALPHA] < 0 || r[WQ_ALPHA] > 255)
    return 0;
  for (i = WQ_X; i <= WQ_Z; ++i)
    if (r[i] < -3276800 || r[i] > 3276700)
      return 0;
  for (i = WQ_PITCH; i <= WQ_ROLL; ++i)
    if (r[i] < 0 || r[i] > 1023)
      return 0;
  for (i = WQ_SX; i <= WQ_SZ; ++i)
    if (r[i] < 0 || r[i] > 64000)
      return 0;
  if (r[WQ_CLIP] < 0 || r[WQ_CLIP] > 15 ||
      r[WQ_RATE] < 0 || r[WQ_RATE] > 65535 ||
      r[WQ_FRAME] < 0 || r[WQ_FRAME] > 1000000 ||
      r[WQ_ANIM] < 0 || r[WQ_ANIM] > 7 ||
      r[WQ_BYTE5] < 0 || r[WQ_BYTE5] > 255 || r[WQ_COLOUR] ||
      r[WQ_FLAGS_LO] < 0 || r[WQ_FLAGS_LO] > 65535 ||
      r[WQ_FLAGS_HI] < 0 || r[WQ_FLAGS_HI] > 65535 ||
      r[WQ_AUX_LO] < 0 || r[WQ_AUX_LO] > 65535 ||
      r[WQ_AUX_HI] < 0 || r[WQ_AUX_HI] > 65535 ||
      r[WQ_DURABLE] < 0 || r[WQ_DURABLE] > 3 ||
      r[WQ_MESH_FLAGS] < 0 || r[WQ_MESH_FLAGS] > 65535 ||
      r[WQ_MESH_READY] < 0 || r[WQ_MESH_READY] > 1 ||
      r[WQ_TEXTURE] < 0 || r[WQ_TEXTURE] > 255 ||
      r[WQ_COLOUR_MODE] < 0 || r[WQ_COLOUR_MODE] > 1 ||
      r[WQ_INSTANCE] < 0 || r[WQ_RECEIPT] < 0)
    return 0;
  for (i = WQ_RED; i <= WQ_COLOUR_ALPHA; ++i)
    if (r[i] < 0 || r[i] > 255 || (!r[WQ_COLOUR_MODE] && r[i]))
      return 0;
  for (i = WQ_FREE0; i < WQ_INSTANCE; ++i)
    if (r[i])
      return 0;
  return q_role_row_ok(r);
}

/* ---- JSON -------------------------------------------------------------- */

typedef struct {
  char *p, *end;
  int ok;
} QOutput;

static void q_put(QOutput *o, char c) {
  if (o->p >= o->end)
    o->ok = 0;
  else
    *o->p++ = c;
}

static void q_text(QOutput *o, const char *s) {
  while (*s)
    q_put(o, *s++);
}

static void q_ws(const char **p) {
  while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t')
    ++*p;
}

static int q_token(const char **p, char c) {
  q_ws(p);
  if (**p != c)
    return 0;
  ++*p;
  return 1;
}

static int q_number(const char **p, int *out) {
  unsigned int v = 0, negative = 0, limit = 0x7fffffffu;
  int n = 0;
  q_ws(p);
  if (**p == '-') {
    negative = 1;
    limit = 0x80000000u;
    ++*p;
  }
  if (**p == '0' && (*p)[1] >= '0' && (*p)[1] <= '9')
    return 0;
  while (**p >= '0' && **p <= '9') {
    unsigned int d = (unsigned int)(*(*p)++ - '0');
    if (++n > 10 || v > (limit - d) / 10u)
      return 0;
    v = v * 10u + d;
  }
  if (!n)
    return 0;
  *out = negative ? (int)(0u - v) : (int)v;
  return 1;
}

int anchor_world_quest_encode(const int rows[][WORLD_QUEST_WORDS],
                              unsigned int count, char *out,
                              unsigned int capacity) {
  QOutput o;
  unsigned int i, j;
  char n[12];
  if (!out || !capacity || count > WORLD_QUEST_MAX)
    return 0;
  o.p = out;
  o.end = out + capacity - 1;
  o.ok = 1;
  q_text(&o, "{\"a\":[");
  for (i = 0; i < count; ++i) {
    if (!anchor_world_quest_row_valid(rows[i]))
      return 0;
    if (i)
      q_put(&o, ',');
    q_put(&o, '[');
    for (j = 0; j < WORLD_QUEST_WORDS; ++j) {
      if (j)
        q_put(&o, ',');
      mnsg_string_write_s32(n, rows[i][j]);
      q_text(&o, n);
    }
    q_put(&o, ']');
  }
  q_text(&o, "]}");
  *o.p = 0;
  return o.ok;
}

int anchor_world_quest_decode(const char *json,
                              int rows[][WORLD_QUEST_WORDS],
                              unsigned int *count) {
  const char *p = json;
  unsigned int n = 0, i, j;
  if (!p || !rows || !count || !q_token(&p, '{') || !q_token(&p, '"') ||
      !q_token(&p, 'a') || !q_token(&p, '"') || !q_token(&p, ':') ||
      !q_token(&p, '['))
    return 0;
  q_ws(&p);
  while (*p != ']') {
    if (n >= WORLD_QUEST_MAX || (n && !q_token(&p, ',')) || !q_token(&p, '['))
      return 0;
    for (i = 0; i < WORLD_QUEST_WORDS; ++i)
      if ((i && !q_token(&p, ',')) || !q_number(&p, &rows[n][i]))
        return 0;
    if (!q_token(&p, ']') || !anchor_world_quest_row_valid(rows[n]))
      return 0;
    /* Identity must be unique inside one message. */
    for (j = 0; j < n; ++j)
      if (rows[j][WQ_FAMILY] == rows[n][WQ_FAMILY] &&
          rows[j][WQ_ROLE] == rows[n][WQ_ROLE] &&
          rows[j][WQ_PARENT] == rows[n][WQ_PARENT] &&
          rows[j][WQ_ORDINAL] == rows[n][WQ_ORDINAL] &&
          rows[j][WQ_PART] == rows[n][WQ_PART])
        return 0;
    ++n;
    q_ws(&p);
  }
  if (!q_token(&p, ']') || !q_token(&p, '}'))
    return 0;
  q_ws(&p);
  if (*p)
    return 0;
  *count = n;
  return 1;
}
