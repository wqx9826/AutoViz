# AutoViz Protocol v2.7

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
VehicleParameters=20。`FinalTargetSet=23` 是 2.7 新增的 optional 字段；它与旧
`ObstacleSet=16` 并存，绝不复用或重解释旧字段。
`PerceptionState=22` 是 2.5 新增的 optional 字段；21 曾用于推导的 control state event，现已
`reserved`。`TaskState=10` 曾用于 task_start_requested，现同样 `reserved`，因为 robot_ws
只提供当前 `task_enable`，不能可靠表达独立的启动事件。

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
- `perception.proto`：来源无关障碍物、测距运动建议和观察目标。
- `vehicle.proto`：通用车辆状态、typed 平台诊断，并引用水下可选扩展。
- `control.proto`：通用控制/Action/Task，并引用水下可选扩展。
- `underwater.proto`：深度、离底高度、水箱、垂推、浮力、声纳、紧急上浮。
- `runtime.proto`：来源、capability 和 DataKind 对应的健康状态。
- `transport.proto`：快照、握手、心跳、错误。

`DataKind` 只稳定标识来源健康状态，不承担订阅。UI 可能显示 topic/type，但不得用其字符串
做业务判断。履带、BMS、DCDC、配电使用 typed 结构；`DiagnosticMetric` 仅保留扩展诊断。

`ObstacleSet.rejection_reason` 是旧泛化障碍物集合的整帧拒绝说明；当前 robot_ws 的
`/targets/final_objects` 不再写入该字段，而是使用 `FinalTargetSet.rejection_reason`。Client 仍必须
消费 `ObstacleSet=16`，以兼容旧 v2 Server 和其他泛化 Adapter；当同一快照同时有 16 与 23 时，
`FinalTargetSet=23` 优先，防止把 reference point/radius 错画为旧盒体。

`FinalTargetSet` 适配当前 robot_ws `FinalTargetArray`：集合 header 与每个 target header 必须为
`odom`；`source_task_id` 与 `mine_number` 仅为感知元数据，不参与 TaskParams 门控。每个 target 的
`reference_point` 仅是感知参考点、绝非几何中心，`radius_m` 是到外轮廓的有限正保守最大距离。
同帧 target ID 重复、frame 非 odom、参考点非有限、半径非有限/非正或分类不在 0..3 时拒绝整帧。
仅 final_class=2（渔网）消费边界：空边界合法并绘制保守圆；无效边界仅该目标回退圆并携带提示。
Server Adapter 与本地 CDR decoder 必须产生相同的集合、回退和拒绝结论。2.7 保持 protocol major=2，
因此其他旧 v2 字段和旧 bag 通道继续可用；旧 FinalTarget CDR 布局只跳过该 Topic，不阻断 bag 预检。
Client 只在一个 FinalTarget CDR frame 完整解码、校验并消费后替换当前集合；跳过的旧布局或截断帧
不得清空上一有效集合，也不得推进该 Topic 的消息计数或新鲜度。

`PerceptionState.range_motion_directive` 与 `PerceptionState.inspection_goal` 分别对应来源无关的
测距运动建议和观察目标，二者不能互相覆盖或在超时时一并清除。前者包含消息自己的任务 ID、
命令序号、动作、m/s 限速和原因；后者包含消息自己的任务/目标 ID、目标点/观察点（m）、ENU
rad 航向、到点保持、模式和 m/s 限速。Client 不得以当前 `TaskState.task_id` 过滤这些消息。
其来源健康状态使用 `DATA_KIND_RANGE_MOTION_DIRECTIVE=12` 和
`DATA_KIND_INSPECTION_GOAL=13`。旧 2.x 快照不含 `perception_state` 时，Client 必须展示为
“该 Server 无此信息”，而不是“等待数据”。

`VehicleState.longitude_deg`/`latitude_deg` 使用 WGS-84 度，`UnderwaterState.usbl_x_m`、
`usbl_y_m`、`usbl_z_m` 使用 Adapter 声明的本地定位坐标和 m 单位；它们是并列定位诊断，
不得覆盖 `position` 或替代 `odom_z`、`depth`、`height_above_bottom_m`。`TaskState.remote_control`
（field 9）携带可选的 crawl 目标、航行百分比、推进器调试量及 `power_supply_enabled`；后者
按物理通路从 1 开始的固定顺序编码。字段只描述观测到的控制意图，协议仍是只读的。

### 远程与回放等价性（强制）

所有已由 Client 模型或 UI 消费的 `VisualizationSnapshot` 字段，都必须同时由远程 Server
快照和本地 rosbag decoder 以相同语义、单位和 optional 缺失规则提供。Client 只能在唯一的
`ProtocolModelConverter` 将该字段转换为内部模型，UI 不得按数据源选择性隐藏或替换字段。
例如经纬度、USBL 与 `TaskState.remote_control` 必须在 Server 连接和本地回放中显示一致。
改变 schema、Server Adapter、CDR decoder、Converter、内部模型或 UI 时，必须同步更新另一条
链路及等价测试；“bag 可显示而远程不可显示”或反向情况均为协议实现缺陷，不可发布。
`TaskState` 的详细 optional 字段也必须保留 protobuf presence；旧 Server 只发送早期 TaskState
时，Client 只能逐字段显示“该 Server 无此信息”，不能显示 `0`、`false` 或“否”。

`ControlCommand.maneuver` 使用 field 15（12..14 已保留），当前取值为 `NONE` 或
`YAW_IN_PLACE`。它与 `ControlCommand.mode` 正交：前者表达原地/中心转向，后者表达
`CRAWL` 或 `SAILING` 平台类型；UI 据此显示“爬行中心转向”或“航行中心转向”。robot_ws
Adapter 只将 `ChassisCommand.mode` 的 10/11 映射为该语义；`mode=9` 水推设备测试/紧急上浮
归入 `SAILING` 平台。`expected_gear=4` 只表达期望中心转向档位，不能反向改变命令模式。
`ControlCommand.source_mode` 存在时保存当前 robot_ws `ChassisCommand.mode` 原值，并作为
Client 的模式展示依据；老协议没有该 optional 字段时，Client 才按 `mode/maneuver` 和垂向
语义回退推导 2/4/5/6/10/11 等显示模式。

`UnderwaterCommand.vertical_control_mode` 使用 field 10，取值为 `NONE`、`DEPTH_HOLD`、
兼容旧数据的 `HEIGHT_HOLD` 和当前 custom_msgs 的 `LANDING`。它是唯一决定垂向目标量和 UI
趋势轴的协议语义；`BuoyancyCommand` 只表达定深/着底动作附带的水箱指令，不单独构成一种动作。
`navigation_mode` 保留为 Adapter 诊断信息，业务显示不得再依赖其数值。对于 robot_ws 的
`SystemRunStates`，Adapter 优先以 `owner=2`（DepthCommand）和 `chassis_mode=1/2` 判定定深/着底；
实机该 action 可将 `navi_mode` 发布为 0。

不要混淆两类 `robot_ws` 模式：`SystemRunStates.owner=2 + chassis_mode=1/2` 是独立的
`DepthCommand`（定深/着底）Action；而 `/chassis_command.mode=4` 是普通自主航行过程中的定深保持，
当前 `mode=5` 预留，`navi_mode=1` 仅给出定深依赖。旧版 `navi_mode=2` 仍按 `HEIGHT_HOLD` 解码，
但不能覆盖当前 mode=2 的着底语义。Move 不能触发 T-Z Action 视图，也不能被标为独立垂向 Action。

`/system_run_states` 是可回放的 action 聚合状态，包含 owner、goal UUID、生命周期、垂向目标
及伴随水箱指令。DepthCommand/Move 的 ROS2 action status、feedback 是隐藏 topic；`ros2 bag
record -a` 不会记录它们。如需回放 action feedback（例如 progress），录制时必须额外使用
`--include-hidden-topics`。协议 `ActionState` 的公开聚合字段始终是主界面与 T-Z 的唯一必需
契约；`message`、原生 status、feedback progress 和最近终态仅为详情诊断字段，缺失时不得影响
主状态。Server 与本地 bag 仅在 UUID 匹配当前 action 时合并这些隐藏 topic 字段。
隐藏消息与公开聚合状态没有到达顺序保证：两端必须按 UUID 暂存原生 status/feedback，待对应的
`SystemRunStates` 到达后再合并；不能因隐藏消息先到而丢弃。Action status 使用 ROS2 的
`RELIABLE + TRANSIENT_LOCAL` QoS，feedback 使用 `RELIABLE` QoS。

robot_ws 的 `SystemRunStates.goal_uuid` 是字符串，发布端按字节用 `%x` 拼接，会丢掉字节
前导零（例如 UUID 字节 `0x06` 变成 `"6"`），使其可能是 16~32 位的非 canonical hex；而隐藏
action topic 的 `goal_id` 是 16 字节 UUID，规范化后是 canonical 32 位 hex。二者直接字符串比较
会失配，因此 Server 与本地 bag 按 UUID 合并 status/feedback 时必须通过把 canonical 侧转成
同样丢零的 lossy 形式做对称比对（`RobotWsProtoConverter::sameGoalUuid` /
`RobotWsCdrDecoder::sameGoalUuid`），同时兼容修复前（丢零）与修复后（canonical）两种
robot_ws 输出。不要在读取 `goal_uuid` 时尝试把非 canonical 字符串补回 32 位——那是有歧义的。

## 单位和兼容

- 长度 m，速度 m/s，加速度 m/s²，时间戳 Unix epoch ns。
- `ChassisCommand`、`ChassisStates`、`SystemRunStates` 当前没有 ROS Header。其 `Header` 只填
  `server_receive_time_ns`（本地回放为 rosbag 记录时间）和 per-topic `sequence`，不伪造
  `source_time_ns`；因此发布端时间及源到 Server 延迟在 UI 中明确为不可用。
- 本地 CDR 对 `ChassisCommand` 按固定长度识别：当前布局为 64 字节，旧布局为 72 字节，后者在
  `heading` 后含已删除的 `dive_speed`。解码器读取该旧字段以保持后续执行器、浮力和声纳字段对齐，
  但不伪造无当前协议对应字段的展示值；其他长度直接拒绝。
- 经度/纬度为 WGS-84 度；USBL 本地位置为 m，且只作定位诊断。
- 角度 rad、角速度 rad/s；heading 东向 0、逆时针/左转正。
- UI 显示层才转成度和 °/s。
- 已使用 field number 永不复用；删除字段 `reserved`。
- 新 optional/repeated 字段可在同 major 演进；语义/单位/framing 不兼容变化提升 major。

v2 是可信局域网只读可视化协议，不包含写控制、TLS、认证、压缩或服务发现。
