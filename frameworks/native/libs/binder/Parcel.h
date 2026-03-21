#ifndef MINIAOSP_PARCEL_H
#define MINIAOSP_PARCEL_H

/* mini-AOSP Parcel — binary serialization container (stub for Stage 0)
 * Real implementation in Stage 0D: writeInt32, writeString, cross-language interop */
#include <stdint.h>
#include <stddef.h>

#define PARCEL_INITIAL_CAPACITY 256

struct parcel {
    uint8_t *data;
    size_t   size;      /* bytes written */
    size_t   capacity;  /* allocated size */
    size_t   read_pos;  /* current read offset */
};

void    parcel_init(struct parcel *p);
void    parcel_destroy(struct parcel *p);

void    parcel_write_int32(struct parcel *p, int32_t val);
void    parcel_write_string(struct parcel *p, const char *val);

int32_t parcel_read_int32(struct parcel *p);
/* Returns pointer into parcel buffer — valid until next write. Caller must NOT free. */
const char *parcel_read_string(struct parcel *p, int32_t *out_len);

const uint8_t *parcel_data(const struct parcel *p);
size_t         parcel_data_size(const struct parcel *p);
void           parcel_set_data(struct parcel *p, const uint8_t *data, size_t size);

#endif /* MINIAOSP_PARCEL_H */
