/* mini-AOSP Parcel stub — replaced with real serialization in Stage 0D */
#include "Parcel.h"
#include <stdlib.h>
#include <string.h>

void parcel_init(struct parcel *p) {
    p->capacity = PARCEL_INITIAL_CAPACITY;
    p->data = (uint8_t *)malloc(p->capacity);
    p->size = 0;
    p->read_pos = 0;
}

void parcel_destroy(struct parcel *p) {
    free(p->data);
    p->data = NULL;
    p->size = 0;
    p->capacity = 0;
    p->read_pos = 0;
}

static void parcel_ensure(struct parcel *p, size_t extra) {
    while (p->size + extra > p->capacity) {
        p->capacity *= 2;
        p->data = (uint8_t *)realloc(p->data, p->capacity);
    }
}

void parcel_write_int32(struct parcel *p, int32_t val) {
    parcel_ensure(p, sizeof(val));
    memcpy(p->data + p->size, &val, sizeof(val));
    p->size += sizeof(val);
}

void parcel_write_string(struct parcel *p, const char *val) {
    int32_t len = (int32_t)strlen(val);
    parcel_write_int32(p, len);
    parcel_ensure(p, (size_t)len);
    memcpy(p->data + p->size, val, (size_t)len);
    p->size += (size_t)len;
}

int32_t parcel_read_int32(struct parcel *p) {
    int32_t val = 0;
    if (p->read_pos + sizeof(val) <= p->size) {
        memcpy(&val, p->data + p->read_pos, sizeof(val));
        p->read_pos += sizeof(val);
    }
    return val;
}

const char *parcel_read_string(struct parcel *p, int32_t *out_len) {
    int32_t len = parcel_read_int32(p);
    if (out_len) *out_len = len;
    if (len > 0 && p->read_pos + (size_t)len <= p->size) {
        const char *s = (const char *)(p->data + p->read_pos);
        p->read_pos += (size_t)len;
        return s; /* NOTE: not null-terminated — use out_len */
    }
    if (out_len) *out_len = 0;
    return "";
}

const uint8_t *parcel_data(const struct parcel *p) { return p->data; }
size_t parcel_data_size(const struct parcel *p) { return p->size; }

void parcel_set_data(struct parcel *p, const uint8_t *data, size_t size) {
    if (size > p->capacity) {
        p->capacity = size;
        p->data = (uint8_t *)realloc(p->data, p->capacity);
    }
    memcpy(p->data, data, size);
    p->size = size;
    p->read_pos = 0;
}
