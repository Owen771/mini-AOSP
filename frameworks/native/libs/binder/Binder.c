/* mini-AOSP Binder stub — replaced with real IPC in Stage 2 */
#include "Binder.h"
#include "Parcel.h"

int32_t binder_transact(struct binder_node *node, uint32_t code,
                        const struct parcel *data, struct parcel *reply) {
    if (node && node->ops && node->ops->on_transact) {
        return node->ops->on_transact(node->impl, code, data, reply);
    }
    return -1;
}
