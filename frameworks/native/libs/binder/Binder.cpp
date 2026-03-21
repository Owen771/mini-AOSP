// mini-AOSP Binder stub — replaced with real IPC in Stage 2
#include "Binder.h"
#include "Parcel.h"

namespace miniaosp {

int32_t BBinder::transact(uint32_t code, const Parcel& data, Parcel* reply) {
    return onTransact(code, data, reply);
}

int32_t BBinder::onTransact(uint32_t code, const Parcel& data, Parcel* reply) {
    // Stub — subclasses override this
    (void)code; (void)data; (void)reply;
    return -1;
}

} // namespace miniaosp
