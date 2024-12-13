cmake_minimum_required(VERSION 3.10)
project(CN_LAB7)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(Glog REQUIRED)

include_directories(${PROJECT_SOURCE_DIR})

add_executable(server Server/Server.cpp)
target_link_libraries(server glog::glog pthread)

add_executable(client Client/Client.cpp)
target_link_libraries(client glog::glog pthread)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build)
