// MySocket.h

#ifndef MYSOCKET_H
#define MYSOCKET_H

#include "Packet.h"
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

class MySocket {
private:
    int sockfd;

public:
    MySocket(int socket_fd) : sockfd(socket_fd) {}
    ~MySocket() { close(sockfd); }

    // 发送 Packet
    bool sendPacket(const Packet& pkt) const;

    // 接收 Packet
    bool recvPacket(Packet& pkt) const;
};

#endif // MYSOCKET_H
