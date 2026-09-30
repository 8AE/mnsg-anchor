#ifndef ANCHOR_CONTROL_MACHINE_INTRO_H
#define ANCHOR_CONTROL_MACHINE_INTRO_H

/* A strictly decoded, fresh owner checkpoint may suppress only the unopened
 * File_58 Control Machine dialogue after local head proximity. */
void anchor_control_machine_intro_preview(unsigned int visit, int fresh);
/* Observer-only owner hint may delay unopened dialogue for 60 controller
 * ticks; only a fresh preview may actually skip it. */
void anchor_control_machine_intro_owner_hint(unsigned int visit,
                                              unsigned int owner);

#endif
