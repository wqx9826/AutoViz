# AutoViz TODO

## v2 本机实现

- [x] Protocol 2.0 完整快照、对称 FrameCodec、capability 和 typed 水下/平台结构。
- [x] 删除 SubscribeRequest、ChannelUpdate、UPSERT/CLEAR。
- [x] 完成 robot_ws 八条消息转换、SnapshotStore 和 20 Hz 发布。
- [x] VisualizationServer 外观接口、握手、心跳、超时、多 Client 和快照合并。
- [x] Client 完整快照原子替换、session 清理、typed converter 和能力驱动 UI。
- [x] Proto framing/round-trip、Server TCP 回环和 ROS 映射测试源码。
- [x] 完成 rosbag + 合成 FinalTarget 本机端到端验收并记录结果。
- [x] Client 原生读取 robot_ws ROS2 Humble SQLite bag，支持完整验证、跳转和调速，无需 ROS/Server。
- [ ] 人工检查真实 Client 的 XY、T-Z、控制曲线、详情、断线和重连观感。

## 后续

- [ ] 在不含 Qt、MSYS2 或 IDE 的 Windows 目标机上，人工验收发布包的图标、配置目录入口、
  qsqlite 插件、原生 bag 回放、固定总览布局和 TCP 连接/重连。
- [ ] 在 Linux 重新执行 AutoVizProto、Client 与 Server 构建及 Client 运行验收；完成 Linux
  Client 可分发打包方案并在干净目标环境验证运行、qsqlite、本地 bag 回放及 TCP 连接/重连。
- [ ] 增加 Client converter 的自动化字段级测试。
- [ ] 用现场高带宽轨迹/障碍物继续压测慢 Client。
- [ ] 按需求实现 Simulation、Log、ROS1 或 DDS Adapter。
- [ ] 离开可信局域网前单独设计认证、TLS 和权限。
