// 引入一些必要的头文件
#include <iostream>
#include <string>
#include <cstring> // For memset
#include <unistd.h> // For close
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in

#define SERVER_PORT 5869

int main() {
    int serverSocket, clientSocket;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    const char* greetingMessage = "Hello from server!";

    // 创建socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "[Error] Failed to create socket" << std::endl;
        return -1;
    }

    // 设置服务器地址信息
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(SERVER_PORT);

    // 绑定socket到本地地址
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cerr << "[Error] Binding failed" << std::endl;
        return -1;
    }

    // 开始监听客户端的连接请求
    if (listen(serverSocket, 1) < 0) { // 最大连接数为1
        std::cerr << "[Error] Listening failed" << std::endl;
        return -1;
    }
    std::cout << "[Info] Server is listening on port " << SERVER_PORT << "..." << std::endl;

    // 接受客户端连接
    clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength);
    if (clientSocket < 0) {
        std::cerr << "[Error] Accepting connection failed" << std::endl;
        return -1;
    }

    std::cout << "[Info] Connected to client!" << std::endl;
    // 向客户端发送问好消息
    send(clientSocket, greetingMessage, strlen(greetingMessage), 0);

    // 关闭sockets
    close(clientSocket);
    close(serverSocket);

    return 0;
}