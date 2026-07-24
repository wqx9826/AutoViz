# AutoViz

AutoViz 的 `feature/client-server` 包含三个工程：

```text
AutoVizProto   唯一协议源码，构建后作为第三方 SDK 使用
AutoVizServer  ROS2 -> protobuf/TCP
AutoVizClient  protobuf/TCP -> Qt UI
```

`main` 仍是现场稳定的 ROS2 单体版本，本分支不会修改 main。

## 第三方库布局

AutoVizProto 的定位是普通第三方库。它的源码只保存一份，分别为 Client 和 Server
生成对应平台的 SDK：

```text
AutoVizClient/third_party/AutoVizProto/
  include/autoviz/
  lib/
  lib/cmake/AutoVizProto/

AutoVizServer/third_party/AutoVizProto/
  include/autoviz/
  lib/
  lib/cmake/AutoVizProto/
```

Client/Server 的 CMake 已内置上述搜索路径，不需要配置环境变量，也不需要手工传
`AutoVizProto_DIR`。`third_party` 中的本机二进制不提交 Git；README 会保留。

## 为什么不是只运行 `cmake ..`

第三方源码变成 `include/` 和 `lib/` 通常有三个阶段：

```text
cmake 配置 -> cmake --build 编译 -> cmake --install 安装 SDK
```

`cmake ..` 只生成构建系统，不会完成编译和安装。下面是标准操作。

## Linux：为 Client 准备 AutoVizProto

在仓库根目录：

```bash
cmake -S AutoVizProto -B build/proto-client \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build build/proto-client -j4
./build/proto-client/autoviz_proto_tests
cmake --install build/proto-client \
  --prefix "$PWD/AutoVizClient/third_party/AutoVizProto"

cmake -S AutoVizClient -B build/client
cmake --build build/client -j4
```

## Linux：为 Server 准备 AutoVizProto

```bash
cmake -S AutoVizProto -B build/proto-server
cmake --build build/proto-server -j4
cmake --install build/proto-server \
  --prefix "$PWD/AutoVizServer/third_party/AutoVizProto"

source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
colcon --log-base AutoVizServer/log build \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install
```

同一台 Linux 机器也可以复用一次 build，再用两个不同的 `--prefix` 安装两次。

## Windows Client

Windows 不能使用 Linux 编译出的 `.a`。应使用和 Client 相同的 MSVC/MinGW、架构和
构建类型编译 AutoVizProto，然后安装到：

```text
AutoVizClient\third_party\AutoVizProto
```

Client CMake 还会自动搜索：

```text
AutoVizClient\third_party\protobuf
AutoVizClient\third_party\Qt5
```

因此可把 protobuf SDK、Qt kit（或指向 Qt kit 的目录链接）放到固定位置，不依赖
系统环境变量。完整命令见 `AutoVizClient/README.md`。

## 协议命名

唯一 schema 位于 `AutoVizProto/proto/autoviz/*.proto`，全部声明
`package autoviz;`。生成头路径是 `autoviz/*.pb.h`，C++ 类型直接是
`autoviz::Envelope` 等。

详细说明：

- `AutoVizProto/README.md`
- `AutoVizClient/README.md`
- `AutoVizServer/README.md`
- `docs/ARCHITECTURE.md`
- `docs/PROTOCOL_DESIGN.md`
