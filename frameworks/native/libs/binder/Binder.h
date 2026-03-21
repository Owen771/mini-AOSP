#ifndef MINIAOSP_BINDER_H
#define MINIAOSP_BINDER_H

/* mini-AOSP Binder — IPC interface base (stub for Stage 0)
 * Real implementation in Stage 2: transact(), onTransact()
 *
 * In C, we use function pointers instead of virtual methods (vtable pattern).
 * This is the same pattern Linux kernel uses for file_operations, inode_operations, etc.
 */
#include <stdint.h>

struct parcel; /* forward declaration */

/* Binder operations — like a C++ vtable */
struct binder_ops {
    int32_t (*on_transact)(void *self, uint32_t code,
                           const struct parcel *data, struct parcel *reply);
    void (*destroy)(void *self);
};

/* A binder node — holds the vtable + implementation pointer */
struct binder_node {
    struct binder_ops *ops;
    void *impl; /* points to the concrete service data */
};

/* Dispatch a transaction through the vtable */
int32_t binder_transact(struct binder_node *node, uint32_t code,
                        const struct parcel *data, struct parcel *reply);

#endif /* MINIAOSP_BINDER_H */
