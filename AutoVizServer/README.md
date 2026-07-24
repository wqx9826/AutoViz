# AutoVizServer

AutoVizServer 是一个独立的 ROS2 workspace。当前 `src/autoviz_server` 包负责：

1. 订阅 robot_ws 的 ROS2 topic。
2. 把 `custom_msgs` 转换成来源无关的 AutoViz protobuf 消息。
3. 缓存最新状态。
4. 通过 TCP 把消息发送给一个或多个 AutoVizClient。

它做的是单向可视化数据转发，不向机器人下发任务或控制命令。

## 先理解：protobuf 和 TCP 不是二选一

如果已经知道怎样使用 TCP，可以把两者理解成不同层次的工具：

- TCP 负责建立连接，并把一串字节可靠、有序地送到对方。
- protobuf 负责规定这些字节代表什么，以及 C++ 对象怎样转换成这些字节。

TCP 本身并不知道收到的字节是车辆位置、规划路径还是一句文本。直接使用 TCP 时，
通常需要自己设计结构体字段、字段顺序、整数大小和版本兼容规则。protobuf 把这部分
工作标准化了。

可以把 TCP 想成快递运输，protobuf 想成双方统一使用的装箱单：

```text
ROS2 消息
   ↓ 字段映射
protobuf C++ 对象
   ↓ SerializeToString()
protobuf payload 字节
   ↓ 添加 4 字节长度
TCP frame
   ↓ Boost.Asio 发送
网络
```

Client 做完全相反的过程：

```text
TCP 字节流
   ↓ 根据 4 字节长度拆出一帧
protobuf payload
   ↓ ParseFromArray()
protobuf C++ 对象
   ↓ ProtocolModelConverter
Client 内部模型与 UI
```

protobuf 不代替 TCP，也不负责连接、重传或端口监听；TCP 也不会自动理解 protobuf。
两者组合后，TCP 解决“怎么送到”，protobuf 解决“送的是什么”。

## `.proto` 为什么可以直接在 C++ 中使用

协议文件位于：

```text
src/autoviz_server/proto/autoviz/
  common.proto
  vehicle.proto
  planning.proto
  perception.proto
  control.proto
  runtime.proto
  transport.proto
```

`.proto` 不是运行时直接读取的配置文件。构建时，CMake 调用 protobuf 编译器
`protoc`：

```text
proto/autoviz/transport.proto
        ↓ protoc --cpp_out
build/.../generated/autoviz/transport.pb.h
build/.../generated/autoviz/transport.pb.cc
```

生成的 `.pb.h/.pb.cc` 提供普通 C++ 类，例如：

```cpp
autoviz::protocol::v1::Envelope envelope;
auto* heartbeat = envelope.mutable_heartbeat();
heartbeat->set_sequence(1);
```

`package autoviz.protocol.v1;` 决定生成类的 C++ namespace；文件位于
`proto/autoviz/transport.proto`，因此生成头文件使用：

```cpp
#include "autoviz/transport.pb.h"
```

Server 和 Client 各保存、各编译一份相同的 `.proto`。两边不共享生成文件，也不需要
使用相同编译器或操作系统；它们只需要遵守相同的字段号和 wire format。

## 为什么不能直接发送 C++ struct

直接把结构体内存交给 TCP 会遇到很多问题：

- Linux 和 Windows 的编译器、对齐方式可能不同。
- `std::string`、`std::vector` 内部保存的是指针，不能直接传到另一台机器。
- 新增字段后，旧 Client 不知道结构体大小发生了变化。
- 字节序和不同整数宽度需要自行处理。

protobuf 只序列化字段值，不发送 C++ 内存布局。字段由稳定的 field number 标识，
所以旧程序通常可以忽略自己不认识的新 optional 字段。

## 为什么还需要 4 字节长度

TCP 是字节流，不保留“发送次数”。Server 调用两次 `send()`，Client 可能：

- 一次 `read()` 只收到半条消息，这叫拆包。
- 一次 `read()` 同时收到两条消息，这叫粘包。

所以每个 AutoViz 消息都使用下面的帧格式：

```text
+----------------------+----------------------------+
| uint32 大端长度 N    | N 字节 protobuf Envelope  |
+----------------------+----------------------------+
```

接收方先凑齐 4 字节，得到 payload 长度，再等待完整的 N 字节，最后才调用 protobuf
解析。单帧最大 16 MiB，长度为 0 或超过上限都会断开连接。

`FrameCodec` 负责的就是这层 framing；protobuf 只负责 Envelope payload。

## `Envelope` 是什么

一条 TCP 连接上会传输多种消息。`transport.proto` 用 `Envelope` 作为统一外包装：

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

protobuf 的 `oneof` 保证一个 Envelope 在同一时刻只装其中一种消息。接收方解析后，
通过 `has_client_hello()`、`has_snapshot()` 等方法判断类型。

## 一次连接实际发生什么

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

步骤解释：

1. Client 建立普通 TCP 连接。
2. ClientHello 告诉 Server 自己支持的协议版本。
3. ServerHello 返回版本、当前 `session_id` 和可用通道。
4. Client 请求订阅；当前空通道列表表示订阅全部。
5. Server 先发送全量 snapshot，让新 Client 立即拿到完整当前状态。
6. 后续某个 ROS topic 更新时，只发送对应 `ChannelUpdate(UPSERT)`。
7. topic 超时或数据变空时发送 `ChannelUpdate(CLEAR)`，避免 Client 继续显示旧数据。
8. 双方通过 heartbeat 判断连接是否仍然有效。

`session_id` 代表一次 Server 生命周期。Server 重启后 session 会变化，Client 必须
清空旧轨迹，不能把两次运行的数据拼在一起。

## Server 内部数据流

以 `/location` 为例：

```text
/location (custom_msgs::msg::Location)
        ↓ AutoVizServerNode::onLocation()
字段转换、单位和方向归一化
        ↓
protobuf VehicleState
        ├── 更新 Server 内存中的 VisualizationSnapshot
        └── 包装成 ChannelUpdate
                  ↓
          TcpServer::broadcast()
                  ↓
          FrameCodec::encodeFrame()
                  ↓
          Boost.Asio async_write()
```

重要文件：

- `src/autoviz_server/src/AutoVizServerNode.cpp`：ROS 订阅、字段映射、缓存和新鲜度。
- `src/autoviz_server/src/TcpServer.cpp`：连接、握手、订阅、收发队列和心跳。
- `src/autoviz_server/src/protocol/FrameCodec.cpp`：长度前缀与 protobuf 序列化。
- `src/autoviz_server/proto/autoviz/transport.proto`：Envelope、握手、快照和更新。
- `src/autoviz_server/config/robot_ws.yaml`：topic、端口、超时和车辆参数。

## 构建

```bash
source /opt/ros/humble/setup.bash
source /home/wqx/LZBK/robot_ws/install/setup.bash

colcon --log-base log build \
  --base-paths src \
  --build-base build \
  --install-base install
```

构建过程会自动执行 `protoc`，不需要手工生成 `.pb.h/.pb.cc`，也不应把生成文件提交到
源码目录。

## 测试

Server 使用 ROS2 的 `ament_cmake_gtest`：

```bash
colcon --log-base log test \
  --base-paths src \
  --build-base build \
  --install-base install
colcon test-result --test-result-base build --verbose
```

测试覆盖 protobuf Envelope 往返、TCP 拆包/粘包、非法长度、超长帧、坏 payload、
全量快照、增量消息和 `CLEAR`。

## 运行

```bash
source install/setup.bash
ros2 launch autoviz_server autoviz_server.launch.py
```

默认监听 `0.0.0.0:39090`。Server 可以先于 Client 启动；Client 连接后会自动完成
握手和全量同步。

## 常见误区

- `.proto` 文件不会通过 TCP 发给 Client；两边在构建前就各自拥有 schema。
- protobuf 不会处理粘包，长度前缀和 FrameCodec 才负责消息边界。
- TCP 连接成功不等于协议兼容；还要检查 ClientHello/ServerHello 的 major 版本。
- `SerializeToString()` 得到的是二进制，不适合直接当文本打印。
- 修改 proto 文件路径后，必须同时修改 proto 内的 `import`、CMake 文件列表和生成头
  文件的 `#include`。
- 修改字段时不能复用已经发布的 field number；删除字段应保留为 `reserved`。
