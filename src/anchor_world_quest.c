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
 * include/anchor_world_quest.h. JSON transport is owned by the parent. */
#ifndef WORLD_QUEST_HOST_TEST
#include "modding.h"
#endif
#include "anchor_world_quest.h"
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
extern int func_800141C4_14DC4(unsigned int);
extern int func_800240DC_24CDC(int);
extern void func_8021664C_5D1B1C(void *, unsigned int, float, unsigned int);
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

typedef struct {
  void *actor, *object;
  unsigned int self, owner, room, serial;
  unsigned int clip, rate, anim;
  unsigned char family, role, ordinal, part;
  unsigned char generation, ready, proxy, hidden, saved;
  unsigned char saved64, mesh_constructed;
} QuestNode;

static QuestNode q_nodes[WORLD_QUEST_MAX];
static int q_rows[WORLD_QUEST_MAX][WORLD_QUEST_WORDS];
static unsigned int q_row_count;
static unsigned int q_room = 0xffffu;
static unsigned int q_visit;
static unsigned int q_serial;
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

/* The rendered model of a role, or 0 when the family stages several models
 * behind one role (the File_74 children) and the row's own model is trusted
 * after the resident-descriptor check. */
static unsigned int role_model(unsigned int role) {
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
  }
  return 0;
}

/* Highest clip index the native callbacks of this role ever select. */
static unsigned int role_clip_max(unsigned int role) {
  switch (role) {
    case WQ_ROLE_GATEWAY_NPC: return 3;
    case WQ_ROLE_KORYUTA_PART: return 2;
    case WQ_ROLE_GMC74_CHILD: return 11;
  }
  return 0;
}

/* Verified default clip and rate (1/256ths) of a role. A role with no native
 * animation binding stays at clip 0 with animation disabled. */
static void role_anim(unsigned int role, unsigned int *clip,
                      unsigned int *rate, unsigned int *anim) {
  *clip = 0;
  *rate = 0;
  *anim = 0;
  switch (role) {
    case WQ_ROLE_GATEWAY_NPC:
      *clip = 3;
      *rate = 85; /* 0.3333 */
      *anim = 1;
      return;
    case WQ_ROLE_GATEWAY_CHILD_A:
      *rate = 13; /* 0.05 */
      *anim = 1;
      return;
    case WQ_ROLE_KORYUTA_BODY:
      *rate = 26; /* 0.1 */
      *anim = 1;
      return;
    case WQ_ROLE_KIHACHI_NPC:
      *rate = 85; /* 0.3333 */
      *anim = 1;
      return;
  }
}

/* Highest part/child index a role stages, or 0 when it stages none. */
static unsigned int role_parts(unsigned int role) {
  switch (role) {
    case WQ_ROLE_KORYUTA_PART: return WORLD_QUEST_KORYUTA_PARTS;
    case WQ_ROLE_GMC74_CHILD: return WORLD_QUEST_GMC74_CHILDREN - 1u;
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
  void **m;
  if (model > 1025u)
    return 0;
  m = D_80236984_5F1E54[model];
  if (!m || !m[0] || !m[1])
    return 0;
  *files = (const unsigned short *)QP(m, 0);
  *clips = (const unsigned int *)QP(m, 4);
  return *files && *clips;
}

/* A missing resource must never reach the native binder. */
static int model_resident(unsigned int model) {
  const unsigned short *files;
  const unsigned int *clips;
  if (!model_tables(model, &files, &clips))
    return 0;
  if (func_800141C4_14DC4(files[0]) == -1)
    return 0;
  if (func_800141C4_14DC4(files[1]) == -1)
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
    if (q_nodes[i].actor == actor)
      return &q_nodes[i];
  return 0;
}

static QuestNode *q_alloc(void *actor) {
  QuestNode *n = q_lookup(actor);
  unsigned int i;
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
  n->ready = n->object ? 1 : 0;
  return n;
}

static void q_unbind(QuestNode *n) {
  if (!n)
    return;
  q_clear(n, sizeof(*n));
}

/* A recycled task slot keeps the pointer but changes generation. The object
 * pointer and the generation byte together prove this is still the same live
 * instance; the +0x68 bit 1 is the native removal flag. */
static int q_live(QuestNode *n) {
  return n->actor && n->object && n->ready &&
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
  if (!n)
    return;
  anchor_world_quest_hide(actor, 0);
  q_unbind(n);
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
    if (!n->actor)
      continue;
    anchor_world_quest_hide(n->actor, 0);
    q_unbind(n);
  }
  q_hidden = 0;
  q_row_count = 0;
}

void anchor_world_quest_room(unsigned int room) {
  if (q_room == room)
    return;
  q_release_all();
  q_room = room;
  q_active = 0;
  q_visit = 0;
}

void anchor_world_quest_reset(int room_changed) {
  if (!room_changed) {
    unsigned int i;
    for (i = 0; i < WORLD_QUEST_MAX; ++i)
      anchor_world_quest_hide(q_nodes[i].actor, 0);
    return;
  }
  q_release_all();
  q_room = 0xffffu;
  q_active = 0;
  q_visit = 0;
  q_serial = 0;
}

unsigned int anchor_world_quest_room_id(void) { return q_room; }

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
  unsigned int model = QH(a, 0x5e), expected = role_model(role);
  unsigned int clip = n->clip, rate = n->rate, anim = n->anim;
  unsigned int i;
  if (!family || !family_room(family, D_800C7AB2))
    return 0;
  /* A task this module constructed is a proxy, never a native source. */
  if (n->proxy)
    return 0;
  if (n->role && n->role != role)
    return 0;
  if (expected && model != expected)
    return 0;
  if (!model_resident(model))
    return 0;
  if (clip > role_clip_max(role))
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
  row[WQ_GEN] = (int)n->generation;
  row[WQ_PARENT] = (int)anchor_world_quest_root_slot(D_800C7AB2, family);
  row[WQ_ORDINAL] = (int)ordinal;
  row[WQ_ENTITY] = (int)QH(a, 0x5c);
  row[WQ_MODEL] = (int)model;
  row[WQ_SLOT] = 0;
  row[WQ_PART] = (int)n->part;
  if (role_phase(role)) {
    row[WQ_PHASE] = (int)QB(a, 0xd0);
    row[WQ_TIMER] = (int)QS(a, 0xd4);
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
  row[WQ_COLOUR] = (int)QW(o, 0x30);
  row[WQ_FLAGS_LO] = (int)(QW(a, 0x60) & 0xffffu);
  row[WQ_FLAGS_HI] = (int)(QW(a, 0x60) >> 16);
  row[WQ_AUX_LO] = (int)(QW(a, 0x64) & 0xffffu);
  row[WQ_AUX_HI] = (int)(QW(a, 0x64) >> 16);
  row[WQ_DURABLE] = q_durable(family);
  row[WQ_TEXTURE] = (int)QB(o, 0x7c);
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
  role_anim(role, &clip, &rate, &anim);
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
  if (!q_live(n))
    return 0;
  if (QH(a, 0x5e) != model) {
    /* Bind the model only after its descriptor and files are resident. */
    if (!model_resident(model))
      return 0;
    QH(a, 0x5e) = (unsigned short)model;
  }
  if (!clip_resident(model, clip))
    return 0;
  if (anim & 1u)
    func_8021664C_5D1B1C(a, clip, (float)rate / 256.0f, anim & 1u);
  QF(o, 8) = (float)row[WQ_X] / 100.0f;
  QF(o, 0xc) = (float)row[WQ_Y] / 100.0f;
  QF(o, 0x10) = (float)row[WQ_Z] / 100.0f;
  QH(o, 0x14) = (unsigned short)row[WQ_PITCH];
  QH(o, 0x16) = (unsigned short)row[WQ_YAW];
  QH(o, 0x18) = (unsigned short)row[WQ_ROLL];
  for (i = 0; i < 3; ++i)
    QF(o, 0x1c + i * 4) = (float)row[WQ_SX + (int)i] / 100.0f;
  QF(o, 0x28) = (float)row[WQ_FRAME] / 100.0f;
  QB(o, 5) = (unsigned char)row[WQ_BYTE5];
  QW(o, 0x30) = (unsigned int)row[WQ_COLOUR];
  /* The peer flag word is restored except for the local shadow bit, which is
   * the result of this client's own floor probe and stays local. */
  QW(a, 0x60) =
      ((((unsigned int)row[WQ_FLAGS_LO] & 0xffffu) |
        (((unsigned int)row[WQ_FLAGS_HI] & 0xffffu) << 16)) &
       ~QUEST_LOCAL_FLAGS) |
      (QW(a, 0x60) & QUEST_LOCAL_FLAGS);
  QW(a, 0x64) = ((unsigned int)row[WQ_AUX_LO] & 0xffffu) |
                (((unsigned int)row[WQ_AUX_HI] & 0xffffu) << 16);
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
  /* A proxy carrier may be tracked on its first apply; it is never captured. */
  n = n ? n : q_alloc(actor);
  if (!n || !q_live(n))
    return 0;
  if (n->role && n->role != role)
    return 0;
  return q_apply_common(n, row);
}

/* ---- proxy decision ---------------------------------------------------- */

int anchor_world_quest_update(void *actor, unsigned int self_id,
                              unsigned int owner) {
  QuestNode *n;
  int engaged;
  if (!actor || !self_id)
    return 0;
  n = q_alloc(actor);
  if (!n)
    return 0;
  if (n->actor != actor || !n->object)
    return 0;
  n->self = self_id;
  n->owner = owner;
  /* A valid shared proxy is one this client did not author. */
  engaged = owner != 0u && owner != self_id;
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
  (void)signature;
  if (q_room != room)
    anchor_world_quest_room(room);
  if (!active) {
    if (q_active) {
      q_active = 0;
      for (i = 0; i < WORLD_QUEST_MAX; ++i)
        anchor_world_quest_hide(q_nodes[i].actor, 0);
      q_row_count = 0;
    }
    return;
  }
  q_active = 1;
  q_visit = visit;
  q_row_count = 0;
  for (i = 0; i < WORLD_QUEST_MAX && q_row_count < WORLD_QUEST_MAX; ++i) {
    QuestNode *n = &q_nodes[i];
    if (!n->actor || n->proxy || !n->role || !n->self || !q_live(n))
      continue;
    if (!q_capture_common(n, n->role, n->ordinal, q_rows[q_row_count]))
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
  void *priv;
  if (!task)
    return 0;
  priv = q_mesh_private(task);
  if (!priv)
    return 0;
  return QW(priv, 0) == QUEST_MESH_FLAGS_WORD &&
         QP(priv, QUEST_MESH_FADE_SLOT) == (void *)q_mesh_fade_noop;
}

int anchor_world_quest_mesh_construct(void *task) {
  QuestNode *n;
  void *object, *priv;
  if (!task)
    return 0;
  if (D_800C7AB2 != WORLD_QUEST_ROOM_GATEWAY)
    return 0;
  n = q_lookup(task);
  if (!n || n->role != WQ_ROLE_GATEWAY_MESH)
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
  return anchor_world_quest_mesh_ready(task);
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
  row[WQ_MESH_READY] = anchor_world_quest_mesh_ready(task);
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
  if (!n || !q_live(n))
    return 0;
  if (n->role && n->role != WQ_ROLE_GATEWAY_MESH)
    return 0;
  if (!n->mesh_constructed) {
    if (!anchor_world_quest_mesh_construct(task))
      return 0;
  }
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

/* ---- row validation ---------------------------------------------------- */

static int q_role_row_ok(const int *r) {
  unsigned int role = (unsigned int)r[WQ_ROLE];
  unsigned int family = (unsigned int)r[WQ_FAMILY];
  unsigned int expected = role_model(role);
  unsigned int part_min, part_max;
  if (!role)
    return family == WQ_FAMILY_NONE && r[WQ_PART] == 0;
  if (family != role_family(role))
    return 0;
  if (expected && (unsigned int)r[WQ_MODEL] != expected)
    return 0;
  if ((unsigned int)r[WQ_CLIP] > role_clip_max(role))
    return 0;
  part_min = role_part_min(role);
  part_max = role_parts(role);
  if ((unsigned int)r[WQ_PART] < part_min ||
      (unsigned int)r[WQ_PART] > part_max)
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
  static const int lo[WORLD_QUEST_WORDS] = {
      WORLD_QUEST_ABI, 0, 0, 1, 0, 1, WQ_LIVE, 0, 0, 0,
      0, 0, 0, 0, 0, -32768, 0, -3276800, -3276800, -3276800,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, -2147483647 - 1, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0};
  static const int hi[WORLD_QUEST_WORDS] = {
      WORLD_QUEST_ABI, 5, 14, 2147483647, 2147483647, 2147483647, WQ_REMOVED,
      255, 256, 255, 1025, 1025, 2, 31, 255, 32767,
      255, 3276700, 3276700, 3276700, 1023, 1023, 1023, 64000, 64000, 64000,
      15, 65535, 1000000, 7, 255, 2147483647, 65535, 65535, 65535, 65535,
      3, 65535, 1, 255, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 2147483647, 2147483647, 2147483647, 2147483647};
  unsigned int i;
  if (!r)
    return 0;
  for (i = 0; i < WORLD_QUEST_WORDS; ++i)
    if (r[i] < lo[i] || r[i] > hi[i])
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
          rows[j][WQ_ORDINAL] == rows[n][WQ_ORDINAL])
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
