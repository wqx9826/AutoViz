# AutoVizProto

AutoVizProto 是 AutoViz 的第三方协议库，也是 `.proto` 的唯一源码。它不依赖 Qt、
ROS2、Boost 或 custom_msgs，只依赖 C++17 和 protobuf。

## 生成的 SDK

标准安装后得到：

```text
<prefix>/
  include/autoviz/
    FrameCodec.h
    *.pb.h
  lib/
    libautoviz_proto.a       # Linux 静态库
    autoviz_proto.lib        # Windows 静态库
    cmake/AutoVizProto/      # 让 CMake 正确导入头文件和库
  share/AutoVizProto/proto/  # 原始 schema，便于查看与其他语言生成
```

`lib/cmake/AutoVizProto` 不是多余运行文件，而是 SDK 的 CMake 元数据。它让消费方使用
一个稳定 target：

```cmake
find_package(AutoVizProto CONFIG REQUIRED)
target_link_libraries(app PRIVATE AutoVizProto::AutoVizProto)
```

这样不需要在 Client/Server 中手写 Linux `.a`、Windows `.lib`、Debug/Release 路径
和 protobuf 的传递依赖。

## Linux 标准构建流程

```bash
./AutoVizProto/scripts/bootstrap_proto.sh
```

该脚本可从任意当前目录运行，它会固定使用 `AutoVizProto/build`，并把 SDK
安装到 Client 和 Server 各自的 `third_party/AutoVizProto`。Linux 下安装 Proto
应使用此脚本，不在 feature 根目录或其他路径新建 Proto build 目录。

脚本完成 CMake 配置、编译和带明确前缀的安装，不需要 `sudo`。协议测试
按需在 `AutoVizProto/build` 中生成和运行。

测试使用 GTest，直接运行，不启用 CTest。

## Windows

在仓库根目录以 PowerShell 运行：

```powershell
.\AutoVizProto\scripts\bootstrap_proto.ps1
```

脚本使用 `AutoVizProto\build` 和 Release 配置，并仅安装 Windows Client SDK 到
`AutoVizClient\third_party\AutoVizProto`。ROS2 Server 只在 Linux 上构建；其 Linux SDK
由 `bootstrap_proto.sh` 安装到 Server 的 `third_party` 目录。
Windows 默认使用 Ninja 生成器，确保选择当前 Kit/PATH 中的 MinGW，而非 CMake 自动回退到 Visual Studio。
需要其他构建类型时可传入，例如
`.\AutoVizProto\scripts\bootstrap_proto.ps1 -Configuration Debug`。
Windows Client 的 protobuf SDK 必须使用与 Qt Client 相同的 MinGW runtime 构建；脚本不会
自动选择 `C:\msys64\ucrt64`。若 SDK 位于其他位置，可显式指定它及 Qt Kit 的编译器：

```powershell
.\AutoVizProto\scripts\bootstrap_proto.ps1 `
  -CMakePrefixPath D:\protobuf-35.1-mingw1310 `
  -CxxCompiler D:\Qt6.10\Tools\mingw1310_64\bin\g++.exe
```

`CMakePrefixPath` 指向 protobuf 的安装前缀；也可将同一 SDK 放到
`AutoVizClient\third_party\protobuf`，脚本会自动使用它。
AutoVizProto 和 Client 必须使用兼容的编译器、架构与 protobuf。若 protobuf 放在
`AutoVizClient\third_party\protobuf`，脚本会自动将其传给 CMake，不设置系统环境变量。

当前 Windows 方案 1 已将 Qt MinGW 13.1、Qt Ninja、独立 CMake 和 `protoc 35.1` 放到用户
`Path` 前端，并设置 `Protobuf_ROOT=D:\protobuf-35.1-mingw1310`、
`Qt6_ROOT=D:\Qt6.10\6.10.0\mingw_64` 及
`CMAKE_PREFIX_PATH=D:\Qt6.10\6.10.0\mingw_64;D:\protobuf-35.1-mingw1310`。
因此重启终端后可直接运行不带参数的 `bootstrap_proto.ps1`。protobuf SDK 直接安装在该
无空格路径中，以兼容 MinGW 资源编译器。

## CMakeLists 怎样阅读

为了便于第一次开发 SDK 时从头阅读，生成、编译和安装逻辑统一保存在一个
`CMakeLists.txt`，并使用中文分段注释：

```text
CMakeLists.txt
  1. 项目基础设置和依赖
  2. protoc 生成 autoviz/*.pb.h/.pb.cc
  3. 编译 pb.cc 和 FrameCodec.cpp
  4. 可选 GTest
  5. 安装 include/lib/schema
  6. 导出 find_package 所需文件
```

`cmake/AutoVizProtoConfig.cmake.in` 仍单独保存，因为它不是另一段构建脚本，而是最终
安装进 SDK 的 `AutoVizProtoConfig.cmake` 的源码模板。主 CMake 中已经明确标出读取、
生成和安装该模板的位置。

消费方因此很简单。若只复制某个 `.a` 并手写 include/lib 路径，Windows `.lib`、
protobuf 传递依赖和不同构建类型都需要在 Client/Server 重复处理，反而更容易出错。

## 协议规则

- 所有文件使用 `package autoviz;`，生成类型如 `autoviz::Envelope`。
- proto2 只使用 optional/repeated，禁止 required。
- field number 永不复用，删除字段使用 `reserved`。
- framing 是 4 字节大端长度 + Envelope，payload 最大 16 MiB。
- 版本常量统一由 `autoviz/ProtocolVersion.h` 提供，业务代码不得散落硬编码版本号。
- UI 行为依赖的字段使用显式 typed 字段；DiagnosticMetric 只承载扩展诊断。
- 不兼容语义、单位或坐标变化提升握手的 protocol major。

当前版本为 2.0。Envelope 只包含 ClientHello、ServerHello、VisualizationSnapshot、
Heartbeat 和 ProtocolError；v1 的订阅与增量 field number 已保留但不再使用。

FrameCodec 的发送和接收 API 对称且显式返回错误：

```cpp
autoviz::FrameBytes bytes;
std::string error;
autoviz::encodeFrame(envelope, bytes, error);

autoviz::FrameDecoder decoder;
std::vector<autoviz::Envelope> messages;
decoder.decode(std::string_view(bytes), messages, error);
```

`FrameBytes` 是 `std::string` 的别名，但内容是二进制，不是文本。它可包含 `\0`；
`decode()` 保留未完成 TCP 数据并在每次调用输出 0～N 个完整 Envelope。
