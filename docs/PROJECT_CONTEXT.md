# AutoViz 项目上下文

本文以 `feature/client-server` 当前源码为准；`main` 保留现场稳定的 ROS2 单体版本。

## 项目定位

AutoViz 是 Qt/C++ 规划控制可视化工具，当前首先服务 robot_ws 水下机器人。目标是让
同一 UI 以后也能接入常规车辆、仿真和日志回放，而不是把 custom_msgs 固化成 UI API。

## 三工程结构

| 工程 | 职责 | 主要依赖 |
| --- | --- | --- |
| AutoVizProto | 唯一 schema、生成 C++、TCP FrameCodec、协议 GTest | C++17、protobuf、测试时 GTest |
| AutoVizClient | TCP 消费、内部模型、DataManager、渲染和 Qt UI | C++17、Qt5、AutoVizProto |
| AutoVizServer | robot_ws ROS Adapter、转换、缓存、新鲜度、TCP 服务 | ROS2、custom_msgs、Boost、AutoVizProto |

AutoVizProto 构建为包含 `include/`、`lib/` 的第三方 SDK。Client/Server 分别从自身
`third_party/AutoVizProto` 查找，不依赖环境变量或兄弟源码目录。唯一 schema 位于
`AutoVizProto/proto/autoviz/*.proto`，package 为 `autoviz`，生成 C++ 类型例如
`autoviz::Envelope`。

## 当前数据链路

```text
ROS2 topics
  -> AutoVizServerNode（映射、单位归一化、快照、新鲜度）
  -> TcpServer（握手、订阅、心跳、广播）
  -> AutoVizProto framing + protobuf
  -> RemoteVisualizationSource
  -> ProtocolModelConverter
  -> DataManager
  -> SceneManager / Qt UI
```

Server 缓存最新一帧。首次连接或重连发送全量快照，之后发送通道增量。ROS topic
超时发送 CLEAR；Client 断线或 Server session 变化时清空旧数据。

## 当前技术基线

- C++17、CMake 3.16+。
- Client：Qt 5.15 Widgets/Network，不依赖 ROS。
- 协议：protobuf 3、proto2 optional、4 字节大端长度前缀。
- Server：ROS2 Humble、rclcpp、Boost.Asio、robot_ws custom_msgs。
- 测试：AutoVizProto 提供直接运行的 GTest，不使用 CTest；Client 不含测试目标。

## 已实现功能

- XY：车辆、历史轨迹、全局/局部路径、参考线、障碍物。
- T-Z：当前/目标深度、动作段、垂向历史和急停标记。
- 图层、居中、拖动、缩放、主题、显示设置。
- 运动总览、定位/控制/路径/Action/任务详情、来源 topic 状态和日志。
- Server 地址/端口、自动重连、session 与连接状态。

robot_ws 映射：

| ROS2 topic | 协议通道 |
| --- | --- |
| `/location` | VehicleState |
| `/targets/final_objects` | ObstacleSet |
| `/chassis_command` | ControlCommand / VerticalCommand |
| `/chassis_states` | ChassisState / Actuator / Battery |
| `/system_run_states` | ActionState |
| `/task_params` | TaskState |
| `/local_path` | local Trajectory |
| `/global_path` | global Trajectory |

Server 将目标角速度 deg/s 转为 rad/s，将底盘反馈“左负右正”取反为统一的“左转为正”。
heading 东向为 0、逆时针为正；UI 显示出口再转为度与 `°/s`。

## 当前限制

1. 只有 robot_ws ROS2 Adapter，尚无 Simulation/Log Adapter。
2. v1 面向可信局域网，没有 TLS、认证或权限。
3. 参考线已有协议和 Client 显示，robot_ws 尚无输入 topic。
4. 尚无背压分级、增量合并或高频丢帧策略。
5. Client/AutoVizProto 的边界适合 Windows，但尚未完成正式 Windows 构建验证。
6. AutoVizProto 目前与三工程同仓；未来可拆为独立仓库/submodule/SDK。

## 开发底线

- C/S 工作留在 feature，未经人工验收不覆盖 main。
- Client 不 include/link ROS，不理解 topic/custom_msgs。
- ROS 字段映射只在 Server。
- schema 只在 AutoVizProto 修改；不在 Client/Server 创建副本。
- 协议表达来源无关语义、单位、时间、清空与兼容行为。
- 新显示字段先改 schema/测试，再做 Server 映射、Client 转换和 UI。
