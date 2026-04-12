cmake_minimum_required(VERSION 3.16)

project(AutoViz VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


option(AUTOVIZ_ENABLE_ROS1 "Enable ROS1 adapter compilation hooks." OFF)
option(AUTOVIZ_ENABLE_ROS2 "Enable ROS2 adapter compilation hooks." OFF)

if(AUTOVIZ_ENABLE_ROS1 AND AUTOVIZ_ENABLE_ROS2)
    message(FATAL_ERROR "AUTOVIZ_ENABLE_ROS1 与 AUTOVIZ_ENABLE_ROS2 不能同时打开。ROS1 与 ROS2 订阅后端当前按互斥模式设计。")
endif()

find_package(Qt5 REQUIRED COMPONENTS Widgets)

if(AUTOVIZ_ENABLE_ROS1)
    # ROS1 接入说明：
    # 1. 在已 source ROS1 环境后打开 AUTOVIZ_ENABLE_ROS1。
    # 2. 后续可在这里补 find_package(catkin QUIET COMPONENTS roscpp std_msgs ...)
    # 3. 若找到了 catkin，再追加 include_directories / target_link_libraries。
endif()


if(AUTOVIZ_ENABLE_ROS2)

    find_package(rclcpp REQUIRED)
    find_package(nav_msgs REQUIRED)
    find_package(geometry_msgs REQUIRED)
    find_package(std_msgs REQUIRED)

    # 1. 包含头文件路径
    include_directories(
        ${rclcpp_INCLUDE_DIRS}
        ${nav_msgs_INCLUDE_DIRS}
        ${geometry_msgs_INCLUDE_DIRS}
        ${std_msgs_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/message/ros2/custom_msgs/include/custom_msgs/ 
    )

    # 2. 链接库路径
    link_directories(
        ${CMAKE_SOURCE_DIR}/message/ros2/custom_msgs/lib        
    )
endif()

set(AUTOVIZ_SOURCES
    src/main.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/core/model/CommonTypes.h
    src/core/model/VehicleState.h
    src/core/model/VehicleState.cpp
    src/core/model/PathTypes.h
    src/core/model/PathTypes.cpp
    src/core/model/ObstacleTypes.h
    src/core/model/ObstacleTypes.cpp
    src/core/model/ControlTypes.h
    src/core/model/ControlTypes.cpp
    src/core/datacenter/DataManager.h
    src/core/datacenter/DataManager.cpp
    src/core/config/VehicleConfig.h
    src/core/config/VehicleConfig.cpp
    src/core/render/SceneManager.h
    src/core/render/SceneManager.cpp
    src/core/ros/RosMsgSubscribeBase.h
    src/core/ros/RosMsgSubscribeBase.cpp
    src/core/ros/Ros1MsgSubsrcribe.h
    src/core/ros/Ros1MsgSubsrcribe.cpp
    src/core/ros/Ros2MsgSubsrcribe.h
    src/core/ros/Ros2MsgSubsrcribe.cpp
    src/core/ros/RosMsgSubsrcribeFactory.h
    src/core/ros/RosMsgSubsrcribeFactory.cpp
    src/ui/VisualizationView.h
    src/ui/VisualizationView.cpp
    src/ui/DisplayControlPanel.h
    src/ui/DisplayControlPanel.cpp
    src/ui/ControlStatusPanel.h
    src/ui/ControlStatusPanel.cpp
    src/ui/LogPanel.h
    src/ui/LogPanel.cpp
    src/ui/charts/ChartPanel.h
    src/ui/charts/ChartPanel.cpp
    src/utils/Logger.h
    src/utils/Logger.cpp
)



add_executable(${PROJECT_NAME} ${AUTOVIZ_SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 始终把 ROS 编译开关定义成 0/1，避免“宏不存在”导致的分支静态检查缺失。
# 代码里统一使用 #if AUTOVIZ_ENABLE_ROS1 / #if AUTOVIZ_ENABLE_ROS2。
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        AUTOVIZ_ENABLE_ROS1=$<BOOL:${AUTOVIZ_ENABLE_ROS1}>
        AUTOVIZ_ENABLE_ROS2=$<BOOL:${AUTOVIZ_ENABLE_ROS2}>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt5::Widgets
)

if( AUTOVIZ_ENABLE_ROS2)
    # ros2 依赖 
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        ${rclcpp_LIBRARIES}
        ${std_msgs_LIBRARIES}
        ${nav_msgs_LIBRARIES}
        ${geometry_msgs_LIBRARIES}
    )
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
        custom_msgs__rosidl_typesupport_cpp
    )
endif()
