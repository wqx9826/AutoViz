# AutoViz 架构

## 当前架构

`feature/client-server` 包含三个独立工程：

```text
                         AutoVizProto
                 schema / generated C++ / FrameCodec
                       /                         \
                      v                           v
ROS2 topics -> AutoVizServer ---- protobuf/TCP ----> AutoVizClient
 robot_ws       Linux + ROS2                       Linux / Windows + Qt
 Adapter                                            |
                                               DataManager
                                              /           \
                                      SceneManager        状态与图表 UI
```

AutoVizProto 先构建为第三方 SDK，分别安装到 Client/Server 自己的
`third_party/AutoVizProto`。两边使用 `find_package(AutoVizProto CONFIG REQUIRED)`
与 `AutoVizProto::AutoVizProto`，CMake 默认搜索本工程的 third_party，不要求环境
变量，也不依赖兄弟目录源码。

## AutoVizProto 边界

- 唯一 schema 位于 `AutoVizProto/proto/autoviz/*.proto`。
- proto package 是 `autoviz`，生成 C++ namespace 直接是 `autoviz`。
- 生成头路径是 `autoviz/*.pb.h`。
- `FrameCodec` 负责 4 字节大端长度前缀与 protobuf Envelope。
- GTest 在该工程验证 framing 和 transport 消息。
- 不依赖 Qt、ROS2、Boost、custom_msgs。
- 顶层 CMake 只描述库和测试；proto 生成与 SDK 安装分别由
  `cmake/GenerateProto.cmake`、`cmake/InstallAutoVizProto.cmake` 管理。

把协议做成第三方 SDK，同时解决两个问题：只有一份 schema，不会人工同步漂移；
Client 迁移到 Windows 时携带对应 Windows SDK，不必携带 Server 或 ROS workspace。
未来 AutoVizProto 可原样拆到独立仓库或发布版本化 SDK。

## Server 边界

`AutoVizServer` 是独立 ROS2 workspace，`src/autoviz_server` 是完整 ament 包。
`AutoVizServerNode` 当前承担 robot_ws Adapter 与快照缓存；`TcpServer` 处理连接、
握手、订阅、心跳和广播，不包含 ROS 消息类型。

Server 负责：

- 订阅来源相关消息。
- 转换为来源无关的协议语义，归一化单位和方向。
- 保存最新快照，新连接后发送完整状态。
- 发送增量更新和显式 CLEAR。
- 维护 topic 频率、消息数和新鲜度。

Server 独立 launch，不修改 robot_ws launch。

## Client 边界

`AutoVizClient` 是纯 Qt Client。它不保存 proto、不运行 protoc、不依赖 ROS：

- `RemoteVisualizationSource`：异步 TCP、framing、握手、重连和 session。
- `ProtocolModelConverter`：protobuf 到 UI 内部模型的唯一转换入口。
- `DataManager`：线程安全快照、历史轨迹和清理边界。
- SceneManager/UI：只消费内部 `VisualizationSnapshot`，不 include protobuf。

Client 没有 CTest 或 GTest；协议测试集中在 AutoVizProto，避免重复测试同一实现。

## 数据生命周期

1. ROS callback 将消息转换为 `autoviz` 领域消息并更新 Server snapshot。
2. Server 对已订阅 Client 广播 `ChannelUpdate(UPSERT)`。
3. 新连接完成 hello/subscribe 后，Server 发送全量 snapshot。
4. ROS topic 超时后 Server 删除缓存字段并广播 `ChannelUpdate(CLEAR)`。
5. Client 断线或检测到新 `session_id` 时清空 DataManager。
6. Client 主线程每 50ms 获取内部 snapshot 并刷新 UI。

## main 与演进边界

`main` 仍是现场使用的 ROS2/Qt 单体版本：

```text
ROS2 Subscriber -> 内部模型 -> DataManager -> SceneManager -> Qt UI
```

feature 的架构修改必须留在 feature 分支，未经人工验收不覆盖 main。未来 Server 可把
robot_ws 映射抽成 ROS/Simulation/Log Adapter；三种 Adapter 都输出同一
AutoVizProto 语义，Client 无需随来源变化。
