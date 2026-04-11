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

find_package(Qt5 REQUIRED COMPONENTS Widgets)

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

if(AUTOVIZ_ENABLE_ROS1)
    # ROS1 接入说明：
    # 1. 在已 source ROS1 环境后打开 AUTOVIZ_ENABLE_ROS1。
    # 2. 后续可在这里补 find_package(catkin QUIET COMPONENTS roscpp std_msgs ...)
    # 3. 若找到了 catkin，再追加 include_directories / target_link_libraries。
    add_compile_definitions(AUTOVIZ_ENABLE_ROS1)
endif()

if(AUTOVIZ_ENABLE_ROS2)
    # ROS2 接入说明：
    # 1. 在已 source ROS2 环境后打开 AUTOVIZ_ENABLE_ROS2。
    # 2. 后续可在这里补 find_package(rclcpp QUIET) 等依赖。
    # 3. 若找到了 rclcpp，再追加 include 目录与链接库。
    add_compile_definitions(AUTOVIZ_ENABLE_ROS2)
endif()

add_executable(${PROJECT_NAME} ${AUTOVIZ_SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt5::Widgets
)

# 默认保持 Mock / 占位模式可编译。
# 即使没有完整 ROS1/ROS2 依赖，也应能构建 UI、内部模型、DataManager 与渲染链路。
