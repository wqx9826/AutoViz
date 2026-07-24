# AutoViz Protocol v1

唯一 schema 位于 `AutoVizProto/proto/autoviz/*.proto`。Client 和 Server 不保存副本，
而是分别从自身 `third_party/AutoVizProto` 链接 `AutoVizProto::AutoVizProto`。

## protobuf 与 TCP 为什么能一起工作

如果你已经会直接使用 TCP，可以把 protobuf 理解为“替你定义并编码结构体”的工具。
TCP 只传递可靠、有序的字节流，它不知道这些字节表示车辆速度还是轨迹，也不保留
`send()` 的消息边界。protobuf 正好负责把结构化对象变成字节并还原，但它不负责联网。

```text
Envelope 对象
  -> protobuf SerializeToString()
  -> payload 字节
  -> 前置 4 字节长度
  -> TCP
  -> 按长度取出完整 payload
  -> protobuf ParseFromArray()
  -> Envelope 对象
```

例如发送方调用一次 `send()`，接收方可能分三次 `read()` 才收到全部内容；两次发送也
可能在一次读取里粘在一起。`FrameCodec` 先读取固定 4 字节的大端长度 N，再等待 N
字节，解决拆包和粘包。得到完整 payload 后 protobuf 才负责解析字段。

`.proto` 只在编译时由 `protoc` 生成 `.pb.h/.pb.cc`。运行时不发送 `.proto` 文件；
双方依据相同 field number 解释 wire format。

## package、路径和版本

所有 schema 声明：

```proto
package autoviz;
```

因此 C++ 类型直接是 `autoviz::Envelope`。文件路径
`proto/autoviz/transport.proto` 让生成头位于 `autoviz/transport.pb.h`。路径和
package 是两个概念，但本项目特意都使用简洁的 `autoviz`，不采用
`autoviz.protocol.v1`。

协议 v1 通过 `ClientHello.protocol_major` / `protocol_minor` 协商，不通过 namespace
表达。这样源码命名保持稳定，wire 兼容性由明确的握手字段控制。

## TCP framing

```text
+----------------------+----------------------------+
| uint32 big-endian N  | N bytes protobuf Envelope |
+----------------------+----------------------------+
```

- 长度不包含 4 字节头。
- N 必须为 1..16 MiB；无效长度立即断开。
- 当前不压缩、不加密。
- TCP 保证有序可靠；应用层 framing 保证消息边界；protobuf 保证结构化编码。

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

- 当前协议版本为 1.0，major 不一致拒绝连接。
- 空 channel 列表表示订阅全部可用通道。
- 连接/重连先取全量快照，之后接收增量。
- `session_id` 标识 Server 生命周期，变化时 Client 必须清空旧数据。
- 心跳周期 1 秒；5 秒未收到数据视为连接失效。

## schema 分域

- `common.proto`：Header、几何和来源无关诊断键值。
- `vehicle.proto`：VehicleState、VerticalState、ChassisState、Actuator、Battery。
- `planning.proto`：PathPoint、TrajectoryPoint、Trajectory、ReferenceLine。
- `perception.proto`：Obstacle、ObstacleSet。
- `control.proto`：ControlCommand、VerticalCommand、ActionState、TaskState。
- `runtime.proto`：来源、topic/通道健康和 Server 诊断。
- `transport.proto`：channel、snapshot/update、握手、心跳和错误。

通道操作：

- `UPSERT`：替换该通道最新值。
- `CLEAR`：明确删除该通道，Client 不得保留上一帧。

## 单位和坐标

- 长度 m；速度 m/s；加速度 m/s²；jerk m/s³。
- 角度 rad；角速度 rad/s，字段名含 `_rad` / `_radps`。
- heading 东向为 0、逆时针为正。
- `odom_z`、`depth`、`height_above_bottom` 不混用。
- Header 使用 Unix epoch ns；轨迹相对时间使用 s。
- UI 显示层将 rad 转度、rad/s 转 `°/s`。

## 兼容规则

1. 已发布 field number 永不复用；删除字段使用 `reserved`。
2. 向后兼容新增使用 optional/repeated，禁止 required。
3. 语义、单位或方向变化不能静默复用旧字段号。
4. Client 忽略未知字段/可选通道，未知 protocol major 拒绝连接。
5. transport/framing 变化必须更新 AutoVizProto GTest 和本文档。

v1 只读，不包含任务下发、控制写入、TLS、认证、UDP、WebSocket、压缩或服务发现。
