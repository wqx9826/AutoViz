# AutoVizServer

AutoVizServer 是独立 ROS2 workspace。`src/autoviz_server` 负责订阅 robot_ws topic，
转换为来源无关的 AutoViz 消息，缓存最新状态，再通过 TCP 单向发送给 Client。它不
向机器人写入任务或控制命令。

## 先理解：protobuf + TCP 到底是什么

如果你已经会 TCP，只需把 protobuf 看成“结构体与字节之间的标准翻译器”。

- TCP 负责连接、可靠传输和顺序，但它只认识连续字节。
- protobuf 负责定义字段并把 C++ 对象编码成跨平台字节，但它不负责联网。
- FrameCodec 给每个 protobuf payload 加长度，让接收方知道消息边界。

发送过程：

```text
ROS2 message
  -> Server 做字段映射和单位归一化
  -> autoviz::VehicleState / Trajectory 等 C++ 对象
  -> 放入 autoviz::Envelope
  -> protobuf SerializeToString() 得到 payload
  -> FrameCodec 加 4 字节长度
  -> Boost.Asio 通过 TCP 发送
```

Client 反向执行：

```text
TCP 字节流
  -> FrameCodec 依据长度拆出完整 payload
  -> protobuf ParseFromArray() 得到 Envelope
  -> 转为 Client 内部模型
  -> UI
```

所以它们不是二选一：TCP 解决“怎么送”，protobuf 解决“送的是什么”，FrameCodec
解决“这一条消息从哪里开始、到哪里结束”。

## 为什么 TCP 还需要 4 字节长度

TCP 不保留 `send()` 次数。Server 发送两条消息，Client 可能一次只读到半条（拆包），
也可能一次读到两条（粘包）。每帧因此采用：

```text
+----------------------+----------------------------+
| uint32 大端长度 N    | N 字节 protobuf Envelope  |
+----------------------+----------------------------+
```

接收方先凑齐 4 字节，再等待 N 字节，最后交给 protobuf。N 必须在 1..16 MiB；非法
长度会终止连接。

## `.proto` 在哪里、怎样变成 C++

唯一 schema 不在 Server 内，而在独立工程：

```text
AutoVizProto/proto/autoviz/*.proto
```

构建 AutoVizProto 时，CMake 调用 protoc 生成 `autoviz/*.pb.h/.pb.cc`，再把生成代码
和 FrameCodec 做成第三方 SDK。Server 从
`AutoVizServer/third_party/AutoVizProto` 自动查找
`AutoVizProto::AutoVizProto`，不复制 `.proto`，不引用 Client，也不要求配置
`AutoVizProto_DIR` 参数或环境变量。

所有 schema 使用：

```proto
package autoviz;
```

所以生成 C++ 类型直接是：

```cpp
#include "autoviz/transport.pb.h"

autoviz::Envelope envelope;
envelope.mutable_heartbeat()->set_sequence(1);
```

运行时不会把 `.proto` 发给 Client。双方已经链接同一协议定义，按稳定 field number
解释 wire format。protobuf 序列化的是字段值，不是 C++ struct 内存，因此 Linux 与
Windows 的对象布局、指针和 padding 差异不会进入网络。

## Envelope、握手、快照和增量

一条连接要承载多类消息，Envelope 是统一外包装：

```text
Envelope
  ├── ClientHello
  ├── ServerHello
  ├── SubscribeRequest
  ├── VisualizationSnapshot
  ├── ChannelUpdate
  ├── Heartbeat
  └── ProtocolError
```

连接流程：

```text
Client                                      Server
  |----- TCP connect ------------------------>|
  |----- ClientHello ------------------------>|
  |<---- ServerHello + session_id ------------|
  |----- SubscribeRequest ------------------->|
  |<---- VisualizationSnapshot 全量快照 -------|
  |<---- ChannelUpdate 增量更新 ---------------|
  |<---> Heartbeat -------------------------->|
```

新 Client 先得到完整 snapshot，以后只有相应 ROS topic 变化时才收
`ChannelUpdate(UPSERT)`。topic 超时或内容需要删除时发送
`ChannelUpdate(CLEAR)`，防止 Client 永久显示旧数据。Server 重启会产生新
`session_id`，Client 据此清空旧轨迹。

## Server 内部数据流

以 `/location` 为例：

```text
/location (custom_msgs::msg::Location)
  -> AutoVizServerNode::onLocation()
  -> 字段映射、单位/方向归一化
  -> autoviz::VehicleState
  -> 更新 VisualizationSnapshot 并生成 ChannelUpdate
  -> TcpServer::broadcast()
  -> autoviz::encodeFrame()
  -> Boost.Asio async_write()
```

主要文件：

- `src/autoviz_server/src/AutoVizServerNode.cpp`：订阅、字段映射、缓存和新鲜度。
- `src/autoviz_server/src/TcpServer.cpp`：连接、握手、订阅、队列和心跳。
- `src/autoviz_server/config/robot_ws.yaml`：topic、端口、超时和车辆参数。
- `AutoVizServer/third_party/AutoVizProto/`：Server 使用的协议 SDK。

## 构建

先在仓库根目录构建安装 AutoVizProto：

```bash
cmake -S AutoVizProto -B build/proto \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build build/proto -j4
./build/proto/autoviz_proto_tests
cmake --install build/proto \
  --prefix "$PWD/AutoVizServer/third_party/AutoVizProto"
```

再构建 Server：

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash

colcon --log-base AutoVizServer/log build \
  --base-paths AutoVizServer/src \
  --build-base AutoVizServer/build \
  --install-base AutoVizServer/install
```

安装后可在 `AutoVizServer/third_party/AutoVizProto/include` 和 `lib` 看到公开头与
库。Server CMake 已配置固定搜索路径；只有 third_party 放在别处时才传
`-DAUTOVIZ_THIRD_PARTY_DIR=/other/path`。

Server 不重复保存 FrameCodec GTest；协议测试在 AutoVizProto 中直接运行，不使用
CTest。ROS 映射变化则必须重新编译 Server，并应增加字段级映射测试。

## 运行

```bash
source AutoVizServer/install/setup.bash
ros2 launch autoviz_server autoviz_server.launch.py
```

默认监听 `0.0.0.0:39090`。Server 可以先启动，Client 连接后会自动握手和全量同步。

## 常见误区

- protobuf 不代替 TCP；它只编码/解析 payload。
- protobuf 不处理粘包；长度前缀和 FrameCodec 负责消息边界。
- `.proto` 不在运行时发送；它用于构建双方的代码。
- TCP 连接成功不代表协议兼容；还必须检查 hello 中的 protocol major。
- 不要直接发送 C++ struct 内存，里面可能包含指针、padding 和平台相关布局。
- `package autoviz` 生成 `autoviz::...`，协议 v1 由握手字段表达，不在 namespace 中。
- 已发布 field number 不得复用；删除字段使用 `reserved`。
