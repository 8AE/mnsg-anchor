#ifndef ANCHOR_REMOTE_APPEARANCE_H
#define ANCHOR_REMOTE_APPEARANCE_H

/* Apply only the native render-hide bit. This must not retire the object,
 * unbind its model, alter scale, or disable its separate collision body. */
void anchor_remote_appearance_apply_hurt(void *object, int appearance_flags,
                                        unsigned short native_frame);

#endif
