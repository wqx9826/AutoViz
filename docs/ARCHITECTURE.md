# AutoViz 架构说明

本文区分当前已经存在的架构和未来目标架构。未来目标仅用于指导设计，当前 `main` 分支不实现 Client/Server 拆分。

## 当前架构

当前是 **Qt Client + ROS2 Adapter 融合** 的单体桌面程序。ROS2 适配层和 Qt UI 最终链接在同一个 `AutoViz` 可执行文件中。

```text
ROS2 topics
    |
    v
Ros2MsgSubsrcribe
    |  callback 中完成字段映射
    v
src/core/model/ 内部标准模型
    |
    v
DataManager
    |  mutex + VisualizationSnapshot
    v
MainWindow / QTimer 读取快照
    |
    +--> SceneManager --> VisualizationView（主视图）
    +--> ControlPanelWidget（控制曲线）
    +--> BottomStatusPanel（总览、详情、日志、Topic 状态）
```

### ROS 订阅层

`src/core/ros/` 负责创建订阅器、维护 ROS2 节点/执行器、接收 topic 并将消息字段转换为内部模型。当前 ROS2 适配器在 `start()` 中创建 8 个 subscription，在后台线程中运行 executor；`initialize()`、`start()`、`stop()` 支持既定生命周期和重复调用保护。

`MainWindow` 根据 `AUTOVIZ_ENABLE_ROS1` / `AUTOVIZ_ENABLE_ROS2` 选择后端。未启用 ROS 时使用 Mock 数据。ROS1 文件目前是接入占位，当前实际现场链路以 ROS2 为准。

### 数据模型层

`src/core/model/` 是渲染和 UI 的业务数据契约，主要包括：

- `VehicleLocation`、`VehicleChassisInfo`、`VehicleConfig`
- `Trajectory`、`ReferenceLine`
- `ObstacleList`
- `ControlCmd`
- `LocalizationStatus`、`ChassisRuntimeStatus`、`ControlCommandStatus`
- 路径、Action、任务、Topic 和运行模式状态

ROS 消息类型不应向 `SceneManager` 或 Qt UI 扩散。

### DataManager

`DataManager` 是当前线程边界和最新数据中心：

- ROS 回调线程通过 `set...` 接口写入。
- UI 主线程通过 `getSnapshot()` 获得完整快照。
- 内部使用 `std::mutex` 保护快照和通道更新时间。
- 通过运行状态字段和容器内容判断通道是否有数据。
- 对真实输入执行约 5 秒新鲜度过滤，过期状态会被清空。
- 维护历史轨迹、路径终点和运行模式推导。

### SceneManager

`SceneManager::updateScene(snapshot)` 是主视图刷新入口。它只接收 `VisualizationSnapshot`，根据主视图模式把标准模型转换为 Qt Graphics Scene 图元：

- XY 模式绘制车辆、历史轨迹、路径、参考线、障碍物和路径终点。
- 垂向模式将垂向动作段转换为时间-深度剖面数据。
- 图层可见性、车辆居中和自动适配由 SceneManager/VisualizationView 协同完成。

### Qt UI 层

`MainWindow` 组织窗口、菜单、刷新定时器、主题和显示配置。`VisualizationView` 负责主视图交互和网格绘制；`BottomStatusPanel` 负责运行总览、详情、日志和 Topic 状态；`ControlPanelWidget` 负责控制曲线和路径误差观察。

## 未来目标架构

目标是拆分为 **Linux Server + Cross Platform Client**。Linux Server 保留 ROS2 和现场数据源依赖，Client 只依赖稳定的通信协议和客户端渲染/UI 能力。

```text
ROS2       Simulation       Log
  |             |             |
  +-------------+-------------+
                v
             Adapter
                |
                v
          AutoViz Server
  采集 / 转换 / 新鲜度 / 缓存 / API
                |
          独立通信协议
                |
                v
          AutoViz Client
       UI / 渲染 / 用户交互
```

### Client 职责

- 负责 UI、渲染和用户交互。
- 消费 Server 提供的标准化数据和状态。
- 展示数据源、时间戳、新鲜度、错误和能力信息。
- 不包含 ROS2 消息头文件，不初始化 ROS2 节点，不理解 ROS2 topic 细节。

### Server 职责

- 负责数据采集、数据转换、ROS 适配和数据缓存。
- 将不同来源转换为与来源无关的内部数据契约。
- 负责消息时间戳、序列、有效性、新鲜度和异常状态。
- 对 Client 提供稳定的快照/流式数据接口及能力信息。
- 让 ROS2、仿真和日志回放可以通过相同的上层接口被 Client 消费。

### Server 内部适配器

Server 内部应采用 Adapter 模式，至少预留以下来源：

- `ROS Adapter`：接入真实 ROS2 数据。
- `Simulation Adapter`：接入仿真数据。
- `Log Adapter`：接入日志、回放或离线数据。

这些 Adapter 只负责来源适配，不应把来源专有类型暴露给协议或 Client。

## 架构演进原则

1. 当前版本保持稳定，优先保证 ROS2 可视化和现场调试能力。
2. 未来 C/S 架构必须通过 feature 分支开发，实施分支固定使用 `feature/client-server`。
3. 不要直接重构当前 `main` 工作版本，不要为了未来架构破坏现有 ROS2 链路。
4. Client 不应该依赖 ROS，包括 ROS2 runtime、消息头文件、topic 名称和 middleware 细节。
5. 通信协议应该独立于具体数据来源，不能把 ROS2 消息定义直接当成 Client API。
6. Server 输出应优先复用当前内部标准模型的语义，并明确单位、坐标系、时间戳和数据新鲜度。
7. 当前 `main` 分支只保留设计文档和稳定性维护；任何 C/S 代码、协议实现或通信框架引入都需要人工确认并在 `feature/client-server` 分支进行。

