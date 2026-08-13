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
    Qt5/            # Qt kit 根目录或目录链接，可选
      include/
      lib/
```

AutoVizProto 必须存在。protobuf/Qt5 若已能被工具链正常找到，可以不复制；若希望工程
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

Qt kit 可放到 `AutoVizClient\third_party\Qt5`，或在该位置创建指向实际 Qt kit 的
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
  其中含车辆尺寸 JSON 和浅色主题 QSS；当前固定使用经 main 分支验证的浅色 UI 基线，暂不提供
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

Windows 部署必须包含 Qt5 Sql 模块及 `plugins/sqldrivers/qsqlite.dll`；无需 ROS2、WSL 或
AutoVizServer。
