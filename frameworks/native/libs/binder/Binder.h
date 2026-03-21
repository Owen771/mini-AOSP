#pragma once
// mini-AOSP Binder — IPC interface base class (stub for Stage 0)
// Real implementation in Stage 2: transact(), onTransact(), IBinder
#include <string>
#include <cstdint>

namespace miniaosp {

class IBinder {
public:
    virtual ~IBinder() = default;
    virtual int32_t transact(uint32_t code, const class Parcel& data, class Parcel* reply) = 0;
};

class BBinder : public IBinder {
public:
    int32_t transact(uint32_t code, const class Parcel& data, class Parcel* reply) override;
protected:
    virtual int32_t onTransact(uint32_t code, const class Parcel& data, class Parcel* reply);
};

} // namespace miniaosp
