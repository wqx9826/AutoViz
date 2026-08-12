# AutoViz C/S v2 本机验收记录

日期：2026-08-13。所有写入仅发生在 `AutoViz-feature`；robot_ws、Tcptest、main 和 rosbag
仅作读取/运行输入。

## 自动化测试

- AutoVizProto：8/8 通过。覆盖二进制零字节、拆包、粘包、多帧、非法/超长长度、损坏
  protobuf、reset、v2 hello/capability 和完整水下/平台快照 round-trip。
- Server TCP：7/7 通过。覆盖非法地址、动态端口、握手前隔离、握手后立即快照、major
  拒绝、多 Client/上限、心跳、Client 超时、重启 session 和慢 Client 最新快照合并。
- robot_ws converter/SnapshotStore：8/8 通过。覆盖八类输入、角速度方向/单位、有效目标、
  typed 水下/平台字段、路径几何与时间、topic 统计和 5 秒超时清空。
- AutoVizClient 和 ROS2 Server 均完成最终构建。

## rosbag 完整链路

输入：只读回放
`/home/wqx/LZBK/data/rosbag/rosbag2_2026_08_12-03_00_17`，从约 1812 秒偏移开始，
持续约 25 秒。v2 探针连接 `127.0.0.1:39190`，持续发 Client Heartbeat。

结果：

```text
hello=1, snapshots=362
capabilities=common/vertical/underwater/platform 全部到达
vehicle/chassis/control/global_path/local_path/action/task 全部到达
```

探针观察到的累计消息数：location 531，chassis_command 1327，chassis_states 1327，
system_run_states 148，task_params 1327，local_path 147，global_path 146。停止回放超过 5 秒
后，这七类 topic 变为 timed_out，后续完整快照不再含对应领域字段。

bag 不含 `/targets/final_objects`。另以合成 `FinalTargetArray` 发布有效尺寸/航向目标，独立
`--require-obstacles` 探针确认 `obstacles=1` 经 ROS -> converter -> store -> TCP 到达。

## 真实 Client 与 session

Qt Client 以 offscreen 平台实际启动，成功输出：

```text
已连接 127.0.0.1:39190，协议握手中
已连接 127.0.0.1:39190（robot_ws ROS2 adapter）
```

另在真实 Client 保持连接时短时回放同一 1812 秒 bag 窗口，Client 持续运行且没有协议解析、
模型转换或 UI 更新错误，确认七类快照并非只由协议探针消费。

停掉 Server 后，Client 清空连接并持续重试；修复了 Qt `Connection refused` 不一定触发
`disconnected` 导致重连中止的问题。Server 再启动后 Client 自动重新握手成功。

重启前 session 为 `18cb1cb450fce51d`，重启后为 `18cb1ce22394edc2`，确认生命周期变化。
历史轨迹的清空由 RemoteVisualizationSource 在断线/session 变化时调用 DataManager reset。

offscreen 验收证明真实 Client 的构建、启动、握手、快照转换和重连链路可运行；最终字体、
布局和交互观感仍保留为有显示器环境下的人工验收项。
