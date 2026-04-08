# AutoViz

AutoViz 是一个基于 C++17 / Qt5 Widgets 的桌面工具，用于逐步接入 ROS 消息包、完成消息类型构建，并为后续无人车规划控制数据可视化提供运行基础。

当前版本的重点不是 topic 绑定和消息解析，而是先把下面这条链路做正确：

1. 启动时检测 ROS1 / ROS2 环境
2. 加载一个或多个标准 ROS 消息包目录
3. 校验消息包是否合法
4. 复制消息包到 AutoViz 内部工作区
5. 根据 ROS1 / ROS2 执行真实编译命令
6. 在界面和日志中展示编译结果

## 当前已实现

- Qt5 主窗口、二维视图、对象详情、控制状态、日志面板
- 中文菜单和中文日志输出
- 启动时自动检测 ROS 环境
- 支持识别：
  - `ROS_VERSION=1` -> ROS1
  - `ROS_VERSION=2` -> ROS2
  - 辅助检测 `roscore`、`catkin_make`、`colcon`
- 左侧“消息包管理面板”
- 支持一次选择多个消息包目录
- 标准消息包校验：
  - 必须包含 `package.xml`
  - 必须包含 `CMakeLists.txt`
  - 必须包含 `msg/`
  - `msg/` 中至少有一个 `.msg`
- 将合法消息包复制到内部工作区：
  - ROS1: `runtime/catkin_ws/src/`
  - ROS2: `runtime/ros2_ws/src/`
- 同名包覆盖确认
- 异步执行真实构建命令，不阻塞 UI
- 实时采集构建 stdout / stderr 并写入日志面板
- 在表格中展示复制状态和编译状态
- 中央视图欢迎态根据“未加载 / 已加载未编译 / 已编译成功”切换提示

## 当前界面说明

### 菜单

- `文件 -> 加载 ROS 消息包`
- `文件 -> 删除选中消息包`
- `视图 -> 重置视图`
- `视图 -> 恢复默认布局`

菜单栏下方重复按钮已经移除，不再显示第二排“加载 ROS 消息包 / 重置视图”按钮。

### 消息包管理面板

左侧面板已从“话题配置面板”调整为“消息包管理面板”，当前职责如下：

- 显示当前 ROS 环境
- 显示当前工作区路径
- 显示 `roscore`、`catkin_make`、`colcon` 可用状态
- 显示消息包列表
- 提供“添加消息包”“删除选中包”“编译消息包”

表格列包括：

- 启用
- 包名
- 原始路径
- 工作区路径
- 消息数
- ROS 适配
- 复制状态
- 编译状态

## ROS 环境识别逻辑

AutoViz 启动后会自动检测一次 ROS 环境。

优先级如下：

1. 读取环境变量 `ROS_VERSION`
2. 读取 `ROS_DISTRO`
3. 检查命令是否存在：
   - `roscore`
   - `catkin_make`
   - `colcon`

识别规则：

- `ROS_VERSION=1`：判定为 ROS1
- `ROS_VERSION=2`：判定为 ROS2
- 如果没有 `ROS_VERSION`，但存在 `roscore` 或 `catkin_make`：推断为 ROS1
- 如果没有 `ROS_VERSION`，但存在 `colcon`：推断为 ROS2
- 都没有：判定为未检测到 ROS

未检测到 ROS 时：

- 状态栏显示“未检测到 ROS 环境”
- 日志面板写入中文说明
- “编译消息包”按钮禁用

## 消息包加载与校验流程

用户通过“文件 -> 加载 ROS 消息包”选择一个或多个目录后，程序按以下流程处理：

1. 校验所选路径是否为目录
2. 检查 `package.xml`
3. 检查 `CMakeLists.txt`
4. 检查 `msg/` 目录
5. 统计 `.msg` 文件数量
6. 提取包名
   - 默认使用目录名
   - 若 `package.xml` 中存在 `<name>`，则以其为准
7. 将合法包复制到内部工作区

非法包不会加入列表，并会：

- 弹出中文 `QMessageBox`
- 在日志面板记录失败原因

## 内部工作区设计

内部工作区位于项目根目录下的 `runtime/`：

- ROS1：`runtime/catkin_ws`
- ROS2：`runtime/ros2_ws`

其中源包目录统一放在：

- ROS1：`runtime/catkin_ws/src/<package_name>`
- ROS2：`runtime/ros2_ws/src/<package_name>`

如果工作区中已存在同名包：

- 程序弹出覆盖确认
- 用户确认后删除旧目录并复制新目录
- 用户取消则不覆盖，并记录“用户取消覆盖”

`runtime/` 已加入 `.gitignore`，不会被提交。

## 编译命令

### ROS1

工作目录：

- `runtime/catkin_ws`

构建命令：

```bash
source /opt/ros/<distro>/setup.bash && if [ ! -f src/CMakeLists.txt ]; then (cd src && catkin_init_workspace); fi && catkin_make
```

如果 `/opt/ros/<distro>/setup.bash` 不存在，则退化为直接执行后续初始化与 `catkin_make`。

说明：

- 编译 ROS1 消息包通常不要求 `roscore` 正在运行
- 但后续订阅和运行节点时仍需要 `roscore`

### ROS2

工作目录：

- `runtime/ros2_ws`

构建命令：

```bash
source /opt/ros/<distro>/setup.bash && colcon build --packages-select <包名列表>
```

如果 `setup.bash` 不存在，则退化为直接执行：

```bash
colcon build --packages-select <包名列表>
```

## 编译状态与日志

构建使用 `QProcess` 异步执行，不阻塞主线程。

支持的编译状态：

- 未编译
- 编译中
- 编译成功
- 编译失败

关键日志均为中文，包括：

- 主窗口初始化完成
- 检测 ROS 环境开始
- 检测到 ROS1 / ROS2 / 未检测到 ROS
- 加载消息包目录
- 校验消息包成功 / 失败
- 检测到 `package.xml`
- 检测到 `CMakeLists.txt`
- 检测到 `msg` 目录
- 检测到 `.msg` 文件数量
- 复制消息包到工作区成功 / 失败
- 工作区初始化完成
- 开始编译消息包
- 执行的构建命令
- 编译成功 / 编译失败
- 删除消息包
- 用户取消覆盖

## 目录结构

```text
AutoViz/
├── CMakeLists.txt
├── README.md
├── configs/
├── runtime/                  # 运行期工作区，已忽略提交
├── src/
│   ├── app/
│   ├── core/
│   │   ├── datasource/
│   │   ├── model/
│   │   ├── render/
│   │   └── ros/              # 新增：ROS 环境、校验、工作区、构建模块
│   ├── ui/
│   └── utils/
└── build/
```

## 构建与运行

在项目根目录执行：

```bash
cmake -S . -B build
cmake --build build -j4
./build/AutoViz
```

如果只是验证程序是否能启动，也可以使用离屏模式：

```bash
QT_QPA_PLATFORM=offscreen ./build/AutoViz
```

## 测试 `plan_control` 消息包

假设目录结构如下：

```text
plan_control/
├── CMakeLists.txt
├── package.xml
└── msg/
    ├── M_Control.msg
    ├── M_Localization.msg
    ├── M_Plan.msg
    ├── M_Route.msg
    ├── M_Task.msg
    ├── M_VehicleInfo.msg
    ├── route_point.msg
    └── trajectory_point.msg
```

测试步骤：

1. 先 `source` 你的 ROS1 或 ROS2 环境
2. 启动 `./build/AutoViz`
3. 确认左侧顶部显示正确的 ROS 环境，例如：
   - `当前 ROS 环境：ROS1 / noetic`
   - `当前 ROS 环境：ROS2 / humble`
4. 点击“添加消息包”
5. 选择 `plan_control` 根目录
6. 确认表格中：
   - 包名为 `plan_control`
   - 消息数为 `8`
   - 已复制到对应 `runtime/.../src/plan_control`
7. 点击“编译消息包”
8. 查看日志面板和表格中的编译结果

## 当前尚未实现

本轮没有继续做以下内容：

- topic 绑定设计
- 消息字段提取
- 路径 / 障碍物 / 控制量业务解析
- 运行时 ROS 订阅
- 运行时消息反射或动态类型支持装载

这些内容会在后续迭代中继续实现。
