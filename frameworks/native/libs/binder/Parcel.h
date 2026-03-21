#pragma once
// mini-AOSP Parcel — serialization container (stub for Stage 0)
// Real implementation in Stage 2: writeInt32, writeString, flatten/unflatten
#include <string>
#include <vector>
#include <cstdint>

namespace miniaosp {

class Parcel {
public:
    void writeInt32(int32_t val);
    void writeString(const std::string& val);

    int32_t readInt32();
    std::string readString();

    const uint8_t* data() const;
    size_t dataSize() const;
    void setData(const uint8_t* data, size_t size);

private:
    std::vector<uint8_t> mData;
    size_t mReadPos = 0;
};

} // namespace miniaosp
