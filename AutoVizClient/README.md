# AutoVizClient

AutoVizClient 是独立的 Qt 可视化客户端。整个目录可以单独复制、构建和部署，不依赖
AutoVizServer、ROS2、custom_msgs 或仓库父目录。

Client 只认识 AutoViz protobuf 协议，不知道 Server 内部订阅了哪些 ROS 消息。

## Client 怎样接收 Server 数据

如果已经熟悉 TCP，可以把 Client 的处理分成四步：

```text
QTcpSocket 收到任意长度的一段字节
        ↓
FrameDecoder 缓存字节，并按 4 字节长度前缀拆帧
        ↓
protobuf 把 payload 解析成 Envelope C++ 对象
        ↓
ProtocolModelConverter 转换成 Client 内部模型
        ↓
DataManager -> SceneManager -> Qt UI
```

TCP 只提供字节流，因此一次 `readyRead` 不保证恰好是一条消息。FrameDecoder 会保留
不完整的半帧，也能从一次读取中拆出多帧。protobuf 在拿到完整 payload 后，负责把
二进制字段还原成 `VehicleState`、`Trajectory`、`ChannelUpdate` 等对象。

Client 的协议源码在：

```text
proto/autoviz/*.proto
```

CMake 在构建目录中生成：

```text
generated/autoviz/*.pb.h
generated/autoviz/*.pb.cc
```

例如 `transport.proto` 对应：

```cpp
#include "autoviz/transport.pb.h"
```

生成文件不提交到源码仓库。Client 与 Server 各自编译自己的 proto 副本，所以复制
AutoVizClient 到 Windows 时不需要携带 Server 工程。

## 主要文件

- `proto/autoviz/`：来源无关的协议定义。
- `src/protocol/FrameCodec.h`：TCP 帧接口。
- `src/protocol/FrameCodec.cpp`：4 字节长度前缀和 Envelope 编解码。
- `src/core/network/RemoteVisualizationSource.cpp`：连接、握手、心跳和重连。
- `src/core/network/ProtocolModelConverter.cpp`：protobuf 到 UI 内部模型的唯一入口。
- `src/core/datacenter/DataManager.cpp`：线程安全快照、历史轨迹和旧数据清理。

## 依赖

- CMake 3.16+
- C++17 编译器
- Qt5 Widgets、Network
- protobuf 3
- GTest（仅构建测试可执行文件时需要）

Client 不使用 CTest。GTest 通过独立开关构建，并直接运行测试程序。

## Linux 构建

只构建应用：

```bash
cmake -S . -B build
cmake --build build -j4
./build/AutoViz
```

同时构建并运行 GTest：

```bash
cmake -S . -B build -DAUTOVIZ_BUILD_TESTS=ON
cmake --build build -j4
./build/autoviz_client_protocol_tests
```

`configs/` 会在构建后复制到可执行文件旁。

## Windows 构建

在已安装 Qt5 和 protobuf 的 Developer Command Prompt 中执行：

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
cmake --build build --config Release
.\build\Release\AutoViz.exe
```

需要测试时，确保 CMake 能找到 GTest，并增加：

```powershell
-DAUTOVIZ_BUILD_TESTS=ON
```

然后直接运行：

```powershell
.\build\Release\autoviz_client_protocol_tests.exe
```

实际 Qt 路径和生成器按本机安装调整。

## 连接过程

Client 默认连接 `127.0.0.1:39090`，也可以在“连接 -> 连接 Server...”中修改。

连接后会自动执行：

1. 发送 ClientHello。
2. 检查 ServerHello 的 protocol major。
3. 保存 Server 的 `session_id`。
4. 请求全量 snapshot。
5. 持续接收 channel update 和 heartbeat。

断线、Server session 变化或收到 `CLEAR` 时，Client 会清理对应旧数据，避免显示上次
连接留下的轨迹或状态。
