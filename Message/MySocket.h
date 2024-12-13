// MySocket.h
#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <cstdint>
#include <cstring>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// 定义通信协议中的消息类型
enum MessageType : uint32_t {
    GET_TIME = 1,
    GET_NAME = 2,
    SEND_MESSAGE = 3,
    DISCONNECT = 4,
    RESPONSE = 100 // 服务器响应
};

// 数据包结构
struct Packet {
    MessageType type;
    std::string data;

    // 序列化数据包到字节流
    std::string serialize() const {
        uint32_t dataLength = 4 + data.size(); // 4字节用于MessageType
        uint32_t netLength = htonl(dataLength + 4); // 总长度包括长度字段本身
        uint32_t netType = htonl(type);

        std::string serialized;
        serialized.append(reinterpret_cast<const char*>(&netLength), sizeof(netLength));
        serialized.append(reinterpret_cast<const char*>(&netType), sizeof(netType));
        serialized.append(data);
        return serialized;
    }

    // 反序列化字节流到数据包
    static Packet deserialize(const std::string& buffer) {
        if (buffer.size() < 8) { // 4字节长度 + 4字节类型
            throw std::runtime_error("Buffer too small to deserialize Packet.");
        }

        Packet pkt;
        uint32_t netLength;
        uint32_t netType;
        memcpy(&netLength, buffer.data(), sizeof(netLength));
        memcpy(&netType, buffer.data() + 4, sizeof(netType));

        uint32_t length = ntohl(netLength);
        pkt.type = static_cast<MessageType>(ntohl(netType));

        if (buffer.size() < length) {
            throw std::runtime_error("Buffer does not contain full Packet data.");
        }

        pkt.data = buffer.substr(8, length - 4); // 4字节已用于MessageType
        return pkt;
    }
};

class MySocket {
private:
    int sockfd;

public:
    // 默认构造函数
    MySocket() : sockfd(-1) {}

    // 构造函数，允许设置文件描述符
    explicit MySocket(int fd) : sockfd(fd) {}

    // 析构函数
    ~MySocket() {
        if (sockfd != -1) {
            close(sockfd);
        }
    }

    // 禁止拷贝构造和拷贝赋值
    MySocket(const MySocket&) = delete;
    MySocket& operator=(const MySocket&) = delete;

    // 移动构造和移动赋值
    MySocket(MySocket&& other) noexcept : sockfd(other.sockfd) {
        other.sockfd = -1;
    }

    MySocket& operator=(MySocket&& other) noexcept {
        if (this != &other) {
            if (sockfd != -1) {
                close(sockfd);
            }
            sockfd = other.sockfd;
            other.sockfd = -1;
        }
        return *this;
    }

    // 连接到指定地址
    void connectTo(const std::string& address, uint16_t port) {
        if (sockfd != -1) {
            throw std::runtime_error("Socket already connected.");
        }
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket.");
        }

        struct sockaddr_in serverAddr;
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid server address.");
        }

        if (::connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw std::runtime_error("Connection to server failed.");
        }
    }

    // 发送数据包
    void sendPacket(const Packet& pkt) const {
        std::string serialized = pkt.serialize();
        size_t totalSent = 0;
        while (totalSent < serialized.size()) {
            ssize_t sent = send(sockfd, serialized.data() + totalSent, serialized.size() - totalSent, 0);
            if (sent <= 0) {
                throw std::runtime_error("Failed to send data.");
            }
            totalSent += sent;
        }
    }

    // 接收数据包
    Packet recvPacket() const {
        // 首先接收长度字段（4字节）
        char lengthBuffer[4];
        size_t received = 0;
        while (received < 4) {
            ssize_t bytes = recv(sockfd, lengthBuffer + received, 4 - received, 0);
            if (bytes <= 0) {
                throw std::runtime_error("Failed to receive packet length.");
            }
            received += bytes;
        }

        uint32_t netLength;
        memcpy(&netLength, lengthBuffer, sizeof(netLength));
        uint32_t length = ntohl(netLength);

        // 接收剩余的数据（length - 4字节已经读取）
        std::string dataBuffer(length - 4, '\0');
        received = 0;
        while (received < (length - 4)) {
            ssize_t bytes = recv(sockfd, &dataBuffer[received], (length - 4) - received, 0);
            if (bytes <= 0) {
                throw std::runtime_error("Failed to receive packet data.");
            }
            received += bytes;
        }

        // 反序列化数据包
        std::string fullBuffer;
        fullBuffer.append(reinterpret_cast<const char*>(&netLength), sizeof(netLength));
        fullBuffer.append(dataBuffer);
        return Packet::deserialize(fullBuffer);
    }

    // 获取底层socket文件描述符
    int getFd() const {
        return sockfd;
    }

    // 设置底层socket文件描述符（仅限服务器接受新的连接时使用）
    void setFd(int fd) {
        if (sockfd != -1) {
            close(sockfd);
        }
        sockfd = fd;
    }
};

#endif // MYSOCKET_H
