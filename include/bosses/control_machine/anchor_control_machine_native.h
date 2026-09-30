#ifndef ANCHOR_CONTROL_MACHINE_NATIVE_H
#define ANCHOR_CONTROL_MACHINE_NATIVE_H

#define ANCHOR_CONTROL_MACHINE_ROOT_WORDS 24
#define ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES 32
#define ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS 9

enum AnchorControlMachineRootField {
    CM_PHASE,
    CM_TIMER,
    CM_HP,
    CM_FLASH,
    CM_ORBIT,
    CM_SPEED,
    CM_SPEED_STEP,
    CM_INTENSITY,
    CM_YAW_CACHE,
    CM_X,
    CM_Y,
    CM_Z,
    CM_RX,
    CM_RY,
    CM_RZ,
    CM_SCALE_X,
    CM_SCALE_Y,
    CM_SCALE_Z,
    CM_FRAME,
    CM_STATUS,
    CM_STATUS_HP,
    CM_FLASH_ALPHA,
    CM_COLOUR,
    CM_COMMAND
};

enum AnchorControlMachineProjectileField {
    CM_PROJECTILE_ID,
    CM_PROJECTILE_BORN,
    CM_PROJECTILE_TIMER,
    CM_PROJECTILE_X,
    CM_PROJECTILE_Y,
    CM_PROJECTILE_Z,
    CM_PROJECTILE_VX,
    CM_PROJECTILE_VY,
    CM_PROJECTILE_VZ
};

typedef struct AnchorControlMachineSnapshot {
    unsigned int root[ANCHOR_CONTROL_MACHINE_ROOT_WORDS];
    unsigned int projectile_count;
    unsigned int projectile[ANCHOR_CONTROL_MACHINE_MAX_PROJECTILES]
                           [ANCHOR_CONTROL_MACHINE_PROJECTILE_WORDS];
} AnchorControlMachineSnapshot;

#if defined(DEBUG_BUTTON_ENABLED) && DEBUG_BUTTON_ENABLED
enum AnchorControlMachineDebugFailure {
    CM_DEBUG_FAIL_READY,
    CM_DEBUG_FAIL_MODEL,
    CM_DEBUG_FAIL_VALID,
    CM_DEBUG_FAIL_PROJECTILES,
    CM_DEBUG_FAIL_HOLD,
    CM_DEBUG_FAIL_RELEASE,
    CM_DEBUG_FAIL_REWIND,
    CM_DEBUG_FAILURE_COUNT
};

typedef struct AnchorControlMachineNativeDebug {
    unsigned int applied_count, pending_age;
    unsigned int failure[CM_DEBUG_FAILURE_COUNT];
    unsigned int phase, timer, hp, status, x, y, z, shots;
} AnchorControlMachineNativeDebug;

void anchor_control_machine_native_debug_take(
    AnchorControlMachineNativeDebug *out);
#endif

int anchor_control_machine_snapshot_valid(const AnchorControlMachineSnapshot *state);
void anchor_control_machine_native_tick(void);
unsigned int anchor_control_machine_native_visit(void);
int anchor_control_machine_native_bound(void);
int anchor_control_machine_native_ready(void);
int anchor_control_machine_native_transport_ready(void);
int anchor_control_machine_native_capture(AnchorControlMachineSnapshot *state);
int anchor_control_machine_native_pending(void);
int anchor_control_machine_native_apply(const AnchorControlMachineSnapshot *state,
                                        int force_recovery);
void anchor_control_machine_native_set_role(int active, int owner, int paused);
int anchor_control_machine_native_take_local_hit(int *sequence);
int anchor_control_machine_native_local_hit_window(void);
int anchor_control_machine_native_queue_hit(const int identity[5]);
int anchor_control_machine_native_hit_pending(void);
void anchor_control_machine_native_clear_hits(void);
void anchor_control_machine_native_clear_local_hits(void);
void anchor_control_machine_native_discard_pending(void);
int anchor_control_machine_native_request_terminal(void);
int anchor_control_machine_native_terminal_fallback(void);

#endif
