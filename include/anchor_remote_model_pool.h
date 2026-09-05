#ifndef ANCHOR_REMOTE_MODEL_POOL_H
#define ANCHOR_REMOTE_MODEL_POOL_H

/* True only inside the CPU record storage owned by an expandable native
 * model/task pool chunk, including fields used as native list backlinks. */
int anchor_remote_model_pool_contains(const void *pointer);

#endif
