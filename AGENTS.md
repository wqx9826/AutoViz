# AutoViz Agent Notes

本文件是修改 AutoViz 前必须读取的协作约束。先读 `README.md`，再读 `docs/`。

## 工程定位与分支

AutoViz 是规划控制可视化调试工具。当前首先支持 robot_ws 水下机器人，但协议与 UI
必须可复用于常规车辆、仿真和日志回放。

- `main` 是 ROS2 与 Qt 融合的现场稳定版本，具有实际调试价值。
- `feature/client-server` 已实现第一阶段 C/S，当前修改应保持该分支边界。
- 未经人工验收，不把 feature 的架构直接重构回 main，也不删除 main 所需的遗留源码。

当前 feature 数据链路：

```text
ROS2/custom_msgs
  -> AutoVizServer/src/autoviz_server
  -> Server 本地 protobuf + TCP
  -> AutoVizClient/src/core/network
  -> AutoVizClient/src/core/model + DataManager
  -> SceneManager
  -> Qt UI
```

## 主要目录

- `AutoVizClient/`：可独立复制和构建的纯 Qt Client 工程。
- `AutoVizClient/proto/`：Client 自己编译的协议副本。
- `AutoVizClient/src/core/network/`：Client 网络连接和协议到内部模型转换。
- `AutoVizClient/src/core/model/`：UI 唯一业务数据契约。
- `AutoVizClient/src/core/datacenter/`：线程安全快照、历史轨迹和本地新鲜度。
- `AutoVizClient/src/core/render/`：SceneManager，消费内部 snapshot。
- `AutoVizClient/src/ui/`：Qt Widgets UI、图表和状态页。
- `AutoVizServer/`：独立 ROS2 workspace。
- `AutoVizServer/src/autoviz_server/`：ament Server 包和 robot_ws Adapter。
- `AutoVizServer/src/autoviz_server/proto/`：Server 自己编译的协议副本。
- `tools/verify_proto_sync.cmake`：仓库内两份 proto 的一致性检查。

## Client 规则

- `AutoVizClient` 必须可脱离仓库父目录独立构建，只依赖 C++17、Qt5
  Widgets/Network 和 protobuf。
- Client 禁止 include/link `rclcpp`、ROS msg、custom_msgs，禁止理解 ROS topic 名称。
- `RemoteVisualizationSource` 只处理 TCP、framing、握手、session、心跳和重连。
- `ProtocolModelConverter` 是 protobuf 到内部模型的唯一入口。
- UI/SceneManager 不得直接 include protobuf；继续只消费 `VisualizationSnapshot`。
- 断线、Server session 变化和 `CLEAR` 必须清空旧数据。

## Server 与 Adapter 规则

- ROS/custom_msgs 映射只允许位于 `AutoVizServer/src/autoviz_server`。
- Server 负责采集、转换、单位归一化、缓存、新鲜度和网络 API。
- Client v1 只读。不要在现有 Envelope 中偷偷加入任务/控制写接口。
- 当前 Server 可针对 robot_ws；未来 ROS/Simulation/Log Adapter 应输出同一协议语义。
- topic 名、端口、超时、最大 Client 和车辆参数保持可配置。
- Server 独立 launch，不修改 robot_ws launch，除非用户明确要求部署集成。

robot_ws 当前输入：

- `/location`
- `/targets/final_objects`
- `/chassis_command`
- `/chassis_states`
- `/system_run_states`
- `/task_params`
- `/local_path`
- `/global_path`

## 协议规则

- schema 分别位于 `AutoVizClient/proto/autoviz/protocol/v1/` 和
  `AutoVizServer/src/autoviz_server/proto/autoviz/protocol/v1/`，两份内容必须完全
  一致并由各自 CMake 直接编译。
- 协议变更后必须运行 `cmake -P tools/verify_proto_sync.cmake`。
- schema 使用 proto2 optional，禁止 required。
- framing 固定为 4 字节大端 payload 长度 + protobuf Envelope，最大 16 MiB。
- 新连接/重连使用全量 snapshot，持续数据使用 channel update。
- 空数据必须用 `CLEAR`，不能依赖 Client 保留或猜测上一帧。
- 已用 field number 不复用；不兼容语义/单位变化提升 protocol major。
- 协议字段必须来源无关。禁止镜像 custom_msgs 或把 ROS 字段路径当 API。
- 通用规划控制字段进入领域消息；来源特有硬件详情使用稳定 diagnostic key。

坐标和单位：

- heading 东向为 0，逆时针/左转为正。
- 协议与内部计算角度为 rad、角速度为 rad/s；UI 显示为度和 `°/s`。
- robot_ws `SystemRunStates.target_angular_velocity` 是 deg/s，Server 转 rad/s。
- robot_ws `ChassisStates.current_angular_velocity` 是左负右正，Server 取反。
- `odom_z`、`depth`、`height_above_bottom` 不得混用。

## DataManager 与 UI 规则

- 外部数据只能通过 `DataManager` 接口写入，不共享可变 snapshot。
- UI 主线程每 50ms 获取 snapshot；网络/Adapter 不直接操作 UI 或 SceneManager。
- 路径、参考线、障碍物通过容器判断显示数据；状态必须遵守新鲜度。
- 历史轨迹由 DataManager 维护，重连新 session 不得拼接旧会话轨迹。
- 运动总览保留任务、运动、控制下发/反馈、硬件健康、垂向、路径和感知告警。
- 细网格 1m、粗网格 5m；车辆尺寸来自 Server 参数，缺失时回退本地配置。
- 中文是当前主要用户界面语言。

## 构建与验证

Client（不 source ROS）：

```bash
cmake -S AutoVizClient -B build/client -DBUILD_TESTING=ON
cmake --build build/client -j4
ctest --test-dir build/client --output-on-failure
```

Server：

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash
colcon --log-base AutoVizServer/log build \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install
```

协议或网络改动至少验证：拆包/粘包、超长帧、握手、全量快照、增量、CLEAR、
断线重连和 session 变化。ROS 映射改动需要在 source robot_ws 后编译 Server。

# AutoViz Development Rules

## Architecture Rules

- 当前 `main` 保持 ROS2 可视化功能和现场稳定性。
- 跨平台架构通过 feature branch 开发，当前实施分支是 `feature/client-server`。
- 禁止直接重构或破坏当前 main 工作版本。

## Client Server Rules

- Client 只负责通信消费、内部模型、UI、渲染和用户交互。
- Server 负责数据采集、转换、缓存、来源适配和稳定 API。
- 通信协议必须独立于具体数据来源。

## Documentation Rules

以下变化必须同步更新 `docs/`：

- 架构变化；
- 数据接口或单位/坐标变化；
- 通信协议、field number、framing 或兼容策略变化；
- 新增可视化模块；
- Server Adapter 新增或删除 topic/数据源。
