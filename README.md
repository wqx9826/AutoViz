# AutoViz

AutoViz 当前的定位是一个面向车辆规划控制链路的可视化工具。

当前工程的主要工作只有一件事：把 ROS 输入数据转换成项目内部统一的数据结构，再把这些标准数据结构稳定地显示到主视图中。

## 1.项目主要工作

当前工程围绕下面这条链路工作：

```text
ROS1 / ROS2 topic
    -> Ros1MsgSubsrcribe / Ros2MsgSubsrcribe
    -> 回调中转换成内部标准模型
    -> DataManager 保存最新一帧数据
    -> MainWindow 定时读取快照
    -> SceneManager
    -> VisualizationView
```

核心原则是：

- 主视图不直接依赖 ROS msg
- 所有显示数据最终都要落到内部标准模型
- ROS1 / ROS2 只是输入来源
- 没有接入的字段，也应该显式写入空结构，避免保留旧数据

## 2.当前工程结构

```text
src/
├── app/
│   └── MainWindow.*                 # 主窗口、订阅初始化、界面刷新
├── core/
│   ├── config/                      # JSON 配置
│   ├── datacenter/                  # DataManager
│   ├── model/                       # 内部标准模型
│   ├── render/                      # SceneManager
│   └── ros/                         # ROS1 / ROS2 订阅实现
└── ui/                              # 主视图、面板、图表骨架
```

当前你真正需要关注的目录主要是：

- `src/core/model/`
- `src/core/datacenter/`
- `src/core/ros/`
- `src/core/render/`
- `src/app/`

## 3.内部标准模型

当前主视图只依赖这些内部模型：

- `VehicleState`
- `Trajectory`
- `ReferenceLine`
- `ObstacleList`
- `ControlCmd`

这些结构定义在：

- `src/core/model/VehicleState.h`
- `src/core/model/PathTypes.h`
- `src/core/model/ObstacleTypes.h`
- `src/core/model/ControlTypes.h`

ROS 回调的职责不是把消息直接画出来，而是把 ROS msg 填入这些结构，然后写入 `DataManager`。

## 4.DataManager 的作用

`DataManager` 是当前工程的数据中心，用于管理最新一帧可视化数据。

主要接口在：

- `setVehicleLocation(...)`
- `setVehicleChassisInfo(...)`
- `setVehicleConfig(...)`
- `setGlobalPath(...)`
- `setLocalPath(...)`
- `setReferenceLine(...)`
- `setObstacles(...)`
- `setControlCmd(...)`
- `getSnapshot()`

使用方式很直接：

- ROS 订阅线程在 callback 中调用 `set...`
- 主线程通过 `getSnapshot()` 拿到最新完整数据
- `SceneManager` 根据这份快照统一重绘

## 5.当前 ROS 接入方式

在：

- `src/core/ros/Ros1MsgSubsrcribe.cpp`
- `src/core/ros/Ros2MsgSubsrcribe.cpp`

里完成以下事情：

- 写死当前需要订阅的 topic 名称
- 创建 subscriber / subscription
- 在 callback 里做字段映射
- 直接调用 `dataManager()->set...`

这条路径最短，也最符合你现在的开发方式。

### 5.1结合当前 ROS2 工程的使用方式

`custom_msgs` 由已 source 的 ROS2 工作区提供；AutoViz 不保存消息头文件或动态库副本。

当前 `Ros2MsgSubsrcribe` 已经按常见 ROS2 节点写法组织：

- `initialize()`
  - 清空当前可视化数据
  - 创建 `rclcpp::Node`
- `start()`
  - 创建 subscription
  - 启动后台线程
  - 后台线程循环执行 `rclcpp::spin_some(...)`
- `stop()`
  - 停止线程
  - 回收 subscription
  - 回收 node

当前 ROS2 代码里维护的话题是：

- `/location`
- `/targets/final_objects`
- `/chassis_command`
- `/chassis_states`
- `/system_run_states`
- `/task_params`
- `/local_path`
- `/global_path`

也就是说，现在用户如果要继续补功能，主要就是补 `Ros2MsgSubsrcribe.cpp` 中四个 callback 的字段映射逻辑。

### 5.2用户应如何补充 ROS2 逻辑

建议直接按下面方式补：

1. 在 `Ros2MsgSubsrcribe.cpp` 中 include 你的消息头文件
2. 在 `start()` 中创建对应 topic 的 subscription
3. 在 callback 中把消息字段映射到内部模型
4. 映射完成后直接写入 `DataManager`

示意如下：

```cpp
void Ros2MsgSubsrcribe::callbackLocationMsg(const custom_msgs::msg::Location::ConstSharedPtr msg)
{
    if (dataManager() == nullptr || msg == nullptr) {
        return;
    }

    autoviz::model::VehicleLocation location;
    location.position.x = msg->odom_x;
    location.position.y = msg->odom_y;
    location.heading = msg->heading;
    location.speed = msg->velocity;

    dataManager()->setVehicleLocation(location);
}
```

其他几类数据也是同样思路：

- `/targets/final_objects` -> `ObstacleList`
- `/chassis_command` -> `ControlCmd`
- `/chassis_states` -> `VehicleChassisInfo`
- `/system_run_states` -> 运行状态
- `/task_params` -> 任务状态
- `/local_path` -> `Trajectory`
- `/global_path` -> `Trajectory`

其中 `/system_run_states.goal_uuid` 与 `/local_path.goal_uuid` 用于校验任务和局部路径绑定关系；`ChassisStates` 的履带电机、BMS、配电和心跳字段用于“运动总览”的硬件健康摘要及底盘详情。紧急上浮在界面中区分控制命令请求、Action 状态和底盘 CAN 实际执行；解除按钮仅显示当前采样，不能被解释为已经解除。底盘 ROS 话题在线不等于每个 CAN 从设备帧都具备独立的新鲜度，因此界面会区分“消息在线”和消息内的显式故障。

如果当前没有某类输入，也应该主动写空值，例如：

```cpp
dataManager()->setReferenceLine(autoviz::model::ReferenceLine{});
dataManager()->setControlCmd(autoviz::model::ControlCmd{});
```

这样界面不会继续显示旧的 mock 数据或者历史残留内容。

## 6.当前订阅与显示流程

当前上层调用流程在 `MainWindow` 中已经固定：

1. 创建 `DataManager`
2. 根据编译开关选择订阅后端
3. 创建 `Ros1MsgSubsrcribe` 或 `Ros2MsgSubsrcribe`
4. 调用 `initialize()`
5. 调用 `start()`
6. 主线程通过 `QTimer` 每 `50ms` 刷新一次界面

这里要注意：

- `start()` 只会在启动时调用一次
- 它不是周期性调用
- 后续真实数据更新依赖 ROS 回调线程持续写入 `DataManager`

当前 `Ros2MsgSubsrcribe` 已经补了基础生命周期保护：

- `m_running` 使用原子变量
- `start()` 支持防重复启动
- `stop()` 支持重复调用
- 析构时会自动调用 `stop()`

## 7.主视图说明

当前主视图已经完成以下约定：

- 主视图只消费内部标准模型
- 支持车辆、全局路径、局部路径、参考线、障碍物显示
- 支持拖动和平移
- 支持滚轮缩放
- 启动时按当前可见图层自动适配；远距离路径不会因原点参与取景而压缩车辆
- 支持车辆居中跟随，关闭后在数据范围明显变化时进行稳定的总览适配；拖动或缩放后保持手动视角，可通过“适配可见数据（F）”恢复

“运动总览”是运行监视面板，优先显示任务链路、当前运动、控制指令（下发/反馈）、硬件健康、垂向状态、路径和感知告警。总览遵循高效显示原则：单位和“当前/目标”“下发/反馈”等字段关系放在标签中，数值区只显示固定顺序的数据，不重复拼接字段说明；未解码的原始底盘、BMS、电机和配电通道码放在详情页或图表中。topic 超时后对应旧的运行状态会被清除，避免继续显示过期数据。

坐标约定：

- 业务 `heading`：东向为 `0`，逆时针为正
- 主视图显示角度：水平向右为 `0`，逆时针为正
- UI 中所有角度、航向、俯仰、横滚、yaw 及角速度统一使用度显示；角速度使用 `°/s`
- 原始数据若为弧度或 `rad/s`，必须在 UI/图表显示出口转换为度或 `°/s`

网格约定：

- 近景细网格：`1m`；近景粗网格：`5m`
- 远景按 `5` 倍递进提高网格间距（如 `5m/25m`、`25m/125m`），基础网格仍为 `1m/5m`

车辆尺寸目前来自：

- `configs/vehicle_params.json`

## 8.构建方式

推荐使用单独构建目录：

```bash
mkdir -p build
cd build
cmake ..
make -j4
```

运行：

```bash
./AutoViz
```

## 9.启用 ROS 后端构建

如果要编译 ROS1 或 ROS2 对应实现，可以在 `cmake` 阶段打开开关。

启用 ROS1：

```bash
mkdir -p build && cd build
cmake .. -DAUTOVIZ_ENABLE_ROS1=ON
make -j4
./AutoViz
```

启用 ROS2：

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
mkdir -p build && cd build
cmake .. -DAUTOVIZ_ENABLE_ROS2=ON
make -j8
./AutoViz
```

`custom_msgs` 由 `find_package(custom_msgs REQUIRED)` 从已 source 的 ROS2 工作区中查找。
AutoViz 不保存任何 ROS 消息头文件或动态库副本；消息包更新后只需重新 source 对应工作区并
重新执行 CMake 配置，避免头文件、动态库与车辆发布端的消息定义不一致。

说明：

- `AUTOVIZ_ENABLE_ROS1` 与 `AUTOVIZ_ENABLE_ROS2` 当前是互斥的，不能同时打开
- 工程内部始终把这两个宏定义为 `0/1`
- 代码中统一使用：
  - `#if AUTOVIZ_ENABLE_ROS1`
  - `#if AUTOVIZ_ENABLE_ROS2`

这样即使不开某个后端，编辑器和编译器也仍然能检查对应分支的语法结构，不会因为“宏未定义”而把整段代码完全跳过。

## 10.非 ROS 模式运行

如果没有开启 ROS 编译开关：

- 不会创建 ROS 订阅对象
- 程序默认使用 `DataManager` 初始化的 mock 数据, 仅用于可视化展示示例
- 这样可以先验证渲染和界面框架

## 11.后续建议优先补充的逻辑

当前下一步最值得继续做的是：

- 根据控制消息补 `ControlCmd`
- 如果 ROS1 也要支持，再按同样模式补 `Ros1MsgSubsrcribe.cpp`

当前阶段，最重要的不是再加配置页面，而是把 ROS 输入稳定、清晰地落到内部标准模型中。
