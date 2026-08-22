# AutoViz Agent Notes

本文件是修改 AutoViz 前必须读取的协作约束。先读 `README.md`，再读 `memory/`。

## 工程定位与分支

AutoViz 是规划控制可视化调试工具。当前首先支持 robot_ws 水下机器人，但协议与 UI
必须可复用于常规车辆、仿真和日志回放。

- `main` 是 ROS2 与 Qt 融合的现场稳定版本。
- `feature/client-server` 实施 C/S 架构，未经人工验收不得重构回 main。
- 不删除 main 所需的遗留源码，不让 feature 的未提交工作影响 main。

当前 feature 数据链路：

```text
ROS2/custom_msgs
  -> AutoVizServer/src/autoviz_server
  -> AutoVizProto: protobuf + FrameCodec
  -> TCP
  -> AutoVizClient/src/core/network
  -> DataManager -> SceneManager -> Qt UI
```

## 主要目录

- `AutoVizProto/`：唯一协议工程；只依赖 C++17、protobuf，提供
  `AutoVizProto::AutoVizProto`。
- `AutoVizProto/proto/autoviz/*.proto`：唯一 schema，禁止在 Client/Server 中复制。
- `AutoVizClient/`：可独立复制和构建的纯 Qt Client。
- `AutoVizClient/src/core/network/`：TCP 连接和协议到内部模型转换。
- `AutoVizClient/src/core/model/`：UI 唯一业务数据契约。
- `AutoVizClient/src/core/datacenter/`：线程安全快照、历史轨迹和本地新鲜度。
- `AutoVizClient/src/core/render/`、`src/ui/`：渲染和 Qt UI。
- `AutoVizServer/`：独立 ROS2 workspace。
- `AutoVizServer/src/autoviz_server/`：ament Server 包和 robot_ws Adapter。

## 依赖边界

- Client 和 Server 必须用 `find_package(AutoVizProto CONFIG REQUIRED)` 消费各自
  `third_party/AutoVizProto` 下的 SDK，不得引用 `../AutoVizProto` 之类源码路径。
- 两个消费工程的 CMake 必须默认搜索自身 `third_party`，不得要求设置系统环境变量；
  非标准位置只通过 `AUTOVIZ_THIRD_PARTY_DIR` 显式覆盖。
- AutoVizProto 不得依赖 Qt、ROS、Boost 或 custom_msgs。
- Client 只依赖 C++17、Qt5 Widgets/Network、protobuf/AutoVizProto；禁止 include/link
  ROS、custom_msgs，也禁止理解 ROS topic 名。
- ROS/custom_msgs 映射只允许位于 Server。
- UI/SceneManager 不得直接 include protobuf；`ProtocolModelConverter` 是 protobuf
  到内部模型的唯一入口。
- `RemoteVisualizationSource` 只处理 TCP、framing、握手、session、心跳和重连。

## Server 与 Adapter 规则

- Server 负责采集、转换、单位归一化、缓存、新鲜度和网络 API。
- Client v1 只读，不在现有 Envelope 中加入任务/控制写接口。
- 当前 Server 可针对 robot_ws；未来 Adapter 应输出同一协议语义。
- topic、端口、超时、最大 Client 和车辆参数保持可配置。
- Server 独立 launch，除非用户明确要求，否则不修改 robot_ws launch。

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

- schema 唯一位于 `AutoVizProto/proto/autoviz/*.proto`。
- proto package 固定为 `autoviz`，生成 C++ namespace 为 `autoviz`；不使用
  `autoviz.protocol.v1` 或 `autoviz::protocol::v1`。
- schema 使用 proto2 optional，禁止 required。
- framing 固定为 4 字节大端 payload 长度 + protobuf Envelope，最大 16 MiB。
- v2 新连接/重连发送全量 snapshot；不使用 SubscribeRequest、ChannelUpdate、UPSERT 或 CLEAR。
- 快照中 optional 字段缺失代表当前无数据；断线和 Server session 变化必须清空旧数据。
- 已用 field number 不复用；不兼容语义/单位变化提升握手中的 protocol major。
- 字段必须来源无关，不镜像 custom_msgs，不把 ROS 字段路径作为 API。

坐标和单位：

- heading 东向为 0，逆时针/左转为正。
- 协议与内部计算角度为 rad、角速度为 rad/s；UI 显示为度和 `°/s`。
- robot_ws `SystemRunStates.target_angular_velocity` 是 deg/s，Server 转 rad/s。
- robot_ws `ChassisStates.current_angular_velocity` 按原始值透传；它是驾驶员视角的转向量，
  不当作 ENU 偏航角速度进行符号转换。
- `odom_z`、`depth`、`height_above_bottom` 不得混用。

## DataManager 与 UI 规则

- 外部数据只能通过 `DataManager` 写入，不共享可变 snapshot。
- UI 主线程每 50ms 获取 snapshot；网络/Adapter 不直接操作 UI 或 SceneManager。
- 远程 Server 与本地 rosbag 是同一业务数据契约的两个输入：所有被 Client 模型或 UI 消费的
  `VisualizationSnapshot` 字段，必须由 Server 快照和 `RobotWsCdrDecoder` 以相同语义、单位和
  optional 缺失规则提供，并统一经过 `ProtocolModelConverter -> DataManager -> UI`。严禁出现仅
  Server 或仅 bag 可显示的字段；协议、Adapter、CDR decoder、Converter、模型或 UI 变更必须同步
  审查两条链路并补充等价验证。
- 历史轨迹由 DataManager 维护，新 session 不得拼接旧会话轨迹。
- 细网格 1m、粗网格 5m；车辆尺寸优先使用 Server 参数，缺失时回退本地配置。
- 中文是当前主要用户界面语言。

## 构建与验证

先构建安装 AutoVizProto，再分别构建 Client 和 Server：

```bash
./AutoVizProto/scripts/bootstrap_proto.sh

cmake -S AutoVizClient -B AutoVizClient/build
cmake --build AutoVizClient/build -j4
```

Linux 下 Proto 必须使用上述脚本，并只在 `AutoVizProto/build` 构建；Windows
使用 `AutoVizProto/scripts/bootstrap_proto.ps1`。Client 只在 `AutoVizClient/build` 构建。
禁止在 feature 项目根目录创建 `build/` 或编译任何子工程。

Server 须在 `AutoVizServer` 目录 source ROS2 和 robot_ws 后使用 `colcon build`；默认从
`AutoVizServer/third_party/AutoVizProto` 查找协议 SDK。Client 不使用 CTest；协议
GTest 归 AutoVizProto。

协议或网络变更至少验证：拆包/粘包、超长帧、握手、全量快照、字段缺失清空、断线重连
和 session 变化。ROS 映射变更需要编译 Server。

## 文档规则

架构、接口、单位/坐标、通信协议、field number、framing、兼容策略、可视化模块或
Server 数据源变化，必须同步更新 `memory/` 及对应工程 README。
