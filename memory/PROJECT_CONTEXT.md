# AutoViz 项目上下文

AutoViz 是 C++/Qt 规划控制可视化调试工具。当前 Adapter 对接 robot_ws 水下机器人，但
协议和通用 UI 不以 custom_msgs 为 API，可迁移到车辆、仿真和日志回放项目。

## 技术基线

| 工程 | 依赖 | 职责 |
| --- | --- | --- |
| AutoVizProto | C++17、protobuf | v2 schema、FrameCodec、协议 GTest、SDK |
| AutoVizClient | C++17、Qt5、AutoVizProto | TCP、内部模型、DataManager、XY/T-Z/UI |
| AutoVizServer | ROS2 Humble、custom_msgs、Boost.Asio、AutoVizProto | robot_ws Adapter、缓存、完整快照 Server |

协议为 2.0，不兼容 feature v1.1，不实现双栈。framing 为 4 字节大端长度 + protobuf
Envelope，最大 16 MiB。传输仅有 ClientHello、ServerHello、VisualizationSnapshot、
Heartbeat、ProtocolError。

## robot_ws 输入

`/location`、`/targets/final_objects`、`/chassis_command`、`/chassis_states`、
`/system_run_states`、`/task_params`、`/local_path`、`/global_path` 全部完成映射。参考线是
通用协议可选字段，当前 Adapter 无输入。

角度为 rad、角速度为 rad/s，heading 东向为 0、逆时针/左转为正。Server 将
SystemRunStates 目标角速度从 deg/s 转 rad/s，将 ChassisStates 左负右正反馈取反。
`odom_z`、`depth`、`height_above_bottom` 保持三个独立字段。

## 当前 UI

保留 main 的 XY、T-Z、路径、障碍物、控制曲线、状态详情、图层、居中、缩放和主题。
UI 根据 capability 启用垂向、水下和平台诊断内容。公共 UI 只看内部模型；ROS topic/type
只作为来源健康诊断文本显示。控制曲线按实际收到的控制命令、定位和底盘反馈采样；控制
命令的 `enabled` 仅表示执行状态，不会丢弃未使能或切换阶段的回放曲线。

Client 菜单中，“主视图显示管理”属于视图操作；“文件”提供已部署 `configs/` 目录入口
（车辆尺寸 JSON 与主题 QSS）以及退出。连接地址和主视图可见项由 Qt 用户设置保存，不作为
项目配置文件编辑入口。

Client 图标保存在 `AutoVizClient/assets/autoviz_icon.png`，通过 Qt resource 内嵌到可执行文件；
运行时不依赖工作目录或外置图标文件。

运动总览固定使用六张卡片的 3×2 布局；连接状态、capability 和数据新鲜度仅更新卡片中的
状态和值，不改变卡片结构或位置，保证未连接、刚连接和稳定运行时的扫读位置一致。

## 默认运行参数

- `publish_rate_hz=20`
- `topic_timeout_ms=5000`
- `max_clients=8`
- heartbeat 1 秒；Client 静默超时 5 秒
- 默认端口 39090

协议面向可信局域网只读调试；没有控制下发、TLS、认证、压缩或服务发现。
