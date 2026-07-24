# AutoViz

AutoViz 是面向机器人规划控制链路的可视化调试工具。`feature/client-server` 由两个
完全独立的工程组成：

```text
robot_ws ROS2 topics
        |
        v
AutoVizServer (Linux / ROS2)
        |
        | protobuf + TCP
        v
AutoVizClient (Linux / Windows / Qt5)
        |
        v
DataManager -> SceneManager -> UI
```

`main` 分支仍是经过现场测试的 ROS2 单体版本；本分支的目录重排不会修改 main。

## 工程目录

```text
AutoVizClient/                         # 独立 Qt Client 工程
  CMakeLists.txt
  proto/autoviz/*.proto               # Client 自己编译的协议副本
  include/ src/ configs/ tests/

AutoVizServer/                         # 独立 ROS2 workspace
  src/autoviz_server/
    CMakeLists.txt
    package.xml
    proto/autoviz/*.proto             # Server 自己编译的协议副本
    include/ src/ config/ launch/ test/

tools/verify_proto_sync.cmake         # 仓库内两份 proto 一致性检查
docs/                                 # 架构、协议和演进文档
```

两个工程不通过 CMake、源码路径或生成文件互相引用。复制 `AutoVizClient/` 到 Windows
时无需携带 Server、ROS2 或仓库其他目录；复制 `AutoVizServer/` 也可以作为独立
colcon workspace 构建。

## 构建 Client

依赖 CMake 3.16+、C++17、Qt5 Widgets/Network 和 protobuf 3。

```bash
cmake -S AutoVizClient -B build/client
cmake --build build/client -j4
./build/client/AutoViz
```

Client 不使用 CTest。需要 GTest 时增加 `-DAUTOVIZ_BUILD_TESTS=ON`，构建后直接运行
`./build/client/autoviz_client_protocol_tests`。

详细的 Linux/Windows 说明见 `AutoVizClient/README.md`。

## 构建和运行 Server

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash

colcon --log-base AutoVizServer/log build \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install

source AutoVizServer/install/setup.bash
ros2 launch autoviz_server autoviz_server.launch.py
```

默认监听 `0.0.0.0:39090`，参数位于
`AutoVizServer/src/autoviz_server/config/robot_ws.yaml`。详细说明见
`AutoVizServer/README.md`。

## 测试与协议同步

Client 使用独立 GTest 可执行文件，Server 使用 `ament_cmake_gtest`：

```bash
colcon --log-base AutoVizServer/log test \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install
colcon test-result --test-result-base AutoVizServer/build --verbose
```

两份 proto 必须同步修改。提交协议变更前运行：

```bash
cmake -P tools/verify_proto_sync.cmake
```

详细上下文见 `docs/PROJECT_CONTEXT.md`、`docs/ARCHITECTURE.md`、
`docs/PROTOCOL_DESIGN.md` 和 `docs/TODO.md`。
