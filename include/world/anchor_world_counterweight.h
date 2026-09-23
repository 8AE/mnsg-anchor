#ifndef ANCHOR_WORLD_COUNTERWEIGHT_H
#define ANCHOR_WORLD_COUNTERWEIGHT_H

/* File_40 entity 0x3CB: room 0x6B's three placed six-piece counterweights.
 * Each placed root owns six sliding children (three extend up, three down).
 * The rider mask and the shared speed live on the root, and the native child
 * update rewrites that mask from the local player alone; this module replaces
 * each child continuation with a pure prediction driven by the aggregate. */
#define WORLD_COUNTERWEIGHT 12
#define WORLD_COUNTERWEIGHT_ENTITY 0x3cb
#define WORLD_COUNTERWEIGHT_ROOM 0x6b

/* 0 index 13..15, 1 entity 0x3CB, 2 kind 12, 3 busy (local rider),
 * 4..6 root XYZ hundredths, 7..9 root pitch/yaw/roll, 10..15 child Y
 * hundredths, 16 present mask 63, 17 selected rider bit, 18 selected speed
 * thousandths, 38 paused, 46 local rider bits, 47 aggregate rider bits,
 * 48/49 generic instance/receipt. 19..37 and 39..45 stay zero. */
#define WC_BUSY 3
#define WC_X 4
#define WC_Y 5
#define WC_Z 6
#define WC_PITCH 7
#define WC_YAW 8
#define WC_ROLL 9
#define WC_CHILD_Y 10
#define WC_PRESENT 16
#define WC_SELECTED 17
#define WC_SPEED 18
#define WC_PAUSED 38
#define WC_LOCAL 46
#define WC_AGGREGATE 47

void anchor_world_counterweight_reset(int room_changed);
void anchor_world_counterweight_register(void *root);
int anchor_world_counterweight_capture(void *root, int *row);
int anchor_world_counterweight_apply(void *root, const int *row);
int anchor_world_counterweight_needs_restore(void *root);
unsigned int anchor_world_counterweight_local_inputs(void *root);
void anchor_world_counterweight_control(void *root, int remote, int paused,
                                        int confirmed, unsigned int aggregate);
void anchor_world_counterweight_begin(void);
void anchor_world_counterweight_end(void);

/* Full 50-word typed contract. The root pose is pinned to the verified room
 * 0x6B placement for the row's index, and each child height is bounded by that
 * child's own extend/retract range, so a decoded row can never drive a child
 * outside its native clamp. The native adapter still checks the live source
 * pose and the resource table before it binds anything. */
static inline int anchor_world_counterweight_valid(const int *r) {
  /* Verified placements, root index -> X, Y, Z, pitch, yaw, roll. */
  static const int pose[3][6] = {{0, -8000, -2000, 0, 256, 0},
                                 {-30000, -8000, 0, 0, 512, 0},
                                 {30000, -8000, 0, 0, 0, 0}};
  /* Child heights relative to the root Y: three extend up, three down. */
  static const int rel_lo[6] = {0, 0, 0, -2000, -6000, -10000};
  static const int rel_hi[6] = {10000, 6000, 2000, 0, 0, 0};
  static const int bits[7] = {0, 1, 2, 4, 8, 16, 32};
  static const int speeds[7] = {0, 2400, 1600, 800, 800, 1600, 2400};
  int i, k, found = -1;
  if (!r)
    return 0;
  if (r[0] < 13 || r[0] > 15 || r[1] != WORLD_COUNTERWEIGHT_ENTITY ||
      r[2] != WORLD_COUNTERWEIGHT || (r[WC_BUSY] != 0 && r[WC_BUSY] != 1) ||
      r[WC_PRESENT] != 63 || (r[WC_PAUSED] != 0 && r[WC_PAUSED] != 1) ||
      r[WC_LOCAL] < 0 || r[WC_LOCAL] > 63 || r[WC_AGGREGATE] < 0 ||
      r[WC_AGGREGATE] > 63 || r[48] < 0 || r[49] < 0)
    return 0;
  for (i = 0; i < 6; ++i)
    if (r[WC_X + i] != pose[r[0] - 13][i])
      return 0;
  for (k = 0; k < 7; ++k)
    if (r[WC_SELECTED] == bits[k])
      found = k;
  if (found < 0 || r[WC_SPEED] != speeds[found])
    return 0;
  for (i = 19; i < 38; ++i)
    if (r[i])
      return 0;
  for (i = 39; i < 46; ++i)
    if (r[i])
      return 0;
  for (i = 0; i < 6; ++i)
    if (r[WC_CHILD_Y + i] < r[WC_Y] + rel_lo[i] ||
        r[WC_CHILD_Y + i] > r[WC_Y] + rel_hi[i])
      return 0;
  return 1;
}
#endif
