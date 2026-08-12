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
只作为来源健康诊断文本显示。

## 默认运行参数

- `publish_rate_hz=20`
- `topic_timeout_ms=5000`
- `max_clients=8`
- heartbeat 1 秒；Client 静默超时 5 秒
- 默认端口 39090

协议面向可信局域网只读调试；没有控制下发、TLS、认证、压缩或服务发现。
