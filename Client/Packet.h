// Packet.h
#ifndef PACKET_H
#define PACKET_H

#include <string>
#include <cstdint>
#include <cstring>

// 定义消息类型
enum MessageType : uint32_t {
    MSG_TYPE_REQUEST = 1,
    MSG_TYPE_RESPONSE = 2,
    MSG_TYPE_INDICATION = 3
};

// 数据包结构体
struct Packet {
    uint32_t type;   // 消息类型
    std::string data; // 消息数据

    Packet() : type(0), data("") {}
    Packet(uint32_t t, const std::string& d) : type(t), data(d) {}
};

#endif // PACKET_H
