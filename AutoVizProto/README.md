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

## 标准构建流程

```bash
cmake -S . -B build -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build build -j4
./build/autoviz_proto_tests
cmake --install build --prefix /目标/third_party/AutoVizProto
```

注意：`cmake -S . -B build` 只是配置；`cmake --build` 才编译；`cmake --install`
才把公开头和库整理到 `include/`、`lib/`。

不要直接执行不带安装前缀的 `make install`。CMake 默认前缀是 `/usr/local`，普通用户
没有写权限；AutoViz 应安装到两个消费工程自己的 `third_party/AutoVizProto`，无需 sudo。
在完整仓库中可直接运行 `./scripts/bootstrap_proto.sh` 一次完成构建、测试和两份安装。

测试使用 GTest，直接运行，不启用 CTest。

## Windows

以 MSVC Release 为例：

```powershell
cmake -S . -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=C:\path\to\protobuf
cmake --build build --config Release
cmake --install build --config Release `
  --prefix C:\path\to\AutoVizClient\third_party\AutoVizProto
```

AutoVizProto 和 Client 必须使用兼容的编译器、架构与 protobuf。若 protobuf 放在
`AutoVizClient\third_party\protobuf`，上面的 `CMAKE_PREFIX_PATH` 指向该目录即可；
它是本次配置参数，不要求设置系统环境变量。

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
