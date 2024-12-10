// Client.cpp

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <glog/logging.h>

// 定义服务端地址和端口
#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 5869

// 消息包结构体
struct Packet {
    std::string data;
};

// 全局变量
std::mutex mtx;
std::condition_variable cv;
std::queue<Packet> msgQueue;
std::atomic<bool> running(true);

// 生产者线程：接收服务器数据
void producer(int clientSocket) {
    char buffer[1024];
    while (running) {
        ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            Packet pkt;
            pkt.data = std::string(buffer);
            
            // 将数据包放入队列
            {
                std::lock_guard<std::mutex> lock(mtx);
                msgQueue.push(pkt);
            }
            cv.notify_one(); // 通知消费者
        } else if (bytesReceived == 0) {
            LOG(INFO) << "服务器关闭连接。";
            running = false;
            cv.notify_all();
            break;
        } else {
            LOG(ERROR) << "接收数据失败。";
            running = false;
            cv.notify_all();
            break;
        }
    }
}

// 消费者线程：处理并显示数据
void consumer() {
    while (running) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, []{ return !msgQueue.empty() || !running; });
        
        while (!msgQueue.empty()) {
            Packet pkt = msgQueue.front();
            msgQueue.pop();
            lock.unlock();
            
            // 处理数据包，例如显示给用户
            std::cout << "收到消息: " << pkt.data << std::endl;
            
            lock.lock();
        }
    }
}

int main(int argc, char* argv[]) {
    // 初始化 glog
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "/home/zimo/CN_LAB7/LogFile"; // 设置log文件保存路径
    FLAGS_alsologtostderr = true; // 同时输出到标准错误
    FLAGS_colorlogtostderr = true;
    FLAGS_stop_logging_if_full_disk = true;
    google::InstallFailureSignalHandler();

    int clientSocket;
    struct sockaddr_in serverAddress;

    // 创建socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    LOG_IF(FATAL, clientSocket < 0) << "[Error] 创建socket失败";

    // 设置服务器地址信息
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &serverAddress.sin_addr) <= 0) {
        LOG(FATAL) << "[Error] 无效的服务器地址。";
    }

    // 连接到服务器
    LOG_IF(FATAL, connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        << "[Error] 连接服务器失败";

    LOG(INFO) << "[Info] 已连接到服务器 " << SERVER_ADDRESS << ":" << SERVER_PORT;

    // 启动生产者和消费者线程
    std::thread prod(producer, clientSocket);
    std::thread cons(consumer);

    // 主线程处理用户输入
    std::string input;
    while (running && std::getline(std::cin, input)) {
        if (input == "exit") {
            running = false;
            break;
        }
        // 发送用户输入到服务器
        ssize_t bytesSent = send(clientSocket, input.c_str(), input.size(), 0);
        if (bytesSent < 0) {
            LOG(ERROR) << "[Error] 发送数据失败";
        } else {
            LOG(INFO) << "已发送消息: " << input;
        }
    }

    // 关闭运行标志并通知线程
    running = false;
    cv.notify_all();

    // 等待生产者和消费者线程结束
    if (prod.joinable()) {
        prod.join();
    }
    if (cons.joinable()) {
        cons.join();
    }

    // 关闭socket
    close(clientSocket);
    LOG(INFO) << "客户端已关闭。";

    google::ShutdownGoogleLogging();
    return 0;
}
