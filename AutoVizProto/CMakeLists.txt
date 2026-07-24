cmake_minimum_required(VERSION 3.16)

project(AutoVizProto VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOVIZ_PROTO_BUILD_TESTS "Build the AutoVizProto GTest executable" OFF)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

find_package(Protobuf REQUIRED)

set(AUTOVIZ_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(AUTOVIZ_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AUTOVIZ_PROTO_FILES
    autoviz/common.proto
    autoviz/vehicle.proto
    autoviz/planning.proto
    autoviz/perception.proto
    autoviz/control.proto
    autoviz/runtime.proto
    autoviz/transport.proto
)

set(AUTOVIZ_PROTO_GENERATED_SOURCES)
set(AUTOVIZ_PROTO_GENERATED_HEADERS)
set(AUTOVIZ_PROTO_ABSOLUTE_FILES)
foreach(proto_file IN LISTS AUTOVIZ_PROTO_FILES)
    string(REPLACE ".proto" ".pb.cc" generated_source "${proto_file}")
    string(REPLACE ".proto" ".pb.h" generated_header "${proto_file}")
    list(APPEND AUTOVIZ_PROTO_GENERATED_SOURCES
         "${AUTOVIZ_PROTO_GENERATED_DIR}/${generated_source}")
    list(APPEND AUTOVIZ_PROTO_GENERATED_HEADERS
         "${AUTOVIZ_PROTO_GENERATED_DIR}/${generated_header}")
    list(APPEND AUTOVIZ_PROTO_ABSOLUTE_FILES
         "${AUTOVIZ_PROTO_ROOT}/${proto_file}")
endforeach()

add_custom_command(
    OUTPUT ${AUTOVIZ_PROTO_GENERATED_SOURCES} ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOVIZ_PROTO_GENERATED_DIR}"
    COMMAND protobuf::protoc
            --cpp_out=${AUTOVIZ_PROTO_GENERATED_DIR}
            --proto_path=${AUTOVIZ_PROTO_ROOT}
            ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    DEPENDS ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    COMMENT "Generating AutoViz protobuf sources"
    VERBATIM
)

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

install(
    TARGETS autoviz_proto
    EXPORT AutoVizProtoTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(
    DIRECTORY "${AUTOVIZ_PROTO_GENERATED_DIR}/autoviz/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/autoviz"
    FILES_MATCHING PATTERN "*.h"
)
install(
    DIRECTORY proto/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AutoVizProto/proto"
    FILES_MATCHING PATTERN "*.proto"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    cmake/AutoVizProtoConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AutoVizProto"
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AutoVizProto"
)
install(
    EXPORT AutoVizProtoTargets
    FILE AutoVizProtoTargets.cmake
    NAMESPACE AutoVizProto::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AutoVizProto"
)
