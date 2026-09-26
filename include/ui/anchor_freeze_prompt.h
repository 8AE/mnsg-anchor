#ifndef ANCHOR_FREEZE_PROMPT_H
#define ANCHOR_FREEZE_PROMPT_H

/* The freeze prompt is a native window with no scenario or input ownership.
 * Other native dialogs may take its slot before starting. */
void anchor_freeze_prompt_yield(void);
int anchor_freeze_prompt_visible(void);

#endif
