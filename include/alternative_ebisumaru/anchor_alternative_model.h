#ifndef ANCHOR_ALTERNATIVE_MODEL_H
#define ANCHOR_ALTERNATIVE_MODEL_H

/* The opening actor stores its mesh and four texture pages in segment 8.
 * Keep the playable action tree in segment 8 and relocate the opening mesh
 * into the second half of a private segment-9 broad file. */
#define ANCHOR_ALTERNATIVE_BROAD_BYTES 0x18000u
#define ANCHOR_ALTERNATIVE_MESH_OFFSET 0x18000u
#define ANCHOR_ALTERNATIVE_RENDER_DATA_SIZE 0x28000u
#define ANCHOR_ALTERNATIVE_RESOURCE_BYTES 0x9d70u
#define ANCHOR_ALTERNATIVE_DISPLAY_START 0x4798u
#define ANCHOR_ALTERNATIVE_DISPLAY_END 0x5d70u

/* Rebases the opening mesh's display-list references from segment 8 to the
 * appended part of segment 9. Returns zero if the expected US asset layout is
 * absent. The caller must copy the resident 0x4D9 file first. */
int anchor_alternative_rebase_mesh(unsigned char *broad, unsigned int resident_size);

/* Replaces only the display references in a private copy of one playable
 * Ebisumaru action tree. Joint links, motion data and model header stay native.
 * The complete raw action-file offset is used because the model header and
 * tree retain their segment-8 offsets after the slice is copied. */
int anchor_alternative_patch_action(unsigned char *slice, unsigned int slice_start,
                                    unsigned int slice_size, unsigned int model_ptr);

#endif
