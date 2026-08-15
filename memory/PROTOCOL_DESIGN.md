# AutoViz Protocol v2.1

唯一 schema 位于 `AutoVizProto/proto/autoviz/*.proto`，全部使用 proto2 optional/repeated
与 `package autoviz`。v2 删除 feature v1.1 的订阅和增量状态机，是不兼容升级。

## FrameCodec

```cpp
using FrameBytes = std::string;
bool encodeFrame(const Envelope&, FrameBytes&, std::string& error);
bool FrameDecoder::decode(std::string_view bytes,
                          std::vector<Envelope>& messages,
                          std::string& error);
```

`SerializeToString()` 输出二进制字节；`std::string` 可以安全保存 `\0`，长度明确，且能直接
交给 Qt/Asio。`decode()` 是流式入口，内部保存未完成 TCP 数据，一次返回 0～N 个消息。

```text
+----------------------+----------------------------+
| uint32 big-endian N  | N bytes protobuf Envelope |
+----------------------+----------------------------+
```

N 为 1..16 MiB；零长、超长或损坏 protobuf 返回错误。`reset()` 丢弃未完成输入。

## Envelope field number

| field | number | 方向/语义 |
| --- | ---: | --- |
| client_hello | 1 | Client -> Server |
| server_hello | 2 | Server -> Client，含版本/session/capability |
| snapshot | 4 | Server -> Client，完整当前状态 |
| heartbeat | 6 | 双向保活 |
| error | 7 | 协议错误 |

field 3（旧 SubscribeRequest）和 5（旧 ChannelUpdate）已 `reserved`，永不复用。

`VisualizationSnapshot` 的 1..4 为 sequence/server_time/session/source；领域字段固定为
VehicleState=10、ChassisState=11、ControlCommand=12、global/local trajectory=13/14、
ReferenceLine=15、ObstacleSet=16、ActionState=17、TaskState=18、RuntimeState=19、
VehicleParameters=20。

## 完整快照语义

```text
Client                         Server
  |------ ClientHello 2.x ------>|
  |<----- ServerHello ------------|
  |<----- latest full snapshot ----|
  |<----- later full snapshots ----|
  |<-----> heartbeat --------------|
```

- major 不一致返回 fatal ProtocolError；同 major 的 minor 可兼容。
- 握手前禁止快照；握手后立即发送最新快照。
- optional 字段缺失就是当前无数据，Client 原子替换，不存在 CLEAR。
- session 变化或断线必须清空全部远程状态和历史轨迹。
- Server 最多 20 Hz 合并脏数据；空闲用心跳，不重复发送相同快照。

## schema 分域

- `common.proto`：Header、几何、Capability、DataKind、扩展诊断。
- `planning.proto`：轨迹、路径点、参考线。
- `perception.proto`：来源无关障碍物。
- `vehicle.proto`：通用车辆状态、typed 平台诊断，并引用水下可选扩展。
- `control.proto`：通用控制/Action/Task，并引用水下可选扩展。
- `underwater.proto`：深度、离底高度、水箱、垂推、浮力、声纳、紧急上浮。
- `runtime.proto`：来源、capability 和 DataKind 对应的健康状态。
- `transport.proto`：快照、握手、心跳、错误。

`DataKind` 只稳定标识来源健康状态，不承担订阅。UI 可能显示 topic/type，但不得用其字符串
做业务判断。履带、BMS、DCDC、配电使用 typed 结构；`DiagnosticMetric` 仅保留扩展诊断。

`ControlCommand.maneuver` 使用 field 15（12..14 已保留），当前取值为 `NONE` 或
`YAW_IN_PLACE`。它与 `ControlCommand.mode` 正交：前者表达原地/中心转向，后者表达
`CRAWL` 或 `SAILING` 平台类型；UI 据此显示“爬行中心转向”或“航行中心转向”。robot_ws
Adapter 将 `ChassisCommand.mode` 的 10/11，以及 `expected_gear` 的 4，统一映射为该语义。

`UnderwaterCommand.vertical_control_mode` 使用 field 10，取值为 `NONE`、`DEPTH_HOLD`、
`HEIGHT_HOLD`。它是唯一决定垂向目标量和 UI 趋势轴的协议语义；`BuoyancyCommand`
只表达定深/定高动作附带的水箱指令，不单独构成一种动作。`navigation_mode` 保留为 Adapter
诊断信息，业务显示不得再依赖其数值。对于 robot_ws 的 `SystemRunStates`，Adapter 优先以
`owner=2`（DepthCommand）和 `chassis_mode=1/2` 判定定深/定高；实机该 action 可将
`navi_mode` 发布为 0。

`/system_run_states` 是可回放的 action 聚合状态，包含 owner、goal UUID、生命周期、垂向目标
及伴随水箱指令。DepthCommand/Move 的 ROS2 action status、feedback 是隐藏 topic；`ros2 bag
record -a` 不会记录它们。如需回放 action feedback（例如 progress），录制时必须额外使用
`--include-hidden-topics`。协议 `ActionState` 的公开聚合字段始终是主界面与 T-Z 的唯一必需
契约；`message`、原生 status、feedback progress 和最近终态仅为详情诊断字段，缺失时不得影响
主状态。Server 与本地 bag 仅在 UUID 匹配当前 action 时合并这些隐藏 topic 字段。

## 单位和兼容

- 长度 m，速度 m/s，加速度 m/s²，时间戳 Unix epoch ns。
- 角度 rad、角速度 rad/s；heading 东向 0、逆时针/左转正。
- UI 显示层才转成度和 °/s。
- 已使用 field number 永不复用；删除字段 `reserved`。
- 新 optional/repeated 字段可在同 major 演进；语义/单位/framing 不兼容变化提升 major。

v2 是可信局域网只读可视化协议，不包含写控制、TLS、认证、压缩或服务发现。
