# AutoViz

AutoViz 的 `feature/client-server` 包含三个工程：

```text
AutoVizProto   唯一协议源码，构建后作为第三方 SDK 使用
AutoVizServer  ROS2 -> protobuf/TCP
AutoVizClient  protobuf/TCP -> Qt UI
```

AutoVizClient 还可在不启动 Server、ROS2、WSL 或虚拟机的情况下，直接打开 robot_ws
录制的 ROS2 Humble `sqlite3` bag。菜单栏选择“回放数据”，加载包含 `metadata.yaml` 和
`.db3` 分片的目录即可进行验证、播放、暂停、跳转及 0.1×～8× 调速。

`main` 仍是现场稳定的 ROS2 单体版本，本分支不会修改 main。

当前协议为不兼容旧 feature v1.1 的 **AutoViz Protocol 2.0**：Server 最多 20 Hz 发送
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

脚本在 `AutoVizProto\build` 构建，然后安装到：

```text
AutoVizClient\third_party\AutoVizProto
```

Client CMake 还会自动搜索：

```text
AutoVizClient\third_party\protobuf
AutoVizClient\third_party\Qt5
```

本地 rosbag 回放使用 Qt SQL 的 SQLite 驱动。部署时除 Qt Widgets/Network 外，还需随
应用携带 `plugins/sqldrivers/qsqlite.dll` 及其 SQLite 运行时依赖；不需要安装 ROS2。

因此可把 protobuf SDK、Qt kit（或指向 Qt kit 的目录链接）放到固定位置，不依赖
系统环境变量。完整命令见 `AutoVizClient/README.md`。

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
