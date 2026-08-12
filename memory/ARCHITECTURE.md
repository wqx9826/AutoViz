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

`main` 是现场稳定的 ROS2/Qt 单体版；v2 重构只在 `feature/client-server` 验收。main、
robot_ws、Tcptest 和 rosbag 都是只读参考，不由本工程构建或 launch 修改。
