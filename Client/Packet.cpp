// Packet.cpp
#include "Packet.h"
#include <arpa/inet.h>

// 序列化 Packet 结构体为字节流
std::string serialize(const Packet& pkt) {
    uint32_t body_length = pkt.data.size();
    uint32_t type = htonl(pkt.type); // 转换为网络字节序
    uint32_t length = htonl(body_length); // 转换为网络字节序

    std::string buffer;
    buffer.append(reinterpret_cast<const char*>(&length), sizeof(length));
    buffer.append(reinterpret_cast<const char*>(&type), sizeof(type));
    buffer.append(pkt.data);
    return buffer;
}

// 反序列化字节流为 Packet 结构体
bool deserialize(const std::string& buffer, Packet& pkt) {
    if (buffer.size() < 8) {
        return false; // 缓冲区不足
    }

    uint32_t length;
    uint32_t type;
    std::memcpy(&length, buffer.data(), sizeof(length));
    std::memcpy(&type, buffer.data() + 4, sizeof(type));

    length = ntohl(length);
    type = ntohl(type);

    if (buffer.size() < 8 + length) {
        return false; // 缓冲区不足
    }

    pkt.type = type;
    pkt.data = buffer.substr(8, length);
    return true;
}
