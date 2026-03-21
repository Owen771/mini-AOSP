// mini-AOSP Parcel stub — replaced with real serialization in Stage 2
#include "Parcel.h"
#include <cstring>

namespace miniaosp {

void Parcel::writeInt32(int32_t val) {
    auto* bytes = reinterpret_cast<const uint8_t*>(&val);
    mData.insert(mData.end(), bytes, bytes + sizeof(val));
}

void Parcel::writeString(const std::string& val) {
    writeInt32(static_cast<int32_t>(val.size()));
    mData.insert(mData.end(), val.begin(), val.end());
}

int32_t Parcel::readInt32() {
    int32_t val = 0;
    if (mReadPos + sizeof(val) <= mData.size()) {
        std::memcpy(&val, mData.data() + mReadPos, sizeof(val));
        mReadPos += sizeof(val);
    }
    return val;
}

std::string Parcel::readString() {
    int32_t len = readInt32();
    if (len > 0 && mReadPos + len <= mData.size()) {
        std::string val(mData.begin() + mReadPos, mData.begin() + mReadPos + len);
        mReadPos += len;
        return val;
    }
    return "";
}

const uint8_t* Parcel::data() const { return mData.data(); }
size_t Parcel::dataSize() const { return mData.size(); }

void Parcel::setData(const uint8_t* data, size_t size) {
    mData.assign(data, data + size);
    mReadPos = 0;
}

} // namespace miniaosp
