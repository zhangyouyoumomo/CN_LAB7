// Server.cpp

#include <iostream>
#include <string>
#include <cstring>          // 用于 memset
#include <unistd.h>         // 用于 close
#include <sys/socket.h>     // 用于 socket 函数
#include <netinet/in.h>     // 用于 sockaddr_in
#include <arpa/inet.h>      // 用于 inet_ntop
#include <vector>
#include <thread>           // 用于多线程

// 引入 glog 头文件
#include <glog/logging.h>

// 定义服务器端口和最大客户端队列
#define SERVER_PORT 5869
#define MAX_CLIENT_QUEUE 20

// 封装 Glog 的类
class GlogWrapper {
public:
    GlogWrapper(const char* program) {
        google::InitGoogleLogging(program);
        FLAGS_log_dir = "/home/zimo/CN_LAB7/LogFile";           // 设置 log 文件保存路径
        FLAGS_alsologtostderr = true;                           // 设置日志消息同时输出到标准错误
        FLAGS_colorlogtostderr = true;                           // 设置记录到标准输出的颜色消息（如果终端支持）
        FLAGS_stop_logging_if_full_disk = true;                 // 磁盘已满时停止记录日志
        FLAGS_log_prefix = true;                                 // 确保日志前缀（文件名和行号）被包含
        FLAGS_minloglevel = google::INFO;                       // 设置最低日志级别为 INFO
        // FLAGS_stderrthreshold = google::WARNING;              // 指定仅输出特定级别或以上的日志
        google::InstallFailureSignalHandler();                  // 安装失败信号处理器
    }
    ~GlogWrapper() {
        google::ShutdownGoogleLogging();
    }
};

// 处理客户端请求的函数
void handleClient(int clientSocket, sockaddr_in clientAddress) {
    // 将客户端 IP 地址转换为字符串
    char ipStr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &clientAddress.sin_addr, ipStr, sizeof(ipStr)) == NULL) {
        LOG(ERROR) << "IP 地址转换失败！";
        close(clientSocket);
        return;
    }

    // 获取客户端端口号并转换为主机字节序
    uint16_t port = ntohs(clientAddress.sin_port);

    // 输出客户端连接信息
    LOG(INFO) << "处理来自 " << ipStr << ":" << port << " 的客户端连接。";

    const char* greetingMessage = "Hello from server!";

    // 向客户端发送问候消息
    ssize_t bytesSent = send(clientSocket, greetingMessage, strlen(greetingMessage), 0);
    if (bytesSent < 0) {
        LOG(ERROR) << "发送消息失败。";
        close(clientSocket);
        return;
    } else {
        LOG(INFO) << "已向客户端发送问候消息。";
    }

    // 持续处理客户端消息
    char buffer[1024];
    while (true) {
        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            LOG(INFO) << "收到来自 " << ipStr << ":" << port << " 的消息: " << buffer;

            // 这里可以根据需要进行回复或其他处理
            std::string response = "服务器已收到您的消息: " + std::string(buffer);
            ssize_t sent = send(clientSocket, response.c_str(), response.size(), 0);
            if (sent < 0) {
                LOG(ERROR) << "发送响应消息失败。";
                break;
            }
        } else if (bytesReceived == 0) {
            LOG(INFO) << "客户端 " << ipStr << ":" << port << " 关闭了连接。";
            break;
        } else {
            LOG(ERROR) << "接收数据失败。";
            break;
        }
    }

    // 关闭客户端套接字
    close(clientSocket);
    LOG(INFO) << "已关闭与客户端 " << ipStr << ":" << port << " 的连接。";
}

int main(int argc, char* argv[]) {
    // 初始化 glog
    GlogWrapper glogWrapper(argc > 0 ? argv[0] : "server");

    int serverSocket, clientSocket;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);

    // 创建 socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        LOG(ERROR) << "创建 socket 失败。";
        return -1;
    }

    // 设置服务器地址信息
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;                // 使用 IPv4
    serverAddress.sin_addr.s_addr = INADDR_ANY;        // 绑定到所有可用接口
    serverAddress.sin_port = htons(SERVER_PORT);       // 设置端口，转换为网络字节序

    // 绑定 socket 到本地地址
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        LOG(ERROR) << "绑定 socket 失败。";
        close(serverSocket);
        return -1;
    }

    // 开始监听客户端的连接请求
    if (listen(serverSocket, MAX_CLIENT_QUEUE) < 0) {
        LOG(ERROR) << "监听失败。";
        close(serverSocket);
        return -1;
    }
    LOG(INFO) << "服务器正在监听端口 " << SERVER_PORT << " ...";

    while (true) {
        // 接受新的连接请求
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (clientSocket < 0) {
            LOG(ERROR) << "接受客户端连接失败。";
            continue; // 继续接受下一个连接
        }

        // 创建新线程处理客户端请求，并分离线程
        std::thread(handleClient, clientSocket, clientAddress).detach();
        LOG(INFO) << "已创建新线程处理客户端连接。";
    }

    // 关闭服务器套接字（实际上这里永远不会执行到）
    close(serverSocket);
    return 0;
}
