#!/bin/bash

# 项目根目录
PROJECT_DIR=$(pwd)

# 编译 Server 和 Client
echo "编译 Server 和 Client..."
g++ -std=c++11 -pthread Server/Server.cpp -lglog -o Server/server
g++ -std=c++11 -pthread Client/Client.cpp -lglog -o Client/Client

# 启动服务器
echo "启动服务器..."
./Server/server &   # 服务器在后台启动

# 启动多个客户端，模拟多个客户端连接
CLIENT_COUNT=5
for ((i=1; i<=CLIENT_COUNT; i++))
do
    echo "启动客户端 $i..."
    ./Client/Client &   # 每个客户端在后台启动
    sleep 1      # 稍作延迟，确保客户端能连接到服务器
done

# 等待所有客户端完成
wait

echo "测试完成。"
