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
#include <unordered_map>
#include <mutex>
#include <csignal> // exit signal handling
#include <atomic>
#include <memory>
#include <glog/logging.h>
#include "Message/MySocket.h" // 引入封装的Socket类
#include <ctime>

#define SERVER_PORT 5869
#define MAX_CLIENT_QUEUE 20

// 全局变量
std::atomic<bool> serverRunning(true); // 全局运行标志
std::mutex clientsMutex;
std::unordered_map<int, std::pair<std::shared_ptr<MySocket>, std::string>> connectedClients; // 客户端ID -> (Socket, IP:Port)
int clientIdCounter = 1; // 客户端ID计数器

// 退出处理函数
void exitHandler(int signal) {
    LOG(INFO) << "Received exit signal (" << signal << "). Shutting down server...";
    serverRunning = false;
    // 关闭服务器socket将在main中处理
}

// 封装日志初始化和清理
class GlogWrapper {
public:
    GlogWrapper(char* program) {
        google::InitGoogleLogging(program);
        FLAGS_log_dir = "../logs";
        FLAGS_alsologtostderr = true;
        FLAGS_colorlogtostderr = true;
        FLAGS_stop_logging_if_full_disk = true;
        google::InstallFailureSignalHandler();
    }
    ~GlogWrapper() {
        google::ShutdownGoogleLogging();
    }
};
int respnsetime=0;
// 处理客户端请求的函数
void handleClient(int clientId, std::shared_ptr<MySocket> clientSocketPtr, std::string clientIp, int clientPort) {
    LOG(INFO) << "Client thread started for " << clientIp << ":" << clientPort 
              << " (Client ID: " << clientId << ")";
    try {
        while (serverRunning) {
            Packet pkt = clientSocketPtr->recvPacket();
            LOG(INFO) << "Received packet of type " << pkt.type 
                      << " from " << clientIp << ":" << clientPort 
                      << " (Client ID: " << clientId << ")";

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
                    response.data = "Zimo Server";
                    break;
                }
                case SEND_MESSAGE: {
                    // 发送消息到指定客户端
                    // 数据格式假设为 "targetId:message"
                    size_t delimiter = pkt.data.find(':');
                    if (delimiter == std::string::npos) {
                        response.data = "Invalid message format. Use targetId:message.";
                        break;
                    }
                    std::string targetIdStr = pkt.data.substr(0, delimiter);
                    std::string message = pkt.data.substr(delimiter + 1);
                    int targetId;
                    try {
                        targetId = std::stoi(targetIdStr);
                    } catch (...) {
                        response.data = "Invalid target client ID.";
                        break;
                    }

                    std::lock_guard<std::mutex> lock(clientsMutex);
                    auto it = connectedClients.find(targetId);
                    if (it != connectedClients.end()) {
                        Packet forwardPkt;
                        forwardPkt.type = SEND_MESSAGE;
                        forwardPkt.data = message;
                        it->second.first->sendPacket(forwardPkt);
                        response.data = "Message sent to client " + std::to_string(targetId) + ".";
                        LOG(INFO) << "Forwarded message from Client " << clientId 
                                  << " to Client " << targetId;
                    } else {
                        response.data = "Target client ID not found.";
                    }
                    break;
                }
                case LIST_CLIENTS: {
                    // 返回在线客户端列表
                    std::lock_guard<std::mutex> lock(clientsMutex);
                    std::string list;
                    for (const auto& [id, clientPair] : connectedClients) {
                        if (id != clientId) { // 不包括自己
                            list += "ID " + std::to_string(id) + ": " + clientPair.second + "\n";
                        }
                    }
                    if (list.empty()) {
                        response.data = "No other clients connected.";
                        clientSocketPtr->sendPacket(response);
                    } else {
                        Packet listPkt;
                        listPkt.type = CLIENT_LIST;
                        listPkt.data = list;
                        clientSocketPtr->sendPacket(listPkt);
                        LOG(INFO) << "Sent client list to Client " << clientId;
                    }
                    continue; // 不发送 RESPONSE 类型的包
                }
                case DISCONNECT: {
                    // 断开连接
                    LOG(INFO) << "Client " << clientIp << ":" << clientPort 
                              << " (ID: " << clientId << ") requested disconnection.";
                    response.data = "Disconnected successfully.";
                    clientSocketPtr->sendPacket(response);
                    throw std::runtime_error("Client requested disconnection.");
                }
                default: {
                    response.data = "Unknown command.";
                    break;
                }
            }

            // 发送响应
            respnsetime++;
            clientSocketPtr->sendPacket(response);
            std::cout<<"respnsetime=="<<respnsetime<<std::endl;
            LOG(INFO) << "Sent response to " << clientIp << ":" << clientPort 
                      << " (Client ID: " << clientId << ")";
        }
    } catch (const std::exception& e) {
        LOG(INFO) << "Client " << clientIp << ":" << clientPort 
                  << " (ID: " << clientId << ") disconnected. Reason: " << e.what();
    }

    // 移除客户端
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        connectedClients.erase(clientId);
    }
    LOG(INFO) << "Closed client connection for " << clientIp << ":" << clientPort 
              << " (Client ID: " << clientId << ")";
}

int main(int argc, char* argv[]) {
    // 初始化glog
    GlogWrapper glog(argv[0]);
    LOG(INFO) << "Server starting on port " << SERVER_PORT << "...";

    // 注册信号处理函数
    std::signal(SIGINT, exitHandler);  // Ctrl + C
    std::signal(SIGQUIT, exitHandler); // Ctrl + '\'
    std::signal(SIGHUP, exitHandler);  // 用户注销

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
    LOG(INFO) << "Server listening on port " << SERVER_PORT 
              << " with max queue: " << MAX_CLIENT_QUEUE;

    // 为处理客户端请求创建线程容器
    std::vector<std::thread> threads;

    while (serverRunning) {
        struct sockaddr_in clientAddress;
        socklen_t addressLength = sizeof(clientAddress);

        // 使用select设置非阻塞模式，以便能够定期检查serverRunning
        fd_set set;
        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(serverSocket, &set);
        timeout.tv_sec = 1; // 1秒超时
        timeout.tv_usec = 0;
        int rv = select(serverSocket + 1, &set, NULL, NULL, &timeout);
        if (rv == -1) {
            if (serverRunning) {
                LOG(ERROR) << "Select error.";
            }
            break;
        } else if (rv == 0) {
            // 超时，继续检查serverRunning
            continue;
        } else {
            if (FD_ISSET(serverSocket, &set)) {
                int clientFd = accept(serverSocket, (struct sockaddr*)&clientAddress, &addressLength);

                if (clientFd < 0) {
                    if (serverRunning) {
                        LOG(ERROR) << "A client failed to connect to server port " << SERVER_PORT << ".";
                    }
                    continue;
                } else {
                    std::string clientIp = inet_ntoa(clientAddress.sin_addr);
                    int clientPort = ntohs(clientAddress.sin_port);
                    LOG(INFO) << "A client has connected from " << clientIp << ":" << clientPort 
                              << " (Server Port: " << SERVER_PORT << ").";

                    // 封装客户端Socket为 shared_ptr
                    std::shared_ptr<MySocket> clientSocketPtr = std::make_shared<MySocket>(clientFd);

                    // 分配客户端ID并存储
                    int clientId;
                    {
                        std::lock_guard<std::mutex> lock(clientsMutex);
                        clientId = clientIdCounter++;
                        connectedClients.emplace(clientId, std::make_pair(clientSocketPtr, clientIp + ":" + std::to_string(clientPort)));
                    }

                    // 为该客户端连接创建新线程进行处理
                    threads.emplace_back(handleClient, clientId, clientSocketPtr, clientIp, clientPort);
                }
            }
        }
    }

    // 关闭服务器socket
    close(serverSocket);
    LOG(INFO) << "Server socket closed. Waiting for client threads to finish...";

    // 向所有连接的客户端发送DISCONNECT消息并关闭它们的socket
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& [id, clientPair] : connectedClients) {
            try {
                Packet pkt;
                pkt.type = DISCONNECT;
                pkt.data = "Server shutting down.";
                clientPair.first->sendPacket(pkt);
                // 关闭socket
                close(clientPair.first->getFd());
                LOG(INFO) << "Sent DISCONNECT to Client " << id;
            } catch (const std::exception& e) {
                LOG(ERROR) << "Error disconnecting client " << id << ": " << e.what();
            }
        }
        connectedClients.clear();
    }

    // 等待所有线程结束
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    LOG(INFO) << "Server shut down gracefully.";
    return 0;
}
