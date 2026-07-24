# AutoViz

AutoViz 是面向机器人规划控制链路的可视化调试工具。`feature/client-server` 由三个
边界清晰、可独立构建的工程组成：

```text
ROS2 topics
    -> AutoVizServer (Linux / ROS2)
    -> protobuf Envelope + TCP
    -> AutoVizClient (Linux / Windows / Qt5)

AutoVizProto
    -> 为 Client 和 Server 提供唯一 schema、生成代码与 TCP FrameCodec
```

`main` 仍是经过现场测试的 ROS2 单体版本；本分支的修改不会改写 main。

## 工程目录

```text
AutoVizProto/                         # 纯 C++/protobuf、可安装的协议工程
  proto/autoviz/*.proto               # 唯一 schema
  include/autoviz/FrameCodec.h
  src/ tests/

AutoVizClient/                        # 纯 Qt Client，不含 ROS
  CMakeLists.txt
  src/ configs/

AutoVizServer/                        # 独立 ROS2 workspace
  src/autoviz_server/
    CMakeLists.txt package.xml
    include/ src/ config/ launch/

docs/                                 # 架构、协议和演进文档
```

proto 声明为 `package autoviz;`，所以生成的 C++ 类型直接位于 `autoviz` namespace，
例如 `autoviz::Envelope`。目录名只决定生成头文件路径
`autoviz/transport.pb.h`，不再出现 `autoviz::protocol::v1`。

## 1. 构建并安装 AutoVizProto

Client 和 Server 都消费安装后的 CMake package，不引用 `AutoVizProto` 源码相对路径：

```bash
cmake -S AutoVizProto -B build/proto \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/proto" \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build build/proto -j4
./build/proto/autoviz_proto_tests
cmake --install build/proto
```

测试使用 GTest，但不注册 CTest。schema、FrameCodec 或 framing 变化应在此工程验证。

## 2. 构建 Client

依赖 CMake 3.16+、C++17、Qt5 Widgets/Network，以及已安装的 AutoVizProto：

```bash
cmake -S AutoVizClient -B build/client \
  -DAutoVizProto_DIR="$PWD/install/proto/lib/cmake/AutoVizProto"
cmake --build build/client -j4
./build/client/AutoViz
```

Client 没有 CTest/GTest，也不需要 ROS。迁移到 Windows 时复制
`AutoVizClient/` 和一个可独立获取的 `AutoVizProto/`（或其预编译安装包）即可，无需
携带 Server。

## 3. 构建和运行 Server

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash

colcon --log-base AutoVizServer/log build \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install \
  --cmake-args \
    -DAutoVizProto_DIR="$PWD/install/proto/lib/cmake/AutoVizProto"

source AutoVizServer/install/setup.bash
ros2 launch autoviz_server autoviz_server.launch.py
```

默认监听 `0.0.0.0:39090`。参数位于
`AutoVizServer/src/autoviz_server/config/robot_ws.yaml`。

详细说明见：

- `AutoVizProto/README.md`
- `AutoVizClient/README.md`
- `AutoVizServer/README.md`
- `docs/ARCHITECTURE.md`
- `docs/PROTOCOL_DESIGN.md`
- `docs/PROJECT_CONTEXT.md`
