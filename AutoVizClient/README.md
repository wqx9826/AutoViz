# AutoVizClient

AutoVizClient 是纯 Qt 可视化客户端，不依赖 AutoVizServer、ROS2 或 custom_msgs。它只
消费安装后的 AutoVizProto CMake package。

## 数据怎样进入 UI

```text
QTcpSocket 字节流
  -> autoviz::FrameDecoder 按 4 字节长度拆帧
  -> protobuf 解析 autoviz::Envelope
  -> ProtocolModelConverter 转为内部 VisualizationSnapshot
  -> DataManager -> SceneManager / Qt UI
```

TCP 一次读取可能只有半帧，也可能包含多帧；FrameDecoder 解决拆包/粘包。protobuf
负责把完整 payload 还原为车辆、轨迹、障碍物和状态对象。

## 依赖与边界

- CMake 3.16+、C++17。
- Qt5 Widgets、Network。
- 已构建安装的 AutoVizProto。

Client 不保存 `.proto`，不调用 protoc，不包含 FrameCodec 的重复实现，也没有
CTest/GTest。协议与 framing 测试统一位于 AutoVizProto。

主要代码：

- `src/core/network/RemoteVisualizationSource.cpp`：连接、握手、心跳、session 和重连。
- `src/core/network/ProtocolModelConverter.cpp`：protobuf 到内部模型的唯一入口。
- `src/core/datacenter/DataManager.cpp`：快照、历史轨迹和旧数据清理。
- `src/core/render/`、`src/ui/`：渲染和界面，不 include protobuf。

## Linux 构建

先安装 AutoVizProto，再配置 Client：

```bash
cmake -S ../AutoVizProto -B ../build/proto \
  -DCMAKE_INSTALL_PREFIX="$PWD/../install/proto"
cmake --build ../build/proto -j4
cmake --install ../build/proto

cmake -S . -B build \
  -DAutoVizProto_DIR="$PWD/../install/proto/lib/cmake/AutoVizProto"
cmake --build build -j4
./build/AutoViz
```

也可把协议安装前缀放进 `CMAKE_PREFIX_PATH`。`configs/` 会复制到可执行文件旁。

## Windows 构建

先用同一编译器/架构构建并安装 AutoVizProto，再构建 Client：

```powershell
cmake -S ..\AutoVizProto -B ..\build\proto `
  -DCMAKE_INSTALL_PREFIX=C:\AutoVizSDK
cmake --build ..\build\proto --config Release
cmake --install ..\build\proto --config Release

cmake -S . -B build `
  -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64 `
  -DAutoVizProto_DIR=C:\AutoVizSDK\lib\cmake\AutoVizProto
cmake --build build --config Release
.\build\Release\AutoViz.exe
```

迁移 Client 时不需要 Server 或 ROS workspace。可以复制 AutoVizClient 与
AutoVizProto 源码，也可以分发已经安装好的 AutoVizProto SDK。

## 连接过程

默认连接 `127.0.0.1:39090`，可在“连接 -> 连接 Server...”修改：

1. Client 发送 ClientHello。
2. 检查 ServerHello 的 protocol major 并保存 `session_id`。
3. 发送订阅请求并接收全量 snapshot。
4. 持续接收 ChannelUpdate 和 Heartbeat。
5. 断线、新 session 或 CLEAR 时清理旧数据。
