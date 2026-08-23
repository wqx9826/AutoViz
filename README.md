# AutoViz

AutoViz 当前主线包含三个工程：

```text
AutoVizProto   唯一协议源码仓库和源码级 CMake target
AutoVizServer  ROS2 -> protobuf/TCP
AutoVizClient  protobuf/TCP -> Qt UI
```

AutoVizClient 也可在不启动 Server、ROS2、WSL 或虚拟机的情况下，直接打开 robot_ws
录制的 ROS2 Humble `sqlite3` bag，支持预检、播放、暂停、跳转及 0.1x 到 8x 调速。

`main` 是当前 C/S v2 主线；现场 ROS2/Qt 单体版本保留在
`legacy/ros2-qt-monolith`。当前 wire 协议为 **AutoViz Protocol 2.3**，Server 最多
20 Hz 发送完整快照，不使用订阅、ChannelUpdate 或 UPSERT/CLEAR。

## Protocol 源码布局

私有仓库 `wqx9826/AutoVizProto` 是 schema、版本和 FrameCodec 的唯一维护源。
本仓库用三个 submodule gitlink 锁定同一个 tag/commit：

```text
AutoVizProto/                              # 独立开发与协议测试
AutoVizClient/third_party/AutoVizProto/   # Client 独立构建
AutoVizServer/third_party/AutoVizProto/   # Server 独立构建
```

克隆开发仓库时初始化 submodule：

```bash
git clone --recursive <AutoViz URL>
# 已有 checkout：
git submodule update --init --recursive
./scripts/verify_protocol_submodules.sh
```

三个路径当前均锁定 `v2.3.0`。Client/Server 直接通过 `add_subdirectory()` 编译 Protocol，
各自构建目录自动运行 `protoc`；不再构建、安装或查找预编译 AutoVizProto SDK。
Google protobuf 本身仍是官方构建/运行依赖。

## 构建

禁止在仓库根目录创建通用 `build/`。Protocol 独立测试、Client 和 Server 的构建产物
分别位于自己的目录。

Protocol 独立测试：

```bash
cmake -S AutoVizProto -B AutoVizProto/build
cmake --build AutoVizProto/build -j4
ctest --test-dir AutoVizProto/build --output-on-failure
```

Linux Client：

```bash
cmake -S AutoVizClient -B AutoVizClient/build -DCMAKE_BUILD_TYPE=Release
cmake --build AutoVizClient/build -j4
./AutoVizClient/build/AutoViz
```

ROS2 Server：

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
cd AutoVizServer
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release
colcon test
```

Server 是 ament workspace，不对 `src/autoviz_server` 单独执行 CMake。colcon 只发现
`autoviz_server`；`third_party/AutoVizProto` 由该包的 CMake 作为子目录编译。

Windows Client 不再运行 Protocol bootstrap 脚本。Qt6 由 IDE Kit/命令行环境提供，
protobuf SDK 放在 `AutoVizClient/third_party/protobuf` 或通过 `CMAKE_PREFIX_PATH` 指定：

```powershell
cmake -S AutoVizClient -B AutoVizClient\build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build AutoVizClient\build --parallel
```

Qt、protobuf 和编译器 ABI 必须匹配；不能把 Linux 静态库或 MSYS2 UCRT64 运行库混入
当前 Qt MinGW 13.1 基线。

## 离线源码与发布

开发机生成包含完整 Protocol 源码、但不含 Git 元数据的离线源码包：

```bash
./AutoVizClient/scripts/package_source.sh
./AutoVizServer/shell/package_source.sh
```

Client 源码包解压后直接 `cmake -S . -B build`；Server 源码包在 source ROS2/robot_ws 后
直接 `colcon build`。两种构建都不需要 Git 或网络。

Client 运行发布包沿用：

```bash
./AutoVizClient/scripts/package_linux.sh
```

Server 运行发布包使用：

```bash
cd AutoVizServer
./shell/package_release.sh
```

Server 包包含 colcon install tree、launch/config、启动脚本、systemd 模板、Protocol
版本/commit 和 `ldd` 依赖清单，不包含编译器、CMake、protoc 或 AutoVizProto 开发文件。
目标机仍须提供 ABI 兼容的 ROS2 Humble、robot_ws/custom_msgs 和列出的运行库。

## 协议命名与版本

唯一 schema 位于 AutoVizProto 私有仓库的 `proto/autoviz/*.proto`，全部声明
`package autoviz;`。生成头为 `autoviz/*.pb.h`，C++ 类型直接是
`autoviz::Envelope` 等。

`VERSION` 是 Protocol 构建版本唯一来源。生成的 `autoviz/ProtocolVersion.h` 提供
major/minor/patch 和完整版本字符串；握手仍只传 major/minor，兼容性仍只比较 major。

详细说明见 `AutoVizProto/README.md`、`AutoVizClient/README.md`、
`AutoVizServer/README.md` 和 `memory/`。
