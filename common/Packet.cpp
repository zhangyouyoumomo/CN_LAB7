// Packet.cpp

#include "Packet.h"

std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> data;
    for (const auto& tlv : tlvs) {
        data.push_back(tlv.tag);
        // 将 length 转为大端序
        data.push_back((tlv.length >> 8) & 0xFF);
        data.push_back(tlv.length & 0xFF);
        data.insert(data.end(), tlv.value.begin(), tlv.value.end());
    }
    return data;
}

bool Packet::deserialize(const std::vector<uint8_t>& data) {
    tlvs.clear();
    size_t pos = 0;
    while (pos + 3 <= data.size()) { // 每个TLV至少3字节
        TLV tlv;
        tlv.tag = data[pos];
        tlv.length = (data[pos + 1] << 8) | data[pos + 2];
        pos += 3;
        if (pos + tlv.length > data.size()) {
            // 长度不匹配
            return false;
        }
        tlv.value.insert(tlv.value.end(), data.begin() + pos, data.begin() + pos + tlv.length);
        tlvs.push_back(tlv);
        pos += tlv.length;
    }
    return pos == data.size();
}
