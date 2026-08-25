# AutoViz C/S v2 架构

## 三工程边界

```text
ROS2/custom_msgs
  -> AutoVizServerNode
  -> RobotWsProtoConverter
  -> SnapshotStore
  -> VisualizationServer / TcpSession
  -> AutoVizProto v2 + TCP
  -> RemoteVisualizationSource
  -> ProtocolModelConverter
  -> DataManager
  -> SceneManager / Qt UI
```

- `AutoVizProto`：唯一 schema、版本、FrameCodec；仅依赖 C++17/protobuf。
- `AutoVizServer`：来源 Adapter、单位归一化、缓存/超时和网络服务。
- `AutoVizClient`：纯 Qt；网络消息只在 converter 入口转成内部模型。

Client 和 Server 使用各自 `third_party/AutoVizProto` 下的安装 SDK，不引用兄弟源码。

## Server 数据流

```text
ROS callback
  -> RobotWsProtoConverter（无状态纯转换）
  -> SnapshotStore（最新值、频率、超时、dirty）
  -> 50 ms timer
  -> VisualizationServer::publishSnapshot(值对象)
  -> asio::post 到唯一 I/O 线程
  -> 已握手 TcpSession 写队列
```

Node 不处理 hello、session 或 socket；VisualizationServer 不理解 ROS。SnapshotStore 仅由
ROS SingleThreadedExecutor 访问。TCP 慢客户端的旧待发快照会被新快照替换，不建立无界
快照队列。控制相关 topic 与其他输入一样只维护当前值；隐藏 Action status/feedback 只刷新
诊断，不冒充 `/system_run_states` 消息推进其序号或接收时间。

## Client 数据流

- `RemoteVisualizationSource`：连接、v2 hello、完整快照、心跳、错误、重连和 session。
- `ProtocolModelConverter`：完整 protobuf snapshot 到内部模型的唯一入口。
- `DataManager`：原子替换值快照；同 session 延续历史轨迹；只接受当前活动输入来源的 replace/reset。
- 三条控制 topic 的当前值均按配置超时清除；不维护派生的控制状态事件历史。
- `SceneManager/UI`：主线程每 50 ms 读内部快照，不 include protobuf，不解析 topic 名。

本地回放是第二条互斥的数据源链路：

```text
rosbag2 metadata v5 + SQLite3 DB3
  -> LocalRosbagPlaybackSource 工作线程
  -> RobotWsCdrDecoder（固定 robot_ws schema、无 ROS 依赖）
  -> AutoViz protobuf snapshot
  -> ProtocolModelConverter -> DataManager -> SceneManager / Qt UI
```

## 数据源显示等价性（强制）

远程 Server 和本地 rosbag 是同一份 Client 业务数据契约的两个输入，不是两套 UI 功能。
两条链路都必须产出语义一致的 `VisualizationSnapshot`，随后统一通过
`ProtocolModelConverter -> DataManager -> SceneManager / Qt UI`。因此，只要某个协议字段被
Client 的模型或 UI 展示，Server 连接与本地 bag 回放都必须能展示该字段；不得让一个来源静默
丢弃、另一个来源可见。

新增、删除或修改协议字段、回放 decoder、`ProtocolModelConverter`、内部模型或状态详情 UI 时，
必须同步审查两条输入链路，并为两者补充字段存在与字段缺失时的等价验证。来源差异只允许体现在
数据本身不可用时的 optional 缺失，不允许体现在字段语义、单位或 UI 可见性上。

回放 adapter 是 Client 中唯一允许理解 ROS topic/type/CDR 的隔离边界。它与远程源互斥，
`ChassisStates` CDR 同时支持旧无尾推/旧含尾推和当前含航向诊断的两种布局，按固定消息长度选择布局；
旧布局缺少的航向诊断和尾推字段保持 optional 缺失，禁止错位解析。当前布局的航向反馈按
custom_msgs 声明的协议缩放值传输，不擅自推断角度单位。
使用 bag 虚拟时钟计算新鲜度；暂停不会让通道按墙钟超时，seek 会重建各通道最近状态并
清空旧历史。

互斥由 `DataManager` 的显式活动来源保证，不依赖网络断开是否及时完成。选择 bag 时
`MainWindow` 先激活 `Ros2Bag` 再请求断开 Server，远程 snapshot、disconnect reset 和自动重连
回调随后即使到达也会被拒绝；切回 Server 时先停止本地 session，再激活 `Remote`。状态栏所示
来源取自实际获准写入的原子快照，而不是某个连接控件的期望状态。

高倍率回放采用有界背压：worker 每次最多占用 4 ms/处理 2000 行，同一批内每个通道只
解码最后一帧；过载时放慢虚拟时钟，不积压任务。暂停、停止和调速因此可在下一时间片被
处理。主场景在本地回放时最多 10 Hz 全量重建，状态区仍保持 20 Hz。

控制状态和路径是顺序敏感通道，不参与上述按 topic 合并。回放 cursor 使用 bag 的
`(timestamp, split index, message id)` 顺序；暂停和调速先把快照同步到报告位置，若处理预算
不足则回退报告位置，禁止进度领先状态。seek 通过预检生成的中心转向边界过滤旧路径。

控制相关的三条 topic 在回放合并前逐条应用，以保证中心转向和路径清理顺序；不记录派生的
模式、档位或履带输出事件历史。

Server 20 Hz 完整快照和本地高倍率回放都可能在 Client 两次 50 ms 刷新之间经过多个命令状态。
底部控制总览、“控制指令”和控制时序均只读取当前完整快照；不维护为显示短暂 mode=0 而设计的
UI 补偿队列、停留计时或“无效（切换）”展示状态。

本地回放每次发布时递增协议 snapshot sequence，并把已应用的 bag 时间、控制命令状态和该序号
作为一个快照写入 `DataManager`。远程 Server 快照和本地 `RobotWsCdrDecoder` 必须产出相同的
控制字段语义后，统一经过 `ProtocolModelConverter -> DataManager -> UI`。控制状态 UI 不做跨来源
数值回退：`cmd/rev` 中 cmd 速度、航向、角速度只取 `/chassis_command`，对应 rev 只取
`/location`，档位反馈取 `/chassis_states`；当前运动另行显示 `/chassis_states` 的爬行速度/角速度
和 `/location` 的航行速度/omega_z。`/system_run_states` 只表达 Action 期望。

控制曲线的目标值只来自 `/chassis_command`，反馈值只来自 `/location`；命令和定位各自在 topic
序号或消息时间变化时追加稀疏样本，横轴使用各自消息时间；UI 刷新不生成采样点，也不以最大
时间戳把多个 topic 合成同一帧。上述字段必须同时通过本地 bag 和远程 TCP 链路验证。

中心转向只由实际控制命令的 mode 10/11 判定。进入时 Server 和本地回放立即清除全局/局部
路径，期间到达的路径只更新来源健康统计；第一条非中心转向命令解除抑制，但旧路径不会恢复，
必须等待切换边界之后的新路径消息。

断线或 session 变化先清空 DataManager，因此旧会话轨迹不拼接。Server 负责字段超时；
Client 的 5 秒 watchdog 只处理连接整体失活。

## 可迁移能力

通用规划控制 capability 支持 XY、车辆运动、轨迹、障碍物和控制曲线；可选 capability：

- `VERTICAL_MOTION`：T-Z、深度和离底高度。
- `UNDERWATER_SYSTEM`：水箱、垂推、浮力、声纳、紧急上浮。
- `PLATFORM_DIAGNOSTICS`：履带、BMS、DCDC、配电。

其他项目只需提供 Adapter 并输出同一通用快照。未声明的领域能力在 Client 隐藏/禁用；
只有新增领域面板才需要扩展 capability 和内部可选模型。

## 分支边界

`main` 是当前 C/S v2 开发主线；此前现场稳定的 ROS2/Qt 单体版保留在
`legacy/ros2-qt-monolith` 分支。robot_ws、Tcptest 和 rosbag 都是只读参考，不由本工程
构建或 launch 修改。
