cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
     AUTOVIZ_PROTOCOL_VERSION LIMIT_COUNT 1)
if(NOT AUTOVIZ_PROTOCOL_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "AutoVizProto VERSION must contain one semantic version (major.minor.patch)")
endif()

project(AutoVizProto VERSION "${AUTOVIZ_PROTOCOL_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(AUTOVIZ_PROTO_BUILD_TESTS_DEFAULT ON)
else()
    set(AUTOVIZ_PROTO_BUILD_TESTS_DEFAULT OFF)
endif()
option(AUTOVIZ_PROTO_BUILD_TESTS
       "Build the AutoVizProto GTest executable"
       ${AUTOVIZ_PROTO_BUILD_TESTS_DEFAULT})

find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    if(WIN32)
        message(FATAL_ERROR
            "Protobuf Config package not found. Add the intended compiler-compatible "
            "protobuf SDK to CMAKE_PREFIX_PATH.")
    endif()
    find_package(Protobuf REQUIRED MODULE)
endif()

set(AUTOVIZ_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(AUTOVIZ_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AUTOVIZ_PROTO_FILES
    autoviz/common.proto
    autoviz/underwater.proto
    autoviz/vehicle.proto
    autoviz/planning.proto
    autoviz/perception.proto
    autoviz/control.proto
    autoviz/runtime.proto
    autoviz/transport.proto
)

set(AUTOVIZ_PROTO_ABSOLUTE_FILES)
set(AUTOVIZ_PROTO_GENERATED_SOURCES)
set(AUTOVIZ_PROTO_GENERATED_HEADERS)
foreach(proto_file IN LISTS AUTOVIZ_PROTO_FILES)
    string(REPLACE ".proto" ".pb.cc" generated_source "${proto_file}")
    string(REPLACE ".proto" ".pb.h" generated_header "${proto_file}")
    list(APPEND AUTOVIZ_PROTO_ABSOLUTE_FILES
         "${AUTOVIZ_PROTO_ROOT}/${proto_file}")
    list(APPEND AUTOVIZ_PROTO_GENERATED_SOURCES
         "${AUTOVIZ_PROTO_GENERATED_DIR}/${generated_source}")
    list(APPEND AUTOVIZ_PROTO_GENERATED_HEADERS
         "${AUTOVIZ_PROTO_GENERATED_DIR}/${generated_header}")
endforeach()

file(MAKE_DIRECTORY "${AUTOVIZ_PROTO_GENERATED_DIR}/autoviz")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/autoviz/ProtocolVersion.h.in"
    "${AUTOVIZ_PROTO_GENERATED_DIR}/autoviz/ProtocolVersion.h"
    @ONLY
)
set(AUTOVIZ_PROTOCOL_VERSION_HEADER
    "${AUTOVIZ_PROTO_GENERATED_DIR}/autoviz/ProtocolVersion.h")

add_custom_command(
    OUTPUT
        ${AUTOVIZ_PROTO_GENERATED_SOURCES}
        ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOVIZ_PROTO_GENERATED_DIR}"
    COMMAND protobuf::protoc
            --cpp_out=${AUTOVIZ_PROTO_GENERATED_DIR}
            --proto_path=${AUTOVIZ_PROTO_ROOT}
            ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    DEPENDS
        ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
        protobuf::protoc
    COMMENT "Generating AutoViz protobuf sources"
    VERBATIM
)

add_library(autoviz_proto STATIC
    ${AUTOVIZ_PROTO_GENERATED_SOURCES}
    ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    ${AUTOVIZ_PROTOCOL_VERSION_HEADER}
    include/autoviz/FrameCodec.h
    src/FrameCodec.cpp
)
add_library(AutoVizProto::AutoVizProto ALIAS autoviz_proto)

set_target_properties(autoviz_proto PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
target_compile_features(autoviz_proto PUBLIC cxx_std_17)
target_include_directories(autoviz_proto
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${AUTOVIZ_PROTO_GENERATED_DIR}"
)
target_link_libraries(autoviz_proto PUBLIC protobuf::libprotobuf)

if(AUTOVIZ_PROTO_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(autoviz_proto_tests tests/FrameCodecTest.cpp)
    target_link_libraries(autoviz_proto_tests
        PRIVATE
            AutoVizProto::AutoVizProto
            GTest::gtest_main
    )
    add_test(NAME autoviz_proto_tests COMMAND autoviz_proto_tests)
endif()
