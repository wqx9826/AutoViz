# AutoViz

AutoViz 当前主线包含三个工程：

```text
AutoVizProto   唯一协议源码，构建后作为第三方 SDK 使用
AutoVizServer  ROS2 -> protobuf/TCP
AutoVizClient  protobuf/TCP -> Qt UI
```

AutoVizClient 还可在不启动 Server、ROS2、WSL 或虚拟机的情况下，直接打开 robot_ws
录制的 ROS2 Humble `sqlite3` bag。菜单栏选择“回放数据”，加载包含 `metadata.yaml` 和
`.db3` 分片的目录即可进行验证、播放、暂停、跳转及 0.1×～8× 调速。

`main` 是当前 C/S v2 主线。此前现场稳定的 ROS2/Qt 单体版本保留在
`legacy/ros2-qt-monolith` 分支，供现场回溯与对照，不再作为后续开发基线。

当前协议为不兼容旧 feature v1.1 的 **AutoViz Protocol 2.2**：Server 最多 20 Hz 发送
完整当前快照，不再使用订阅、ChannelUpdate 或 UPSERT/CLEAR。八条 robot_ws 输入均由
Server Adapter 转换；Client 根据通用规划控制、垂向、水下和平台诊断 capability 组织 UI。

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

## 构建目录约定

禁止在 feature 仓库根目录创建通用 `build/`。三个工程的构建产物必须彼此隔离：

```text
AutoVizProto/build/   CMake 构建目录
AutoVizClient/build/  CMake 构建目录
AutoVizServer/build/  colcon 生成（同时使用 AutoVizServer/install 和 log）
```

- Linux 下的 AutoVizProto SDK 统一由 `AutoVizProto/scripts/bootstrap_proto.sh` 构建和安装。
- Windows 下使用 `AutoVizProto/scripts/bootstrap_proto.ps1`。
- Client 始终在 `AutoVizClient/build` 配置、编译和运行。
- Server 始终从 `AutoVizServer` 目录执行 `colcon build`，不用纯 CMake 构建。

## Linux：准备 AutoVizProto 并构建 Client

在仓库根目录：

```bash
./AutoVizProto/scripts/bootstrap_proto.sh
```

该脚本固定在 `AutoVizProto/build` 构建协议库，再把 SDK 分别安装到 Client 和 Server。
Linux 下不再手工使用其他 Proto 构建目录。随后在 Client 自己的目录中构建：

```bash
cmake -S AutoVizClient -B AutoVizClient/build
cmake --build AutoVizClient/build -j4
./AutoVizClient/build/AutoViz
```

生成 Linux 发布包（含 Qt/protobuf 与 qsqlite 插件）并运行：

```bash
./AutoVizClient/scripts/package_linux.sh
./AutoVizClient/scripts/AutoViz.sh
```

完整的 Client 编译、打包、发布目录和目标机要求见
[`AutoVizClient/README.md`](AutoVizClient/README.md)。

## Linux：为 Server 准备 AutoVizProto

```bash
./AutoVizProto/scripts/bootstrap_proto.sh

source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
cd AutoVizServer
colcon build
```

`colcon` 会在 `AutoVizServer` 内管理 `build/`、`install/` 和 `log/`。不要对 Server
直接执行 `cmake`/`make`，也不要把这些产物放到 feature 根目录。

## Windows Client

Windows 不能使用 Linux 编译出的 `.a`。应使用和 Client 相同的 MSVC/MinGW、架构和
构建类型运行：

```powershell
.\AutoVizProto\scripts\bootstrap_proto.ps1
cmake -S AutoVizClient -B AutoVizClient\build
cmake --build AutoVizClient\build --config Release
```

脚本在 `AutoVizProto\build` 构建，然后仅安装到：

```text
AutoVizClient\third_party\AutoVizProto
```

Client CMake 还会自动搜索自身携带的协议/依赖 SDK：

```text
AutoVizClient\third_party\protobuf
```

Qt 不放入上述搜索列表；Windows Qt6 路径由 Qt Creator Kit、CLion CMake Profile 或命令行
环境提供，Linux Qt5 由系统开发包提供。

本地 rosbag 回放使用 Qt SQL 的 SQLite 驱动。部署时除 Qt Widgets/Network 外，还需随
应用携带 `plugins/sqldrivers/qsqlite.dll` 及其 SQLite 运行时依赖；不需要安装 ROS2。

Windows Client 固定使用 Qt6，Linux Client 固定使用 Qt5。Qt 与编译器由当前 IDE Kit/
Toolchain 或命令行 CMake 环境提供；Client CMake 不选择 Qt 安装或编译器。protobuf 与
AutoVizProto SDK 仍必须和当前平台、架构及编译器 ABI 匹配。完整命令见
`AutoVizClient/README.md`。

当前 Windows 基线为 Qt 6.10、Qt Kit 自带 MinGW 13.1、protobuf 35.1 静态库。protobuf
直接安装在无空格路径 `D:\protobuf-35.1-mingw1310`，以兼容 MinGW 资源编译器。不得把
MSYS2 UCRT64 编译的 protobuf/Abseil DLL 混入该基线。

切换到 Windows 前请只复制源码和对应 Windows SDK；不要复制 Linux 的 `build/` 或 Linux
静态库。Client 的应用图标已内嵌在 `AutoVizClient/assets/autoviz_icon.png`，主题和车辆尺寸
配置由构建过程复制到可执行文件旁的 `configs/`。

## 协议命名

唯一 schema 位于 `AutoVizProto/proto/autoviz/*.proto`，全部声明
`package autoviz;`。生成头路径是 `autoviz/*.pb.h`，C++ 类型直接是
`autoviz::Envelope` 等。

详细说明：

- `AutoVizProto/README.md`
- `AutoVizClient/README.md`
- `AutoVizServer/README.md`
- `memory/ARCHITECTURE.md`
- `memory/PROTOCOL_DESIGN.md`
- `memory/VALIDATION.md`
- `AutoVizServer/docs/BOOST_ASIO_GUIDE.md`
