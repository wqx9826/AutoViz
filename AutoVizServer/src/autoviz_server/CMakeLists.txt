cmake_minimum_required(VERSION 3.16)
project(autoviz_server VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(custom_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(Protobuf REQUIRED)
find_package(rclcpp REQUIRED)

set(AUTOVIZ_PROTOCOL_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(AUTOVIZ_PROTOCOL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AUTOVIZ_PROTOCOL_FILES
    autoviz/protocol/v1/common.proto
    autoviz/protocol/v1/vehicle.proto
    autoviz/protocol/v1/planning.proto
    autoviz/protocol/v1/perception.proto
    autoviz/protocol/v1/control.proto
    autoviz/protocol/v1/runtime.proto
    autoviz/protocol/v1/transport.proto
)

set(AUTOVIZ_PROTOCOL_GENERATED_SOURCES)
set(AUTOVIZ_PROTOCOL_GENERATED_HEADERS)
set(AUTOVIZ_PROTOCOL_ABSOLUTE_FILES)
foreach(proto_file IN LISTS AUTOVIZ_PROTOCOL_FILES)
    string(REPLACE ".proto" ".pb.cc" generated_source "${proto_file}")
    string(REPLACE ".proto" ".pb.h" generated_header "${proto_file}")
    list(APPEND AUTOVIZ_PROTOCOL_GENERATED_SOURCES
         "${AUTOVIZ_PROTOCOL_GENERATED_DIR}/${generated_source}")
    list(APPEND AUTOVIZ_PROTOCOL_GENERATED_HEADERS
         "${AUTOVIZ_PROTOCOL_GENERATED_DIR}/${generated_header}")
    list(APPEND AUTOVIZ_PROTOCOL_ABSOLUTE_FILES
         "${AUTOVIZ_PROTOCOL_PROTO_ROOT}/${proto_file}")
endforeach()

add_custom_command(
    OUTPUT ${AUTOVIZ_PROTOCOL_GENERATED_SOURCES} ${AUTOVIZ_PROTOCOL_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOVIZ_PROTOCOL_GENERATED_DIR}"
    COMMAND protobuf::protoc
            --cpp_out=${AUTOVIZ_PROTOCOL_GENERATED_DIR}
            --proto_path=${AUTOVIZ_PROTOCOL_PROTO_ROOT}
            ${AUTOVIZ_PROTOCOL_ABSOLUTE_FILES}
    DEPENDS ${AUTOVIZ_PROTOCOL_ABSOLUTE_FILES}
    COMMENT "Generating AutoVizServer protobuf sources"
    VERBATIM
)

add_library(autoviz_server_protocol STATIC
    ${AUTOVIZ_PROTOCOL_GENERATED_SOURCES}
    ${AUTOVIZ_PROTOCOL_GENERATED_HEADERS}
    include/autoviz/protocol/FrameCodec.h
    src/protocol/FrameCodec.cpp
)
add_library(AutoVizServer::Protocol ALIAS autoviz_server_protocol)

target_include_directories(autoviz_server_protocol
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${AUTOVIZ_PROTOCOL_GENERATED_DIR}"
)
target_link_libraries(autoviz_server_protocol PUBLIC protobuf::libprotobuf)
target_compile_features(autoviz_server_protocol PUBLIC cxx_std_17)

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
    AutoVizServer::Protocol
    Boost::system
)

if(BUILD_TESTING)
    find_package(ament_cmake_gtest REQUIRED)
    ament_add_gtest(autoviz_server_protocol_tests test/FrameCodecTest.cpp)
    target_link_libraries(autoviz_server_protocol_tests AutoVizServer::Protocol)
endif()

install(TARGETS autoviz_server_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

ament_package()
