# AutoViz TODO

## v2 本机实现

- [x] Protocol 2.0 完整快照、对称 FrameCodec、capability 和 typed 水下/平台结构。
- [x] 删除 SubscribeRequest、ChannelUpdate、UPSERT/CLEAR。
- [x] 完成 robot_ws 八条消息转换、SnapshotStore 和 20 Hz 发布。
- [x] VisualizationServer 外观接口、握手、心跳、超时、多 Client 和快照合并。
- [x] Client 完整快照原子替换、session 清理、typed converter 和能力驱动 UI。
- [x] Proto framing/round-trip、Server TCP 回环和 ROS 映射测试源码。
- [x] 完成 rosbag + 合成 FinalTarget 本机端到端验收并记录结果。
- [ ] 人工检查真实 Client 的 XY、T-Z、控制曲线、详情、断线和重连观感。

## 后续

- [ ] Windows 构建 AutoVizProto 与 AutoVizClient，并人工验收图标、配置目录入口、固定总览布局、
  控制曲线回放和 TCP 连接/重连。
- [ ] 增加 Client converter 的自动化字段级测试。
- [ ] 用现场高带宽轨迹/障碍物继续压测慢 Client。
- [ ] 按需求实现 Simulation、Log、ROS1 或 DDS Adapter。
- [ ] C/S 人工验收后再决定合入 main 和旧单体版本保留策略。
- [ ] 离开可信局域网前单独设计认证、TLS 和权限。
