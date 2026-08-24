#include "autoviz_server/AutoVizServerNode.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

#include "autoviz_server/RobotWsProtoConverter.h"

namespace autoviz_server {
namespace wire = ::autoviz;

namespace {
std::string uuidToString(const unique_identifier_msgs::msg::UUID& uuid)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto byte : uuid.uuid) {
        stream << std::setw(2) << static_cast<unsigned>(byte);
    }
    return stream.str();
}
}  // namespace

AutoVizServerNode::AutoVizServerNode()
    : Node("autoviz_server")
{
    m_serverConfig.bindAddress = declare_parameter<std::string>("host_ip", "0.0.0.0");
    const auto port = declare_parameter<std::int64_t>("port", 39090);
    m_serverConfig.port = static_cast<std::uint16_t>(
        std::clamp<std::int64_t>(port, 1, 65535));
    m_serverConfig.maxClients = static_cast<std::size_t>(
        std::max<std::int64_t>(1, declare_parameter<std::int64_t>("max_clients", 8)));
    m_topicTimeout = std::chrono::milliseconds(
        std::max<std::int64_t>(
            1, declare_parameter<std::int64_t>("topic_timeout_ms", 5000)));
    m_publishRateHz = static_cast<int>(std::clamp<std::int64_t>(
        declare_parameter<std::int64_t>("publish_rate_hz", 20), 1, 100));

    m_topics.location = declare_parameter<std::string>("topics.location", "/location");
    m_topics.obstacles = declare_parameter<std::string>(
        "topics.obstacles", "/targets/final_objects");
    m_topics.rangeMotionDirective = declare_parameter<std::string>(
        "topics.range_motion_request", "/detection/range_motion_request");
    m_topics.inspectionGoal = declare_parameter<std::string>(
        "topics.inspection_request_goal", "/detection/inspection_request_goal");
    m_topics.controlCommand = declare_parameter<std::string>(
        "topics.control_command", "/chassis_command");
    m_topics.chassisState = declare_parameter<std::string>(
        "topics.chassis_state", "/chassis_states");
    m_topics.actionState = declare_parameter<std::string>(
        "topics.action_state", "/system_run_states");
    m_topics.taskState = declare_parameter<std::string>(
        "topics.task_state", "/task_params");
    m_topics.localPath = declare_parameter<std::string>("topics.local_path", "/local_path");
    m_topics.globalPath = declare_parameter<std::string>("topics.global_path", "/global_path");
    m_topics.depthActionStatus = declare_parameter<std::string>(
        "topics.depth_action_status", "/depth_command_action/_action/status");
    m_topics.depthActionFeedback = declare_parameter<std::string>(
        "topics.depth_action_feedback", "/depth_command_action/_action/feedback");
    m_topics.moveActionStatus = declare_parameter<std::string>(
        "topics.move_action_status", "/move_action/_action/status");
    m_topics.moveActionFeedback = declare_parameter<std::string>(
        "topics.move_action_feedback", "/move_action/_action/feedback");

    std::vector<SnapshotStore::TopicSpec> topicSpecs{
        {m_topics.location, "custom_msgs/msg/Location", wire::DATA_KIND_VEHICLE_STATE},
        {m_topics.obstacles, "custom_msgs/msg/FinalTargetArray", wire::DATA_KIND_OBSTACLES},
        {m_topics.rangeMotionDirective, "custom_msgs/msg/RangeMotionRequest", wire::DATA_KIND_RANGE_MOTION_DIRECTIVE},
        {m_topics.inspectionGoal, "custom_msgs/msg/InspectionRequestGoal", wire::DATA_KIND_INSPECTION_GOAL},
        {m_topics.controlCommand, "custom_msgs/msg/ChassisCommand", wire::DATA_KIND_CONTROL_COMMAND},
        {m_topics.chassisState, "custom_msgs/msg/ChassisStates", wire::DATA_KIND_CHASSIS_STATE},
        {m_topics.actionState, "custom_msgs/msg/SystemRunStates", wire::DATA_KIND_ACTION_STATE},
        {m_topics.taskState, "custom_msgs/msg/TaskParams", wire::DATA_KIND_TASK_STATE},
        {m_topics.localPath, "custom_msgs/msg/TrajectoryMsg", wire::DATA_KIND_LOCAL_TRAJECTORY},
        {m_topics.globalPath, "nav_msgs/msg/Path", wire::DATA_KIND_GLOBAL_TRAJECTORY}};
    m_store = std::make_unique<SnapshotStore>(std::move(topicSpecs));

    auto* source = &m_serverIdentity.source;
    source->set_source_id("robot_ws");
    source->set_communication_type("ROS2");
    source->set_communication_version("Humble");
    source->set_description("robot_ws ROS2 adapter");
    source->add_capability(wire::CAPABILITY_COMMON_PLANNING_CONTROL);
    source->add_capability(wire::CAPABILITY_VERTICAL_MOTION);
    source->add_capability(wire::CAPABILITY_UNDERWATER_SYSTEM);
    source->add_capability(wire::CAPABILITY_PLATFORM_DIAGNOSTICS);
    m_store->setSource(*source);

    wire::VehicleParameters parameters;
    parameters.set_length_m(declare_parameter<double>("vehicle_length_m", 4.9));
    parameters.set_width_m(declare_parameter<double>("vehicle_width_m", 1.95));
    parameters.set_wheel_base_m(declare_parameter<double>("wheel_base_m", 2.85));
    m_store->setVehicleParameters(parameters);
    createSubscriptions();
}

AutoVizServerNode::~AutoVizServerNode()
{
    m_server.stop();
}

bool AutoVizServerNode::start()
{
    std::string error;
    if (!m_server.start(m_serverConfig, m_serverIdentity, &error)) {
        RCLCPP_ERROR(get_logger(), "启动 AutoViz TCP Server 失败：%s", error.c_str());
        return false;
    }

    // 先保存一份只有参数和来源状态的初始快照，新 Client 握手后可立即收到。
    m_server.publishSnapshot(m_store->buildSnapshot(nowNs(), m_server.clientCount()));
    m_store->markPublished();
    const auto interval = std::chrono::milliseconds(1000 / m_publishRateHz);
    m_publishTimer = create_wall_timer(interval, std::bind(&AutoVizServerNode::publishSnapshot, this));
    RCLCPP_INFO(get_logger(),
                "AutoViz Server v2 listening on %s:%u, publish=%d Hz",
                m_serverConfig.bindAddress.c_str(),
                static_cast<unsigned>(m_server.boundPort()),
                m_publishRateHz);
    return true;
}

std::uint64_t AutoVizServerNode::nowNs() const
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void AutoVizServerNode::createSubscriptions()
{
    // rosbag 回放和实时发布都兼容 best_effort。控制审计通道使用更深的
    // 有界队列，避免 SingleThreadedExecutor 短时繁忙时在进入回调前丢掉切换帧。
    const auto latestQos = rclcpp::QoS(rclcpp::KeepLast(10)).best_effort();
    const auto controlAuditQos = rclcpp::QoS(rclcpp::KeepLast(100)).best_effort();
    m_locationSubscription = create_subscription<custom_msgs::msg::Location>(
        m_topics.location, latestQos,
        std::bind(&AutoVizServerNode::onLocation, this, std::placeholders::_1));
    m_obstacleSubscription = create_subscription<custom_msgs::msg::FinalTargetArray>(
        m_topics.obstacles, latestQos,
        std::bind(&AutoVizServerNode::onObstacles, this, std::placeholders::_1));
    m_rangeMotionSubscription = create_subscription<custom_msgs::msg::RangeMotionRequest>(
        m_topics.rangeMotionDirective, latestQos,
        std::bind(&AutoVizServerNode::onRangeMotionDirective, this, std::placeholders::_1));
    m_inspectionGoalSubscription = create_subscription<custom_msgs::msg::InspectionRequestGoal>(
        m_topics.inspectionGoal, latestQos,
        std::bind(&AutoVizServerNode::onInspectionGoal, this, std::placeholders::_1));
    m_controlSubscription = create_subscription<custom_msgs::msg::ChassisCommand>(
        m_topics.controlCommand, controlAuditQos,
        std::bind(&AutoVizServerNode::onControl, this, std::placeholders::_1));
    m_chassisSubscription = create_subscription<custom_msgs::msg::ChassisStates>(
        m_topics.chassisState, controlAuditQos,
        std::bind(&AutoVizServerNode::onChassis, this, std::placeholders::_1));
    m_actionSubscription = create_subscription<custom_msgs::msg::SystemRunStates>(
        m_topics.actionState, controlAuditQos,
        std::bind(&AutoVizServerNode::onAction, this, std::placeholders::_1));
    m_taskSubscription = create_subscription<custom_msgs::msg::TaskParams>(
        m_topics.taskState, latestQos,
        std::bind(&AutoVizServerNode::onTask, this, std::placeholders::_1));
    m_localPathSubscription = create_subscription<custom_msgs::msg::TrajectoryMsg>(
        m_topics.localPath, latestQos,
        std::bind(&AutoVizServerNode::onLocalPath, this, std::placeholders::_1));
    m_globalPathSubscription = create_subscription<nav_msgs::msg::Path>(
        m_topics.globalPath, latestQos,
        std::bind(&AutoVizServerNode::onGlobalPath, this, std::placeholders::_1));
    // 原生 action topic 仅补充详情诊断；公开 SystemRunStates 才是主界面契约。
    const auto actionStatusQos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable().transient_local();
    const auto actionFeedbackQos = rclcpp::QoS(rclcpp::KeepLast(10)).reliable();
    m_depthActionStatusSubscription = create_subscription<action_msgs::msg::GoalStatusArray>(
        m_topics.depthActionStatus, actionStatusQos,
        std::bind(&AutoVizServerNode::onActionStatus, this, std::placeholders::_1));
    m_moveActionStatusSubscription = create_subscription<action_msgs::msg::GoalStatusArray>(
        m_topics.moveActionStatus, actionStatusQos,
        std::bind(&AutoVizServerNode::onActionStatus, this, std::placeholders::_1));
    m_depthActionFeedbackSubscription = create_subscription<custom_msgs::action::DepthCommand::Impl::FeedbackMessage>(
        m_topics.depthActionFeedback, actionFeedbackQos,
        std::bind(&AutoVizServerNode::onDepthActionFeedback, this, std::placeholders::_1));
    m_moveActionFeedbackSubscription = create_subscription<custom_msgs::action::Move::Impl::FeedbackMessage>(
        m_topics.moveActionFeedback, actionFeedbackQos,
        std::bind(&AutoVizServerNode::onMoveActionFeedback, this, std::placeholders::_1));
}

void AutoVizServerNode::publishSnapshot()
{
    m_store->expire(std::chrono::steady_clock::now(), m_topicTimeout);
    const auto clientCount = m_server.clientCount();
    if (!m_store->dirty() && clientCount == m_lastPublishedClientCount) {
        return;
    }
    m_server.publishSnapshot(m_store->buildSnapshot(nowNs(), clientCount));
    m_lastPublishedClientCount = clientCount;
    m_store->markPublished();
}

void AutoVizServerNode::onLocation(custom_msgs::msg::Location::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateVehicleState(
        RobotWsProtoConverter::vehicleState(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onObstacles(custom_msgs::msg::FinalTargetArray::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateObstacles(
        RobotWsProtoConverter::obstacles(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onRangeMotionDirective(
    custom_msgs::msg::RangeMotionRequest::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateRangeMotionDirective(
        RobotWsProtoConverter::rangeMotionDirective(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onInspectionGoal(
    custom_msgs::msg::InspectionRequestGoal::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateInspectionGoal(
        RobotWsProtoConverter::inspectionGoal(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onControl(custom_msgs::msg::ChassisCommand::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateControlCommand(
        RobotWsProtoConverter::controlCommand(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onChassis(custom_msgs::msg::ChassisStates::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateChassisState(
        RobotWsProtoConverter::chassisState(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onAction(custom_msgs::msg::SystemRunStates::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    auto next = RobotWsProtoConverter::actionState(*message, receiveTimeNs);
    // 聚合状态的更新频率可高于隐藏 feedback/status；同一 UUID 上保留已收到的可选诊断。
    if (m_hasLatestActionState && next.goal_id() == m_latestActionState.goal_id()) {
        if (m_latestActionState.has_native_status()) {
            next.set_native_status(m_latestActionState.native_status());
            next.set_native_status_time_ns(m_latestActionState.native_status_time_ns());
        }
        if (m_latestActionState.has_feedback_progress()) {
            next.set_feedback_progress(m_latestActionState.feedback_progress());
            next.set_feedback_time_ns(m_latestActionState.feedback_time_ns());
        }
    }
    mergeCachedActionDiagnostic(&next);
    m_latestActionState = std::move(next);
    m_hasLatestActionState = true;
    m_store->updateActionState(m_latestActionState, receiveTimeNs);
}

void AutoVizServerNode::onActionStatus(action_msgs::msg::GoalStatusArray::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    for (const auto& item : message->status_list) {
        const auto goalId = uuidToString(item.goal_info.goal_id);
        updateActionNativeStatus(goalId, item.status, receiveTimeNs);
    }
}

void AutoVizServerNode::onDepthActionFeedback(
    custom_msgs::action::DepthCommand::Impl::FeedbackMessage::ConstSharedPtr message)
{
    if (!message) return;
    updateActionProgress(uuidToString(message->goal_id), message->feedback.progress, nowNs());
}

void AutoVizServerNode::onMoveActionFeedback(
    custom_msgs::action::Move::Impl::FeedbackMessage::ConstSharedPtr message)
{
    if (!message) return;
    updateActionProgress(uuidToString(message->goal_id), message->feedback.progress, nowNs());
}

void AutoVizServerNode::updateActionNativeStatus(const std::string& goalId,
                                                  std::int32_t status,
                                                  std::uint64_t receiveTimeNs)
{
    if (goalId.empty()) return;
    auto& diagnostic = diagnosticFor(goalId);
    diagnostic.hasNativeStatus = true;
    diagnostic.nativeStatus = status;
    diagnostic.nativeStatusTimeNs = receiveTimeNs;
    if (!m_hasLatestActionState
        || !RobotWsProtoConverter::sameGoalUuid(goalId, m_latestActionState.goal_id())) return;
    m_latestActionState.set_native_status(status);
    m_latestActionState.set_native_status_time_ns(receiveTimeNs);
    m_store->updateActionDiagnostics(m_latestActionState);
}

void AutoVizServerNode::updateActionProgress(const std::string& goalId,
                                              double progress,
                                              std::uint64_t receiveTimeNs)
{
    if (goalId.empty()) return;
    auto& diagnostic = diagnosticFor(goalId);
    diagnostic.hasFeedbackProgress = true;
    diagnostic.feedbackProgress = progress;
    diagnostic.feedbackTimeNs = receiveTimeNs;
    if (!m_hasLatestActionState
        || !RobotWsProtoConverter::sameGoalUuid(goalId, m_latestActionState.goal_id())) return;
    m_latestActionState.set_feedback_progress(progress);
    m_latestActionState.set_feedback_time_ns(receiveTimeNs);
    m_store->updateActionDiagnostics(m_latestActionState);
}

AutoVizServerNode::ActionDiagnostic& AutoVizServerNode::diagnosticFor(const std::string& goalId)
{
    auto [it, inserted] = m_actionDiagnostics.try_emplace(goalId);
    if (inserted) {
        m_actionDiagnosticOrder.push_back(goalId);
        while (m_actionDiagnosticOrder.size() > 128) {
            m_actionDiagnostics.erase(m_actionDiagnosticOrder.front());
            m_actionDiagnosticOrder.pop_front();
        }
    }
    return it->second;
}

void AutoVizServerNode::mergeCachedActionDiagnostic(::autoviz::ActionState* action) const
{
    if (action == nullptr || action->goal_id().empty()) return;
    // goal_id 可能是丢前导零的 lossy hex，逐项比对以命中 canonical 缓存键。
    for (const auto& [cachedGoalId, diagnostic] : m_actionDiagnostics) {
        if (!RobotWsProtoConverter::sameGoalUuid(action->goal_id(), cachedGoalId)) continue;
        if (diagnostic.hasNativeStatus) {
            action->set_native_status(diagnostic.nativeStatus);
            action->set_native_status_time_ns(diagnostic.nativeStatusTimeNs);
        }
        if (diagnostic.hasFeedbackProgress) {
            action->set_feedback_progress(diagnostic.feedbackProgress);
            action->set_feedback_time_ns(diagnostic.feedbackTimeNs);
        }
        return;
    }
}

void AutoVizServerNode::onTask(custom_msgs::msg::TaskParams::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateTaskState(
        RobotWsProtoConverter::taskState(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onLocalPath(custom_msgs::msg::TrajectoryMsg::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateLocalTrajectory(
        RobotWsProtoConverter::localTrajectory(*message, receiveTimeNs), receiveTimeNs);
}

void AutoVizServerNode::onGlobalPath(nav_msgs::msg::Path::ConstSharedPtr message)
{
    if (!message) return;
    const auto receiveTimeNs = nowNs();
    m_store->updateGlobalTrajectory(
        RobotWsProtoConverter::globalTrajectory(*message, receiveTimeNs), receiveTimeNs);
}

}  // namespace autoviz_server
