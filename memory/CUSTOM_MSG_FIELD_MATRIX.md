# custom_msgs 字段对齐矩阵

本矩阵以 `/home/wqx/LZBK/robot_ws/src/custom_msgs` 当前定义和 robot_ws 实际发布代码为准。
“远程/回放”两列必须进入同一个 `VisualizationSnapshot` 字段，再经过
`ProtocolModelConverter -> DataManager -> UI`；未订阅的消息不推断字段含义。

## 当前八条基础输入和两条可选感知请求

| 输入 | 消息字段覆盖 | Server | 本地 CDR | Client/UI |
| --- | --- | --- | --- | --- |
| `/location` | 全部标量，包括 start/Gauss/origin/odom heading/omega X/Y/USBL words | `VehicleState` typed 字段 | `decodeLocation` 同序读取 | 定位详情；heading 与 odom_heading 并列显示，不做一致性告警 |
| `/targets/final_objects` | Array task_id/mine_number；目标 header/ID/class/reference XY/radius；仅渔网 boundary XY | `FinalTargetSet`（snapshot field 23） | `decodeFinalTargets`，严格当前 CDR 布局 | 参考点 + 保守圆；合法渔网边界画多边形，无效/缺失边界回退圆 |
| `/chassis_command` | 原始 mode、使能、速度、角速度、挡位、水推/垂向/浮力/声纳 | `ControlCommand` | `decodeCommand` | 控制详情；angular_velocity 原值透传 |
| `/chassis_states` | 水推/履带执行器、航向 Kp/目标/实际/误差/输出、五组推进器电机反馈、BMS、配电、底盘状态 | `ChassisState` typed 字段；field 14 保留两组尾推，field 20 写五组推进器 | 旧无尾推 277、旧含尾推 341、当前无尾推 325、当前含尾推 389、当前五电机 485；严格按长度选择 | 底盘详情“航向驱动器”固定五行；旧布局仅有两组尾推时三组垂推显示 `--`，航向诊断不在 Client UI 展示 |
| `/system_run_states` | Action 聚合 owner/goal/state/message/mode/目标/浮力/紧急上浮 | `ActionState` | `decodeAction` | Action 主状态；隐藏 status/feedback 仅作诊断 |
| `/task_params` | 任务使能、动作使能、急停、解除按钮、遥控/调试/配电 | `TaskState` | `decodeTask` | 当前任务使能与动作使能分别显示 |
| `/detection/range_motion_request` | header、任务 ID、命令序号、动作、`float32` 限速、原因 | `PerceptionState.range_motion_directive` | `decodeRangeMotionRequest`；严格处理 header/string/float32 CDR 对齐 | 感知详情；消息任务 ID 原样显示，不按 TaskParams 过滤 |
| `/detection/inspection_request_goal` | header、任务/目标 ID、目标/观察 `Point`、航向、保持、模式、`float32` 限速 | `PerceptionState.inspection_goal` | `decodeInspectionRequestGoal`；严格处理 Point 对齐 | 感知详情；两个请求独立新鲜度/超时 |
| `/local_path` | header、goal UUID、每点 pose/twist/acceleration/time | `Trajectory` | `decodeLocalPath` | 现有 XY 渲染，详情保留完整字段 |
| `/global_path` | header、每点 pose | `Trajectory` | `decodeGlobalPath` | 现有 XY 渲染，详情保留完整字段 |

## Action 与未订阅消息

`Move.action` 和 `DepthCommand.action` 的公开运行值来自 `/system_run_states`；隐藏
`GoalStatusArray` 和 feedback topic 仅在 bag 记录了 hidden topics 时合并。goal/result 本身
当前没有独立可回放输入，因此不伪造协议字段。

`FinalTarget`、`FinalTargetArray`、`Point`、`TrajectoryPointMsg` 和 `TrajectoryMsg` 是基础输入或感知请求
中的嵌套/载体定义，已分别在目标和路径行展开；它们不是额外输入 topic。

当前没有输入订阅的消息：`BehaviorTreeStates`、`InspectionResponse`、`InstructionState`、
`NavigationSensorCommand`、`Object`、`RawIns`、`Scene`。它们不能被当作当前输入的隐式来源。

## 关键语义

- 当前 `ChassisCommand.mode` 的 0..11 定义以 `custom_msgs/msg/ChassisCommand.msg` 为准；Server 和 CDR 都写入 `ControlCommand.source_mode`，Client 在该字段存在时按原始码展示（包括 mode=2 着底、mode=4 定深、mode=5 预留），避免通过通用平台/垂向语义丢失模式。旧协议缺少 `source_mode` 时才使用通用回退。`DepthCommand.vertical_mode=2` 和 `SystemRunStates(owner=2,chassis_mode=2)` 也表示着底；协议保留旧 `HEIGHT_HOLD`，用于旧版 `navi_mode=2` 或旧 Action 数据的兼容显示。
- mode=9“水推设备测试/紧急上浮”归入水推类平台；mode=10/11 仍只通过原始 mode 产生航行/爬行中心转向语义，`expected_gear=4` 不改变模式。
- `ChassisCommand.angular_velocity`、`ChassisStates.current_angular_velocity`、
  `Location.omega_z` 按源值透传；协议单位为 rad/s，UI 转为 deg/s。
- `TaskParams.task_enable` 在协议中只表示当前任务使能；robot_ws 未提供可可靠回放的独立启动事件，
  因此不虚构“启动请求”字段。
- `odom_z`、`depth`、`height_above_bottom` 始终是三个独立字段。
- 当前 `FinalTarget` 的 array/target header 必须为 `odom`，同帧 target_id 唯一，reference X/Y 必须有限、radius 必须为有限正数、final_class 必须在 0..3；违反任一条件整帧拒绝。`reference_point` 不是几何中心，协议/UI 不得伪造 Z、尺寸、朝向或经纬度。仅 CLASS_NET 消费 boundary：空边界合法且回退保守圆；点数不足 3、非有限或零面积时仅该目标回退圆。Server 与本地 CDR 使用相同规则；整帧拒绝时 `FinalTargetSet.rejection_reason` 必须说明原因并由 UI 显示，有效后续帧清除提示。旧 FinalTarget CDR 布局仅跳过该 Topic，不影响 bag 其他通道。
- `/detection/range_motion_request` 与 `/detection/inspection_request_goal` 共享 `PerceptionState` 容器但不是同一状态；Server 和回放端必须分别统计、分别过期，且不得因当前 `TaskParams.task_id` 不同而丢弃消息。
- `TaskParams` 的详情字段以 protobuf optional presence 为准：旧 Server 未发送的单个字段仅显示“该 Server 无此信息”；本地 bag 完全没有相应 topic 则显示“该 bag 无此 Topic”。

## ChassisStates CDR 兼容

所有字段为定长标量。解码器严格按封装后的有效消息长度选择布局：旧无尾推 277、旧含尾推
341、当前无尾推 325、当前含尾推 389、当前五电机 485 字节；每种布局额外允许最多 7 字节全零
CDR 尾部填充。485 按消息声明顺序读取两组尾推后新增的左前、右前、后垂推，向 field 14 保留
两组尾推，并向 field 20 输出固定顺序的五组 ID：`left_tail_thruster`、`right_tail_thruster`、
`left_vertical_thruster`、`right_vertical_thruster`、`back_vertical_thruster`。其他长度、截断或
非零尾部直接报不兼容，禁止错位尝试解析；旧布局缺失的字段保持 optional 缺失，不填零冒充有效值。

历史 `ChassisCommand` 也有固定旧布局：72 字节的 `heading` 后带已删除的 `dive_speed`，当前布局
为 64 字节。CDR decoder 根据精确长度读取旧字段以维持后续执行器/浮力/声纳字段对齐；`dive_speed`
当前没有等价协议字段，因此不虚构 UI 数据。旧布局中的 `mode=2` 保持当时由 `navi_mode` 给出的
垂向语义，当前 64 字节布局的 `mode=2` 才是着底。
