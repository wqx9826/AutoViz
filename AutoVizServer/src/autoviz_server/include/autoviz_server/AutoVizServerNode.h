#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

#include <custom_msgs/msg/chassis_command.hpp>
#include <custom_msgs/msg/chassis_states.hpp>
#include <custom_msgs/msg/final_target_array.hpp>
#include <custom_msgs/msg/location.hpp>
#include <custom_msgs/msg/range_motion_request.hpp>
#include <custom_msgs/msg/inspection_request_goal.hpp>
#include <custom_msgs/msg/system_run_states.hpp>
#include <custom_msgs/msg/task_params.hpp>
#include <custom_msgs/msg/trajectory_msg.hpp>
#include <custom_msgs/action/depth_command.hpp>
#include <custom_msgs/action/move.hpp>
#include <action_msgs/msg/goal_status_array.hpp>
#include <unique_identifier_msgs/msg/uuid.hpp>
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
        std::string rangeMotionDirective;
        std::string inspectionGoal;
        std::string controlCommand;
        std::string chassisState;
        std::string actionState;
        std::string taskState;
        std::string localPath;
        std::string globalPath;
        std::string depthActionStatus;
        std::string depthActionFeedback;
        std::string moveActionStatus;
        std::string moveActionFeedback;
    };

    std::uint64_t nowNs() const;
    void createSubscriptions();
    void publishSnapshot();

    void onLocation(custom_msgs::msg::Location::ConstSharedPtr message);
    void onObstacles(custom_msgs::msg::FinalTargetArray::ConstSharedPtr message);
    void onRangeMotionDirective(custom_msgs::msg::RangeMotionRequest::ConstSharedPtr message);
    void onInspectionGoal(custom_msgs::msg::InspectionRequestGoal::ConstSharedPtr message);
    void onControl(custom_msgs::msg::ChassisCommand::ConstSharedPtr message);
    void onChassis(custom_msgs::msg::ChassisStates::ConstSharedPtr message);
    void onAction(custom_msgs::msg::SystemRunStates::ConstSharedPtr message);
    void onTask(custom_msgs::msg::TaskParams::ConstSharedPtr message);
    void onLocalPath(custom_msgs::msg::TrajectoryMsg::ConstSharedPtr message);
    void onGlobalPath(nav_msgs::msg::Path::ConstSharedPtr message);
    void onActionStatus(action_msgs::msg::GoalStatusArray::ConstSharedPtr message);
    void onDepthActionFeedback(custom_msgs::action::DepthCommand::Impl::FeedbackMessage::ConstSharedPtr message);
    void onMoveActionFeedback(custom_msgs::action::Move::Impl::FeedbackMessage::ConstSharedPtr message);
    void updateActionNativeStatus(const std::string& goalId, std::int32_t status, std::uint64_t receiveTimeNs);
    void updateActionProgress(const std::string& goalId, double progress, std::uint64_t receiveTimeNs);
    void mergeCachedActionDiagnostic(::autoviz::ActionState* action) const;

    struct ActionDiagnostic {
        bool hasNativeStatus{false};
        std::int32_t nativeStatus{0};
        std::uint64_t nativeStatusTimeNs{0};
        bool hasFeedbackProgress{false};
        double feedbackProgress{0.0};
        std::uint64_t feedbackTimeNs{0};
    };

    ActionDiagnostic& diagnosticFor(const std::string& goalId);

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
    rclcpp::Subscription<custom_msgs::msg::RangeMotionRequest>::SharedPtr m_rangeMotionSubscription;
    rclcpp::Subscription<custom_msgs::msg::InspectionRequestGoal>::SharedPtr m_inspectionGoalSubscription;
    rclcpp::Subscription<custom_msgs::msg::ChassisCommand>::SharedPtr m_controlSubscription;
    rclcpp::Subscription<custom_msgs::msg::ChassisStates>::SharedPtr m_chassisSubscription;
    rclcpp::Subscription<custom_msgs::msg::SystemRunStates>::SharedPtr m_actionSubscription;
    rclcpp::Subscription<custom_msgs::msg::TaskParams>::SharedPtr m_taskSubscription;
    rclcpp::Subscription<custom_msgs::msg::TrajectoryMsg>::SharedPtr m_localPathSubscription;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr m_globalPathSubscription;
    rclcpp::Subscription<action_msgs::msg::GoalStatusArray>::SharedPtr m_depthActionStatusSubscription;
    rclcpp::Subscription<action_msgs::msg::GoalStatusArray>::SharedPtr m_moveActionStatusSubscription;
    rclcpp::Subscription<custom_msgs::action::DepthCommand::Impl::FeedbackMessage>::SharedPtr m_depthActionFeedbackSubscription;
    rclcpp::Subscription<custom_msgs::action::Move::Impl::FeedbackMessage>::SharedPtr m_moveActionFeedbackSubscription;
    rclcpp::TimerBase::SharedPtr m_publishTimer;
    ::autoviz::ActionState m_latestActionState;
    bool m_hasLatestActionState{false};
    // 隐藏 Action topic 和公开 SystemRunStates 没有全序保证。按 UUID 暂存诊断，
    // 使先到的 status/feedback 也能在聚合状态到达时合并。键是隐藏 topic 的 canonical
    // UUID；m_actionDiagnosticOrder 记录插入顺序，保证超限时按 FIFO 淘汰而不是
    // unordered_map 的任意 begin()。
    std::unordered_map<std::string, ActionDiagnostic> m_actionDiagnostics;
    std::deque<std::string> m_actionDiagnosticOrder;
};

}  // namespace autoviz_server
