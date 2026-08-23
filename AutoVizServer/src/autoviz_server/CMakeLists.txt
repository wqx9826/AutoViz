cmake_minimum_required(VERSION 3.16)
project(autoviz_server VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 项目根目录
set(AUTOVIZ_SERVER_WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# 默认只搜索 Server 自己携带的第三方依赖；非标准位置可显式覆盖。
set(AUTOVIZ_THIRD_PARTY_DIR
    "${AUTOVIZ_SERVER_WORKSPACE_DIR}/third_party"
    CACHE PATH "AutoVizServer third-party dependency directory"
)

# 兼容旧 SDK 构建留下的 cache：当 third-party 根被缓存成
# .../third_party/AutoVizProto 时，恢复为父目录并重算源码路径。
get_filename_component(_AUTOVIZ_THIRD_PARTY_NAME "${AUTOVIZ_THIRD_PARTY_DIR}" NAME)
if(_AUTOVIZ_THIRD_PARTY_NAME STREQUAL "AutoVizProto")
    get_filename_component(_AUTOVIZ_MIGRATED_THIRD_PARTY_DIR
                           "${AUTOVIZ_THIRD_PARTY_DIR}" DIRECTORY)
    if(EXISTS "${_AUTOVIZ_MIGRATED_THIRD_PARTY_DIR}/AutoVizProto/CMakeLists.txt")
        message(STATUS "Migrating cached AutoVizProto SDK path to source dependency layout")
        set(AUTOVIZ_THIRD_PARTY_DIR
            "${_AUTOVIZ_MIGRATED_THIRD_PARTY_DIR}"
            CACHE PATH "AutoVizServer third-party dependency directory" FORCE)
        set(AUTOVIZ_PROTO_SOURCE_DIR
            "${_AUTOVIZ_MIGRATED_THIRD_PARTY_DIR}/AutoVizProto"
            CACHE PATH "AutoVizProto source directory" FORCE)
    endif()
endif()
unset(_AUTOVIZ_THIRD_PARTY_NAME)
unset(_AUTOVIZ_MIGRATED_THIRD_PARTY_DIR)

set(AUTOVIZ_PROTO_SOURCE_DIR
    "${AUTOVIZ_THIRD_PARTY_DIR}/AutoVizProto"
    CACHE PATH "AutoVizProto source directory"
)
if(NOT EXISTS "${AUTOVIZ_PROTO_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "AutoVizProto source is missing at ${AUTOVIZ_PROTO_SOURCE_DIR}. "
        "Initialize submodules with: git submodule update --init --recursive")
endif()

# 配置CMAKE_PREFIX_PATH
list(PREPEND CMAKE_PREFIX_PATH
    "${AUTOVIZ_THIRD_PARTY_DIR}/protobuf"
)

find_package(ament_cmake REQUIRED)
find_package(action_msgs REQUIRED)
find_package(unique_identifier_msgs REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)
find_package(custom_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)

add_subdirectory(
    "${AUTOVIZ_PROTO_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}/autoviz_proto"
)

add_executable(autoviz_server_node
    src/main.cpp
    src/AutoVizServerNode.cpp
    src/RobotWsProtoConverter.cpp
    src/SnapshotStore.cpp
    src/VisualizationServer.cpp
    src/TcpSession.cpp
)
target_include_directories(autoviz_server_node PRIVATE include)
ament_target_dependencies(autoviz_server_node
    action_msgs
    unique_identifier_msgs
    custom_msgs
    geometry_msgs
    nav_msgs
    rclcpp
)
target_link_libraries(autoviz_server_node
    AutoVizProto::AutoVizProto
    Boost::system
    Threads::Threads
)

if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)
    ament_add_gtest(tcp_server_test
        tests/TcpServerTest.cpp
        src/VisualizationServer.cpp
        src/TcpSession.cpp
    )
    target_include_directories(tcp_server_test PRIVATE include)
    target_link_libraries(tcp_server_test
        AutoVizProto::AutoVizProto
        Boost::system
        Threads::Threads
    )

    ament_add_gtest(robot_ws_converter_test
        tests/RobotWsProtoConverterTest.cpp
        src/RobotWsProtoConverter.cpp
        src/SnapshotStore.cpp
    )
    target_include_directories(robot_ws_converter_test PRIVATE include)
    ament_target_dependencies(robot_ws_converter_test
        action_msgs
        unique_identifier_msgs
        custom_msgs
        geometry_msgs
        nav_msgs
        rclcpp
    )
    target_link_libraries(robot_ws_converter_test AutoVizProto::AutoVizProto)

    # rosbag 端到端验收使用的只读协议探针：连接真实 Server 并断言快照字段。
    add_executable(autoviz_protocol_probe tests/ProtocolProbe.cpp)
    target_link_libraries(autoviz_protocol_probe
        AutoVizProto::AutoVizProto
        Boost::system
        Threads::Threads
    )
endif()

install(TARGETS autoviz_server_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_package()
