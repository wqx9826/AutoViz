# AutoViz TODO

## P0：现场验证

- [x] 保留 main 的 ROS2 单体版本，C/S 仅在 feature 开发。
- [x] 建立独立 AutoVizProto、纯 Qt Client、ROS2 Server 三工程。
- [x] Client/Server 完成 Linux 编译和本机 TCP 握手验证。
- [ ] 使用真实 robot_ws 话题做整车/水池回放验证。
- [ ] 对比 main 与 C/S 的 XY、T-Z、总览、详情和曲线。
- [ ] 验证断 topic、断 Server、重启和重连时无旧状态残留。

## P1：协议与测试

- [x] 唯一 schema 迁移到 `AutoVizProto/proto/autoviz`。
- [x] proto package 简化为 `autoviz`，移除 `autoviz::protocol::v1`。
- [x] AutoVizProto 可安装并导出 `AutoVizProto::AutoVizProto`。
- [x] Client/Server 通过 `find_package` 消费安装包，不引用兄弟源码。
- [x] FrameCodec 和 GTest 从 Client/Server 集中到 AutoVizProto。
- [x] proto2、快照、UPSERT/CLEAR、握手、心跳和 16 MiB framing。
- [ ] 增加 ROS msg -> proto、proto -> 内部模型字段级测试。
- [ ] 增加 golden snapshot，覆盖水平、垂向、急停和空障碍物。
- [ ] 压测高频轨迹、障碍物和慢 Client，确定背压上限。
- [ ] 形成稳定 diagnostic key 注册表。

## P2：跨平台

- [ ] 在 Windows 构建并安装 AutoVizProto。
- [ ] 在 Windows 构建 AutoVizClient，验证中文字体、QSettings、重连和高 DPI。
- [ ] 形成 AutoVizProto/Client 的 Windows SDK 与运行时依赖清单。
- [ ] 评估把 AutoVizProto 拆为独立仓库、submodule 或版本化二进制包。
- [ ] 评估 Qt6 迁移，不与首次 C/S 现场验收绑定。

## P3：Server 演进

- [ ] 从 AutoVizServerNode 提取 Adapter 接口与 Server Core。
- [ ] 实现 Simulation Adapter 和 Log Adapter。
- [ ] 根据实测决定通道采样、合并、压缩或 UDP。
- [ ] 离开可信局域网前设计认证、TLS 和权限。

## 人工确认

- feature 合并到 main 前确认显示一致性、现场稳定性和回滚方案。
- 增加任务/控制写接口前单独安全评审。
- protocol major、单位、坐标方向和字段语义变化必须人工评审。
