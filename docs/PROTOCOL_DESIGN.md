# AutoViz Protocol v1

本文同时记录已实现协议和后续演进约束。schema 分别位于
`AutoVizClient/proto/autoviz/protocol/v1/` 和
`AutoVizServer/src/autoviz_server/proto/autoviz/protocol/v1/`。两份 schema
必须完全一致，由各自工程直接编译，并通过
`cmake -P tools/verify_proto_sync.cmake` 校验。

## 通信目标

- Linux Server 接入 ROS2，Linux/Windows/后续平台运行相同 Client。
- Client 不安装 ROS2，不接触 custom_msgs。
- 同一协议未来可承载 ROS Adapter、Simulation Adapter 和 Log Adapter。
- 第一阶段只读、可信局域网、多 Client。

## 数据格式选择

| 格式 | 结论 |
| --- | --- |
| JSON | 适合人工诊断，不作为高频主链路 |
| protobuf | 已选用；有 schema、跨平台、字段演进明确 |
| 自定义 binary | v1 不采用，除非性能测量证明必要 |

已实现选择为 protobuf。schema 使用 `proto2`，允许 optional 字段表达“未提供”；
禁止新增 `required`，以免阻断兼容演进。

设计借鉴 Apollo 的消息分域、统一 Header，以及 PathPoint 与 TrajectoryPoint 分离思路，
但没有复制 Apollo 消息，也没有把 Apollo 的完整 Debug 对象作为 v1 契约。robot_ws
消息只用于第一版 Adapter 映射，协议不镜像 custom_msgs。

## 通信方式

| 方式 | 结论 |
| --- | --- |
| TCP | v1 已采用，可靠有序，适合快照和状态流 |
| UDP | 尚未采用；未来仅对可丢弃高频数据评估 |
| WebSocket | 尚未采用；浏览器 Client 成为目标时再评估 |

### TCP framing

```text
+----------------------+----------------------------+
| uint32 big-endian N  | N bytes protobuf Envelope |
+----------------------+----------------------------+
```

- 长度不包含 4 字节头。
- `N` 必须为 `1..16 MiB`；无效长度立即断开。
- TCP 粘包/拆包由 `FrameDecoder` 处理。
- 当前不压缩，不加密。

## 握手和同步

```text
Client                         Server
  |------ ClientHello ---------->|
  |<----- ServerHello -----------|
  |------ SubscribeRequest ------>|
  |<----- Full Snapshot ----------|
  |<----- ChannelUpdate ----------|
  |<-----> Heartbeat ------------>|
```

- 协议版本当前为 `1.0`；major 不一致视为不兼容。
- Client 空 channel 列表表示订阅全部可用通道。
- 连接/重连先取全量快照，之后接收增量。
- `session_id` 标识 Server 生命周期；session 变化必须清空 Client 旧数据。
- 心跳周期 1 秒；5 秒未收到对端数据视为连接失效。

## 领域消息

- `common.proto`：Header、几何、来源无关诊断键值树。
- `vehicle.proto`：VehicleState、VerticalState、ChassisState、Actuator、Battery。
- `planning.proto`：PathPoint、TrajectoryPoint、Trajectory、ReferenceLine。
- `perception.proto`：Obstacle、ObstacleSet。
- `control.proto`：ControlCommand、VerticalCommand、ActionState、TaskState。
- `runtime.proto`：来源信息、topic/通道健康和 Server 诊断。
- `transport.proto`：channel、snapshot/update、握手、心跳和错误。

通道更新有两种操作：

- `UPSERT`：替换该通道最新值。
- `CLEAR`：明确删除该通道，Client 不得保留上一帧。

## 单位和坐标

- 长度：米；速度：`m/s`；加速度：`m/s²`；jerk：`m/s³`。
- 角度：弧度；角速度：`rad/s`。字段名包含 `_rad` / `_radps`。
- heading：东向为 0、逆时针为正。
- 垂向 `odom_z`、`depth`、`height_above_bottom` 是不同语义，不得互相覆盖。
- 时间：Header 使用 Unix epoch 纳秒；轨迹相对时间使用秒。
- UI 显示时把弧度转换为度，把 `rad/s` 转换为 `°/s`。

## Server API 原则

- Server 接口面向车辆、规划、控制、感知和运行状态，不面向 ROS msg。
- Client 不关心数据来自 ROS2、simulation、middleware 或日志。
- ROS/Simulation/Log Adapter 都应输出相同 schema。
- 来源特有且 UI 仍需显示的硬件字段使用稳定 diagnostic key，不使用 ROS 字段路径。
- 新的通用规控语义优先增加领域字段；不要把所有内容都塞进 diagnostic metric。

## 兼容规则

1. 已发布字段号永不复用，删除字段应 `reserved`。
2. 向后兼容新增使用 optional/repeated 新字段。
3. 语义、单位或坐标方向变化不能静默复用旧字段号。
4. Client 忽略未知字段和未知可选通道；未知 protocol major 拒绝连接。
5. transport 变更必须补帧测试；消息映射变更必须补转换/回放测试。

## v1 不包含

- Client 到机器人任务下发、控制指令或参数修改。
- TLS、认证、权限和公网访问。
- UDP/WebSocket、压缩、服务发现。
- Apollo 全量 planning/control debug。
