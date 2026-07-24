cmake_minimum_required(VERSION 3.16)

project(AutoVizProto VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOVIZ_PROTO_BUILD_TESTS "Build the AutoVizProto GTest executable" OFF)

include(GNUInstallDirs)
find_package(Protobuf REQUIRED)

include(cmake/GenerateProto.cmake)

add_library(autoviz_proto STATIC
    ${AUTOVIZ_PROTO_GENERATED_SOURCES}
    ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    include/autoviz/FrameCodec.h
    src/FrameCodec.cpp
)
add_library(AutoVizProto::AutoVizProto ALIAS autoviz_proto)

set_target_properties(autoviz_proto PROPERTIES
    EXPORT_NAME AutoVizProto
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(autoviz_proto PUBLIC cxx_std_17)
target_include_directories(autoviz_proto
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${AUTOVIZ_PROTO_GENERATED_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(autoviz_proto PUBLIC protobuf::libprotobuf)

if(AUTOVIZ_PROTO_BUILD_TESTS)
    find_package(GTest REQUIRED)
    add_executable(autoviz_proto_tests tests/FrameCodecTest.cpp)
    target_link_libraries(autoviz_proto_tests
        PRIVATE
            AutoVizProto::AutoVizProto
            GTest::gtest_main
    )
endif()

include(cmake/InstallAutoVizProto.cmake)
