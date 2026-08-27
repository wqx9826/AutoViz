#pragma once

#include <cstdint>
#include <string_view>

#include <custom_msgs/msg/chassis_command.hpp>
#include <custom_msgs/msg/chassis_states.hpp>
#include <custom_msgs/msg/final_target_array.hpp>
#include <custom_msgs/msg/location.hpp>
#include <custom_msgs/msg/range_motion_request.hpp>
#include <custom_msgs/msg/inspection_request_goal.hpp>
#include <custom_msgs/msg/system_run_states.hpp>
#include <custom_msgs/msg/task_params.hpp>
#include <custom_msgs/msg/trajectory_msg.hpp>
#include <nav_msgs/msg/path.hpp>

#include "autoviz/transport.pb.h"

namespace autoviz_server {

// robot_ws 的 ROS 消息只允许在本类转换为来源无关的 AutoViz 协议。
// 所有函数都是纯转换，便于逐字段单元测试，也让 ROS 回调保持一眼可读。
class RobotWsProtoConverter final {
public:
    static ::autoviz::VehicleState vehicleState(
        const custom_msgs::msg::Location& message, std::uint64_t receiveTimeNs);
    static ::autoviz::FinalTargetSet finalTargets(
        const custom_msgs::msg::FinalTargetArray& message, std::uint64_t receiveTimeNs);
    static ::autoviz::RangeMotionDirective rangeMotionDirective(
        const custom_msgs::msg::RangeMotionRequest& message, std::uint64_t receiveTimeNs);
    static ::autoviz::InspectionGoal inspectionGoal(
        const custom_msgs::msg::InspectionRequestGoal& message, std::uint64_t receiveTimeNs);
    static ::autoviz::ControlCommand controlCommand(
        const custom_msgs::msg::ChassisCommand& message, std::uint64_t receiveTimeNs);
    static ::autoviz::ChassisState chassisState(
        const custom_msgs::msg::ChassisStates& message, std::uint64_t receiveTimeNs);
    static ::autoviz::ActionState actionState(
        const custom_msgs::msg::SystemRunStates& message, std::uint64_t receiveTimeNs);
    static ::autoviz::TaskState taskState(
        const custom_msgs::msg::TaskParams& message, std::uint64_t receiveTimeNs);
    static ::autoviz::Trajectory localTrajectory(
        const custom_msgs::msg::TrajectoryMsg& message, std::uint64_t receiveTimeNs);
    static ::autoviz::Trajectory globalTrajectory(
        const nav_msgs::msg::Path& message, std::uint64_t receiveTimeNs);

    // robot_ws 的 SystemRunStates.goal_uuid 由 %x 逐字节拼接，会丢掉字节的前导零
    //（例如 0x06 变成 "6"），与隐藏 action topic（GoalStatusArray/FeedbackMessage）中
    // 16 字节 UUID 的 canonical 32 位 hex 直接字符串比较会失配。本函数把 canonical 侧
    // 转成同样的 lossy 形式做对称比对，兼容修复前（丢零）和修复后（canonical）两种
    // robot_ws 输出。
    static bool sameGoalUuid(std::string_view a, std::string_view b);
};

}  // namespace autoviz_server
