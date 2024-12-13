// MySocket.cpp

#include "MySocket.h"
#include <arpa/inet.h>

bool MySocket::sendPacket(const Packet& pkt) const {
    std::vector<uint8_t> data = pkt.serialize();
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = send(sockfd, data.data() + totalSent, data.size() - totalSent, 0);
        if (sent <= 0) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool MySocket::recvPacket(Packet& pkt) const {
    // 先读取所有数据到缓冲区（假设发送方发送完整的包）
    std::vector<uint8_t> buffer;
    uint8_t temp[1024];
    ssize_t bytesReceived;
    while ((bytesReceived = recv(sockfd, temp, sizeof(temp), 0)) > 0) {
        buffer.insert(buffer.end(), temp, temp + bytesReceived);
        // 尝试解析
        if (pkt.deserialize(buffer)) {
            return true;
        }
    }
    return false;
}
