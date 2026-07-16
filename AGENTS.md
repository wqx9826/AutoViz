# AutoViz Agent Notes

本文件记录 AutoViz 工程的固定背景知识和协作约定，供后续代码代理快速接续工作。修改本工程前应先阅读 `README.md` 和本文件。

## 工程定位

AutoViz 是面向车辆规划控制链路的可视化调试小工具。当前核心目标是把 ROS1 / ROS2 输入数据转换为项目内部统一模型，再稳定显示到主视图中。

主数据链路：

```text
ROS1 / ROS2 topic
    -> Ros1MsgSubsrcribe / Ros2MsgSubsrcribe
    -> 回调中转换成内部标准模型
    -> DataManager 保存最新一帧数据
    -> MainWindow 定时读取快照
    -> SceneManager
    -> VisualizationView
```

关键原则：

- 主视图不直接依赖 ROS msg。
- 所有显示数据最终都应落到 `src/core/model/` 下的内部标准模型。
- ROS1 / ROS2 只是输入适配层，不应把 ROS 消息类型向 UI、渲染层扩散。
- 没有接入的字段或通道，也应显式写入空结构，避免界面保留旧数据。

## 主要目录

- `src/app/`: `MainWindow`，负责主窗口、订阅初始化、定时刷新和 UI 面板组织。
- `src/core/model/`: 内部标准模型，是渲染层和数据中心的唯一业务数据契约。
- `src/core/datacenter/`: `DataManager`，保存最新一帧 `VisualizationSnapshot`。
- `src/core/ros/`: ROS1 / ROS2 订阅后端和字段映射。
- `src/core/render/`: `SceneManager`，把快照转换成主视图图元。
- `src/ui/`: `VisualizationView`、显示配置、日志、图表等 UI 组件。
- `src/core/config/`: JSON 配置读取，目前主要读取车辆尺寸。
- `configs/vehicle_params.json`: 车辆长度、宽度、轴距配置。

## 内部标准模型

主视图当前依赖以下模型：

- `VehicleState`: 车辆定位与底盘状态。
- `Trajectory`: 全局路径、局部路径。
- `ReferenceLine`: 参考线。
- `ObstacleList`: 障碍物列表。
- `ControlCmd`: 控制指令。

对应文件：

- `src/core/model/VehicleState.h`
- `src/core/model/PathTypes.h`
- `src/core/model/ObstacleTypes.h`
- `src/core/model/ControlTypes.h`
- `src/core/model/CommonTypes.h`

ROS 回调应该创建并填充这些结构，然后调用 `DataManager::set...`。不要在回调里直接操作 UI 或 `SceneManager`。

## DataManager 约定

`DataManager` 是线程边界和数据中心：

- ROS 订阅线程在 callback 中调用 `setVehicleLocation`、`setLocalPath` 等接口写入。
- UI 主线程通过 `getSnapshot()` 获取完整快照。
- 内部使用 `std::mutex` 保护快照，外部不要绕过接口共享可变状态。
- `resetVisualizationData(inputSource)` 会清空可视化数据并记录输入来源。
- `initializeMockData()` 仅用于非 ROS 模式示例显示。

状态字段由写入内容推导：

- path / reference line / obstacles 通过容器是否为空判断是否有数据。
- vehicle location、chassis、control cmd 通过关键字段是否非零或非默认判断是否有数据。

## ROS 接入约定

当前工程支持互斥的 ROS 后端编译开关：

- `AUTOVIZ_ENABLE_ROS1`
- `AUTOVIZ_ENABLE_ROS2`

两个开关不能同时打开。代码中统一使用：

```cpp
#if AUTOVIZ_ENABLE_ROS1
#if AUTOVIZ_ENABLE_ROS2
```

不要使用“宏是否定义”的写法，因为 CMake 会始终把两个宏定义为 `0/1`。

### ROS2 当前话题

`Ros2MsgSubsrcribe` 当前维护：

- `/location`
- `/scene`
- `/local_path`
- `/global_path`

主要补功能的位置是 `src/core/ros/Ros2MsgSubsrcribe.cpp` 的 callback 字段映射逻辑。

当前已映射：

- `/location` -> `VehicleLocation`
- `/local_path` -> `Trajectory`
- `/global_path` -> `Trajectory`

当前待重点完善：

- `/scene` -> `ObstacleList`
- 控制消息 -> `ControlCmd`
- 需要时补 `ReferenceLine`
- 若支持 ROS1，按同样模式完善 `Ros1MsgSubsrcribe.cpp`

### ROS2 生命周期

`Ros2MsgSubsrcribe` 的生命周期约定：

- `initialize()` 清空可视化数据并创建 `rclcpp::Node`。
- `start()` 创建 subscriptions，启动后台线程循环 `rclcpp::spin_some(...)`。
- `stop()` 停止线程并释放 subscriptions 和 node。
- 析构会调用 `stop()`。
- `start()` 需要支持重复调用保护。
- `stop()` 需要支持重复调用。

## UI 和渲染约定

- `MainWindow` 创建 `DataManager`，根据编译开关选择订阅后端。
- 主线程通过 `QTimer` 每 `50ms` 调用 `refreshVisualization()`。
- `SceneManager::updateScene(snapshot)` 是主视图刷新入口。
- `VisualizationView` 负责视图交互，如拖动、平移、滚轮缩放。
- 默认支持车辆居中视角和图层显示配置。

坐标和网格：

- 业务 `heading`: 东向为 `0`，逆时针为正。
- 主视图显示角度：水平向右为 `0`，逆时针为正。
- 细网格为 `1m`，粗网格为 `5m`。
- 车辆尺寸来自 `configs/vehicle_params.json`。

## 构建和运行

普通非 ROS 模式：

```bash
mkdir -p build
cd build
cmake ..
make -j4
./AutoViz
```

ROS1 模式：

```bash
mkdir -p build
cd build
cmake .. -DAUTOVIZ_ENABLE_ROS1=ON
make -j4
./AutoViz
```

ROS2 模式：

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
mkdir -p build
cd build
cmake .. -DAUTOVIZ_ENABLE_ROS2=ON
make -j4
./AutoViz
```

ROS2 模式通过 `find_package(custom_msgs REQUIRED)` 使用已 source 的 ROS2 工作区安装包。AutoViz 不保存 `custom_msgs` 的头文件或动态库副本；若消息包更新，重新 source 工作区并重新执行 CMake 配置即可。

## 修改建议

- 优先保持数据链路清晰：ROS callback -> 内部模型 -> `DataManager` -> snapshot -> `SceneManager`。
- 新增显示字段时，先扩展或复用 `src/core/model/` 的标准模型，再考虑渲染。
- 不要让 Qt UI、渲染层直接 include ROS 消息头。
- 不要在 callback 中直接访问 UI 对象。
- 不要把 mock 数据和真实 ROS 数据混在同一输入源下。
- 如果某个 ROS topic 暂无数据，显式写空结构以清掉旧显示。
- 保持 C++17、Qt5 Widgets、CMake 现有风格。
- 当前类名里已有 `Subsrcribe` 拼写，除非做全局重命名任务，否则不要局部改名造成接口不一致。

## 代码风格提示

- 命名空间主要使用 `autoviz::model`、`autoviz::datacenter`、`autoviz::ros`、`autoviz::render`。
- 现有 UI 文案多为中文，新增用户可见文案保持一致。
- 现有工程使用 Qt 容器和 Qt 类型较多，如 `QVector`、`QString`。
- 日志使用 `Logger::instance()`。
- 构建系统当前只要求 Qt5 Widgets；ROS 依赖只在对应开关打开时查找。
