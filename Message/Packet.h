// Message/Packet.h
#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <cstring>
#include <vector>

// 定义消息类型
enum class MessageType : uint8_t {
    GREETING = 1,
    TIME_REQUEST = 2,
    NAME_REQUEST = 3,
    CLIENT_LIST_REQUEST = 4,
    MESSAGE = 5,        // 广播消息
    SEND_MESSAGE = 6,   // 发送到特定客户端
    EXIT = 7,
    RESPONSE = 8        // 服务器响应
};

// 定义消息子类型
enum class MessageSubType : uint8_t {
    NONE = 0,
    BROADCAST = 1,
    DIRECT = 2,
    SUCCESS = 3,
    FAILURE = 4
};

// 数据包头部结构
struct PacketHeader {
    MessageType type;
    MessageSubType subType;
    uint32_t length; // 数据体长度（字节数）

    // 序列化头部
    std::vector<char> serialize() const {
        std::vector<char> buffer(sizeof(PacketHeader));
        buffer[0] = static_cast<char>(type);
        buffer[1] = static_cast<char>(subType);
        uint32_t netLength = htonl(length);
        std::memcpy(buffer.data() + 2, &netLength, sizeof(netLength));
        return buffer;
    }

    // 反序列化头部
    static PacketHeader deserialize(const char* buffer) {
        PacketHeader header;
        header.type = static_cast<MessageType>(buffer[0]);
        header.subType = static_cast<MessageSubType>(buffer[1]);
        uint32_t netLength;
        std::memcpy(&netLength, buffer + 2, sizeof(netLength));
        header.length = ntohl(netLength);
        return header;
    }
};

// 完整数据包结构
struct Packet {
    PacketHeader header;
    std::vector<char> data;

    // 序列化整个数据包
    std::vector<char> serialize() const {
        std::vector<char> buffer = header.serialize();
        buffer.insert(buffer.end(), data.begin(), data.end());
        return buffer;
    }

    // 反序列化整个数据包
    static Packet deserialize(const char* buffer, size_t totalLength) {
        Packet pkt;
        pkt.header = PacketHeader::deserialize(buffer);
        if (pkt.header.length > 0) {
            pkt.data.assign(buffer + sizeof(PacketHeader), buffer + totalLength);
        }
        return pkt;
    }
};

#endif // PACKET_H
