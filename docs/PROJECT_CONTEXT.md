# AutoViz 项目上下文

本文是未来 ChatGPT/Codex 接手 AutoViz 时的事实入口。内容以
`feature/client-server` 当前源码为准；`main` 仍保留现场稳定的 ROS2 单体版本。

## 项目定位

AutoViz 是 Qt/C++ 机器人规划控制可视化工具，当前首先服务水下机器人现场调试。
它显示规划控制调试所需的车辆/机器人定位、底盘反馈、控制指令、障碍物、参考线、
规划路径、任务与系统状态，并特别支持 robot_ws 的垂向动作。

目标不是把 robot_ws 消息固定成 UI 接口，而是复用同一 UI 接入真实机器人、仿真和
日志回放。当前已完成第一阶段 robot_ws ROS2 Server 与纯 Qt Client。

## 当前技术栈

| 部分 | 当前基线 |
| --- | --- |
| 语言 | C++17 |
| Client UI | Qt 5.15 Widgets + Network |
| 协议 | protobuf 3（当前环境 3.12.4），proto2 optional |
| 传输 | 原始 TCP，4 字节大端长度前缀 |
| Server 网络 | Boost.Asio（当前环境 Boost 1.74） |
| Server ROS | ROS2 Humble、rclcpp、robot_ws `custom_msgs` |
| 构建 | Client/协议用 CMake 3.16+；Server 用 ament_cmake/colcon |

`AutoVizClient` 构建不查找 `rclcpp`、`custom_msgs` 或任何 ROS 包。只有
`AutoVizServer/src/autoviz_server` 需要 source ROS2 与 robot_ws。

## 当前版本状态

### 分支关系

- `main`：ROS2 Adapter 与 Qt 程序融合的现场稳定版本，已有实际调试价值。
- `feature/client-server`：当前分支，已实现第一阶段 C/S 最小闭环。
- 未经现场验证和人工合并决策，不得用 feature 分支直接替换 `main`。

### 当前 feature 架构

```text
ROS2 topics
    -> AutoViz Server / robot_ws Adapter
    -> 来源无关 protobuf snapshot/update
    -> TCP
    -> RemoteVisualizationSource
    -> ProtocolModelConverter
    -> DataManager
    -> SceneManager
    -> Qt UI
```

Server 缓存最新一帧，Client 首次连接/重连收到全量快照，之后消费通道增量。Server
会在 ROS topic 5 秒未更新后发送 `CLEAR`；Client 断线或 Server session 变化时也会
清空旧数据。`DataManager` 继续提供第二层本地新鲜度保护。

## 已实现功能

### Client 显示

- XY 视图：车辆、历史轨迹、全局路径、局部路径、参考线、障碍物。
- T-Z 垂向剖面：当前/目标深度、动作段、垂向历史和急停标记。
- 自动或手动选择 XY/T-Z 主视图。
- 图层开关、车辆居中、拖动、缩放、重置、主题和显示设置持久化。
- 运动总览、定位/控制/路径/Action/任务详情、ROS 来源 topic 状态和日志。
- 速度、航向与路径误差控制曲线。
- Server 地址/端口配置、自动重连、连接状态和来源描述。

### robot_ws Adapter

| ROS2 topic | 协议通道 |
| --- | --- |
| `/location` | `VehicleState`，含独立 `odom_z/depth/height` |
| `/targets/final_objects` | `ObstacleSet` |
| `/chassis_command` | `ControlCommand` / `VerticalCommand` |
| `/chassis_states` | `ChassisState`、Actuator、Battery、通用诊断 metric |
| `/system_run_states` | `ActionState` |
| `/task_params` | `TaskState` |
| `/local_path` | 带时间和运动学字段的 local `Trajectory` |
| `/global_path` | global `Trajectory` |

Server 将 `SystemRunStates.target_angular_velocity` 从 `deg/s` 统一为 `rad/s`；将
robot_ws 底盘反馈“左负右正”取反为协议统一的“逆时针/左转为正”。heading 为东向
`0`、逆时针为正。UI 显示出口仍统一转换成度和 `°/s`。

### 协议和网络

- proto2 optional，不使用 required。
- 1 个全量 `VisualizationSnapshot` 和显式 `UPSERT/CLEAR` 通道更新。
- ClientHello、ServerHello、SubscribeRequest、Heartbeat、ProtocolError。
- 4 字节大端长度 + protobuf Envelope，单帧上限 16 MiB。
- Server 默认最多 8 个 Client，1 秒心跳，5 秒连接/数据超时。
- v1 仅只读可视化；不存在任务下发或控制命令写接口。

## 当前限制

1. Server 当前只有 robot_ws ROS2 Adapter；Simulation/Log Adapter 尚未实现。
2. v1 是可信局域网协议，没有 TLS、认证、权限或公网部署能力。
3. 参考线已有协议和 Client 显示通道，但 robot_ws Adapter 尚无对应 ROS topic。
4. 当前没有背压分级、增量合并或高频通道丢帧策略；TCP 慢 Client 的长期行为需压测。
5. Windows Client 的目录和依赖边界已满足，但尚未在 Windows 工具链完成正式构建验证。
6. Client 和 Server 各保存并编译一份 proto；仓库内通过
   `tools/verify_proto_sync.cmake` 防止 schema 漂移。

## 开发底线

- 不修改或重写 `main` 的现场稳定历史；C/S 工作只在 feature 分支推进。
- Client 不得 include ROS 头、链接 ROS 或理解 topic/custom_msgs。
- ROS/custom_msgs 字段映射只能位于 Server Adapter。
- 协议必须表达来源无关的规划控制语义、单位、时间和清空行为。
- 新增显示字段先明确协议语义，同步两份 proto，再转换到
  `AutoVizClient/src/core/model/`，最后驱动 UI。
- 空数据、topic 超时、断线和 session 变化都必须清理旧状态。
