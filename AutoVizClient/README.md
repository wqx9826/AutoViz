# AutoVizClient

AutoVizClient 是纯 Qt Client，不依赖 ROS。AutoVizProto、protobuf 和 Qt 都可按固定布局
放入工程自己的 `third_party/`，CMake 会自动搜索，不要求系统环境变量。

## third_party 布局

```text
AutoVizClient/
  third_party/
    AutoVizProto/
      include/
      lib/
    protobuf/       # 本地 protobuf SDK，可选
      include/
      lib/
    Qt5/            # Qt kit 根目录或目录链接，可选
      include/
      lib/
```

AutoVizProto 必须存在。protobuf/Qt5 若已能被工具链正常找到，可以不复制；若希望工程
完全使用固定依赖，则放到上述目录。

## Linux

从仓库根目录构建并安装协议 SDK：

```bash
cmake -S AutoVizProto -B build/proto-client \
  -DAUTOVIZ_PROTO_BUILD_TESTS=ON
cmake --build build/proto-client -j4
./build/proto-client/autoviz_proto_tests
cmake --install build/proto-client \
  --prefix "$PWD/AutoVizClient/third_party/AutoVizProto"
```

然后 Client 无需附加 AutoVizProto 参数：

```bash
cmake -S AutoVizClient -B build/client
cmake --build build/client -j4
./build/client/AutoViz
```

## Windows

先把 AutoVizProto 源码包放在任意临时位置，或使用仓库中的 `AutoVizProto`。假设
protobuf SDK 已放在 `AutoVizClient\third_party\protobuf`：

```powershell
cmake -S AutoVizProto -B build\proto-client `
  -DCMAKE_PREFIX_PATH="$PWD\AutoVizClient\third_party\protobuf"
cmake --build build\proto-client --config Release
cmake --install build\proto-client --config Release `
  --prefix "$PWD\AutoVizClient\third_party\AutoVizProto"
```

Qt kit 可放到 `AutoVizClient\third_party\Qt5`，或在该位置创建指向实际 Qt kit 的
目录链接。之后直接构建：

```powershell
cmake -S AutoVizClient -B build\client
cmake --build build\client --config Release
.\build\client\Release\AutoViz.exe
```

这里没有设置 `AutoVizProto_DIR`、`Qt5_DIR` 或系统环境变量。若 third_party 实际放在
其他位置，可显式传 `-DAUTOVIZ_THIRD_PARTY_DIR=C:\your\sdk` 覆盖默认目录。

注意：

- Windows 不能使用 Linux 生成的 `.a`，必须使用对应工具链的 `.lib`。
- AutoVizProto、protobuf 和 Client 应使用相同架构（例如全部 x64）。
- MSVC Runtime、Debug/Release 也应一致。

## 数据流与边界

```text
QTcpSocket
  -> autoviz::FrameDecoder
  -> autoviz::Envelope
  -> ProtocolModelConverter
  -> DataManager
  -> SceneManager / Qt UI
```

Client 不保存 `.proto`、不运行 protoc、不包含 FrameCodec 副本，也没有 CTest/GTest。
协议测试统一在 AutoVizProto。

默认连接 `127.0.0.1:39090`。断线、新 session 或 CLEAR 都会清理旧数据。
