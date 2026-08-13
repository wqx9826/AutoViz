# AutoViz C/S v2 本机验收记录

日期：2026-08-13。所有写入仅发生在 `AutoViz-feature`；robot_ws、Tcptest、main 和 rosbag
仅作读取/运行输入。

## 当前提交前状态

- Linux 使用 `AutoVizProto/scripts/bootstrap_proto.sh` 引导协议 SDK：从脚本位置解析仓库，
  固定在 `AutoVizProto/build` 构建，并将 SDK 安装到 Client 和 Server 各自的 `third_party`。
  协议测试需按需从该构建目录单独执行。
- Windows 使用独立的 `AutoVizProto/scripts/bootstrap_proto.ps1`；Client 仅在
  `AutoVizClient/build` 构建，Server 仅在 `AutoVizServer` 中使用 colcon 构建。
- feature 仓库根目录不创建 `build/`，不用于任何子工程的编译产物。
- Client 在 Linux 已重新 CMake 配置和构建通过。控制曲线保留未使能控制命令，支持遥控/自主
  切换和 rosbag 回放观察；`enabled` 只作为状态展示。
- 运动总览固定为六张卡片的 3×2 排布，连接、capability 与数据到达只更新内容，不改变位置。
- Client 图标由 Qt resource 内嵌；外置运行时资源仅包括 `configs/vehicle_params.json` 与主题
  QSS，构建后自动复制到可执行文件目录。
- Windows 尚未验证。需在 Windows 以同一工具链重新构建 AutoVizProto、安装 SDK，并构建
  AutoVizClient；Linux 产物不可复用。

## 自动化测试

- AutoVizProto：8/8 通过。覆盖二进制零字节、拆包、粘包、多帧、非法/超长长度、损坏
  protobuf、reset、v2 hello/capability 和完整水下/平台快照 round-trip。
- Server TCP：7/7 通过。覆盖非法地址、动态端口、握手前隔离、握手后立即快照、major
  拒绝、多 Client/上限、心跳、Client 超时、重启 session 和慢 Client 最新快照合并。
- robot_ws converter/SnapshotStore：8/8 通过。覆盖八类输入、角速度方向/单位、有效目标、
  typed 水下/平台字段、路径几何与时间、topic 统计和 5 秒超时清空。
- AutoVizClient 和 ROS2 Server 均完成最终构建。

## rosbag 完整链路

输入：只读回放
`/home/wqx/LZBK/data/rosbag/rosbag2_2026_08_12-03_00_17`，从约 1812 秒偏移开始，
持续约 25 秒。v2 探针连接 `127.0.0.1:39190`，持续发 Client Heartbeat。

结果：

```text
hello=1, snapshots=362
capabilities=common/vertical/underwater/platform 全部到达
vehicle/chassis/control/global_path/local_path/action/task 全部到达
```

探针观察到的累计消息数：location 531，chassis_command 1327，chassis_states 1327，
system_run_states 148，task_params 1327，local_path 147，global_path 146。停止回放超过 5 秒
后，这七类 topic 变为 timed_out，后续完整快照不再含对应领域字段。

bag 不含 `/targets/final_objects`。另以合成 `FinalTargetArray` 发布有效尺寸/航向目标，独立
`--require-obstacles` 探针确认 `obstacles=1` 经 ROS -> converter -> store -> TCP 到达。

## 真实 Client 与 session

Qt Client 以 offscreen 平台实际启动，成功输出：

```text
已连接 127.0.0.1:39190，协议握手中
已连接 127.0.0.1:39190（robot_ws ROS2 adapter）
```

另在真实 Client 保持连接时短时回放同一 1812 秒 bag 窗口，Client 持续运行且没有协议解析、
模型转换或 UI 更新错误，确认七类快照并非只由协议探针消费。

停掉 Server 后，Client 清空连接并持续重试；修复了 Qt `Connection refused` 不一定触发
`disconnected` 导致重连中止的问题。Server 再启动后 Client 自动重新握手成功。

重启前 session 为 `18cb1cb450fce51d`，重启后为 `18cb1ce22394edc2`，确认生命周期变化。
历史轨迹的清空由 RemoteVisualizationSource 在断线/session 变化时调用 DataManager reset。

offscreen 验收证明真实 Client 的构建、启动、握手、快照转换和重连链路可运行；最终字体、
布局和交互观感仍保留为有显示器环境下的人工验收项。

## Client 原生 bag 回放

新增 ROS-free CDR/SQLite 回放后，`AutoVizClientPlaybackTests` 完整扫描
`/home/wqx/LZBK/data/rosbag` 的 19 个 bag，共成功解码 3,762,626 条受支持消息，覆盖七个
常驻通道；现有 bag 未包含障碍物通道。测试同时覆盖大/小端
CDR、截断和非法 bool。

样例中 2026-08-06 前的 ChassisCommand 为含 `dive_speed` 的 72 字节布局，之后为 64 字节
布局，两者均完成全量解码。`AutoVizPlaybackSourceSmoke` 对
`rosbag2_2026_08_12-03_00_17` 完成 metadata/SQLite/CDR 全量预检、seek、8× 和 EOF 状态验证。
Client 最终构建通过，并以 Qt offscreen 启动无崩溃。Windows 的 qsqlite 部署与实际视觉观感
仍需在 Windows 图形环境人工验收。

高倍率响应回归增加了 worker 时间片测试：8× 连续播放期间发出暂停命令，样例 bag 实测
worker 确认延迟为 0 ms（验收上限 300 ms），随后完成 seek、继续播放和 EOF。播放批次按
通道合并，主场景限频，避免高倍率下 UI 事件队列和完整 snapshot 重绘互相放大。

深色主题切换会触发 Qt5 对整棵控件树的 QSS 重抛光，且无法保证底部状态/详情/日志的局部样式一致。
当前固定使用 main 分支验证过的浅色 QSS 基线，并移除“外观”菜单；主视图与回放浮层保留高 DPI 适配。

播放速度浮层使用独立自绘 `PillRateSlider`，不继承 `QSlider`：六个内置档位、32 px 胶囊轨道、
40 px 白色手柄、140 ms OutCubic 吸附动画，以及 hover/pressed 缩放均不依赖图形效果。以
`QT_SCALE_FACTOR=1.5` 生成 930×267 深浅预览图，控件未裁切或变形。
