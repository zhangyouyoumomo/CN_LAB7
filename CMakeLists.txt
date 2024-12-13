# 设置最低CMake版本
cmake_minimum_required(VERSION 3.10)

# 定义项目名称
project(CN_LAB7)

# 设置可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找 Glog 库
find_package(glog REQUIRED)

# 添加可执行目标
add_executable(Server Server/Server.cpp)
add_executable(Client Client/Client.cpp)

# 链接 Glog 库
target_link_libraries(Server PRIVATE glog::glog)
target_link_libraries(Client PRIVATE glog::glog)

# 定义 make clean_all 的自定义命令
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all build files"
)

# 定义 make run 的自定义命令
add_custom_target(run
    COMMAND ./run.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running CN_LAB7/run.sh script"
)
