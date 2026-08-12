# AutoVizServer：按 Tcptest 模型阅读 Boost.Asio

如果已经理解 `socket + accept + session + read/write`，当前代码可以直接对应到同一个模型。
Asio 只是把“阻塞等待/epoll 分发”改成“先登记异步操作，完成后进入回调”。

## 类与职责

```text
AutoVizServerNode
  ROS callback -> RobotWsProtoConverter -> SnapshotStore
  50 ms timer  -> VisualizationServer::publishSnapshot

VisualizationServer::Impl
  io_context + acceptor + 一个 I/O 线程
  Client 容器、握手、版本、session、心跳、超时

TcpSession
  一个 tcp::socket
  async_read_some -> FrameDecoder -> Envelope callback
  FrameCodec -> 写队列 -> async_write
```

## Accept -> Session -> Read/Write

1. `VisualizationServer::start()` 完成 `open/bind/listen`，调用 `acceptNext()`，随后让唯一
   I/O 线程执行 `io_context.run()`。
2. `acceptNext()` 的 `async_accept` 回调拿到 socket，创建一个 `TcpSession`，然后在回调
   末尾再次调用 `acceptNext()`，等价于手写 accept 循环的下一轮。
3. `TcpSession::start()` 调用 `readNext()`。`async_read_some` 每次只保证“收到一些字节”，
   所以交给 `FrameDecoder::decode()`；一次可能得到 0～N 个 Envelope。
4. 完整 Envelope 回调进入 `VisualizationServer::handleEnvelope()`。这里处理 ClientHello、
   Heartbeat 和 ProtocolError，不进入 ROS Node。
5. `publishSnapshot()` 用 `asio::post` 把 ROS 线程产生的不可变快照值投递到 I/O 线程。
6. `TcpSession::send()` 编码帧并进入队列；只有队首启动 `async_write`，完成回调弹出队首
   并继续下一项，因此同一 socket 永远不会并发写。

## 与手写 POSIX 的对应关系

| 手写模型 | 当前 Asio 代码 |
| --- | --- |
| `socket/bind/listen` | `VisualizationServer::start()` |
| `accept` 循环 | `acceptNext()` + 完成回调再次登记 |
| 每客户端 fd/context | `TcpSession` |
| `recv` + 拆包缓存 | `async_read_some` + `FrameDecoder` |
| `send` 部分写/EAGAIN | 串行队列 + `async_write` |
| `epoll_wait`/事件循环 | `io_context.run()` |
| 定时扫描连接 | `steady_timer` 心跳回调 |

## 为什么没有 `RecvMsg()`

轮询式 `RecvMsg()` 会要求调用方不断检查 socket，重新承担 epoll/线程循环。Asio 的接收
入口就是 `TcpSession::readNext()` 注册的回调：有字节时框架自动调用，处理完再登记下一次。
Node 因此只面对 `start / publishSnapshot / stop`，不会混入网络状态机。

## 线程与背压

ROS 当前使用 SingleThreadedExecutor，所以 SnapshotStore 不加锁。快照按值通过
`asio::post` 进入 I/O 线程；客户端容器和写队列只在该线程访问。

TCP 发送可能比 20 Hz 快照生成慢。队首正在写的内存不能改；队首之后若已有待发快照，
新快照替换它，从而最多保留“正在写的一帧 + 最新待发快照”。ServerHello、Heartbeat 和
ProtocolError 使用不可替换队列项。

## 推荐阅读顺序

1. `AutoVizServerNode::onLocation()` 和 `publishSnapshot()`：先看清业务主线。
2. `VisualizationServer::start()` / `publishSnapshot()`：外观接口和线程边界。
3. `acceptNext()` / `handleEnvelope()`：连接与握手。
4. `TcpSession::readNext()`：字节流解帧。
5. `TcpSession::send()` / `writeNext()`：串行发送和快照合并。
6. `scheduleHeartbeat()` / `stop()`：定时器与关闭顺序。
