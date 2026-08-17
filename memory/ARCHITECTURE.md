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
快照队列。

## Client 数据流

- `RemoteVisualizationSource`：连接、v2 hello、完整快照、心跳、错误、重连和 session。
- `ProtocolModelConverter`：完整 protobuf snapshot 到内部模型的唯一入口。
- `DataManager`：原子替换值快照；同 session 延续历史轨迹。
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
使用 bag 虚拟时钟计算新鲜度；暂停不会让通道按墙钟超时，seek 会重建各通道最近状态并
清空旧历史。

高倍率回放采用有界背压：worker 每次最多占用 4 ms/处理 2000 行，同一批内每个通道只
解码最后一帧；过载时放慢虚拟时钟，不积压任务。暂停、停止和调速因此可在下一时间片被
处理。主场景在本地回放时最多 10 Hz 全量重建，状态区仍保持 20 Hz。

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
