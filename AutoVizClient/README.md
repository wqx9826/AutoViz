# AutoVizClient

AutoVizClient 是纯 Qt 桌面程序：可通过 TCP 连接 AutoVizServer，也可直接回放 robot_ws 的
ROS2 Humble SQLite3 bag。运行 Client 不需要 ROS2。

控制状态详情分别显示 Action 期望模式、控制节点下发模式和底盘执行反馈，并附 ROS 接收/rosbag
记录时间、topic 序号、刷新年龄和过期状态。当前 robot_ws 三个相关消息没有发布端 Header，
因此发布端时间与源到 Server 接收延迟会显示为“不可用”。

运动总览中的数据来源是固定契约：

- “控制指令”中的 cmd 速度、航向、角速度、模式、档位和使能只来自
  `/chassis_command`；速度、航向和角速度的 rev 只来自 `/location`，档位反馈只来自
  `/chassis_states`。Action 期望值只在 Action 详情中显示，不作为 cmd 或 rev 回退值。
- “当前运动”同时显示“爬行速度/爬行角速度”（`/chassis_states` 履带控制器反馈）和
  “航行速度/角速度 omega_z”（`/location` 定位反馈）；当前航向也来自 `/location`。
- 左侧控制曲线的目标值只取 `/chassis_command`，反馈值只取 `/location`。航行模式绘制航向角
  `°`，爬行模式绘制角速度 `°/s`；模式切换时不会把两类物理量拼接到同一曲线段。命令和
  定位反馈在各自 topic 序号或消息时间变化时追加，50 ms UI 刷新只重绘，不生成采样点。

本地回放按 bag 消息顺序处理控制状态和路径。进入 mode 10/11 时立即清除全局/局部路径，
退出后只显示新到达的路径；暂停、调速和 seek 不会让进度位置领先已应用的控制状态。
“详细信息 -> 控制时序”同时显示三条控制来源的接收时间、序号、年龄、过期状态与当前 session
的模式/档位/使能/履带输出事件；页面提供独立纵向滚动，在底部面板较矮时仍可查看完整事件表。
其中“控制状态时间关联”表按数据来源、当前值和 Goal UUID 的长文本优先分配宽度，序号、刷新年龄
等短字段使用紧凑固定宽度；窄窗口由表格自身横向滚动，不覆盖单元格内容。
Server 的 20 Hz 完整快照或高倍率回放可能在一次 Client 刷新之间经过多个控制状态；控制时序
页面保留来源事件历史，但运动总览和控制指令只显示当前完整快照，不为短暂 mode=0 增加 UI
补偿队列、停留计时或“无效（切换）”展示状态。

远程 Server 与本地 bag 由 `DataManager` 的活动数据源所有权强制互斥。开始选择 bag 时先把
所有权切到 `Ros2Bag`，再异步断开 Server；切回 Server 时先停止 bag session，再把所有权切到
`Remote`。不属于当前活动来源的延迟 snapshot 和 reset 会被拒绝，因此开启 `auto_connect`
也不会让迟到的远程 mode 覆盖本地回放状态。每个本地快照携带单调序号，控制总览、控制指令和
控制时序在一次 50 ms UI 刷新中共同使用同一份命令状态与 topic 序号。

本文只说明 Client 的编译、运行和 Linux 打包。所有命令均从**仓库根目录**
`AutoViz/` 执行；不要在仓库根目录创建 `build/`。

## 目录与产物

```text
AutoVizClient/
  build/       # CMake 编译目录：可删除后重新配置，不提交
  package/
    AutoViz-Linux/  # Linux 发布包：打包脚本生成，不提交
  scripts/
    AutoViz.sh          # 启动 package/AutoViz-Linux/ 内的发布程序
    package_linux.sh    # 生成 package/AutoViz-Linux/
    package_source.sh   # 生成含完整 Protocol 源码的离线源码包
    package_windows.ps1 # 增量构建并生成 Windows 发布包
```

`AutoVizClient/build/AutoViz` 是开发构建产物；`AutoVizClient/package/AutoViz-Linux/` 才是可复制的 Linux
发布目录。不要直接运行发布包中的 `AutoViz.bin`，应使用 `scripts/AutoViz.sh` 或
`package/AutoViz-Linux/bin/AutoViz`。

## Linux：首次编译

### 1. 初始化 Protocol 源码

```bash
git submodule update --init --recursive
./scripts/verify_protocol_submodules.sh
```

`AutoVizClient/third_party/AutoVizProto` 是锁定到 `v2.3.0` 的源码 submodule。Client
CMake 会在自己的 build 目录中运行 protoc 并编译 Protocol，不需要提前构建或安装 SDK。

### 2. 配置并编译 Client

```bash
cmake -S AutoVizClient -B AutoVizClient/build -DCMAKE_BUILD_TYPE=Release
cmake --build AutoVizClient/build --parallel
```

Linux 固定使用 Qt5。配置阶段直接查找 Core、Widgets、Network 和 Sql；需要
可用的 C++17 编译器、CMake、Qt5 开发包和 protobuf 开发包。

### 3. 运行开发版本

```bash
./AutoVizClient/build/AutoViz
```

开发版本会连接默认地址 `127.0.0.1:39090`；可在程序中改为实际 Server 地址，或通过菜单
“回放数据”打开 bag。

## Linux：打包与运行发布版

完成上述编译后执行：

```bash
./AutoVizClient/scripts/package_linux.sh
```

脚本会先增量编译，然后**删除并重建** `AutoVizClient/package/AutoViz-Linux/`，避免旧 Qt 库或插件残留。
生成结构如下：

```text
AutoVizClient/package/AutoViz-Linux/
  bin/
    AutoViz                 # 安装后的启动器
    AutoViz.bin             # Qt 主程序，不直接启动
    configs/                # 车辆参数与主题
    plugins/platforms/      # qxcb、qoffscreen
    plugins/sqldrivers/     # qsqlite，bag 回放需要
    qt.conf
  lib/                      # Qt5、Qt5XcbQpa、protobuf
```

在开发机验证或交付后运行：

```bash
./AutoVizClient/scripts/AutoViz.sh
# 或：cd AutoVizClient/package/AutoViz-Linux/bin && ./AutoViz
```

整体复制 `package/AutoViz-Linux/` 即可交付，目录名可以改；如果只复制发布包，目标机从
`<发布包>/bin/AutoViz` 启动。`scripts/AutoViz.sh` 仅适用于仓库内固定的
`package/AutoViz-Linux/` 位置。

该包面向相同 CPU 架构和兼容 ABI 的 Linux 发行版（当前验证基线：Ubuntu 22.04、Qt 5.15、
protobuf 3.12）。它包含 Qt/protobuf 与所需插件，但不包含 glibc、显示服务器、显卡驱动、
X11/Wayland 基础库；目标机必须提供这些系统组件。

## 离线源码包

从开发仓库生成可独立复制的源码包：

```bash
./AutoVizClient/scripts/package_source.sh
```

输出位于 `AutoVizClient/package/source/`。包内的
`third_party/AutoVizProto` 是完整源码而不是 gitlink，并带 `SOURCE_MANIFEST.txt` 记录
Protocol 版本和 commit。目标机无需 Git 或网络，解压后直接执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Linux：可选验证

真实 bag 可验证 SQLite 插件与原生回放链路：

```bash
./AutoVizClient/build/AutoVizClientPlaybackTests /path/to/rosbag2_*
./AutoVizClient/build/AutoVizPlaybackSourceSmoke /path/to/rosbag2_xxx
```

发布包已在清空开发环境变量的本机进程中完成启动及 qsqlite bag 验证；仍需在一台未安装
Qt/protobuf 开发环境的 Linux 目标机进行真实桌面显示和真实 Server 重连验收。

## Windows：编译与打包

Windows 固定使用 Qt6。Qt 路径和编译器完全由 Qt Creator Kit、CLion Toolchain 或命令行
CMake 参数提供；Client 的 `CMakeLists.txt` 不选择编译器，也不写死或探测 Qt/MSYS2/IDE
安装路径。切换 Kit 后必须清除该 Kit 原有的 CMake 构建目录/缓存，并为新 Kit 提供 ABI
匹配的 protobuf SDK。AutoVizProto 是随 Client 编译的源码，不再有独立 ABI SDK。不要将
Linux 的 `build/`、`package/` 或 `.so/.a`
文件带到 Windows。

### 方案 1（当前基线）：Qt Creator Qt 6.10 / MinGW 13.1

当前 Windows 开发环境以 Qt Online Installer 安装的 Qt Creator Kit 为基准：

```text
Qt               D:\Qt6.10\6.10.0\mingw_64
C/C++ 编译器     D:\Qt6.10\Tools\mingw1310_64\bin\gcc.exe / g++.exe
Ninja            D:\Qt6.10\Tools\Ninja\ninja.exe
protobuf         D:\protobuf-35.1-mingw1310
```

protobuf 35.1 使用上述 MinGW 13.1 编译为静态库，并直接安装在无空格路径
`D:\protobuf-35.1-mingw1310`。该路径可避免 MinGW `windres` 编译 `assets/autoviz.rc` 时
错误拆分带空格的传递 include 路径。

当前用户环境已配置为：

```text
Protobuf_ROOT=D:\protobuf-35.1-mingw1310
CMAKE_PREFIX_PATH=D:\Qt6.10\6.10.0\mingw_64;D:\protobuf-35.1-mingw1310
Qt6_ROOT=D:\Qt6.10\6.10.0\mingw_64
Path 前端依次为独立 CMake、Qt Ninja、Qt MinGW 13.1、protobuf bin
```

独立安装的 CMake 可以作为 Windows 默认 CMake；编译器则固定使用 Qt Kit 自带的
MinGW 13.1。不要以另一套独立 MinGW 替换它，除非 Qt、protobuf 和 Client 全部用那一套
编译器重新构建。

修改用户环境变量后必须完全退出并重启 Qt Creator、CLion 和终端。首次构建前确认
`AutoVizClient\third_party\AutoVizProto\VERSION` 存在；配置 Client 时会自动生成并编译
protobuf C++ 源码。

在新打开的 PowerShell 中，以下是 Windows 的基础编译方式；它直接使用
`AutoVizClient\build` 根目录，已验证可用：

```powershell
Set-Location E:\Coding\AutoViz\AutoVizClient\build
cmake -G "MinGW Makefiles" .. -DCMAKE_BUILD_TYPE=Release
mingw32-make.exe -j 8
```

完成上述构建后，标准的发布命令为：

```powershell
cmake --install . --prefix ..\package\AutoViz-Qt6-MinGW
```

`scripts\package_windows.ps1` 仅封装上述 `cmake --build`/`cmake --install`；它不配置
CMake、Qt 或编译器，因此首次运行前 `build` 中必须已有 `CMakeCache.txt`。当前默认输出目录
为 `package\AutoViz-Qt6-MinGW`。

首次从 MSYS2、Ninja、Visual Studio 或其他编译器切换到该流程时，必须先清除
`AutoVizClient\build` 根目录的 CMake 缓存和生成文件；不同生成器、不同 MinGW 不能共享
同一个构建目录。新终端中可用下列命令确认工具链：

```powershell
Get-Command cmake.exe, g++.exe, mingw32-make.exe
```

它们应分别来自 `D:\Program Files\CMake\bin`、
`D:\Qt6.10\Tools\mingw1310_64\bin` 和同一 Qt MinGW 目录，而不是 `C:\msys64`。

Qt Creator 的 Kit 选择上述 Qt 6.10、GCC/G++ 13.1、Ninja 和 CMake。首次切换时删除该 Kit
旧构建目录，或执行“清除 CMake 配置”后重新配置。Qt Creator 会从 Kit 提供 Qt 路径，并从
用户环境找到 protobuf；不需要给项目增加 Qt 路径变量。

CLion 没有 Qt Creator 的 Qt Kit 概念，因此只需一次性完成以下设置：

- Toolchain 的 C/C++ 编译器选择上述 `gcc.exe`/`g++.exe`，生成器选择 Ninja；
- CMake Profile 增加 `-DCMAKE_PREFIX_PATH=D:/Qt6.10/6.10.0/mingw_64`；
- 使用位于 `AutoVizClient/build/` 下的独立构建目录，不与 Qt Creator 共用缓存。

IDE 会保存这些设置。当前用户环境已提供 Qt/protobuf 前缀；若在没有这些环境变量的其他
终端中进行独立验证，可显式提供 Qt 前缀：

```powershell
$QtRoot = 'D:\Qt6.10\6.10.0\mingw_64'
$ProtoRoot = [Environment]::GetEnvironmentVariable('CMAKE_PREFIX_PATH', 'User')
$env:CMAKE_PREFIX_PATH = "$QtRoot;$ProtoRoot"

cmake -S AutoVizClient -B AutoVizClient\build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build AutoVizClient\build --parallel

$PackageRoot = 'E:\Coding\AutoViz\AutoVizClient\package\AutoViz-Qt6-MinGW'
cmake --install AutoVizClient\build --prefix $PackageRoot
```

当前 protobuf 为静态库，因此 `AutoViz.exe` 不依赖 `libprotobuf.dll` 或 Abseil DLL；发布目录
仍需包含 Qt6、MinGW runtime、`qwindows.dll`、`qsqlite.dll`、配置和 `qt.conf`，这些内容由
`cmake --install` 收集。

### 备选方案：原 MSYS2 UCRT64 手动流程（保留）

下面保留原有 MSYS2 流程，适用于整套依赖均由 UCRT64 构建的独立 ABI 环境。它不能与上面
Qt MinGW 13.1 编译的 protobuf 或 Client 构建目录混用。

#### 1. 安装官方依赖

在 PowerShell 中执行：

```powershell
C:\msys64\usr\bin\bash.exe -lc 'pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-protobuf'

$UcrtRoot = 'C:\msys64\ucrt64'
$env:PATH = "$UcrtRoot\bin;$env:PATH"
$env:CMAKE_PREFIX_PATH = $UcrtRoot
```

#### 2. 用 MinGW Makefiles 配置并多线程编译 Release Client

```powershell
$ProjectRoot = 'E:\Coding\AutoViz\AutoVizClient'
# 若此前使用过 Ninja、Visual Studio 或其他生成器，先清理该专用构建目录。
Remove-Item -Recurse -Force "$ProjectRoot\build" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$ProjectRoot\build" | Out-Null
Set-Location "$ProjectRoot\build"
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make.exe -j $env:NUMBER_OF_PROCESSORS
```

`-j 8` 表示最多同时编译 8 个任务；请按 CPU 的逻辑核心数调整，例如 4 核/8 线程可用
`-j 8`，也可使用 `-j $env:NUMBER_OF_PROCESSORS` 自动取 Windows 报告的逻辑核心数：

```powershell
mingw32-make.exe -j $env:NUMBER_OF_PROCESSORS
```

也可以在仓库根目录执行与上述 `mingw32-make` 等价的 CMake 通用命令：

```powershell
cmake --build AutoVizClient\build --parallel $env:NUMBER_OF_PROCESSORS
```

#### 3. 生成发布目录并启动

`$PackageRoot` 必须是新建的空目录；发布时整体复制该目录，而不是只复制 `AutoViz.exe`。

```powershell
$ProjectRoot = 'E:\Coding\AutoViz\AutoVizClient'
# 清空 package 目录
Remove-Item -Recurse -Force "$ProjectRoot\package" -ErrorAction SilentlyContinue
$PackageRoot = "$ProjectRoot\package\AutoViz-MinGW"
cmake --install "$ProjectRoot\build" --prefix $PackageRoot
& "$PackageRoot\bin\AutoViz.exe"
```

正常构建只定义 Client targets；安装、Qt/protobuf 运行库和插件收集规则位于
`AutoVizClient/cmake/InstallRules.cmake`，仅在执行 `cmake --install` 时生效。

#### 4. 使用脚本增量构建并打包

调试时可继续用 CLion 或 Qt Creator 配置、构建和运行 `AutoVizClient/build/`。以下脚本不重新
配置 CMake、不删除 IDE 的构建缓存；它只对该目录执行 Release 增量构建，然后删除并重建
`package/AutoViz-MinGW/`。

```powershell
# 从仓库根目录执行；-Run 会在打包完成后启动程序。
.\AutoVizClient\scripts\package_windows.ps1
.\AutoVizClient\scripts\package_windows.ps1 -Run
```

脚本默认以 Windows 报告的逻辑核心数并行构建；可按需限制并行数，例如
`.\AutoVizClient\scripts\package_windows.ps1 -Parallel 8`。首次运行前，`AutoVizClient/build/`
必须已由当前统一的 Qt MinGW 13.1、Qt6 和 protobuf 配置完成；脚本会检查其中的
`CMakeCache.txt`，避免误把发布操作指向未配置的目录。

发布目录包含 `bin/AutoViz.exe`、Qt/protobuf/MinGW DLL、`configs/`、`plugins/platforms/qwindows.dll`、
`plugins/sqldrivers/qsqlite.dll` 和 `qt.conf`。`qsqlite.dll` 是本地 bag 回放所必需的。

若使用 Qt Online Installer 而不是 MSYS2，必须改用该 Qt Kit 的同一 MinGW 与同一 ABI 的
protobuf SDK；不可混用 MSYS2 UCRT64 的 `libprotobuf.dll`。

## qt.conf 的作用

`qt.conf` 是 Qt 程序启动时读取的路径配置。包内的内容为：

```ini
[Paths]
Prefix = .
Plugins = plugins
```

它告诉 Qt：以可执行文件所在的 `bin/` 为根，插件目录是 `bin/plugins/`。因此 Qt 能找到
`platforms` 里的桌面平台插件以及 `sqldrivers/qsqlite`，不需要在目标机设置
`QT_PLUGIN_PATH` 或依赖开发机 Qt 安装路径。

现在仅保留 `AutoVizClient/scripts/qt.conf`：CMake 在 Linux 和 Windows 安装时都从该唯一来源
复制到发布包的 `bin/qt.conf`。旧的 `AutoVizClient/deploy/qt.conf` 是未被引用的重复文件，已删除。

## 运行边界

- Client 不依赖 ROS2；bag 回放只支持当前 robot_ws 的固定消息布局。
- Client 与 Server 的默认 TCP 端口为 `39090`，Server 默认最多接受 8 个 Client。
- 本地回放和远程连接由 `DataManager` 活动来源强制互斥；异步断开/重连产生的迟到写入会被拒绝。
- 远程 Server 快照和本地 bag 解码必须提供相同的 Client 可见字段、语义、单位及字段缺失行为，
  并统一经 `ProtocolModelConverter -> DataManager -> UI`；任何字段不得只在其中一种数据源显示。
- 主题、车辆尺寸等运行时文件位于可执行文件同级 `configs/`，交付时必须整体保留。
