# AutoVizClient

AutoVizClient 是纯 Qt Client，不依赖 ROS。它既可连接 AutoVizServer，也可直接回放
robot_ws ROS2 bag。AutoVizProto、protobuf 和 Qt 都可按固定布局
放入工程自己的 `third_party/`，CMake 会自动搜索，不要求系统环境变量。

## third_party 布局

```text
AutoVizClient/
  third_party/
    AutoVizProto/
      include/
      lib/
    protobuf/       # 本地 protobuf SDK，可选
      include/
      lib/
    Qt6/            # Windows 优先使用的 Qt kit 根目录或目录链接，可选
      include/
      lib/
    Qt5/            # Qt6 不可用时的兼容回退，例如 Linux
      include/
      lib/
```

AutoVizProto 必须存在。protobuf/Qt6/Qt5 若已能被工具链正常找到，可以不复制；若希望工程
完全使用固定依赖，则放到上述目录。

## Linux

从仓库根目录运行 Linux 协议引导脚本：

```bash
./AutoVizProto/scripts/bootstrap_proto.sh
```

该脚本只使用 `AutoVizProto/build`。然后 Client 无需附加 AutoVizProto 参数，
并且必须使用自己的 `AutoVizClient/build`：

```bash
cmake -S AutoVizClient -B AutoVizClient/build
cmake --build AutoVizClient/build -j4
./AutoVizClient/build/AutoViz
```

禁止在 feature 仓库根目录创建 `build/`，也不将 Client 配置到 `build/client`。

## Windows

在仓库根目录运行 Windows 引导脚本。它会自动使用
`AutoVizClient\third_party\protobuf` 中的 protobuf SDK（若该目录存在）：

```powershell
.\AutoVizProto\scripts\bootstrap_proto.ps1
```

Windows 优先使用 Qt6，找不到 Qt6 时才回退 Qt5。Client 必须在 IDE 中选择与 Qt kit
相同的 MinGW Kit；CMake 不会再自动选择 MSYS2 的 UCRT64 工具链。

建议优先统一为 MSYS2 UCRT64：它已提供 protobuf、编译器和一套一致的 GCC runtime；
只需安装 Qt6 包即可，不需要手工编译 protobuf：

```powershell
C:\msys64\usr\bin\bash.exe -lc 'pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base'
```

随后在 IDE 中选择 `C:\msys64\ucrt64\bin\g++.exe`，直接 Reload CMake 即可。
Windows Client 默认从 `C:\msys64\ucrt64` 查找 Qt6、protobuf 与插件；此时 Qt 与
`libprotobuf.dll` 来自同一个 Kit，可以安全打包。若 MSYS2 安装在其他位置，才需要将
`AUTOVIZ_QT_ROOT`、`AUTOVIZ_QT_PLUGINS_DIR` 和 `CMAKE_PREFIX_PATH` 指向该 Kit。

若坚持使用 Qt Online Installer 的 `mingw_64` Kit，则 MSYS2 UCRT64 发行的
`libprotobuf.dll` 不能作为 Client 运行依赖；需要改用**由该 Qt Kit 的 MinGW 构建的
protobuf SDK**（可放到 `AutoVizClient\\third_party\\protobuf`），再用同一编译器构建
AutoVizProto 和 Client。

若采用 Qt Online Installer，Qt kit 位于 `D:\Qt6.10\6.10.0\mingw_64` 时：

```powershell
$QtRoot = 'D:\Qt6.10\6.10.0\mingw_64'
$QtCompiler = 'D:\Qt6.10\Tools\mingw1310_64\bin\g++.exe'
.\AutoVizProto\scripts\bootstrap_proto.ps1 `
  -CxxCompiler $QtCompiler `
  -CMakePrefixPath E:\SDK\protobuf-qt-mingw
cmake -S AutoVizClient -B AutoVizClient\build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=$QtCompiler `
  "-DCMAKE_PREFIX_PATH=$QtRoot;E:\SDK\protobuf-qt-mingw"
cmake --build AutoVizClient\build --parallel
```

### CLion

CLion/Qt Creator 都可以使用；选择 Qt 对应的 MinGW Kit 后，把 `CMAKE_PREFIX_PATH` 设为
`<QtRoot>;<protobuf-sdk-root>`，并把 `AUTOVIZ_QT_ROOT` 设为 `<QtRoot>`。Linux 固定使用
Qt5。切换 Kit 后，必须删除 `AutoVizProto/build` 和 `AutoVizClient/build`，再用新 Kit
重新配置；一个 Client 构建目录内不可混用工具链。

Qt kit 可放到 `AutoVizClient\third_party\Qt6`（或 Qt5 回退目录），或在对应位置创建指向实际 Qt kit 的
目录链接。之后直接构建：

```powershell
cmake -S AutoVizClient -B AutoVizClient\build
cmake --build AutoVizClient\build --config Release
.\AutoVizClient\build\Release\AutoViz.exe
```

构建后可先用一个真实 bag 做无界面链路检查（将路径替换为 Windows 上的实际目录）：

```powershell
$Bags = Get-ChildItem D:\data\rosbag -Directory -Filter "rosbag2_*" |
  ForEach-Object FullName
& .\AutoVizClient\build\Release\AutoVizClientPlaybackTests.exe $Bags
.\AutoVizClient\build\Release\AutoVizPlaybackSourceSmoke.exe `
  D:\data\rosbag\rosbag2_2026_08_12-03_00_17
```

然后启动 `AutoViz.exe`，人工检查“回放数据”、播放/暂停、跳转、0.1×～8× 调速、图标和
`configs` 目录入口；再连接 AutoVizServer 检查 TCP 断线与重连。Windows 待验收项同步记录在
`memory/TODO.md`。

这里没有设置 `AutoVizProto_DIR`、`Qt5_DIR` 或系统环境变量。若 third_party 实际放在
其他位置，可显式传 `-DAUTOVIZ_THIRD_PARTY_DIR=C:\your\sdk` 覆盖默认目录。

注意：

- Windows 不能使用 Linux 生成的 `.a`，必须使用对应工具链的 `.lib`。
- AutoVizProto、protobuf 和 Client 应使用相同架构（例如全部 x64）。
- MSVC Runtime、Debug/Release 也应一致。
- 不要复制 Linux 的构建产物；Windows Proto 使用 `AutoVizProto\build`，Client 使用
  `AutoVizClient\build`。Client 构建后会自动把 `configs/` 复制到 `AutoViz.exe` 旁。
  其中含车辆尺寸 JSON 和浅色主题 QSS；当前固定使用已验收的浅色 UI 基线，暂不提供
  深色切换，避免 Qt5 全局 QSS 换肤造成界面卡顿或局部控件色彩不一致；窗口图标来自已内嵌的
  `assets/autoviz_icon.png`，无需复制外置图标。

## 数据流与边界

```text
QTcpSocket
  -> autoviz::FrameDecoder
  -> autoviz::Envelope
  -> ProtocolModelConverter
  -> DataManager
  -> SceneManager / Qt UI
```

Client 不保存 `.proto`、不运行 protoc、不包含 FrameCodec 副本。回放验证程序是普通测试
可执行文件，不引入 CTest/GTest。
协议测试统一在 AutoVizProto。

默认连接 `127.0.0.1:39090`。当前协议为 v2 完整快照：ClientHello 完成后接收
ServerHello、VisualizationSnapshot、Heartbeat 和 ProtocolError，不发送订阅，也不处理
ChannelUpdate。断线或新 session 会清空全部远程数据和历史轨迹；同一 session 内
DataManager 原子替换快照并延续历史轨迹。

Client 的业务状态按内部 `VisualizationChannel`/capability 匹配；ROS topic、DDS reader
或日志字段名称只作为诊断文本显示，UI 不解析这些来源字符串。通用项目始终使用 XY、
轨迹、障碍物和控制曲线；只有 Server 声明垂向、水下或平台诊断 capability 时，相关视图
和状态区域才启用。所有 protobuf 字段先经 `ProtocolModelConverter` 转为内部模型，
UI/SceneManager 不直接读取 protobuf。

## 当前 Client UI 行为

- 运动总览固定为六张状态卡片的 3×2 布局，连接状态和 capability 只更新卡片内容，不改变位置。
- 控制曲线保留实际收到的未使能控制命令，便于观察 rosbag 回放和遥控/自主切换；`enabled`
  仍在状态区表达执行状态。
- “主视图显示管理”位于“视图”菜单；“文件”菜单可打开运行时 `configs/` 目录。
- 菜单栏“回放数据”可加载 ROS2 Humble rosbag2 metadata v5 + SQLite3 bag。验证在工作线程
  完成，检查 metadata、全部分片、SQLite quick_check、topic/type/CDR 和所有受支持消息。
- 回放提供开始、暂停/继续、停止、进度跳转，以及主视图右上角 0.1/0.25/0.5/1/2/4/8×
  分档滑块和 0.10～8.00 连续倍率输入。开始回放会断开远程 Server；重新连接 Server 会
  停止本地回放。

## 本地 ROS2 Bag 兼容范围

首版内置 ROS-free 的 CDR 解码器，支持 `/location`、`/targets/final_objects`、
`/chassis_command`、`/chassis_states`、`/system_run_states`、`/task_params`、
`/local_path` 和 `/global_path`。障碍物以及其他缺失通道只产生警告；至少一个受支持通道
即可播放。解码器同时兼容 2026-08-06 前含 `dive_speed` 的 72 字节 ChassisCommand 与当前
64 字节布局。

bag 本身不携带 custom_msgs 完整定义，因此该功能不宣称支持任意 ROS2 自定义消息；消息
布局变化时必须同步更新 `core/playback/RobotWsCdrDecoder`。多 DB3 分片按时间合并读取，
不会把整个 bag 放入内存。

Linux 验证命令：

```bash
cmake --build AutoVizClient/build -j4
./AutoVizClient/build/AutoVizClientPlaybackTests /home/wqx/LZBK/data/rosbag/rosbag2_*
./AutoVizClient/build/AutoVizPlaybackSourceSmoke \
  /home/wqx/LZBK/data/rosbag/rosbag2_2026_08_12-03_00_17
```

Windows 开发环境的 `PATH` 只需包含所选 Qt6/Qt5 kit 的 `bin` 目录；无需 ROS2、WSL 或
AutoVizServer。该目录必须与 Qt、protobuf 和编译器属于同一个 Kit，不能混用。

## Windows 打包

Windows 发布版使用 MSYS2 UCRT64 的 Qt6、protobuf 和 MinGW runtime。先确保已安装：

```powershell
C:\msys64\usr\bin\bash.exe -lc 'pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-protobuf'
```

首次打包或切换过 IDE Kit 时，在仓库根目录执行以下完整流程。`$PackageRoot` 可以改成
任意你希望存放发布包的**空目录**，例如移动硬盘、交付目录或版本归档目录；不会写到工程根
目录，也不要求发布目录位于仓库中。

```powershell
$UcrtRoot = 'C:\msys64\ucrt64'
$env:PATH = "$UcrtRoot\bin;$env:PATH"

# 1. 用同一 Kit 构建并安装协议 SDK 到 Client third_party。
.\AutoVizProto\scripts\bootstrap_proto.ps1 `
  -CxxCompiler "$UcrtRoot\bin\g++.exe" `
  -CMakePrefixPath $UcrtRoot

# 2. 配置并构建 Client。日常开发可由 CLion / Qt Creator 代替这一步。
cmake -S AutoVizClient -B AutoVizClient\build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER="$UcrtRoot\bin\g++.exe" `
  -DCMAKE_PREFIX_PATH=$UcrtRoot `
  -DAUTOVIZ_QT_ROOT=$UcrtRoot `
  -DAUTOVIZ_QT_PLUGINS_DIR="$UcrtRoot\share\qt6\plugins"
cmake --build AutoVizClient\build --parallel

# 3. 将完整、可双击运行的发布包安装到你指定的位置。
$PackageRoot = 'E:\Release\AutoViz-UCRT64'
cmake --install AutoVizClient\build --prefix $PackageRoot

# 4. 验证发布包。无需 Qt、MSYS2 或 IDE 的 PATH。
& "$PackageRoot\bin\AutoViz.exe"
```

如果 Client 已由 IDE 以 UCRT64 Kit 成功构建，只需要重复第 3 步即可重新打包到另一个目录：

```powershell
$PackageRoot = 'F:\交付\AutoViz-2026-08-14'
cmake --install AutoVizClient\build --prefix $PackageRoot
```

发布目录结构如下；交付或复制时应整体保留 `bin/`，不要只拿走 `AutoViz.exe`：

```text
<PackageRoot>/
  bin/
    AutoViz.exe
    qt.conf                    # 让双击时定位包内 Qt 插件
    *.dll                      # Qt、protobuf、UCRT64 runtime 及其传递依赖
    configs/
    plugins/
      platforms/qwindows.dll
      sqldrivers/qsqlite.dll
    log/
      autoviz-YYYYMMDD.log     # 程序启动、连接、告警和错误日志
```

程序以 Windows GUI 子系统发布，资源管理器双击不会显示控制台。运行日志优先写入
`<PackageRoot>\bin\log`；只有发布目录不可写时，才回退到用户本地应用数据目录。
`cmake --install` 不会删除旧文件，若要制作干净的新版本，请把 `$PackageRoot` 指向新建的
空目录。Windows 可执行文件使用 `assets/autoviz_icon.ico` 作为资源管理器图标；Qt 窗口图标
继续使用 `resources.qrc` 中的 PNG 资源。

## Linux 打包状态

Linux 下的 AutoVizProto、Client 和 Server 构建，以及 Client 运行链路已经验收；但 Client
的可分发打包和在干净 Linux 目标环境中的部署尚未验证。当前不要把 Windows 的 UCRT64 发布
目录用于 Linux，也不要将任何本机 `package/`、`build/` 或第三方二进制提交到 Git。

后续需根据目标发行版和 Qt5/protobuf 的部署策略确定 Linux 发布流程，并至少验证应用启动、
Qt SQLite 插件、本地 bag 回放和 TCP 断线重连。
