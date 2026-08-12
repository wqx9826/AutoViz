#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <custom_msgs/msg/chassis_command.hpp>
#include <custom_msgs/msg/chassis_states.hpp>
#include <custom_msgs/msg/final_target_array.hpp>
#include <custom_msgs/msg/location.hpp>
#include <custom_msgs/msg/system_run_states.hpp>
#include <custom_msgs/msg/task_params.hpp>
#include <custom_msgs/msg/trajectory_msg.hpp>
#include <nav_msgs/msg/path.hpp>
#include <rclcpp/rclcpp.hpp>

#include "autoviz_server/SnapshotStore.h"
#include "autoviz_server/VisualizationServer.h"

namespace autoviz_server {

class AutoVizServerNode final : public rclcpp::Node {
public:
    AutoVizServerNode();
    ~AutoVizServerNode() override;

    bool start();

private:
    struct TopicNames {
        std::string location;
        std::string obstacles;
        std::string controlCommand;
        std::string chassisState;
        std::string actionState;
        std::string taskState;
        std::string localPath;
        std::string globalPath;
    };

    std::uint64_t nowNs() const;
    void createSubscriptions();
    void publishSnapshot();

    void onLocation(custom_msgs::msg::Location::ConstSharedPtr message);
    void onObstacles(custom_msgs::msg::FinalTargetArray::ConstSharedPtr message);
    void onControl(custom_msgs::msg::ChassisCommand::ConstSharedPtr message);
    void onChassis(custom_msgs::msg::ChassisStates::ConstSharedPtr message);
    void onAction(custom_msgs::msg::SystemRunStates::ConstSharedPtr message);
    void onTask(custom_msgs::msg::TaskParams::ConstSharedPtr message);
    void onLocalPath(custom_msgs::msg::TrajectoryMsg::ConstSharedPtr message);
    void onGlobalPath(nav_msgs::msg::Path::ConstSharedPtr message);

    TopicNames m_topics;
    std::unique_ptr<SnapshotStore> m_store;
    VisualizationServer m_server;
    VisualizationServerConfig m_serverConfig;
    VisualizationServerIdentity m_serverIdentity;
    std::chrono::milliseconds m_topicTimeout{5000};
    int m_publishRateHz{20};
    std::size_t m_lastPublishedClientCount{0};

    rclcpp::Subscription<custom_msgs::msg::Location>::SharedPtr m_locationSubscription;
    rclcpp::Subscription<custom_msgs::msg::FinalTargetArray>::SharedPtr m_obstacleSubscription;
    rclcpp::Subscription<custom_msgs::msg::ChassisCommand>::SharedPtr m_controlSubscription;
    rclcpp::Subscription<custom_msgs::msg::ChassisStates>::SharedPtr m_chassisSubscription;
    rclcpp::Subscription<custom_msgs::msg::SystemRunStates>::SharedPtr m_actionSubscription;
    rclcpp::Subscription<custom_msgs::msg::TaskParams>::SharedPtr m_taskSubscription;
    rclcpp::Subscription<custom_msgs::msg::TrajectoryMsg>::SharedPtr m_localPathSubscription;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr m_globalPathSubscription;
    rclcpp::TimerBase::SharedPtr m_publishTimer;
};

}  // namespace autoviz_server
