/* Preserve native four-byte field placement with host-sized fixture pointers. */
void *reflection_read_pointer(const void *record, unsigned int offset);
void reflection_write_pointer(void *record, unsigned int offset, void *value);
#define TSURAMI_READ_POINTER(p, off) reflection_read_pointer(p, off)
#define TSURAMI_WRITE_POINTER(p, off, value) reflection_write_pointer(p, off, value)
