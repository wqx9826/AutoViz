# AutoVizServer

AutoVizServer 是独立 ROS2 workspace。当前 Adapter 读取 robot_ws 的八个 topic，转换为
来源无关的 AutoViz Protocol v2 完整快照，并通过 TCP 只读发送给 Qt Client。

## 从 ROS 回调到 TCP 的固定数据流

```text
ROS callback
  -> RobotWsProtoConverter（纯字段转换、单位/方向归一化）
  -> SnapshotStore（各数据种类最新值、频率、超时、dirty）
  -> 50 ms publish timer（最高 20 Hz 合并）
  -> VisualizationServer::publishSnapshot()
  -> TcpSession -> FrameCodec -> Boost.Asio async_write
```

ROS 回调不联网，只做一次转换和一次缓存更新。网络层不 include ROS/custom_msgs。完整快照
缺少某个 optional 字段，表示该数据当前不存在；topic 超过 `topic_timeout_ms` 后，
`SnapshotStore` 移除该字段，Client 原子替换快照时自然清空，不需要 UPSERT/CLEAR 状态机。

## 对外网络接口

```cpp
VisualizationServer server;
server.start(config, identity, &error);
server.publishSnapshot(snapshot);
const auto count = server.clientCount();
server.stop();
```

异步 Asio 不提供轮询式 `RecvMsg()`：`start()` 已经注册 accept/read 回调，ClientHello、
版本检查、session、心跳和超时都由 `VisualizationServer` 自动处理。Node 只需要发布完整
快照。详细阅读顺序见 [Boost.Asio 指南](docs/BOOST_ASIO_GUIDE.md)。

## v2 连接流程

```text
Client                                      Server
  |----- TCP connect ------------------------>|
  |----- ClientHello (protocol 2.x) --------->|
  |<---- ServerHello + capability + session --|
  |<---- 最新 VisualizationSnapshot ----------|
  |<---- 后续完整 VisualizationSnapshot -------|
  |<---> Heartbeat -------------------------->|
```

- 握手前不发送快照；major 不兼容时返回 fatal `ProtocolError` 后断开。
- 新连接握手后立即得到最新完整快照。
- 心跳默认 1 秒；5 秒没有收到 Client 数据则关闭连接。
- 最大连接数默认 8。
- 慢 Client 的正在发送帧保留，排队中的旧快照由最新快照替换；握手、心跳和错误帧不丢。
- Server 每次启动生成新 `session_id`；Client 必须清空旧状态和历史轨迹。

## robot_ws Adapter

| topic | v2 数据 | 关键归一化 |
| --- | --- | --- |
| `/location` | VehicleState + UnderwaterState | odom_z/depth/离底高度保持独立 |
| `/targets/final_objects` | ObstacleSet | 只读取当前真实 ID、分类、中心、尺寸、有效标志 |
| `/chassis_command` | ControlCommand + UnderwaterCommand | 通用运动与水下命令分层 |
| `/chassis_states` | ChassisState + UnderwaterChassisState + PlatformDiagnostics | 反馈角速度取反为左转正 |
| `/system_run_states` | ActionState + UnderwaterCommand | 目标角速度 deg/s 转 rad/s |
| `/task_params` | TaskState + UnderwaterTaskState | 急停与解除紧急上浮 |
| `/local_path` | local Trajectory | pose、航向、速度、加速度、相对/绝对时间、goal ID、长度 |
| `/global_path` | global Trajectory | pose、四元数航向、长度 |

默认 capability 为通用 XY、垂向运动、水下系统和平台诊断。参考线仍是可选通用协议字段，
robot_ws 当前没有对应输入，不伪造 topic。

## 配置

`config/robot_ws.yaml` 提供 bind/port、`max_clients`、`publish_rate_hz`（默认 20）、
`topic_timeout_ms`（默认 5000）、八个 topic 和车辆长宽/轴距。Server 独立 launch，不修改
robot_ws launch。

## 构建和测试

先从仓库根目录安装协议 SDK，再构建 Server：

```bash
./AutoVizProto/scripts/bootstrap_proto.sh

source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
colcon --log-base AutoVizServer/log build \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install
colcon --log-base AutoVizServer/log test \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install
```

`robot_ws_converter_test` 覆盖八类消息和快照超时；`tcp_server_test` 覆盖动态端口、握手
隔离、完整快照、多 Client/上限、版本拒绝、心跳、超时、重启 session 与慢 Client 合并。

运行：

```bash
source AutoVizServer/install/setup.bash
ros2 launch autoviz_server autoviz_server.launch.py
```

本协议面向可信局域网可视化，不包含控制下发、TLS、认证、压缩或服务发现。
