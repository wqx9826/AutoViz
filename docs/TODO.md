# AutoViz TODO

本清单区分当前稳定性工作和未来 C/S 演进工作。当前 `main` 分支不实施 C/S 设计，只维护现有 ROS2 可视化版本和相关文档。

## P0：保持 ROS 版本稳定

- 保持当前 ROS2 订阅、字段映射、DataManager 快照和 Qt 渲染链路稳定。
- 继续保证现场调试所需的路径、障碍物、控制、底盘、任务和运行状态显示。
- 保持 Topic 新鲜度和过期数据清除规则，避免旧状态误导现场判断。
- 新增或修正 ROS2 字段时，先落到内部标准模型，不让 ROS msg 扩散到 UI/渲染层。
- 维护 `custom_msgs` 工作区、ROS2 Humble 构建说明和车辆配置说明。

## P1：设计 Client/Server 架构

- 明确 Linux Server 与跨平台 Client 的边界。
- 明确 Server 的数据采集、转换、缓存、新鲜度和 API 职责。
- 明确 Client 的 UI、渲染和用户交互职责。
- 明确 ROS Adapter、Simulation Adapter、Log Adapter 的统一输入/输出语义。
- 设计来源无关的快照、状态、能力和错误模型。

## P2：设计 protobuf 协议

- 以 protobuf 作为数据描述格式候选并评审 schema 演进策略。
- 设计版本、能力、时间戳、序列号、单位、坐标系、有效性和超时字段。
- 评估完整快照与增量更新的边界。
- 评估 protobuf 在 TCP 上的 framing、重连、心跳、背压和错误处理原则。
- 保持协议独立于 ROS2、simulation 和 log 的具体消息类型。

## P3：实现 feature 分支

- 只有在人工确认后，才能开始 C/S 代码、通信协议和通信框架实现。
- 实施分支固定为 `feature/client-server`，不得直接在当前 `main` 分支重构。
- 先实现最小可用的 Linux Server + Client 链路，再逐步接入 Simulation/Log Adapter。
- 实现前补充协议 schema、兼容策略、测试边界和回滚方案。

## 人工确认点

在当前 `main` 分支下不允许实现 C/S 设计。任何 Client/Server 拆分、protobuf schema、TCP/WebSocket/UDP 接入，都必须先人工确认，并新建 `feature/client-server` 分支后再操作。

