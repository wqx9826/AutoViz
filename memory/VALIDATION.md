# AutoViz C/S v2 本机验收记录

日期：2026-08-13。所有写入仅发生在 `AutoViz-feature`；robot_ws、Tcptest、main 和 rosbag
仅作读取/运行输入。

## 2026-08-14 Linux Client 发布包验证

- 清除了从旧 worktree 复制而来的 Client/Server CMake 缓存，按当前工作树重新配置 Client
  Release 构建，并在 `AutoVizServer` 内重新 `colcon build`。不修改 robot_ws、bag 或 main。
- 新增 `AutoVizClient/scripts/package_linux.sh`。它将 Client 安装到固定的
  `package/AutoViz-Linux/` 目录；包内携带
  Qt5、Qt5XcbQpa、protobuf、`qxcb`、`qoffscreen`、`qsqlite`、配置和 `qt.conf`。`bin/AutoViz`
  是启动器，仅向运行时追加包内 `lib/`，`AutoViz.bin` 不直接作为交付入口。
- 实际生成 `/tmp/autoviz-linux-package-20260814`。`ldd` 在包内库路径下确认 Qt5 Network/Sql/
  Widgets/Gui/Core 与 protobuf 全部解析到该发布目录；以 `env -i`、无开发环境变量和
  `QT_QPA_PLATFORM=offscreen` 启动成功，日志写入包内 `bin/log/`。
- 将现有 `AutoVizClientPlaybackTests` 临时置入该发布目录并只使用包内 `lib/`、`plugins/`，对
  `rosbag2_2026_08_12-06_33_29` 通过 CDR 自检并成功读取 141,982 条支持消息，证明包内
  `qsqlite` 可用。测试工具未保留在交付目录。
- 当前源码重建后执行 `tcp_server_test`：7/7 通过，覆盖握手+最新全量快照、major 拒绝、两个
  独立 Client 与上限、心跳/超时、重启 session 及慢 Client 最新快照合并。首次在受限沙箱内
  的 `listen` 返回 `Operation not permitted`；在授权的 localhost 测试环境重跑通过。
- 发布脚本已统一放到 `AutoVizClient/scripts/`：`package_linux.sh` 固定重建
  `package/AutoViz-Linux/`，并在打包前清理该目录。仓库入口 `scripts/AutoViz.sh` 与交付入口
  `package/AutoViz-Linux/bin/AutoViz` 均在清空环境变量、offscreen 平台下启动成功。
  `qt.conf` 只保留 `scripts/qt.conf` 一个来源，CMake 安装时复制为发布包 `bin/qt.conf`。
  Windows 打包说明已恢复到 Client README，但仍待 Windows 实机验收。

该发布方案针对 Ubuntu 22.04 兼容 ABI，不尝试打包 glibc、X11/Wayland 基础库或显卡驱动。
仍需在一台不安装 Qt/protobuf 开发环境的 Linux 目标机完成真实桌面和真实 Server 的人工验收。

## 当前提交前状态

- Linux 使用 `AutoVizProto/scripts/bootstrap_proto.sh` 引导协议 SDK：从脚本位置解析仓库，
  固定在 `AutoVizProto/build` 构建，并将 SDK 安装到 Client 和 Server 各自的 `third_party`。
  协议测试需按需从该构建目录单独执行。
- Windows 使用独立的 `AutoVizProto/scripts/bootstrap_proto.ps1`；Client 仅在
  `AutoVizClient/build` 构建，Server 仅在 `AutoVizServer` 中使用 colcon 构建。
- feature 仓库根目录不创建 `build/`，不用于任何子工程的编译产物。
- Client 在 Linux 已重新 CMake 配置和构建通过。控制曲线保留实际收到的控制命令，目标值来自
  `/chassis_command`，反馈值来自 `/location`；`enabled` 只作为当前命令状态展示。
- 运动总览固定为六张卡片的 3×2 排布，连接、capability 与数据到达只更新内容，不改变位置。
- Client 图标由 Qt resource 内嵌；外置运行时资源仅包括 `configs/vehicle_params.json` 与主题
  QSS，构建后自动复制到可执行文件目录。
- Windows 已使用 MSYS2 UCRT64 Qt6 完成 Client 构建并生成发布目录；发布包仍需在不含 Qt、
  MSYS2 或 IDE 的目标机上完成双击启动与本地回放人工验收。
- Linux 下的 C/S 构建、Client 运行已验收；Linux Client 的可分发打包及在干净目标环境的部署
  尚未验证，不能以 Windows 发布流程或产物替代。

## 自动化测试

控制状态时间关联验收：`/system_run_states`、`/chassis_command`、`/chassis_states` 的
source stamp 缺失必须保持缺失，接收时间与 per-topic sequence 必须可见；统一 5 秒超时后
当前状态缺失而 session 审计事件保留。使用
`rosbag2_2026_08_17-03_18_59` 验证本地回放的中心转向/自主爬行切换在事件历史中不中断。

2026-08-17 使用该 bag 直接反序列化确认 `/chassis_command` 为
`mode=11,gear=4 -> mode=0,gear=0 -> mode=6,gear=1`，且不存在 `mode=6,gear=4`。
DB3 原始 CDR 共包含 1,592 条 `mode=0,is_enable=0`：多数中心转向退出区间持续 80～100 ms，
另有 `04:49.162～05:15.582` 的 26.42 秒连续区间和末尾约 5.02 秒连续区间。8 倍速时短区间
小于一次 50 ms UI 刷新，Server 20 Hz 完整快照也存在相同的合并窗口。控件级测试将包含
`11 -> 0 -> 6` 后当前值已经为 6 的单份快照直接交给 UI，断言总览只显示当前
mode=6，不通过 UI 队列重放短暂 mode=0；该测试不依赖固定 bag seek 时间，同时覆盖远程和本地
共用的展示逻辑。
`AutoVizPlaybackSourceSmoke` 会 seek 到 42 秒中心转向区间断言两条路径为空，再 seek 到 81 秒
断言 mode=6 与边界后新路径恢复，再 seek 到 131 秒断言下一次真实 mode=11 仍能进入中心转向。
从 79 秒开始的 1 倍速连续回放通过 `11 -> 0 -> 6` 后，当前快照为 mode=6，实测暂停确认延迟
0 ms；8 倍速同样验证最终当前命令与底盘状态。运动总览不再为了短暂 mode=0 维护过渡队列，
而是直接显示当前完整快照；
`AutoVizControlStatusUiTests` 对 coalesced `11 -> 0 -> 6` 快照立即断言 mode=6。Server
`robot_ws_converter_test` 为 17/17，通过 mode/gear 独立性、路径立即清除、中心转向期间抑制及
退出后等待新路径；TCP 测试在允许 localhost listen 的环境为 7/7。

Server 额外覆盖同一个 20 Hz 发布周期内连续收到 `11 -> 0 -> 6`：当前命令最终为 mode=6。
隐藏 Action status/feedback 的诊断刷新
不会增加 `/system_run_states` 的 message count、sequence 或 last receive time。

真实 Server 链路使用 `ros2 bag play rosbag2_2026_08_17-03_18_59 --rate 8.0`，Server 在
`127.0.0.1:39191` 以 20 Hz 发布，协议探针连续读取 18 秒、361 份完整快照。探针从 TCP
观察到 `/chassis_command` 回调计数 19,248、`/chassis_states` 19,335、`/system_run_states` 3,601。
说明 20 Hz 全量快照只表达当前状态；中间状态变化不再作为派生事件重复发送。

`AutoVizControlStatusUiTests` 通过控件级回归：隐藏总览时依次写入 mode `11 -> 0 -> 6`，重新显示
后“当前运动”“控制指令”和控制时序关联表均直接读取最新的“自主爬行”快照，
mode=6、topic sequence=103；并断言爬行/航行速度与指令 rev 的来源值彼此独立。测试还确认
激活 bag 后迟到的 Remote mode=11 snapshot/reset 均被拒绝，切回 Remote 后迟到的 bag snapshot/reset
也被拒绝，当前活动来源的数据保持不变。普通总览值不再
每帧触发 QSS repolish，状态 badge 只在样式等级变化时重抛光。

本地回放 snapshot sequence 已验证单调递增，并与已应用 bag 时间、控制命令状态在同一原子
快照中发布；UI 进度不领先该快照时间。Client 状态刷新统一由 MainWindow 的 50 ms 定时器驱动，
底部面板不再叠加独立的墙钟 50 ms 拦截。

`rosbag2_2026_08_17-03_31_33` 已通过 Client 完整 CDR 扫描和本地回放 smoke：19656 条支持
消息全部解码，`/chassis_command` 中 4267 条非零角速度在协议和两个 Client 控制模型中值
一致。回归测试覆盖 Action 角速度为 0 时 cmd 仍取控制命令，以及 `ChassisStates` 反馈角速度
按 robot_ws 源值透传到协议和 Client，不做挡位或符号归一化。

### 控制字段来源与双链路门禁（2026-08-18）

| Client 展示字段 | 远程 Server/TCP | 本地 bag 回放 | UI 用途 |
| --- | --- | --- | --- |
| cmd 速度/航向/角速度/模式/档位/使能 | `/chassis_command` -> `ControlCommand` | CDR `/chassis_command` -> `ControlCommand` | “控制指令” cmd |
| rev 速度/航向/omega_z | `/location` -> `VehicleState` | CDR `/location` -> `VehicleState` | “控制指令” rev、控制曲线反馈 |
| 爬行速度/爬行角速度/档位反馈 | `/chassis_states` -> `ChassisState` | CDR `/chassis_states` -> `ChassisState` | “当前运动”、档位 rev |
| 航行速度/角速度 omega_z/当前航向 | `/location` -> `VehicleState` | CDR `/location` -> `VehicleState` | “当前运动” |

验证必须分别执行两条链路并比较同一输入值：

1. 远程链路：ROS2 Adapter -> `SnapshotStore` -> TCP 完整快照 -> `ProtocolModelConverter` -> UI。
2. 本地链路：SQLite/DB3 CDR -> `RobotWsCdrDecoder` -> `ProtocolModelConverter` -> UI。

两条链路必须同时断言 cmd、rev、爬行/航行拆分值的数值、单位、时间戳和 optional 缺失清空；
只验证协议探针或只验证单一来源均不满足发布门禁。

本次实现验证结果：`AutoVizControlStatusUiTests` 通过当前快照状态与来源拆分断言，并以
`rosbag2_2026_08_17-03_18_59` 完成真实 UI 回放；`AutoVizPlaybackSourceSmoke` 对同一 bag
完成 12 通道、seek、8 倍速暂停和 EOF 验证；`AutoVizClientPlaybackTests` 对
`rosbag2_2026_08_17-03_31_33` 解码 19,656 条支持消息。远程链路的
`robot_ws_converter_test` 为 17/17，`tcp_server_test` 为 7/7。

### 数据源显示等价性（发布门禁）

对于每个已在 Client 模型或 UI 展示的协议字段，验证必须同时覆盖：

- ROS -> Server -> TCP 完整快照 -> `ProtocolModelConverter` -> Client 内部模型/UI；
- rosbag CDR -> `RobotWsCdrDecoder` -> 同一 `ProtocolModelConverter` -> Client 内部模型/UI。

两条链路必须断言相同字段值、单位和字段缺失清空行为。协议、Adapter、CDR decoder、内部模型、
Converter 或 UI 的任何变更都要更新此验证；任一字段仅在远程或仅在回放显示即视为失败，禁止发布。

- AutoVizProto：8/8 通过。覆盖二进制零字节、拆包、粘包、多帧、非法/超长长度、损坏
  protobuf、reset、v2 hello/capability 和完整水下/平台快照 round-trip。
- Server TCP：7/7 通过。覆盖非法地址、动态端口、握手前隔离、握手后立即快照、major
  拒绝、多 Client/上限、心跳、Client 超时、重启 session 和慢 Client 最新快照合并。
- robot_ws converter/SnapshotStore：17/17 通过。覆盖八类输入、角速度方向/单位、有效目标、
  typed 水下/平台字段、路径几何与时间、topic 统计、5 秒超时清空、单个发布周期内的
  `11 -> 0 -> 6` 事件保留，以及隐藏 Action 诊断不污染公开 topic 元数据。
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

历史样例中 2026-08-06 前的 ChassisCommand 为含 `dive_speed` 的 72 字节布局，之后为不含该
字段的 64 字节布局；decoder 以固定长度区分两种布局并读取旧字段以保持后续字段对齐，其他长度
直接拒绝。当前 robot_ws 已删除 `dive_speed`，因此它不映射为协议/UI 字段。Client 回放解码器还支持
四种 ChassisStates 固定布局（277/341/325/389 字节，含封装头，分别对应旧无尾推、旧含尾推、当前无尾推、当前含航向诊断和尾推），旧布局缺失字段保持 optional 缺失。回归自检同时构造
64/72 字节 ChassisCommand CDR，验证执行器、浮力和声纳字段不会错位。`AutoVizPlaybackSourceSmoke` 对
`rosbag2_2026_08_12-03_00_17` 完成 metadata/SQLite/CDR 全量预检、seek、8× 和 EOF 状态验证。
Client 最终构建通过，并以 Qt offscreen 启动无崩溃。Windows 的 qsqlite 部署与实际视觉观感
仍需在 Windows 图形环境人工验收。

高倍率响应回归增加了 worker 时间片测试：8× 连续播放期间发出暂停命令，样例 bag 实测
worker 确认延迟为 0 ms（验收上限 300 ms），随后完成 seek、继续播放和 EOF。播放批次按
通道合并，主场景限频，避免高倍率下 UI 事件队列和完整 snapshot 重绘互相放大。

深色主题切换会触发 Qt5 对整棵控件树的 QSS 重抛光，且无法保证底部状态/详情/日志的局部样式一致。
当前固定使用已验收的浅色 QSS 基线，并移除“外观”菜单；主视图与回放浮层保留高 DPI 适配。

播放速度浮层使用独立自绘 `PillRateSlider`，不继承 `QSlider`：六个内置档位、32 px 胶囊轨道、
40 px 白色手柄、140 ms OutCubic 吸附动画，以及 hover/pressed 缩放均不依赖图形效果。以
`QT_SCALE_FACTOR=1.5` 生成 930×267 深浅预览图，控件未裁切或变形。

## 2026-08-15 Action 隐藏 topic 合并审查与 goal_uuid 规范化修复

输入 bag：`/home/wqx/LZBK/data/rosbag/rosbag2_2026_08_15-08_44_34/`。该 bag 含隐藏 action
topic：`/move_action/_action/status`（`GoalStatusArray`，15 条，TRANSIENT_LOCAL）、
`/move_action/_action/feedback`（`Move_FeedbackMessage`，1562 条）；depth 的 status/feedback
为 0 条。

审查发现并修复一个会让隐藏 topic 合并失效的问题：robot_ws 发布
`SystemRunStates.goal_uuid` 时按字节用 `%x` 拼接，丢掉字节前导零（如 UUID 字节 `0x06` →
`"6"`）。实测同一 goal 的 `goal_uuid` 是 31 位 `c5cc4cd52699c9c14e8148408868456`，而隐藏
topic 的 canonical UUID 是 32 位 `c5cc4cd52699c9c14e81484088068456`。Server `uuidToString`
与 Client `readUuid` 都产生 canonical 32 位 hex，因此旧实现的精确字符串匹配永远失配，
`native_status`/`feedback_progress` 从未被合并/转发。

修复：新增 `RobotWsProtoConverter::sameGoalUuid`（Server）与 `RobotWsCdrDecoder::sameGoalUuid`
（Client），把 canonical 侧转成同样丢零的 lossy 形式做对称比对，兼容修复前后的 robot_ws。
同时把 Server `m_actionDiagnostics` 的超限淘汰从 `unordered_map` 任意 `begin()` 改为 FIFO
（`deque` 记录插入顺序）。

验证：Server `robot_ws_converter_test` 13/13、`tcp_server_test` 7/7 通过；Client
`AutoVizClientPlaybackTests` 自检通过，并已改为按 `supportedTopics()` 扫描 104,415 条
（含隐藏 topic）无失败，1562 条 `SystemRunStates` 全部命中 native_status 与
feedback_progress。端到端回归（ROS 播放 bag + Server + 探针）确认合并生效：快照
`with_native_status`/`with_feedback` 从 0 变为 301/301，执行中的 Move action 输出
status=2（EXECUTING）、progress 递增。

遗留（待团队决策）：robot_ws 端 `goal_uuid` 丢零是数据源头问题，最稳妥是在 robot_ws 用
`%02x` 修正；AutoViz 侧已容错，但对已录制的坏格式 bag 只能靠 lossy 对称比对匹配。

## 2026-08-24 TaskParams 与感知详情接入验证

协议升级至 2.5：`VisualizationSnapshot.perception_state=22` 保持 optional，包含彼此独立的
`RangeMotionDirective` 和 `InspectionGoal`；旧 2.x Server 不发送该字段仍可连接。协议 GTest
9/9 通过，覆盖感知状态 frame round-trip 与旧快照缺失 `PerceptionState`。Client synthetic CDR
测试覆盖 RangeMotion 的 header/string/`float32` 对齐、InspectionGoal 的 `Point`/`float32` 对齐，
并确认一个请求替换或过期不会清除另一个请求。详情 UI 测试覆盖 TaskParams 五分栏、感知三分栏、
标签顺序、旧 TaskState 的逐字段“该 Server 无此信息”、消息自身 task ID 以及 FinalTarget 的拒绝原因和
Z 列。

Server 重新构建通过。`robot_ws_converter_test` 的 22 项覆盖两类感知请求转换和独立过期；
`tcp_server_test` 在允许本机 loopback 绑定的环境中通过，`colcon test-result --verbose` 汇总为
30 tests、0 errors、0 failures、0 skipped。沙箱内 `listen()` 会被策略拒绝，因此 TCP 测试必须在
允许 loopback 的受控环境运行。

指定本地回放输入为
`/home/wqx/LZBK/data/rosbag/rosbag2_2026_08_18-09_12_43`。`AutoVizClientPlaybackTests` 通过，
扫描 151,608 条受支持消息；`AutoVizControlStatusUiTests` 以 offscreen 模式从 0 秒 8 倍速播放，
确认 TaskParams 已进入 Client 模型，并在详情页显示任务 ID、任务使能、动作使能和 1 至 16 路配电指令。
该 bag 不包含 `/targets/final_objects`、`/detection/range_motion_request` 或
`/detection/inspection_request_goal`，因此它只能作为 `/task_params` 的真实回放证据，不能证明感知
topic 的实际录制回放；后者由合成 CDR 和 Server 转换测试覆盖。

同一 bag 用 `ros2 bag play --rate 8.0` 驱动 Server，并以 protocol probe 和 offscreen Qt Client
连接 `127.0.0.1:39090`。probe 在 8 秒得到 161 份快照、`task=1`，当时 `/task_params` 计数为 3,324；
Client 日志确认“已连接 127.0.0.1:39090（robot_ws ROS2 adapter）”。该窗口中的三个感知 topic
均为 count=0、timed_out=1，符合 bag 内容，未被错误显示为有效感知数据。

## 2026-08-27 FinalTarget 2.7 当前消息布局

- Protocol 升级为 **2.7**：保留 `VisualizationSnapshot.obstacles=16` 的旧语义和全部其他 v2
  字段，新增 optional `VisualizationSnapshot.final_targets=23`。无需因 FinalTarget 变化升级 protocol
  major，其他旧 v2 数据继续可接收。
- `FinalTargetSet` 表达当前 `/targets/final_objects`：array 的 `task_id`、`mine_number` 与每个目标的
  header、ID、分类、reference XY、radius 和仅渔网适用的 boundary。`reference_point` 明确不是几何中心。
- Protocol GTest：`autoviz_proto_tests` **9/9 通过**，覆盖 FinalTargetSet frame round-trip 与 2.7 版本常量。
- Server 已 source 当前 robot_ws `custom_msgs` 并重建。`robot_ws_converter_test` **15 项通过**，覆盖
  task_id/mine_number、圆形目标、合法渔网边界、非法边界回退圆、非法半径和非 odom frame 整帧拒绝；
  `tcp_server_test` **7/7 通过**。`colcon test-result --verbose` 为 **31 tests、0 errors、0 failures、0 skipped**。
- Client synthetic CDR 覆盖当前 `FinalTargetArray` 的 header、task_id、mine_number、reference XY、radius、
  分类、当前 boundary sequence 对齐、参考点非有限拒绝和非正半径拒绝，以及截断/不兼容目标帧不会替换
  上一有效 `final_targets`。旧 FinalTarget CDR 布局不能安全识别；验证或播放时只跳过
  `/targets/final_objects`，不使定位、控制、底盘、Action、TaskParams、路径等其他已支持 topic 的 bag
  加载失败，也不推进被跳过目标帧的消息计数或新鲜度。另有 field-16 `ObstacleSet` Converter 自检，确保
  旧 v2 Server 与泛化 Adapter 的障碍物数据仍可显示；同一快照有 field 23 时以当前 FinalTarget 语义优先。
- 感知详情表显示目标 ID、类别、参考点 X/Y、保守半径、边界点数、展示形状和更新时间；不显示或伪造已删除的
  Z、经纬度、长宽高或朝向。渔网空边界合法并绘制保守圆，无效边界显示“边界无效，回退圆”。
- `AutoVizControlStatusUiTests` offscreen 与 Client 主程序构建均通过。既有 bag
  `rosbag2_2026_08_18-09_12_43` 已扫描 151,608 条支持消息并通过；它不含目标通道，不能替代当前
  FinalTarget 的真实录包验收。

## 2026-08-28 推进器电机反馈 Protocol 2.8

- Protocol 升级为 **2.8**（major 仍为 2）：`ChassisState.thruster_motor=20` 使用五个固定 ID
  `left_tail_thruster`、`right_tail_thruster`、`left_vertical_thruster`、
  `right_vertical_thruster`、`back_vertical_thruster`，每组保留母线电流 A、控制器温度 °C、目标/实际
  rpm。旧 `tail_thruster_motor=14` 未改动，Server 对同一 `ChassisStates` 仍只写其两组尾推，同时向
  field 20 写完整五组；不缩放、不换符号、不补造零值。
- Client `ProtocolModelConverter` 优先 field 20，缺失时回退 field 14；底盘详情删除“航向控制器诊断”，
  “航向驱动器”固定显示五行。旧布局仅有两组尾推时三组垂推严格显示 `--`。
- CDR 自检覆盖 485 字节当前五电机布局、277/341/325/389 字节历史布局、field 14 回退、截断、未知
  长度、超过 7 字节尾部填充和非零尾部拒绝；485 验证五组值及后续水箱、履带、BMS 字段未错位。
- 发布门禁仍要求用同一五电机样本比较 `ROS -> Server -> TCP -> ProtocolModelConverter -> UI` 与
  `CDR -> RobotWsCdrDecoder -> ProtocolModelConverter -> UI` 的 ID、顺序、四项数值、单位、缺失行为和
  页面文本；另须以当前 `ChassisStates.msg` 实录 bag 与至少一份旧 bag 完成两条人工回放验收。

### 后续真实链路门禁

必须用**当前** FinalTargetArray 定义录制一份含圆形目标、合法/空/非法渔网边界、无效参考点或半径以及
后续恢复合法帧的 bag。实时链路 `robot_ws -> AutoVizServer -> TCP -> UI` 与回放链路
`bag CDR -> RobotWsCdrDecoder -> UI` 必须比较 task_id、mine_number、ID、分类、reference XY、radius、
边界顶点、回退/拒绝结论；允许不同的仅是接收时间或 bag 记录时间。
