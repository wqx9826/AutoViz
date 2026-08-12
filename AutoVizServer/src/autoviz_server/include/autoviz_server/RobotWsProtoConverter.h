#pragma once

#include <cstdint>

#include <custom_msgs/msg/chassis_command.hpp>
#include <custom_msgs/msg/chassis_states.hpp>
#include <custom_msgs/msg/final_target_array.hpp>
#include <custom_msgs/msg/location.hpp>
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
    static ::autoviz::ObstacleSet obstacles(
        const custom_msgs::msg::FinalTargetArray& message, std::uint64_t receiveTimeNs);
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
};

}  // namespace autoviz_server
