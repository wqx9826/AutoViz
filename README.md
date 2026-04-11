# AutoViz

AutoViz 现已从“ROS 消息包管理工具”重构为“车辆规划控制数据可视化工具”。

当前版本的核心链路是：

1. 维护统一的内部标准数据结构
2. 通过 ROS1 / ROS2 订阅实现接入外部消息
3. 在订阅回调中把 ROS msg 直接转成内部模型
4. 由 `DataManager` 管理最新一帧数据
5. 主视图只消费内部模型，显示车辆、路径、参考线、障碍物

## 当前目录重点

```text
src/
├── app/
├── core/
│   ├── datacenter/          # 统一数据中心
│   ├── config/              # 基础 JSON 配置
│   ├── model/               # 内部标准模型
│   ├── render/              # 场景渲染
│   └── ros/                 # ROS1 / ROS2 订阅骨架
├── message/                 # 用户自定义消息头文件放置目录
├── ui/
│   └── charts/              # 控制曲线面板骨架
└── utils/
```

## 当前已完成

- 移除旧 ROS 消息包加载、复制、编译管理 UI 与依赖
- 建立新的内部标准模型：
  - `VehicleState`
  - `PathTypes`
  - `ObstacleTypes`
  - `ControlTypes`
- 车辆长宽/轴距由 `configs/vehicle_params.json` 管理
- 建立线程安全的 `DataManager`
- 建立 `RosMsgSubscribeBase / Ros1MsgSubsrcribe / Ros2MsgSubsrcribe`
- 主视图改为只依赖内部模型
- 主视图支持启动自动缩放、左键/中键/右键拖动、滚轮缩放
- 右侧面板改为显示控制面板
- 底部保留控制状态、控制曲线骨架、日志面板
- 默认使用 Mock 数据运行

## 当前数据流

```text
ROS1 / ROS2 回调
    -> Ros1MsgSubsrcribe / Ros2MsgSubsrcribe
    -> 直接转换为内部模型
    -> DataManager::setVehicleState / setGlobalPath / ...
    -> MainWindow 定时刷新
    -> SceneManager
    -> VisualizationView
```

说明：

- 当前不再保留 topic 配置 UI。
- 当前不强制 parser 中间层。
- 建议直接在 `Ros1MsgSubsrcribe.cpp` 或 `Ros2MsgSubsrcribe.cpp` 内部完成：
  - topic 名称维护
  - subscriber / subscription 初始化
  - callback
  - ROS msg 到内部模型的字段映射
  - `DataManager::set...` 回填

## 用户后续应修改的位置

- `src/message/`
  - 放置你自己的 ROS1 / ROS2 / custom 消息头文件
- `src/core/ros/Ros1MsgSubsrcribe.cpp`
  - 直接在代码中写 topic 名称
  - 补 ROS1 订阅与 callback
  - 在回调中把 ROS msg 转成内部模型
  - 直接调用 `dataManager()->setVehicleState(...)`
  - 如果某类数据当前没有 topic，也请主动写入空结构，例如 `setReferenceLine(model::ReferenceLine{})`
- `src/core/ros/Ros2MsgSubsrcribe.cpp`
  - 直接在代码中写 topic 名称
  - 补 ROS2 subscription 与 callback
  - 在回调中把 ROS msg 转成内部模型
  - 直接调用 `dataManager()->setGlobalPath(...)` 等接口
  - 如果某类数据当前没有 topic，也请主动写入空结构
- `src/core/ros/RosMsgSubsrcribeFactory.cpp`
  - 通过 `switch` 选择 ROS1 / ROS2 订阅实现
  - 未启用 ROS 编译开关时返回空指针，界面直接使用 `DataManager` 里的 Mock 数据
- `configs/vehicle_params.json`
  - 调整车辆长宽和轴距

## 构建

```bash
cmake -S . -B build
cmake --build build -j4
./build/AutoViz
```

开启 ROS adapter 骨架编译：

```bash
cmake -S . -B build -DAUTOVIZ_ENABLE_ROS1=ON
cmake -S . -B build -DAUTOVIZ_ENABLE_ROS2=ON
```

说明：

- 当前 ROS 订阅类仍是骨架，不强制依赖真实 ROS 库。
- 当前没有 topic 配置界面，topic 名称直接在 ROS1/ROS2 订阅代码里维护。
- 未启用 ROS 编译开关时，程序默认显示 `DataManager` 初始化的 Mock 数据。
- 进入 ROS 订阅模式后，会先清空全部可视化通道，避免继续显示旧的 Mock 数据。
- 后续如果你要真正接 ROS1 / ROS2，可在 `CMakeLists.txt` 中按注释补 `find_package(...)` 和链接逻辑。
