# AutoViz 架构

## 当前架构

`feature/client-server` 已从单体架构演进为可运行的 C/S 结构：

```text
Linux / robot_ws

ROS2 topics
    |
    v
AutoVizServerNode
  - robot_ws 字段映射
  - 单位/方向归一化
  - topic 新鲜度
  - 最新快照缓存
    |
    v
TcpServer (Boost.Asio)
    |
    | AutoViz Protocol v1
    v

Linux / Windows Client

RemoteVisualizationSource (QTcpSocket)
    |
ProtocolModelConverter
    |
DataManager (VisualizationSnapshot)
    |
    +--> SceneManager --> VisualizationView
    +--> ControlPanelWidget
    +--> BottomStatusPanel
```

### 协议层

Client 与 Server 各自持有一份协议源码：

- `AutoVizClient/proto/autoviz/*.proto` 与
  `AutoVizServer/src/autoviz_server/proto/autoviz/*.proto` 按 common、vehicle、
  planning、perception、control、runtime、transport 分域。
- 两边 CMake 分别调用 `protoc` 并生成自己的静态协议库，不引用对方目录。
- 当前生成文件路径为 `generated/autoviz/*.pb.h/.pb.cc`；proto package 仍是
  `autoviz.protocol.v1`，因此 C++ namespace 不随源码目录扁平化而变化。
- 两份 schema 必须逐文件一致，由仓库根同步检查校验；该检查不是任一工程的构建依赖。
- `FrameCodec` 只负责 4 字节大端长度前缀和 protobuf Envelope。
- 两侧 FrameCodec 都不依赖 Qt、ROS 或 Boost。

### Server 层

`AutoVizServer` 是独立 ROS2 workspace，`src/autoviz_server` 是完整 ament 包，不加入
robot_ws 源码树，也不修改 robot_ws launch。`AutoVizServerNode` 当前既是第一版
Adapter，也是快照所有者；`TcpServer` 只处理连接、握手、订阅、心跳和广播，不包含
ROS 类型。

Server 的边界职责：

- 订阅与数据来源有关的消息。
- 转换成协议标准语义并归一化单位/坐标方向。
- 保存最新快照，连接后发送完整状态。
- 发送后续通道更新和显式清空。
- 维护 topic 频率、消息数和 5 秒新鲜度。

### Client 层

`AutoVizClient` 是可单独复制和构建的纯 Client 工程。其 CMake 仅查找 Qt5
Widgets/Network、protobuf 以及开启 `AUTOVIZ_BUILD_TESTS` 时的 GTest；Client 不
使用 CTest，工程目录中也不保存 ROS Adapter。

Client 的边界职责：

- `RemoteVisualizationSource` 处理异步 TCP、协议握手、重连和 session。
- `ProtocolModelConverter` 是协议模型到 UI 内部模型的唯一转换入口。
- `DataManager` 继续承担快照、历史轨迹和线程安全边界。
- SceneManager/UI 不 include protobuf，也不 include ROS。

## main 的旧架构

`main` 分支仍是：

```text
ROS2 Subscriber -> 内部模型 -> DataManager -> SceneManager -> Qt UI
```

它正在现场使用且有实际调试价值。feature 实现不等于已经允许删除、重构或覆盖 main。
未来合并应以并行验证、回滚能力和现场确认作为前提。

## 目标架构

当前 Server 的 robot_ws 映射未来应收敛到明确 Adapter 接口：

```text
ROS Adapter       Simulation Adapter       Log Adapter
      \                    |                    /
       +--------- Standard Snapshot ---------+
                         |
                  AutoViz Server Core
          cache / freshness / session / API
                         |
                  AutoViz Protocol
                         |
                  AutoViz Client
```

Client 始终只负责 UI、渲染和用户交互；Server 始终负责采集、转换、缓存和来源适配。
替换 Adapter 不应要求修改 Client。

## 数据生命周期

1. ROS callback 生成协议领域消息并更新 Server snapshot。
2. Server 对订阅 Client 广播 `ChannelUpdate(UPSERT)`。
3. 新连接发送 hello，Client 发送订阅请求，Server 返回全量 snapshot。
4. ROS topic 超时后 Server 删除缓存字段并广播 `ChannelUpdate(CLEAR)`。
5. Client 断线或发现不同 session 时重置 `DataManager`，避免跨会话残留。
6. Client 每 50ms 读取内部 snapshot 刷新现有 UI。

## 架构演进原则

1. `main` 保持 ROS2 现场版本稳定。
2. C/S 架构继续通过 `feature/client-server` 或其后续 feature 分支开发。
3. 不直接在 `main` 重构或引入通信框架。
4. Client 不依赖 ROS、custom_msgs、topic 名称或 middleware。
5. 协议独立于 ROS2、simulation 和 log。
6. Server Adapter 不操作 Qt UI；Client 网络层不操作 SceneManager。
7. 协议变更必须先保证向后兼容或提升 protocol major，同时修改两份 schema，并同步
   更新文档和测试。
