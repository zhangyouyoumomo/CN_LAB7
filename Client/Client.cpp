#include <iostream>
#include <string>
#include <cstring> // For memset
#include <unistd.h> // For close
#include <sys/socket.h> // For socket functions
#include <arpa/inet.h> // For inet_addr
#include <glog/logging.h> // For glog

// 定义服务端地址和端口
#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 5869

class GlogWrapper{ /* 封装Glog */ 
public:
    GlogWrapper(char* program) {
        google::InitGoogleLogging(program);
        FLAGS_log_dir="/home/zimo/CN_LAB7/LogFile"; //设置log文件保存路径及前缀
        FLAGS_alsologtostderr = true; //设置日志消息除了日志文件之外是否去标准输出
        FLAGS_colorlogtostderr = true; //设置记录到标准输出的颜色消息（如果终端支持）
        FLAGS_stop_logging_if_full_disk = true;   //设置是否在磁盘已满时避免日志记录到磁盘
        // FLAGS_stderrthreshold=google::WARNING; //指定仅输出特定级别或以上的日志
        google::InstallFailureSignalHandler();
    }
    ~GlogWrapper() { google::ShutdownGoogleLogging(); }
};

int main(int argc, char* argv[]) {
    // 初始化 glog
    auto glog = GlogWrapper(argv[0]);
    int clientSocket;
    struct sockaddr_in serverAddress;
    char buffer[1024] = {0};

    // 创建socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    LOG_IF(FATAL, clientSocket < 0) << "[Error] Failed to create socket";

    // 设置服务器地址信息
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);
    serverAddress.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);

    // 连接到服务器
    LOG_IF(FATAL, connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
        << "[Error] Connection failed";

    LOG(INFO) << "[Info] Connected to server at " << SERVER_ADDRESS << ":" << SERVER_PORT;

    // 接收消息
    ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        LOG(INFO) << "[Info] Message from server: " << buffer;
    } else {
        LOG(ERROR) << "[Error] Failed to receive data";
    }

    // 关闭socket
    close(clientSocket);

    return 0;
}