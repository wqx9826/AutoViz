# AutoVizClient

AutoVizClient 是纯 Qt 桌面程序：可通过 TCP 连接 AutoVizServer，也可直接回放 robot_ws 的
ROS2 Humble SQLite3 bag。运行 Client 不需要 ROS2。

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
    package_windows.ps1 # 增量构建并生成 Windows 发布包
```

`AutoVizClient/build/AutoViz` 是开发构建产物；`AutoVizClient/package/AutoViz-Linux/` 才是可复制的 Linux
发布目录。不要直接运行发布包中的 `AutoViz.bin`，应使用 `scripts/AutoViz.sh` 或
`package/AutoViz-Linux/bin/AutoViz`。

## Linux：首次编译

### 1. 准备协议 SDK

```bash
./AutoVizProto/scripts/bootstrap_proto.sh
```

该命令在 `AutoVizProto/build/` 构建协议库，并安装 Linux SDK 到
`AutoVizClient/third_party/AutoVizProto/`。只需在协议源码、编译器或 protobuf 变更后重做。

### 2. 配置并编译 Client

```bash
cmake -S AutoVizClient -B AutoVizClient/build -DCMAKE_BUILD_TYPE=Release
cmake --build AutoVizClient/build --parallel
```

Linux 当前使用 Qt5。配置阶段会自动从当前 Qt kit 查找 Widgets、Network、Sql 和插件；需要
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

## Linux：可选验证

真实 bag 可验证 SQLite 插件与原生回放链路：

```bash
./AutoVizClient/build/AutoVizClientPlaybackTests /path/to/rosbag2_*
./AutoVizClient/build/AutoVizPlaybackSourceSmoke /path/to/rosbag2_xxx
```

发布包已在清空开发环境变量的本机进程中完成启动及 qsqlite bag 验证；仍需在一台未安装
Qt/protobuf 开发环境的 Linux 目标机进行真实桌面显示和真实 Server 重连验收。

## Windows：编译与打包

Windows 发布包必须使用同一个工具链构建 Qt、protobuf、AutoVizProto 和 Client。推荐 MSYS2
UCRT64 Qt6；不要将 Linux 的 `build/`、`package/` 或 `.so/.a` 文件带到 Windows。

### 1. 安装依赖并准备协议 SDK

在 PowerShell 中执行：

```powershell
C:\msys64\usr\bin\bash.exe -lc 'pacman -S --needed mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-protobuf'

$UcrtRoot = 'C:\msys64\ucrt64'
$env:PATH = "$UcrtRoot\bin;$env:PATH"
.\AutoVizProto\scripts\bootstrap_proto.ps1 `
  -CxxCompiler "$UcrtRoot\bin\g++.exe" `
  -CMakePrefixPath $UcrtRoot
```

### 2. 用 MinGW Makefiles 配置并多线程编译 Release Client

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

### 3. 生成发布目录并启动

`$PackageRoot` 必须是新建的空目录；发布时整体复制该目录，而不是只复制 `AutoViz.exe`。

```powershell
$ProjectRoot = 'E:\Coding\AutoViz\AutoVizClient'
# 清空 package 目录
Remove-Item -Recurse -Force "$ProjectRoot\package" -ErrorAction SilentlyContinue
$PackageRoot = "$ProjectRoot\package\AutoViz-UCRT64"
cmake --install "$ProjectRoot\build" --prefix $PackageRoot
& "$PackageRoot\bin\AutoViz.exe"
```

### 4. 使用脚本增量构建并打包

调试时可继续用 CLion 或 Qt Creator 配置、构建和运行 `AutoVizClient/build/`。以下脚本不重新
配置 CMake、不删除 IDE 的构建缓存；它只对该目录执行 Release 增量构建，然后删除并重建
`package/AutoViz-UCRT64/`。

```powershell
# 从仓库根目录执行；-Run 会在打包完成后启动程序。
.\AutoVizClient\scripts\package_windows.ps1
.\AutoVizClient\scripts\package_windows.ps1 -Run
```

脚本默认以 Windows 报告的逻辑核心数并行构建；可按需限制并行数，例如
`.\AutoVizClient\scripts\package_windows.ps1 -Parallel 8`。首次运行前，`AutoVizClient/build/`
必须已由同一套 MSYS2 UCRT64 MinGW、Qt6 和 protobuf 配置完成；脚本会检查其中的
`CMakeCache.txt`，避免误把发布操作指向 IDE 以外的目录。

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
- 本地回放和远程连接互斥：开始回放会断开 Server；连接 Server 会停止回放。
- 主题、车辆尺寸等运行时文件位于可执行文件同级 `configs/`，交付时必须整体保留。
