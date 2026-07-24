# AutoViz TODO

## P0：保护现场版本与验证 C/S

- [x] 保留 `main` 的 ROS2 单体版本，不在 main 直接实现 C/S。
- [x] 在 `feature/client-server` 实现物理与构建均独立的 ROS2 Server 和纯 Qt Client。
- [x] Client/Server 均完成 Linux 编译，完成本机 TCP 握手验证。
- [ ] 使用真实 robot_ws 话题做整车/水池数据回放验证。
- [ ] 对比 main 与 C/S 的 XY、T-Z、总览、详情和曲线，形成验收清单。
- [ ] 验证断 topic、断 Server、Server 重启和 Client 重连时无旧状态残留。

## P1：协议与测试加固

- [x] proto2 v1 schema、全量快照、通道 UPSERT/CLEAR、握手和心跳。
- [x] 4 字节大端 framing 和 16 MiB 上限单元测试。
- [x] Client/Server 分别使用 GTest 验证本地 proto 与 FrameCodec。
- [x] 增加两份 proto 的文件集合与 SHA256 同步检查。
- [ ] 增加 robot_ws 消息到 proto、proto 到内部模型的字段级自动测试。
- [ ] 增加录制的 golden snapshot，覆盖水平、垂向、急停和空障碍物。
- [ ] 压测高频轨迹/障碍物和慢 Client，确定背压与队列上限。
- [ ] 评审 diagnostic key 清单并形成稳定注册表。

## P2：跨平台 Client

- [ ] 在 Windows 上安装 Qt5/Qt6 与 protobuf 并完成正式构建。
- [ ] 增加 Windows 打包和运行时依赖清单。
- [ ] 验证中文字体、QSettings、网络重连和高 DPI。
- [ ] 评估 Qt6 迁移，但不得与 C/S 首次现场验证绑在同一改动中。

## P3：Server 演进

- [ ] 从 `AutoVizServerNode` 提取明确的 Adapter 接口与 Server Core。
- [ ] 实现 Simulation Adapter。
- [ ] 实现 Log Adapter 和可重复回放时钟。
- [ ] 根据实测决定是否增加通道采样、合并、压缩或 UDP。
- [ ] 若离开可信局域网，先设计认证/TLS/权限，再开放部署。

## 人工确认事项

- feature 合并或替换 main 前，需要现场人员确认显示一致性与回滚方案。
- v1 增加任何写接口（任务、控制、参数）前，需要单独安全评审。
- protocol major、单位、坐标方向和字段语义变化必须人工评审。
- 是否将 Server 最终迁入 robot_ws、保持当前独立包，需在部署验证后决定。
