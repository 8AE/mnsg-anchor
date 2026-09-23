#ifndef ANCHOR_WORLD_QUEST_H
#define ANCHOR_WORLD_QUEST_H

/* Shared-presentation adapter for the four custom quest scenes that place
 * scripted, non-enemy actor graphs:
 *
 *   Gateway Viewpoint    File_53    room 0x153               entity 0x316
 *   Koryuta's flight     File_46    room 0x155               entity 0x1B0
 *   Kihachi's scene      File_62    rooms 0x16A / 0x182      entity 0x315
 *   Gorgeous Music Cstl  File_74/75 room 0xC1  entities 0x35C / 0x35D
 *
 * Every one of these graphs mixes portable visual scalars with a LOCAL
 * handshake inside the same task fields: the coordinator, the player/dialogue
 * script, the camera, the full-screen fade and the temporary-bit bank are all
 * local scene control and are never shared. This module therefore never
 * patches a native phase, timer or temporary bit. It captures the visible
 * scalar state, hides the original render of a tracked actor only while a
 * valid reconstructed proxy is active, and re-applies the same scalars to a
 * render-only proxy that binds native models and resources with a pure
 * initializer.
 *
 * Wire format: WORLD_QUEST_WORDS signed 32-bit words per record, at most
 * WORLD_QUEST_MAX records. No task, object, resource, script or callback
 * address is ever accepted from the network; every identity is pointer-free
 * (family, role, parent roster slot, activation ordinal, part). Native task
 * generations and render pointers are local and never enter a packet.
 * The header below enumerates the whole ABI: bump WORLD_QUEST_ABI whenever any
 * index, range or role meaning changes.
 *
 * The JSON envelope is owned by the parent transport. encode()/decode() are
 * provided so both sides agree on the schema without duplicating it. */

#define WORLD_QUEST_MAX 96
#define WORLD_QUEST_WORDS 64
#define WORLD_QUEST_JSON 98304

/* Row ABI. Increment on any field, range or role change. */
#define WORLD_QUEST_ABI 2

/* Field order of one record. 45..61 are reserved and must stay zero. */
enum {
  WQ_ABI = 0,     /* WORLD_QUEST_ABI */
  WQ_FAMILY,      /* WQ_FAMILY_* */
  WQ_ROLE,        /* WQ_ROLE_* */
  WQ_SELF,        /* explicit capturing client id (0 = unset) */
  WQ_OWNER,       /* explicit authoritative client id (0 = unset) */
  WQ_SERIAL,      /* >= 1, monotone per (family, role, parent, ordinal) */
  WQ_LIFE,        /* WQ_LIVE | WQ_CLAIM | WQ_REMOVED */
  WQ_GEN,         /* reserved zero: native generation is never transmitted */
  WQ_PARENT,      /* one-based placed roster slot of the family root */
  WQ_ORDINAL,     /* activation ordinal inside the parent (1..255) */
  WQ_ENTITY,      /* actor+0x5C */
  WQ_MODEL,       /* rendered model id, actor+0x5E */
  WQ_SLOT,        /* verified native model mode/static slot, 0..7 */
  WQ_PART,        /* role-local part/child index, 0..31 */
  WQ_PHASE,       /* native callback phase scalar */
  WQ_TIMER,       /* native callback timer/counter scalar */
  WQ_ALPHA,       /* mesh private+0x27 only; 0 for every other role */
  WQ_X,           /* object+0x08, hundredths */
  WQ_Y,           /* object+0x0C, hundredths */
  WQ_Z,           /* object+0x10, hundredths */
  WQ_PITCH,       /* object+0x14, 10-bit */
  WQ_YAW,         /* object+0x16, 10-bit */
  WQ_ROLL,        /* object+0x18, 10-bit */
  WQ_SX,          /* object+0x1C, hundredths */
  WQ_SY,          /* object+0x20, hundredths */
  WQ_SZ,          /* object+0x24, hundredths */
  WQ_CLIP,        /* bound clip index */
  WQ_RATE,        /* object+0x7E, 1/256ths */
  WQ_FRAME,       /* object+0x28, hundredths */
  WQ_ANIM,        /* object+0x7C low 3 bits (loop/animate flags) */
  WQ_BYTE5,       /* raw object+0x05 render byte */
  WQ_COLOUR,      /* reserved zero: object+0x30 is a local render pointer */
  WQ_FLAGS_LO,    /* actor+0x60 low half */
  WQ_FLAGS_HI,    /* actor+0x60 high half */
  WQ_AUX_LO,      /* actor+0x64 low half */
  WQ_AUX_HI,      /* actor+0x64 high half */
  WQ_DURABLE,     /* bit0: family durable completion observed (read-only) */
  WQ_MESH_FLAGS,  /* mesh private+0x00 typed flag word, 0x2D41 when ready */
  WQ_MESH_READY,  /* 1 when native mesh arrays have been constructed */
  WQ_TEXTURE,     /* raw object+0x7C local render/texture flag byte */
  WQ_RED = 40,    /* typed primitive colour, 0..255 */
  WQ_GREEN = 41,
  WQ_BLUE = 42,
  WQ_COLOUR_ALPHA = 43,
  WQ_COLOUR_MODE = 44, /* 0=none, 1=typed RGBA */
  WQ_FREE0 = 45,
  WQ_INSTANCE = 62, /* local task-incarnation id; zero on wire */
  WQ_RECEIPT = 63   /* locally echoed offer token; zero on wire */
};

enum { WQ_LIVE = 0, WQ_CLAIM = 1, WQ_REMOVED = 2 };

/* Family = the placed root whose script owns the graph. */
enum {
  WQ_FAMILY_NONE = 0,
  WQ_FAMILY_GATEWAY = 1, /* File_53, room 0x153 */
  WQ_FAMILY_KORYUTA = 2, /* File_46, room 0x155 */
  WQ_FAMILY_KIHACHI = 3, /* File_62, rooms 0x16A and 0x182 */
  WQ_FAMILY_GMC74 = 4,   /* File_74, room 0xC1, root 0x35C */
  WQ_FAMILY_GMC75 = 5    /* File_75, room 0xC1, root 0x35D */
};

/* Role = the verified child recipe inside that family. */
enum {
  WQ_ROLE_NONE = 0,
  WQ_ROLE_GATEWAY_NPC = 1,     /* model 0x31B, entity 0x316 */
  WQ_ROLE_GATEWAY_CHILD_A = 2, /* model 0x24E, phase actor+0xD0 */
  WQ_ROLE_GATEWAY_CHILD_B = 3, /* model 0x24E, phase +0xD0, timer +0xD4 */
  WQ_ROLE_GATEWAY_DECOR = 4,   /* model 0x33F, inert callback */
  WQ_ROLE_GATEWAY_MESH = 5,    /* model 0x24E, private 0x80 grid mesh */
  WQ_ROLE_KORYUTA_BODY = 6,    /* model 0x1B0 placed root */
  WQ_ROLE_KORYUTA_PART = 7,    /* model 0x1B4 descriptor, renders 0x1B0 */
  WQ_ROLE_KIHACHI_NPC = 8,     /* model 0x315 visible NPC */
  WQ_ROLE_KIHACHI_RISE = 9,    /* model 0x10D rising effect */
  WQ_ROLE_KIHACHI_COLOUR = 10, /* model 1 colour effect */
  WQ_ROLE_GMC74_ROOT = 11,     /* invisible controller; never a visual row */
  WQ_ROLE_GMC74_CHILD = 12,    /* one of the 22 staged children */
  WQ_ROLE_GMC75_ROOT = 13,     /* invisible controller; never a visual row */
  WQ_ROLE_GMC75_CHILD = 14     /* File_75 three staged visual children */
};

#define WORLD_QUEST_ROOM_GATEWAY 0x153
#define WORLD_QUEST_ROOM_KORYUTA 0x155
#define WORLD_QUEST_ROOM_KIHACHI_A 0x16a
#define WORLD_QUEST_ROOM_KIHACHI_B 0x182
#define WORLD_QUEST_ROOM_GMC 0xc1

/* Verified placed roster slots (one-based) of each family root. */
#define WORLD_QUEST_SLOT_GATEWAY 5
#define WORLD_QUEST_SLOT_KORYUTA 5
#define WORLD_QUEST_SLOT_KIHACHI_A 7
#define WORLD_QUEST_SLOT_KIHACHI_B 2
#define WORLD_QUEST_SLOT_GMC74 4
#define WORLD_QUEST_SLOT_GMC75 5

/* Koryuta's eleven body parts are numbered 1..11 in the native constructor. */
#define WORLD_QUEST_KORYUTA_PARTS 11
/* File_74 has 22 child pointers at private+0x04..+0x58. */
#define WORLD_QUEST_GMC74_CHILDREN 22
#define WORLD_QUEST_GMC75_CHILDREN 3

/* ---- schema helpers ---------------------------------------------------- */

/* 1 when the row is a well-formed record of this ABI: every word in range,
 * family/role consistent, identity pointer-free, reserved words zero. It does
 * not touch the game and does not check that resources are resident. */
int anchor_world_quest_row_valid(const int *row);

int anchor_world_quest_encode(const int rows[][WORLD_QUEST_WORDS],
                              unsigned int count, char *out,
                              unsigned int capacity);
int anchor_world_quest_decode(const char *json,
                              int rows[][WORLD_QUEST_WORDS],
                              unsigned int *count);

/* ---- identity ---------------------------------------------------------- */

/* Pointer-free identity extracted from a row. Any output may be null. */
int anchor_world_quest_identity(const int *row, unsigned int *family,
                                unsigned int *role, unsigned int *parent,
                                unsigned int *ordinal);

/* The placed roster slot of the family root that owns this room, or 0. */
unsigned int anchor_world_quest_root_slot(unsigned int room,
                                          unsigned int family);

/* ---- lifecycle --------------------------------------------------------- */

/* Set the current room. A change releases every tracked actor and clears the
 * proxy state; the original render byte of each tracked actor is restored. */
void anchor_world_quest_room(unsigned int room);
void anchor_world_quest_reset(int room_changed);

/* Per-frame lifecycle. inactive (0) releases all proxies and restores every
 * hidden render byte. active (1) re-captures the tracked actors into the
 * snapshot the parent reads with anchor_world_quest_rows(). */
void anchor_world_quest_frame(unsigned int room, unsigned int signature,
                              unsigned int visit, int active);

/* Set the local client ID used by automatic source discovery. */
void anchor_world_quest_set_self(unsigned int self_id);

/* Accept selected peer rows for this room. The call may schedule native
 * child creation, so it returns the number fully applied now; retry pending
 * rows on subsequent frames. Only module-owned render proxies are applied.
 * A row with INSTANCE=0 may start a missing proxy. A nonzero INSTANCE must
 * match the local task incarnation. RECEIPT must be nonzero. */
unsigned int anchor_world_quest_receive(
    const int rows[][WORLD_QUEST_WORDS], unsigned int count,
    unsigned int self_id);

/* Expose local proxy instances and acknowledged receipts to the transport.
 * Each returned row is pointer-free and obeys the same ABI. */
unsigned int anchor_world_quest_status(
    int out[][WORLD_QUEST_WORDS], unsigned int capacity);

const int *anchor_world_quest_rows(void);
unsigned int anchor_world_quest_row_count(void);
unsigned int anchor_world_quest_room_id(void);

/* ---- capture / apply --------------------------------------------------- */

/* Register one local actor with its role recipe before capture/apply/mesh
 * construction. ordinal is the activation ordinal inside the family root
 * (1-based); part is the role-local part/child index and must be 0 for a role
 * that stages no parts. Returns 1 when the role belongs to the current room
 * and the part is in range. */
int anchor_world_quest_register(void *actor, unsigned int role,
                                unsigned int ordinal, unsigned int part);

/* Capture one live tracked actor. role is the WQ_ROLE_* this actor plays;
 * ordinal is the activation ordinal inside the family root (1-based).
 * Returns 0 when the actor is not a live instance of that role, when its
 * native generation no longer matches, or when the typed resources are not
 * resident. The row is pointer-free. */
int anchor_world_quest_capture(void *actor, unsigned int role,
                               unsigned int ordinal, int *row);

/* Apply a peer row to a local actor. Never replays a scene constructor, a
 * dialogue, a camera, a fade or a temporary bit: only the portable scalars of
 * the role, the typed model/clip binding and the render-hide bit are touched.
 * Returns 1 when the row was applied. */
int anchor_world_quest_apply(void *actor, const int *row);

/* Per-frame proxy decision with explicit self/owner ids. Tracks actor under
 * the explicit self id, records owner, and returns 1 while the local render of
 * that actor must stay hidden because a valid shared proxy is active. */
int anchor_world_quest_update(void *actor, unsigned int self_id,
                              unsigned int owner);

/* Bridge primitive: stamp and validate a captured row for transport with the
 * explicit self/owner ids. Returns 1 when out is a legal record. */
int anchor_world_quest_bridge(const int *row, unsigned int self_id,
                              unsigned int owner, int *out);

/* ---- render hide ------------------------------------------------------- */

/* Native render-hide contract: object+0x64 bit 0 makes the model draw path
 * return without drawing (verified at func_80016C44_17844). It never retires
 * the object, unbinds its model, alters scale or disables its collision body.
 * The original byte is saved on the first hide and written back verbatim on
 * release, so a native value is never destroyed. */
int anchor_world_quest_hide(void *actor, int hidden);
int anchor_world_quest_hidden(void *actor);
/* Restore the saved render byte and forget the actor. */
void anchor_world_quest_release(void *actor);

/* The pair a caller uses around the native per-actor integration so the
 * native AI/scope reset always observes the original render byte and the
 * render still sees the hide: begin restores the original byte, end re-applies
 * the hide while a proxy is engaged. */
void anchor_world_quest_scope_begin(void *actor);
void anchor_world_quest_scope_end(void *actor);

/* ---- Gateway mesh ------------------------------------------------------ */

/* The mesh is the one role whose constructor allocates: func_08001BD0_70FC80
 * allocates the private 0x80 block, installs the callback pair and schedules
 * func_80024160_24D60, which allocates the seven sub-arrays. It is safe to run
 * exactly once, in a task this module owns, while that task is current.
 *
 * mesh_construct() refuses a second call, refuses a task whose +0xD0 is not
 * empty and refuses a wrong room. After the native constructor returns it
 * replaces private+0x20 (the fade driver func_08001E60_70FF10) with a no-op so
 * the replica never writes temporary bits 8/0xC/0xD/0xF/0x10 and never drives
 * the native teardown. The private 0x80 block stays owned by the task
 * destructor; this module never frees it and never frees the sub-arrays. */
int anchor_world_quest_mesh_construct(void *task);
int anchor_world_quest_mesh_neutralize(void *task);
int anchor_world_quest_mesh_ready(void *task);
int anchor_world_quest_mesh_capture(void *task, int *row);
int anchor_world_quest_mesh_apply(void *task, const int *row);

#endif
