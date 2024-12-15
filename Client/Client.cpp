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
#include <csignal> // exit signal handling
#include "Message/MySocket.h" // 引入封装的Socket类
#include <limits>

// 定义服务器地址和端口
#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 5869

// 全局变量
std::mutex mtx;
std::condition_variable cv;
std::queue<Packet> msgQueue;
std::atomic<bool> running(true);

// 生产者线程：接收服务器数据
void producer(MySocket& clientSocketObj, int localPort) {
    try {
        while (running) {
            Packet received = clientSocketObj.recvPacket();
            Packet pkt;
            pkt.type = received.type;
            pkt.data = received.data;

            // 将数据包放入队列
            {
                std::lock_guard<std::mutex> lock(mtx);
                msgQueue.push(pkt);
            }
            cv.notify_one(); // 通知消费者
        }
    } catch (const std::exception& e) {
        if (running) { // 仅在未请求退出时记录错误
            LOG(INFO) << "Producer thread exiting. Reason: " << e.what();
            running = false;
            cv.notify_all();
        }
    }
}

// 消费者线程：处理并显示数据
void consumer(int localPort) {
    int messageCount = 0; // 计数器，记录接收的消息次数
    while (running) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, []{ return !msgQueue.empty() || !running; });

        while (!msgQueue.empty()) {
            Packet pkt = msgQueue.front();
            msgQueue.pop();
            lock.unlock();

            // 处理数据包，例如显示给用户
            if (pkt.type == SEND_MESSAGE) {
                messageCount++;
                std::cout << "\n====================\n";
                std::cout << "[来自其他客户端的消息 #" << messageCount << "]: " << pkt.data << std::endl;
                std::cout << "====================\n";
                LOG(INFO) << "Received message #" << messageCount 
                          << " from server (Client Local Port: " << localPort << ").";
            } else if (pkt.type == CLIENT_LIST) {
                std::cout << "\n====================\n";
                std::cout << "[在线客户端列表]:\n" << pkt.data << std::endl;
                std::cout << "====================\n";
                LOG(INFO) << "Received client list from server (Client Local Port: " 
                          << localPort << ").";
            } else if (pkt.type == RESPONSE) {
                std::cout << "\n====================\n";
                std::cout << "[服务器响应]: " << pkt.data << std::endl;
                std::cout << "====================\n";
                LOG(INFO) << "Received response from server: " << pkt.data 
                          << " (Client Local Port: " << localPort << ").";
            } else {
                std::cout << "\n====================\n";
                std::cout << "[未知消息类型]: " << pkt.data << std::endl;
                std::cout << "====================\n";
                LOG(INFO) << "Received unknown packet type: " << pkt.type 
                          << " (Client Local Port: " << localPort << ").";
            }

            lock.lock();
        }
    }
}

// 功能菜单
void printMenu() {
    std::cout << "\n====================\n";
    std::cout << "请选择操作：" << std::endl;
    std::cout << "1. 获取服务器时间" << std::endl;
    std::cout << "2. 获取服务器名称" << std::endl;
    std::cout << "3. 发送消息到其他客户端" << std::endl;
    std::cout << "4. 获取在线客户端列表" << std::endl;
    std::cout << "5. 断开连接" << std::endl;
    std::cout << "6. 退出" << std::endl;
    std::cout << "====================\n";
    std::cout << "请输入选项 (1-6): ";
}

// 输入处理函数（运行在单独的线程）
void handleUserInput(MySocket& clientSocketObj, int localPort, std::atomic<bool>& runningFlag) {
    while (runningFlag) {
        printMenu();
        int choice;
        if (!(std::cin >> choice)) {
            // 输入失败，可能是EOF或其他原因
            if (!runningFlag) {
                break;
            }
            std::cin.clear(); // 清除错误标志
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 忽略无效输入
            std::cout << "输入无效，请重新输入。" << std::endl;
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 忽略剩余的换行符

        Packet pkt;
        bool shouldBreak = false;
        switch (choice) {
            case 1: { // 获取服务器时间
                pkt.type = GET_TIME;
                pkt.data = "";
                break;
            }
            case 2: { // 获取服务器名称
                pkt.type = GET_NAME;
                pkt.data = "";
                break;
            }
            case 3: { // 发送消息到其他客户端
                std::string targetIdStr, message;
                std::cout << "请输入目标客户端ID: ";
                if (!(std::cin >> targetIdStr)) {
                    std::cin.clear();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    std::cout << "输入无效，请重新发送消息。" << std::endl;
                    continue;
                }
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // 忽略剩余的换行符
                std::cout << "请输入要发送的消息: ";
                std::getline(std::cin, message);
                pkt.type = SEND_MESSAGE;
                pkt.data = targetIdStr + ":" + message; // 格式化为 "targetId:message"
                break;
            }
            case 4: { // 获取在线客户端列表
                pkt.type = LIST_CLIENTS;
                pkt.data = "";
                break;
            }
            case 5: { // 断开连接
                pkt.type = DISCONNECT;
                pkt.data = "";
                try {
                    clientSocketObj.sendPacket(pkt);
                    LOG(INFO) << "Sent DISCONNECT request.";
                } catch (const std::exception& e) {
                    LOG(ERROR) << "发送断开请求失败: " << e.what();
                }
                runningFlag = false;
                cv.notify_all();
                shouldBreak = true;
                break;
            }
            case 6: { // 退出
                if (runningFlag ) {
                    // 先发送断开请求
                    pkt.type = DISCONNECT;
                    pkt.data = "";
                    try {
                        clientSocketObj.sendPacket(pkt);
                        LOG(INFO) << "Sent DISCONNECT request.";
                    } catch (const std::exception& e) {
                        LOG(ERROR) << "发送断开请求失败: " << e.what();
                    }
                }
                runningFlag = false;
                cv.notify_all();
                shouldBreak = true;
                break;
            }
            default: {
                std::cout << "无效的选项，请重新选择。" << std::endl;
                continue;
            }
        }

        // 发送请求到服务器
        if (choice >=1 && choice <=5) {
            try {
                clientSocketObj.sendPacket(pkt);
                LOG(INFO) << "已发送请求类型 " << pkt.type;
            } catch (const std::exception& e) {
                LOG(ERROR) << "发送数据失败: " << e.what();
                runningFlag = false;
                cv.notify_all();
                break;
            }
        }

        if (shouldBreak) {
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    // 初始化 glog
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "../logs"; // 设置log文件保存路径
    FLAGS_alsologtostderr = true; // 同时输出到标准错误
    FLAGS_colorlogtostderr = true;
    FLAGS_stop_logging_if_full_disk = true;
    google::InstallFailureSignalHandler();

    MySocket clientSocketObj;

    // 用户输入连接请求
    std::cout << "=== 客户端启动 ===" << std::endl;
    std::cout << "是否连接到服务器 " << SERVER_ADDRESS << ":" << SERVER_PORT << "? (y/n): ";
    char connectChoice;
    std::cin >> connectChoice;
    std::cin.ignore(); // 忽略剩余的换行符

    if (connectChoice == 'y' || connectChoice == 'Y') {
        std::cout << "正在连接到服务器 " << SERVER_ADDRESS << ":" << SERVER_PORT << " ..." << std::endl;
        try {
            // 连接到服务器
            clientSocketObj.connectTo(SERVER_ADDRESS, SERVER_PORT);
        } catch (const std::exception& e) {
            LOG(FATAL) << "[Error] " << e.what();
        }

        // 获取客户端本地端口号
        struct sockaddr_in localAddress;
        socklen_t localAddressLength = sizeof(localAddress);
        if (getsockname(clientSocketObj.getFd(), (struct sockaddr*)&localAddress, &localAddressLength) == -1) {
            LOG(ERROR) << "获取本地地址失败。";
        }
        int localPort = ntohs(localAddress.sin_port);

        LOG(INFO) << "[Info] 已连接到服务器 " << SERVER_ADDRESS << ":" << SERVER_PORT 
                  << " (Client Local Port: " << localPort << ").";

        // 启动生产者和消费者线程
        std::thread prod(producer, std::ref(clientSocketObj), localPort);
        std::thread cons(consumer, localPort);

        // 启动用户输入线程
        std::thread inputThread(handleUserInput, std::ref(clientSocketObj), localPort, std::ref(running));

        // 主线程等待 running 变为 false
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 等待输入线程结束
        if (inputThread.joinable()) {
            inputThread.join();
        }

        // 等待生产者和消费者线程结束
        if (prod.joinable()) {
            prod.join();
        }
        if (cons.joinable()) {
            cons.join();
        }

        // 关闭socket
        if (clientSocketObj.getFd() != -1) {
            close(clientSocketObj.getFd());
        }
        LOG(INFO) << "客户端已关闭。 (Client Local Port: " << localPort << ")";
    } else {
        std::cout << "未连接到服务器。" << std::endl;
        LOG(INFO) << "Client chose not to connect to the server.";
    }

    google::ShutdownGoogleLogging();
    return 0;
}
