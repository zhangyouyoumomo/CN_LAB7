#!/bin/bash

# 项目根目录
PROJECT_DIR=$(pwd)

# 启动服务器
echo "启动服务器..."
${PROJECT_DIR}/build/bin/Server &   # 指定生成的可执行文件路径
sleep 2      

# 启动多个客户端，模拟多个客户端连接
CLIENT_COUNT=3
for ((i=1; i<=CLIENT_COUNT; i++))
do
    echo "启动客户端 $i..."
    ${PROJECT_DIR}/build/bin/Client &   # 指定生成的可执行文件路径
    # sleep 2      # 稍作延迟，确保客户端能连接到服务器
done

# 等待所有客户端完成
wait

echo "测试完成。"
