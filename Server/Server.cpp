// Server.cpp
#include <iostream>
#include <string>
#include <cstring>        // For memset
#include <unistd.h>       // For close
#include <sys/socket.h>   // For socket functions
#include <netinet/in.h>   // For sockaddr_in
#include <arpa/inet.h>    // For inet_ntoa
#include <thread>         // For std::thread
#include <vector>         // For std::vector
#include <glog/logging.h> // For glog

#define SERVER_PORT 5869
#define MAX_CLIENT_QUEUE 20

class GlogWrapper {
public:
    GlogWrapper(char* program) {
        google::InitGoogleLogging(program);
        // 根据实际情况修改日志存储路径，如 /tmp/logs ，需要保证可写
        FLAGS_log_dir = "./logs";
        FLAGS_alsologtostderr = true;
        FLAGS_colorlogtostderr = true;
        FLAGS_stop_logging_if_full_disk = true;
        google::InstallFailureSignalHandler();
    }
    ~GlogWrapper() {
        google::ShutdownGoogleLogging();
    }
};

void handleClient(int clientSocket, std::string clientIp, int clientPort) {
    // 在该线程中处理客户端请求
    LOG(INFO) << "Client thread started for " << clientIp << ":" << clientPort 
              << " (Server Port: " << SERVER_PORT << ")";
    const char* greetingMessage = "Hello from server!";
    int messageCount = 0; // 计数器，记录发送的消息次数
    
    // 向客户端发送问候消息
    for(int time = 1; time <=3; time++){
        ssize_t sentBytes = send(clientSocket, greetingMessage, strlen(greetingMessage), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (sentBytes < 0) {
            LOG(ERROR) << "Failed to send greeting message to " << clientIp << ":" << clientPort 
                       << " (Server Port: " << SERVER_PORT << ")";
        } else {
            messageCount++;
            LOG(INFO) << "Sent greeting message #" << messageCount 
                      << " to " << clientIp << ":" << clientPort 
                      << " (Server Port: " << SERVER_PORT << ")";
        }
    }

    // 根据需要，在这里添加更多的与客户端通信的逻辑
    // ...
    close(clientSocket);
    LOG(INFO) << "Closed client connection for " << clientIp << ":" << clientPort 
              << " (Server Port: " << SERVER_PORT << ")";
}

int main(int argc, char* argv[]) {
    // 初始化glog
    auto glog = GlogWrapper(argv[0]);
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
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &addressLength);

        if (clientSocket < 0) {
            LOG(ERROR) << "A client failed to connect to server port " << SERVER_PORT << ".";
            continue;
        } else {
            std::string clientIp = inet_ntoa(clientAddress.sin_addr);
            int clientPort = ntohs(clientAddress.sin_port);
            LOG(INFO) << "A client has connected from " << clientIp << ":" << clientPort 
                      << " (Server Port: " << SERVER_PORT << ").";

            // 为该客户端连接创建新线程进行处理
            threads.emplace_back(handleClient, clientSocket, clientIp, clientPort);
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
