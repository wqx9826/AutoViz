# AutoViz 项目上下文

本文是 AutoViz 的长期上下文入口。内容以当前 `main` 分支和当前源码为准，记录已经存在的能力、当前边界以及不能被未来设计破坏的约束。未来的 Client/Server 设计目前只作为演进方向，不代表已经实现。

## 项目定位

AutoViz 是一个基于 Qt/C++ 的机器人可视化工具，当前主要用于水下机器人现场调试。当前版本以 ROS2 数据可视化为主，将 ROS2 消息转换为项目内部标准模型，再把标准模型稳定显示到主视图、运行状态面板和控制曲线中。

它当前是面向规划、控制、底盘和感知链路的现场调试程序，不是已经完成跨平台部署的通用可视化客户端。

## 当前技术栈

| 项目 | 当前实现 |
| --- | --- |
| 语言 | C++17，CMake 中强制 `CMAKE_CXX_STANDARD 17` |
| UI | Qt5 Widgets；CMake 只要求 `Qt5::Widgets`，没有锁定 Qt5 的 minor 版本 |
| ROS2 | 以 ROS2 Humble 环境为当前构建和运行基线 |
| ROS2 依赖 | `rclcpp`、`nav_msgs`、`geometry_msgs`、`std_msgs`、外部工作区提供的 `custom_msgs` |
| 构建 | CMake 3.16 或更高版本，通常使用 Make 构建 |
| 配置 | `configs/vehicle_params.json` 提供车辆长度、宽度和轴距 |
| ROS 编译开关 | `AUTOVIZ_ENABLE_ROS1` 与 `AUTOVIZ_ENABLE_ROS2` 互斥，代码统一使用 `#if` 判断 |

普通非 ROS 构建和 ROS2 构建示例：

```bash
mkdir -p build
cd build
cmake ..
make -j4
./AutoViz
```

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
mkdir -p build
cd build
cmake .. -DAUTOVIZ_ENABLE_ROS2=ON
make -j4
./AutoViz
```

`custom_msgs` 不保存在 AutoViz 仓库中，必须由已经 source 的 ROS2 工作区提供。ROS1 目录和编译开关仍然存在，但 `Ros1MsgSubsrcribe` 当前主要是接入骨架，不能视为与 ROS2 等价的已完成后端。

## 当前版本状态

### 版本判断

当前版本：**ROS2 强耦合版本**。

当前程序、ROS2 订阅器和 Qt UI 在同一个可执行程序中。虽然 UI 和渲染层遵守“消费内部标准模型、不直接消费 ROS msg”的代码边界，但整个应用的构建和运行仍然依赖 ROS2 及 `custom_msgs`。

当前版本正在大量现场测试，已经具备实际调试价值。稳定性、数据新鲜度和现场可诊断性优先于未来架构的抽象完整性。未来的 C/S 方案不能通过破坏当前版本来实现。

### 当前主数据链路

简化链路如下：

```text
ROS2 Subscriber
        |
        v
DataManager
        |
        v
SceneManager
        |
        v
Qt UI
```

实际执行链路还包括消息转换和刷新调度：

```text
ROS2 topic
    -> Ros2MsgSubsrcribe callback
    -> 内部标准模型
    -> DataManager（互斥锁保护的最新快照）
    -> MainWindow 的 QTimer（50ms）
    -> SceneManager
    -> VisualizationView / 状态面板 / 控制曲线
```

`MainWindow` 负责创建 `DataManager`、选择订阅后端、启动订阅器并定时读取快照。ROS2 回调线程只负责转换和写入数据中心，不直接操作 Qt UI 或 `SceneManager`。

## 已实现功能

以下内容是从当前源码确认存在的能力，不包含未来规划：

### ROS2 数据接入

当前 ROS2 订阅器维护以下话题：

| Topic | 内部模型/状态 |
| --- | --- |
| `/location` | `VehicleLocation`、`LocalizationStatus` |
| `/targets/final_objects` | `ObstacleList` |
| `/chassis_command` | `ControlCmd`、`ControlCommandStatus` |
| `/chassis_states` | `VehicleChassisInfo`、`ChassisRuntimeStatus`，包括履带电机、BMS、配电和心跳摘要 |
| `/system_run_states` | `ActionRuntimeStatus` |
| `/task_params` | `TaskRuntimeStatus` |
| `/local_path` | `Trajectory`、局部路径运行状态 |
| `/global_path` | `Trajectory`、全局路径运行状态和路径终点状态 |

任务 `goal_uuid` 和局部路径 `goal_uuid` 会进入内部状态，用于显示和检查任务/路径绑定关系。

### 内部数据中心

- `src/core/model/` 定义车辆、轨迹、参考线、障碍物、控制和运行状态等内部标准模型。
- `DataManager` 用 `std::mutex` 保护 `VisualizationSnapshot`，提供 ROS 线程写入和 UI 主线程读取的线程边界。
- 非 ROS 构建可以加载 Mock 数据，用于检查 UI 和渲染；Mock 数据不应与真实 ROS 数据混用。
- 真实输入按通道进行新鲜度处理。当前实时数据默认约 5 秒超时；定位、底盘、路径、参考线、障碍物、控制、任务和 Action 等过期数据会被清空，避免界面继续显示旧状态。
- 车辆历史轨迹由 `DataManager` 根据定位更新维护，并支持从 UI 清空。

### 主视图和渲染

- 俯视 XY 视图：显示车辆、历史轨迹、全局路径、局部路径、参考线和障碍物。
- 垂向剖面视图：显示垂向动作段的时间-深度（T-Z）趋势，支持当前深度、目标深度、起始深度和急停事件标记；动作结束或数据超时后可以冻结上一段垂向动作。
- 自动主视图模式会根据当前运行模式在 XY 与垂向剖面之间切换；也可以手动选择视图模式。
- 图层显示可以分别开关车辆、历史轨迹、全局路径、参考线、局部路径和障碍物，并通过 `QSettings` 保存。
- 支持车辆居中、自动适配、拖动/平移、滚轮缩放和视图重置。
- 网格约定为细格 `1m`、粗格 `5m`。
- 车辆尺寸从 `configs/vehicle_params.json` 读取。

### 运行状态、详情和图表

- “运动总览”显示任务链路、当前运动、控制指令下发/反馈、硬件健康、垂向状态、路径和感知告警。
- 详情页显示控制、路径、动作、垂向、底盘、电机、BMS、配电等更完整字段。
- ROS Topic 页显示消息数量、更新时间、年龄、频率和等待/超时状态。
- 日志页显示运行日志。
- 控制曲线面板提供速度跟踪、航向跟踪和路径误差曲线；支持暂停、清空、10/30/60 秒窗口和自动缩放。
- UI 中角度、航向、俯仰、横滚、yaw 以及角速度统一以度或 `°/s` 显示；内部计算可以继续使用弧度，但显示出口必须转换单位。
- 支持自动、浅色和深色主题。

## 当前限制

1. Qt 应用和 ROS2 适配器融合在同一程序中，CMake 直接查找 ROS2 和 `custom_msgs`，部署时必须准备 ROS2 环境及匹配的消息工作区。
2. Windows 部署困难：当前应用不能脱离 ROS2、`custom_msgs` 和 Linux 现场工作区作为独立客户端交付。
3. 当前话题名称、消息类型和字段映射在 ROS2 适配层中维护，尚未抽象成跨平台、跨数据源的网络接口。
4. 当前不适合作为不依赖 ROS 的通用可视化客户端；客户端/服务器拆分尚未实现。
5. ROS1 适配仍是占位骨架，不能把 ROS1 编译开关理解为完整的 ROS1 产品支持。
6. 当前没有 AutoViz Server、通信协议或 protobuf schema；`docs/PROTOCOL_DESIGN.md` 只记录设计原则。

## 开发底线

- 不要为了未来 C/S 架构直接重构当前 `main` 工作版本。
- 新输入先映射到 `src/core/model/`，再写入 `DataManager`，最后由快照驱动 UI。
- UI 和渲染层不得依赖 ROS 消息头文件。
- 无数据或超时数据必须写入/呈现为空结构，不能保留旧显示。
- 当前主线的首要目标仍是 ROS2 现场调试稳定性。

