#include "modding.h"
#include "anchor_runtime.h"

/*
 * The actual startup menu is shown from func_80002040_2C40 frame hooks.
 * Keep this file as the launch-gate home while avoiding thread-blocking
 * hooks that can stall the RT64/UI startup path.
 */
