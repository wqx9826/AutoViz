# AutoVizProto

AutoVizProto 是 AutoViz 的源码级协议模块，也是 `.proto` 的唯一源码仓库。它只依赖
C++17 和 Google Protobuf，不依赖 Qt、ROS2、Boost 或 custom_msgs。

## 消费方式

Client 和 Server 将本仓库锁定为 submodule，并直接加入各自构建：

```cmake
add_subdirectory(third_party/AutoVizProto autoviz_proto)
target_link_libraries(app PRIVATE AutoVizProto::AutoVizProto)
```

配置消费工程时，CMake 在其构建目录中调用 `protoc`，生成
`generated/autoviz/*.pb.h` 和 `*.pb.cc`，随后编译静态 target `autoviz_proto`。
不再先构建、安装或查找 AutoVizProto SDK。

官方 protobuf 仍是构建依赖。Linux 可使用系统 `libprotobuf-dev` 和
`protobuf-compiler`；Windows 应通过 `CMAKE_PREFIX_PATH` 选择与 Client 编译器 ABI
兼容的 protobuf SDK。

## 独立构建和测试

独立配置本仓库时协议测试默认开启：

```bash
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

被 Client 或 Server 通过 `add_subdirectory()` 引入时，测试默认关闭。需要显式构建时可设
`-DAUTOVIZ_PROTO_BUILD_TESTS=ON`。

## 版本

`VERSION` 是构建版本的唯一来源。CMake 根据它生成
`autoviz/ProtocolVersion.h`，保留现有 `kProtocolMajor`、`kProtocolMinor` 和
`isProtocolMajorCompatible()`，并提供 `kProtocolPatch`、`kProtocolVersion`。

当前版本为 2.3.0。握手 wire message 继续只携带 major/minor，兼容性继续只比较 major；
Git tag 和消费工程的 submodule commit 用于确认构建时使用的精确源码版本。

## 协议规则

- 所有 schema 使用 `package autoviz;`，生成类型如 `autoviz::Envelope`。
- proto2 只使用 optional/repeated，禁止 required。
- field number 永不复用，删除字段使用 `reserved`。
- framing 是 4 字节大端长度加 Envelope，payload 最大 16 MiB。
- 不兼容语义、单位、坐标或 framing 变化必须提升 protocol major。
- `FrameCodec`、schema 和版本规则归本仓库维护，消费工程不得复制修改。

FrameCodec API 保持对称：

```cpp
autoviz::FrameBytes bytes;
std::string error;
autoviz::encodeFrame(envelope, bytes, error);

autoviz::FrameDecoder decoder;
std::vector<autoviz::Envelope> messages;
decoder.decode(std::string_view(bytes), messages, error);
```

`FrameBytes` 是二进制字节容器，可包含零字节。`decode()` 保留未完成 TCP 数据，并在每次
调用输出 0 到 N 个完整 Envelope。
