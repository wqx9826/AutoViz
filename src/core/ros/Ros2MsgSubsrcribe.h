#pragma once

#include <atomic>
#include <memory>
#include <thread>


#if AUTOVIZ_ENABLE_ROS2
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/path.hpp>

#include "custom_msgs/msg/chassis_command.hpp"
#include "custom_msgs/msg/chassis_states.hpp"
#include "custom_msgs/msg/location.hpp"
#include "custom_msgs/msg/scene.hpp"
#include "custom_msgs/msg/trajectory_msg.hpp"
#endif

#include "core/ros/RosMsgSubscribeBase.h"

namespace autoviz::ros {

class Ros2MsgSubsrcribe : public RosMsgSubscribeBase {
public:
    explicit Ros2MsgSubsrcribe(datacenter::DataManager* dataManager);
    ~Ros2MsgSubsrcribe() override;

    SubscribeBackend backend() const override;
    bool initialize(QString* errorMessage = nullptr) override;
    bool start(QString* errorMessage = nullptr) override;
    void stop() override;
    QString statusSummary() const override;

private:
    std::atomic_bool m_running{false};
    std::thread m_spinThread;

    model::VehicleLocation vehicleLocation_; // 车辆位置
    model::VehicleChassisInfo vehicleChassisInfo_; // 车辆底盘信息
    model::ObstacleList obstacles_; // 障碍物列表
    model::ControlCmd controlCmd_; // 控制指令
    model::Trajectory global_path_; // 全局路径
    model::Trajectory local_path_; // 本地路径
    model::ReferenceLine reference_line_; // 参考线

#if AUTOVIZ_ENABLE_ROS2

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp::Subscription<custom_msgs::msg::Location>::SharedPtr m_sub_location;
    rclcpp::Subscription<custom_msgs::msg::Scene>::SharedPtr m_sub_scene;
    rclcpp::Subscription<custom_msgs::msg::ChassisCommand>::SharedPtr m_sub_chassis_command;
    rclcpp::Subscription<custom_msgs::msg::ChassisStates>::SharedPtr m_sub_chassis_states;
    rclcpp::Subscription<custom_msgs::msg::TrajectoryMsg>::SharedPtr m_sub_trajectory;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr m_sub_path;

private:

    void callbackLocationMsg(const custom_msgs::msg::Location::ConstSharedPtr msg);
    void callbackSceneMsg(const custom_msgs::msg::Scene::ConstSharedPtr msg);
    void callbackChassisCommandMsg(const custom_msgs::msg::ChassisCommand::ConstSharedPtr msg);
    void callbackChassisStatesMsg(const custom_msgs::msg::ChassisStates::ConstSharedPtr msg);
    void callbackLocalPathMsg(const custom_msgs::msg::TrajectoryMsg::ConstSharedPtr msg);
    void callbackGlobalPathMsg(const nav_msgs::msg::Path::ConstSharedPtr msg);

#endif
};



}  // namespace autoviz::ros
