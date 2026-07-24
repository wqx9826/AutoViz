#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

#include <custom_msgs/msg/chassis_command.hpp>
#include <custom_msgs/msg/chassis_states.hpp>
#include <custom_msgs/msg/final_target_array.hpp>
#include <custom_msgs/msg/location.hpp>
#include <custom_msgs/msg/system_run_states.hpp>
#include <custom_msgs/msg/task_params.hpp>
#include <custom_msgs/msg/trajectory_msg.hpp>
#include <nav_msgs/msg/path.hpp>
#include <rclcpp/rclcpp.hpp>

#include "autoviz/transport.pb.h"
#include "autoviz_server/TcpServer.h"

namespace autoviz_server {

class AutoVizServerNode final : public rclcpp::Node {
public:
    AutoVizServerNode();
    ~AutoVizServerNode() override;

private:
    struct TopicMonitor {
        std::string name;
        std::string type;
        ::autoviz::ChannelId channel;
        std::chrono::steady_clock::time_point lastReceive;
        std::uint64_t lastReceiveNs = 0;
        std::uint64_t messageCount = 0;
        double frequencyHz = 0.0;
        bool timedOut = true;
    };

    std::uint64_t nowNs() const;
    ::autoviz::VisualizationSnapshot snapshot() const;
    ::autoviz::ChannelUpdate makeUpdate(::autoviz::ChannelId channel);
    void publish(::autoviz::ChannelUpdate update);
    void recordTopic(const std::string& topic);
    std::string topicName(::autoviz::ChannelId channel) const;
    void onTimer();
    void clearTimedOutChannel(::autoviz::ChannelId channel);
    void updateRuntimeState();
    void createSubscriptions();

    void onLocation(custom_msgs::msg::Location::ConstSharedPtr message);
    void onObstacles(custom_msgs::msg::FinalTargetArray::ConstSharedPtr message);
    void onControl(custom_msgs::msg::ChassisCommand::ConstSharedPtr message);
    void onChassis(custom_msgs::msg::ChassisStates::ConstSharedPtr message);
    void onAction(custom_msgs::msg::SystemRunStates::ConstSharedPtr message);
    void onTask(custom_msgs::msg::TaskParams::ConstSharedPtr message);
    void onLocalPath(custom_msgs::msg::TrajectoryMsg::ConstSharedPtr message);
    void onGlobalPath(nav_msgs::msg::Path::ConstSharedPtr message);

    mutable std::mutex m_mutex;
    ::autoviz::VisualizationSnapshot m_snapshot;
    std::unordered_map<std::string, TopicMonitor> m_topics;
    TcpServer m_tcpServer;
    std::string m_sessionId;
    std::uint64_t m_sequence = 0;
    std::chrono::milliseconds m_topicTimeout{5000};

    rclcpp::Subscription<custom_msgs::msg::Location>::SharedPtr m_locationSubscription;
    rclcpp::Subscription<custom_msgs::msg::FinalTargetArray>::SharedPtr m_obstacleSubscription;
    rclcpp::Subscription<custom_msgs::msg::ChassisCommand>::SharedPtr m_controlSubscription;
    rclcpp::Subscription<custom_msgs::msg::ChassisStates>::SharedPtr m_chassisSubscription;
    rclcpp::Subscription<custom_msgs::msg::SystemRunStates>::SharedPtr m_actionSubscription;
    rclcpp::Subscription<custom_msgs::msg::TaskParams>::SharedPtr m_taskSubscription;
    rclcpp::Subscription<custom_msgs::msg::TrajectoryMsg>::SharedPtr m_localPathSubscription;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr m_globalPathSubscription;
    rclcpp::TimerBase::SharedPtr m_timer;
};

}  // namespace autoviz_server
