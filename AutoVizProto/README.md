# AutoVizProto

AutoVizProto 是 AutoViz 的独立协议工程，也是 `.proto` 文件的唯一真实来源。它不依赖
Qt、ROS2、Boost 或 custom_msgs，只依赖 C++17 和 protobuf，因此可在 Linux 与
Windows 上单独构建、安装和分发。

## 它提供什么

- `proto/autoviz/*.proto`：车辆、规划、感知、控制、运行状态和传输消息。
- `autoviz/*.pb.h`：构建时由 `protoc` 生成的 C++ 头文件。
- `autoviz/FrameCodec.h`：TCP 的 4 字节大端长度前缀编解码。
- `AutoVizProto::AutoVizProto`：供其他 CMake 工程链接的导入 target。
- `autoviz_proto_tests`：覆盖拆包、粘包、非法长度、损坏 payload、握手、快照、
  增量和 CLEAR 的 GTest 可执行文件。

所有 proto 都声明：

```proto
package autoviz;
```

所以生成类型是 `autoviz::Envelope`、`autoviz::VehicleState` 等。这里没有
`protocol` 子命名空间。`v1` 仍是握手时的协议主版本概念，不写进 C++ namespace。

## 构建、测试和安装

```bash
cmake -S AutoVizProto -B build/proto \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/proto" \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build build/proto -j4
./build/proto/autoviz_proto_tests
cmake --install build/proto
```

测试使用 GTest，但项目不启用 CTest；直接运行测试程序即可。关闭测试时可省略
`AUTOVIZ_PROTO_BUILD_TESTS`。

安装目录包含库、生成头、公开 FrameCodec 头、原始 proto 和 CMake package 配置。
消费方写法：

```cmake
find_package(AutoVizProto CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE AutoVizProto::AutoVizProto)
```

配置时若安装前缀不在默认搜索路径，传入：

```bash
-DAutoVizProto_DIR=/path/to/install/lib/cmake/AutoVizProto
```

Windows 的安装目录通常仍有 `lib/cmake/AutoVizProto`；也可将安装前缀加入
`CMAKE_PREFIX_PATH`。

## 为什么不在 Client 和 Server 各放一份

两份 schema 看起来方便迁移，实际会产生“改了一边、忘了另一边”的协议漂移。
AutoVizProto 把唯一 schema 做成可安装依赖，同时保留可移植性：

- Windows 只需取得 AutoVizClient 与 AutoVizProto，不需要 ROS Server。
- Server 只链接已安装的协议包，不读取 Client 源码。
- 将来把 AutoVizProto 拆成独立仓库、Git submodule 或预编译 SDK 时，Client/Server
  的 `find_package` 接口无需改变。

## 兼容约束

- 已使用的 protobuf field number 永不复用，删除字段使用 `reserved`。
- 新字段使用 optional/repeated，禁止 required。
- 不兼容的语义、单位或坐标变化必须提升握手的 protocol major。
- `package autoviz` 是公开生成代码 API；更改它会造成 C++ 源码级不兼容。
- framing 固定为 4 字节大端 payload 长度 + Envelope，payload 最大 16 MiB。
