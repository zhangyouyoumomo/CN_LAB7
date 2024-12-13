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
#include "Message/MySocket.h" // 引入封装的Socket类

// 定义服务端地址和端口
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
            pkt.data = received.data;

            // 将数据包放入队列
            {
                std::lock_guard<std::mutex> lock(mtx);
                msgQueue.push(pkt);
            }
            cv.notify_one(); // 通知消费者
        }
    } catch (const std::exception& e) {
        LOG(INFO) << "Producer thread exiting. Reason: " << e.what();
        running = false;
        cv.notify_all();
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
            messageCount++;
            std::cout << "收到消息 #" << messageCount << ": " << pkt.data << std::endl;
            LOG(INFO) << "Received message #" << messageCount << " from server (Client Local Port: " 
                      << localPort << ").";

            lock.lock();
        }
    }
}

// 功能菜单
void printMenu() {
    std::cout << "\n请选择操作：" << std::endl;
    std::cout << "1. 获取服务器时间" << std::endl;
    std::cout << "2. 获取服务器名称" << std::endl;
    std::cout << "3. 发送消息到服务器" << std::endl;
    std::cout << "4. 断开连接" << std::endl;
    std::cout << "5. 退出" << std::endl;
    std::cout << "请输入选项 (1-5): ";
}

int main(int argc, char* argv[]) {
    // 初始化 glog
    google::InitGoogleLogging(argv[0]);
    FLAGS_log_dir = "logs"; // 设置log文件保存路径
    FLAGS_alsologtostderr = true; // 同时输出到标准错误
    FLAGS_colorlogtostderr = true;
    FLAGS_stop_logging_if_full_disk = true;
    google::InstallFailureSignalHandler();

    MySocket clientSocketObj;

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

    // 主线程处理用户输入
    while (running) {
        printMenu();
        int choice;
        std::cin >> choice;
        std::cin.ignore(); // 忽略剩余的换行符

        Packet pkt;
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
            case 3: { // 发送消息到服务器
                std::string message;
                std::cout << "请输入要发送的消息: ";
                std::getline(std::cin, message);
                pkt.type = SEND_MESSAGE;
                pkt.data = message;
                break;
            }
            case 4: { // 断开连接
                pkt.type = DISCONNECT;
                pkt.data = "";
                try {
                    clientSocketObj.sendPacket(pkt);
                } catch (const std::exception& e) {
                    LOG(ERROR) << "发送断开请求失败: " << e.what();
                }
                running = false;
                cv.notify_all();
                break;
            }
            case 5: { // 退出
                if (running) {
                    // 先发送断开请求
                    pkt.type = DISCONNECT;
                    pkt.data = "";
                    try {
                        clientSocketObj.sendPacket(pkt);
                    } catch (const std::exception& e) {
                        LOG(ERROR) << "发送断开请求失败: " << e.what();
                    }
                }
                running = false;
                cv.notify_all();
                break;
            }
            default: {
                std::cout << "无效的选项，请重新选择。" << std::endl;
                continue;
            }
        }

        // 发送请求到服务器
        if (choice >=1 && choice <=4) {
            try {
                clientSocketObj.sendPacket(pkt);
                LOG(INFO) << "已发送请求类型 " << pkt.type;
            } catch (const std::exception& e) {
                LOG(ERROR) << "发送数据失败: " << e.what();
                running = false;
                cv.notify_all();
                break;
            }
        }

        if (choice == 4 || choice == 5) {
            break;
        }
    }

    // 等待生产者和消费者线程结束
    if (prod.joinable()) {
        prod.join();
    }
    if (cons.joinable()) {
        cons.join();
    }

    // 关闭socket
    close(clientSocketObj.getFd());
    LOG(INFO) << "客户端已关闭。 (Client Local Port: " << localPort << ")";

    google::ShutdownGoogleLogging();
    return 0;
}
