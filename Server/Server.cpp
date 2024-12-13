// Server.cpp
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <vector>
#include <glog/logging.h>
#include "Message/MySocket.h" // 引入封装的Socket类
#include <ctime>

#define SERVER_PORT 5869
#define MAX_CLIENT_QUEUE 20

class GlogWrapper {
public:
    GlogWrapper(char* program) {
        google::InitGoogleLogging(program);
        FLAGS_log_dir = "logs";
        FLAGS_alsologtostderr = true;
        FLAGS_colorlogtostderr = true;
        FLAGS_stop_logging_if_full_disk = true;
        google::InstallFailureSignalHandler();
    }
    ~GlogWrapper() {
        google::ShutdownGoogleLogging();
    }
};

// 处理客户端请求的函数
void handleClient(MySocket clientSocketObj, std::string clientIp, int clientPort) {
    LOG(INFO) << "Client thread started for " << clientIp << ":" << clientPort 
              << " (Server Port: " << SERVER_PORT << ")";
    try {
        while (true) {
            Packet pkt = clientSocketObj.recvPacket();
            LOG(INFO) << "Received packet of type " << pkt.type 
                      << " from " << clientIp << ":" << clientPort;

            Packet response;
            response.type = RESPONSE;

            switch (pkt.type) {
                case GET_TIME: {
                    // 获取当前服务器时间
                    std::time_t now = std::time(nullptr);
                    response.data = std::ctime(&now);
                    break;
                }
                case GET_NAME: {
                    // 返回服务器名称
                    response.data = "MyServer v1.0";
                    break;
                }
                case SEND_MESSAGE: {
                    // 发送消息到指定客户端（假设单客户端）
                    // 由于本例中服务器没有维护客户端列表，简单地回应成功
                    LOG(INFO) << "Client " << clientIp << ":" << clientPort 
                              << " requests to send message: " << pkt.data;
                    response.data = "Message received: " + pkt.data;
                    break;
                }
                case DISCONNECT: {
                    // 断开连接
                    LOG(INFO) << "Client " << clientIp << ":" << clientPort << " requested disconnection.";
                    response.data = "Disconnected successfully.";
                    clientSocketObj.sendPacket(response);
                    throw std::runtime_error("Client requested disconnection.");
                }
                default: {
                    response.data = "Unknown command.";
                    break;
                }
            }

            // 发送响应
            clientSocketObj.sendPacket(response);
            LOG(INFO) << "Sent response to " << clientIp << ":" << clientPort;
        }
    } catch (const std::exception& e) {
        LOG(INFO) << "Client " << clientIp << ":" << clientPort << " disconnected. Reason: " << e.what();
    }

    LOG(INFO) << "Closed client connection for " << clientIp << ":" << clientPort 
              << " (Server Port: " << SERVER_PORT << ")";
}

int main(int argc, char* argv[]) {
    // 初始化glog
    GlogWrapper glog(argv[0]);
    LOG(INFO) << "Server starting on port " << SERVER_PORT << "...";

    int serverSocket;
    struct sockaddr_in serverAddress;

    // 创建socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOG(ERROR) << "Failed to create socket on port " << SERVER_PORT;
        return -1;
    }
    LOG(INFO) << "Socket created successfully on port " << SERVER_PORT << ".";

    // 设置服务器地址信息
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY; // 接受任意本地网卡IP
    serverAddress.sin_port = htons(SERVER_PORT);

    // 绑定socket到本地地址
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        LOG(ERROR) << "Binding failed on port " << SERVER_PORT;
        close(serverSocket);
        return -1;
    }
    LOG(INFO) << "Binding successful on port " << SERVER_PORT;

    // 开始监听客户端连接请求
    if (listen(serverSocket, MAX_CLIENT_QUEUE) < 0) {
        LOG(ERROR) << "Listening failed on port " << SERVER_PORT;
        close(serverSocket);
        return -1;
    }
    LOG(INFO) << "Server listening on port " << SERVER_PORT << " with max queue: " 
              << MAX_CLIENT_QUEUE;

    // 为处理客户端请求创建线程容器
    std::vector<std::thread> threads;
    while (true) {
        struct sockaddr_in clientAddress;
        socklen_t addressLength = sizeof(clientAddress);
        int clientFd = accept(serverSocket, (struct sockaddr*)&clientAddress, &addressLength);

        if (clientFd < 0) {
            LOG(ERROR) << "A client failed to connect to server port " << SERVER_PORT << ".";
            continue;
        } else {
            std::string clientIp = inet_ntoa(clientAddress.sin_addr);
            int clientPort = ntohs(clientAddress.sin_port);
            LOG(INFO) << "A client has connected from " << clientIp << ":" << clientPort 
                      << " (Server Port: " << SERVER_PORT << ").";

            // 封装客户端Socket
            MySocket clientSocketObj(clientFd);

            // 为该客户端连接创建新线程进行处理
            threads.emplace_back(handleClient, std::move(clientSocketObj), clientIp, clientPort);
        }
    }

    // 一般不会到达这里，除非服务端异常退出或有退出机制
    // 清理：确保所有线程结束
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    close(serverSocket);
    LOG(INFO) << "Server shutting down on port " << SERVER_PORT << ".";
    return 0;
}
