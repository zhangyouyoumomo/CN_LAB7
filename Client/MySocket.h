// MySocket.h
#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "Packet.h"

class MySocket {
private:
    int sockfd;

public:
    // 构造函数
    MySocket(int socket_fd = -1) : sockfd(socket_fd) {}

    // 构造并连接
    bool connect_to(const std::string& ip, uint16_t port) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) return false;

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
            close(sockfd);
            return false;
        }

        if (::connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            return false;
        }
        return true;
    }

    // 发送 Packet
    bool send_packet(const Packet& pkt) const {
        std::string serialized = serialize(pkt);
        size_t total_sent = 0;
        size_t to_send = serialized.size();
        const char* data = serialized.c_str();

        while (total_sent < to_send) {
            ssize_t sent = send(sockfd, data + total_sent, to_send - total_sent, 0);
            if (sent <= 0) return false;
            total_sent += sent;
        }
        return true;
    }

    // 接收 Packet
    bool recv_packet(Packet& pkt) const {
        char header[8];
        size_t received = 0;
        while (received < 8) {
            ssize_t ret = recv(sockfd, header + received, 8 - received, 0);
            if (ret <= 0) return false;
            received += ret;
        }

        uint32_t length, type;
        std::memcpy(&length, header, sizeof(length));
        std::memcpy(&type, header + 4, sizeof(type));
        length = ntohl(length);
        type = ntohl(type);

        std::string body(length, '\0');
        received = 0;
        while (received < length) {
            ssize_t ret = recv(sockfd, &body[received], length - received, 0);
            if (ret <= 0) return false;
            received += ret;
        }

        pkt.type = type;
        pkt.data = body;
        return true;
    }

    // 关闭 Socket
    void close_socket() {
        if (sockfd != -1) {
            close(sockfd);
            sockfd = -1;
        }
    }

    // 析构函数
    ~MySocket() {
        close_socket();
    }
};

#endif // MYSOCKET_H
