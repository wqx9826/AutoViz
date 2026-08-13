cmake_minimum_required(VERSION 3.16)

project(AutoVizProto VERSION 0.3.0 LANGUAGES CXX)

# =============================================================================
# 1. 项目基础设置
# =============================================================================
#
# AutoVizProto 的核心工作：
#   1) 使用 protoc 将 proto/autoviz/*.proto 生成 C++ 代码；
#   2) 将生成的 *.pb.cc 和 FrameCodec.cpp 编译成静态库；
#   3) 安装为包含 include/、lib/ 和 CMake package 配置的标准 SDK。
#
# Client 和 Server 最终只需要：
#   find_package(AutoVizProto CONFIG REQUIRED)
#   target_link_libraries(... AutoVizProto::AutoVizProto)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOVIZ_PROTO_BUILD_TESTS "Build the AutoVizProto GTest executable" OFF)

# GNUInstallDirs 提供跨平台标准安装目录变量：
#   CMAKE_INSTALL_INCLUDEDIR -> include
#   CMAKE_INSTALL_LIBDIR     -> Linux 通常为 lib，部分平台可能为 lib64
#   CMAKE_INSTALL_BINDIR     -> bin
#   CMAKE_INSTALL_DATADIR    -> share
include(GNUInstallDirs)

# 用于生成支持 find_package 的 Config 和 Version 文件。
include(CMakePackageConfigHelpers)

# protobuf::protoc 用于生成代码；protobuf::libprotobuf 用于编译和链接。
# 前缀必须由调用方或本地 third_party 显式提供，不能自动选中 MSYS2 UCRT64：
# Client 的 Qt MinGW runtime 与该目录中的 GCC runtime 可能不同。
find_package(Protobuf REQUIRED CONFIG)

# =============================================================================
# 2. 将 *.proto 生成 *.pb.h 和 *.pb.cc
# =============================================================================
#
# 生成文件放在构建目录，而不是源码目录：
#   <build>/generated/autoviz/*.pb.h
#   <build>/generated/autoviz/*.pb.cc
#
# 这样不会把不同平台、不同 protobuf 版本生成的文件写进源码树。

set(AUTOVIZ_PROTO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(AUTOVIZ_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# 显式列出 schema，而不使用 GLOB。增删 proto 时必须同步修改这个列表，
# CMake 才能准确识别协议文件集合的变化。
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

# 根据 autoviz/xxx.proto 计算：
#   输入绝对路径：<source>/proto/autoviz/xxx.proto
#   生成源文件：  <build>/generated/autoviz/xxx.pb.cc
#   生成头文件：  <build>/generated/autoviz/xxx.pb.h
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

# 定义 protoc 生成规则：
#   --proto_path 是 import 根目录，因此 proto 中可写 import "autoviz/common.proto"；
#   --cpp_out 是生成根目录，因此生成结果保留 autoviz/*.pb.h 的目录结构；
#   OUTPUT/DEPENDS 让 proto 变化时自动重新生成，未变化时不重复执行。
add_custom_command(
    OUTPUT
        ${AUTOVIZ_PROTO_GENERATED_SOURCES}
        ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${AUTOVIZ_PROTO_GENERATED_DIR}"
    COMMAND protobuf::protoc
            --cpp_out=${AUTOVIZ_PROTO_GENERATED_DIR}
            --proto_path=${AUTOVIZ_PROTO_ROOT}
            ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    DEPENDS ${AUTOVIZ_PROTO_ABSOLUTE_FILES}
    COMMENT "Generating AutoViz protobuf sources"
    VERBATIM
)

# =============================================================================
# 3. 编译 AutoVizProto 库
# =============================================================================

# 将生成代码和 FrameCodec 编译为：
#   Linux   -> libautoviz_proto.a
#   Windows -> autoviz_proto.lib
add_library(autoviz_proto STATIC
    ${AUTOVIZ_PROTO_GENERATED_SOURCES}
    ${AUTOVIZ_PROTO_GENERATED_HEADERS}
    include/autoviz/FrameCodec.h
    src/FrameCodec.cpp
)

# 当前构建树中的标准别名。安装后的同名 target 由 install(EXPORT) 生成。
add_library(AutoVizProto::AutoVizProto ALIAS autoviz_proto)

set_target_properties(autoviz_proto PROPERTIES
    # 导出后使用 AutoVizProto::AutoVizProto，而不是 AutoVizProto::autoviz_proto。
    EXPORT_NAME AutoVizProto
    # 允许该静态库以后安全链接到共享库。
    POSITION_INDEPENDENT_CODE ON
)

target_compile_features(autoviz_proto PUBLIC cxx_std_17)

# BUILD_INTERFACE 在源码构建和 GTest 中生效：
#   - 手写公开头位于 <source>/include；
#   - protobuf 生成头位于 <build>/generated。
#
# INSTALL_INTERFACE 在安装后的 Client/Server 中生效：
#   - include 是相对 SDK 安装前缀的路径；
#   - 不记录本机源码和 build 绝对路径，因此 SDK 可以整体移动。
target_include_directories(autoviz_proto
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${AUTOVIZ_PROTO_GENERATED_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# PUBLIC 表示 protobuf 是公开传递依赖：
# 安装的 *.pb.h 需要 protobuf 头，静态库最终也需要消费方链接 protobuf。
target_link_libraries(autoviz_proto PUBLIC protobuf::libprotobuf)

# =============================================================================
# 4. 可选 GTest
# =============================================================================
#
# Client/Server 不重复测试 FrameCodec。开启选项后在协议库中统一生成测试程序，
# 按工程约定直接运行，不注册 CTest。

if(AUTOVIZ_PROTO_BUILD_TESTS)
    find_package(GTest REQUIRED)

    add_executable(autoviz_proto_tests tests/FrameCodecTest.cpp)
    target_link_libraries(autoviz_proto_tests
        PRIVATE
            AutoVizProto::AutoVizProto
            GTest::gtest_main
    )
endif()

# =============================================================================
# 5. 安装头文件、库和原始 schema
# =============================================================================

# 安装库，并把 target 加入 AutoVizProtoTargets 导出集合：
#   ARCHIVE -> 静态库 .a/.lib
#   LIBRARY -> Linux 等平台的共享库 .so
#   RUNTIME -> Windows DLL 或可执行文件
install(
    TARGETS autoviz_proto
    EXPORT AutoVizProtoTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# 安装手写公开头：include/autoviz/FrameCodec.h。
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# 安装 protoc 生成的公开头：include/autoviz/*.pb.h。
# *.pb.cc 已经编译进库，不需要交给消费方再次编译。
install(
    DIRECTORY "${AUTOVIZ_PROTO_GENERATED_DIR}/autoviz/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/autoviz"
    FILES_MATCHING PATTERN "*.h"
)

# 原始 schema 不参与 C++ 链接，但作为 SDK 资料保留，
# 便于查看协议或未来生成 Python/C#/其他语言代码。
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/proto/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AutoVizProto/proto"
    FILES_MATCHING PATTERN "*.proto"
)

# =============================================================================
# 6. 生成并安装 find_package 所需文件
# =============================================================================

set(AUTOVIZ_PROTO_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/AutoVizProto"
)

# 生成 AutoVizProtoConfigVersion.cmake。
# 消费方请求具体版本时，CMake 用它判断 SDK 版本是否兼容。
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

# 这里就是 AutoVizProtoConfig.cmake.in 被读取的位置：
#   输入：源码中的 cmake/AutoVizProtoConfig.cmake.in
#   输出：构建目录中的 AutoVizProtoConfig.cmake
#
# 模板负责查找 protobuf 并加载 AutoVizProtoTargets.cmake；
# @PACKAGE_INIT@ 会展开成支持 SDK 整体迁移的初始化代码。
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AutoVizProtoConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfig.cmake"
    INSTALL_DESTINATION "${AUTOVIZ_PROTO_CMAKE_INSTALL_DIR}"
)

# 上一步只是生成文件；这里才将 Config 和 Version 文件复制进 SDK。
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/AutoVizProtoConfigVersion.cmake"
    DESTINATION "${AUTOVIZ_PROTO_CMAKE_INSTALL_DIR}"
)

# 自动生成并安装：
#   AutoVizProtoTargets.cmake
#   AutoVizProtoTargets-<config>.cmake
#
# Config.cmake 是 find_package 的入口，它加载 Targets.cmake；
# Targets.cmake 最终创建 AutoVizProto::AutoVizProto。
install(
    EXPORT AutoVizProtoTargets
    FILE AutoVizProtoTargets.cmake
    NAMESPACE AutoVizProto::
    DESTINATION "${AUTOVIZ_PROTO_CMAKE_INSTALL_DIR}"
)
