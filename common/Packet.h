// Packet.h

#ifndef PACKET_H
#define PACKET_H

#include <vector>
#include <cstdint>
#include <string>
#include <unordered_map>

struct TLV {
    uint8_t tag;
    uint16_t length;
    std::vector<uint8_t> value;
};

class Packet {
public:
    std::vector<TLV> tlvs;

    // 序列化：将 Packet 转换为字节流
    std::vector<uint8_t> serialize() const;

    // 反序列化：将字节流转换为 Packet
    bool deserialize(const std::vector<uint8_t>& data);
};

#endif // PACKET_H
