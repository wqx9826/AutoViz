cmake_minimum_required(VERSION 3.16)
project(autoviz_server VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(AutoVizProto CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(custom_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)

add_executable(autoviz_server_node
    src/main.cpp
    src/AutoVizServerNode.cpp
    src/TcpServer.cpp
)
target_include_directories(autoviz_server_node PRIVATE include)
ament_target_dependencies(autoviz_server_node
    custom_msgs
    geometry_msgs
    nav_msgs
    rclcpp
)
target_link_libraries(autoviz_server_node
    AutoVizProto::AutoVizProto
    Boost::system
)

install(TARGETS autoviz_server_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_package()
