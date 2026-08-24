# AutoViz 项目上下文

AutoViz 是 C++/Qt 规划控制可视化调试工具。当前 Adapter 对接 robot_ws 水下机器人，但
协议和通用 UI 不以 custom_msgs 为 API，可迁移到车辆、仿真和日志回放项目。

## 技术基线

| 工程 | 依赖 | 职责 |
| --- | --- | --- |
| AutoVizProto | C++17、protobuf | v2 schema、FrameCodec、协议 GTest、SDK |
| AutoVizClient | C++17、Qt6（Windows）/ Qt5（Linux）、AutoVizProto | TCP、内部模型、DataManager、XY/T-Z/UI |
| AutoVizServer | ROS2 Humble、custom_msgs、Boost.Asio、AutoVizProto | robot_ws Adapter、缓存、完整快照 Server |

Windows 开发基线统一为 Qt 6.10、Qt Kit 自带 MinGW 13.1 和同工具链编译的 protobuf 35.1
静态库；Qt Creator/CLion 必须选择该编译器，禁止混入 MSYS2 UCRT64 的 protobuf/Abseil
二进制。Linux Client 保持 Qt5，并使用 Linux 本机工具链重建自己的协议 SDK。

协议为 2.5，不兼容 feature v1.1，不实现双栈。2.5 通过 `VisualizationSnapshot.perception_state=22`
兼容扩展感知请求；旧 Server 的缺失 optional 字段不能在 Client 伪造成零或 false。framing 为 4 字节大端长度 + protobuf
Envelope，最大 16 MiB。传输仅有 ClientHello、ServerHello、VisualizationSnapshot、
Heartbeat、ProtocolError。

## robot_ws 输入

`/location`、`/targets/final_objects`、`/chassis_command`、`/chassis_states`、
`/system_run_states`、`/task_params`、`/local_path`、`/global_path` 全部完成映射；
`/detection/range_motion_request` 与 `/detection/inspection_request_goal` 分别映射到同一
`PerceptionState` 的两个独立 optional 字段。参考线是通用协议可选字段，当前 Adapter 无输入。

角度为 rad、角速度为 rad/s，heading 东向为 0、逆时针/左转为正。Server 将
SystemRunStates 目标角速度从 deg/s 转 rad/s；ChassisStates 与 Location 角速度按源值透传。
`odom_z`、`depth`、`height_above_bottom` 保持三个独立字段。

`ChassisCommand.mode` 的当前 0..11 定义由 Server 和本地 CDR 原值写入
`ControlCommand.source_mode`；Client 有该字段时按原始模式显示，缺失时才使用协议通用语义
回退，因此旧协议仍可解析但不会伪造当前模式码。

动作类别按 `SystemRunStates.owner` 和 `chassis_mode` 共同判定：只有
`owner=2` 的 `DepthCommand` 且 `chassis_mode=1/2` 是独立垂向动作；`owner=1` 的
`Move` 即使在 `chassis_mode=4/5/10` 使用定深依赖或旧版定高依赖，也仍是水平航行机动，不能切换 T-Z；
当前 `custom_msgs` 的 mode=2 是着底，旧版 navi_mode=2 仅作为兼容数据保留。

`VehicleState` 还可选携带 WGS-84 经度/纬度（度）以及 USBL 解算位置（m）；这些值仅用于
定位详情，不替代 odom 坐标或三种垂向量。`TaskState.remote_control` 承载来源无关的人工操纵、
推进器调试和固定顺序的配电通路指令；Client 只读显示，绝不经该协议下发控制。Server 与本地
rosbag Adapter 必须产生完全相同的字段语义。

## 当前 UI

保留 main 的 XY、T-Z、路径、障碍物、控制曲线、状态详情、图层、居中、缩放和主题。
UI 根据 capability 启用垂向、水下和平台诊断内容。公共 UI 只看内部模型；ROS topic/type
只作为来源健康诊断文本显示。运动总览与控制指令的数据源固定为：`/chassis_command` 提供
cmd 速度/航向/角速度/模式/档位/使能，`/location` 提供速度/航向/omega_z 的 rev，
`/chassis_states` 仅提供爬行速度、爬行角速度和档位反馈。左侧控制曲线的目标值来自
`/chassis_command`，反馈值来自 `/location`；控制命令的 `enabled` 只表达当前执行状态，
不再驱动额外的“无效/未使能”过渡展示逻辑。
底盘详情中的尾推遥测按左、右尾推分行显示；每行固定为母线电流、控制器温度、目标转速和实际转速，
不将两侧数据拼接为单行长文本。

详情页固定顺序为 ROS Topic、TaskParams、定位、底盘、控制、路径、感知信息、Action 信息、
任务状态、控制时序；保留的垂向诊断页位于控制时序之后。TaskParams 的详细字段保留 protobuf optional presence：老 Server 有
`TaskState` 但不具备某字段时，只有该字段显示“该 Server 无此信息”。感知信息不按当前
`TaskParams.task_id` 过滤，三个输入各自的任务 ID 原样显示；FinalTarget 整帧被拒绝时，
拒绝原因必须在此页保留到下一有效目标帧到达为止。

Client 菜单中，“主视图显示管理”属于视图操作；“文件”提供已部署 `configs/` 目录入口
（车辆尺寸 JSON 与浅色主题 QSS）以及退出；当前固定使用已验收的浅色 UI 基线，暂不开放
深色外观切换，避免 Qt5 全局 QSS 换肤造成局部颜色不一致。连接地址和主视图可见项由 Qt 用户设置保存，不作为
项目配置文件编辑入口。

菜单栏“回放数据”提供 ROS-free 的 robot_ws DB3 回放。窗口完成全量预检、播放/暂停/停止、
进度跳转；主视图右上角提供 0.1×～8× 分档与连续倍率输入。本地回放和远程 Server 数据源
互斥，所有控件沿用当前浅色主题与 UI 缩放。

Client 图标保存在 `AutoVizClient/assets/autoviz_icon.png`，是透明 RGBA PNG，通过 Qt resource
内嵌到可执行文件；运行时不依赖工作目录或外置图标文件。

运动总览固定使用六张卡片的 3×2 布局；连接状态、capability 和数据新鲜度仅更新卡片中的
状态和值，不改变卡片结构或位置，保证未连接、刚连接和稳定运行时的扫读位置一致。
详细信息的“控制时序”关联表中，数据来源、当前值和 Goal UUID 为长文本列，时间/序号/年龄/状态
为紧凑列；列宽按职责固定，窄窗口允许表格自身横向滚动，禁止单元格互相覆盖。

## 默认运行参数

- `publish_rate_hz=20`
- `topic_timeout_ms=5000`
- `max_clients=8`
- heartbeat 1 秒；Client 静默超时 5 秒
- 默认端口 39090

协议面向可信局域网只读调试；没有控制下发、TLS、认证、压缩或服务发现。

## 构建边界

- feature 仓库根目录不创建 `build/`，不在根目录汇总编译子工程。
- Linux AutoVizProto 仅用 `AutoVizProto/scripts/bootstrap_proto.sh`，构建目录固定为
  `AutoVizProto/build`；Windows 使用 `bootstrap_proto.ps1`。
- Windows 脚本只将 Windows SDK 安装到 `AutoVizClient/third_party/AutoVizProto`；ROS2
  Server 的 Linux SDK 只由 Linux 脚本安装到 `AutoVizServer/third_party/AutoVizProto`。
- AutoVizClient 构建目录固定为 `AutoVizClient/build`。
- AutoVizServer 在 `AutoVizServer` 目录使用 `colcon build`，产物位于 Server 自己的
  `build/`、`install/` 和 `log/`。
