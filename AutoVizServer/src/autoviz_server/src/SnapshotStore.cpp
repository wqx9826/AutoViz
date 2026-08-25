#include "autoviz_server/SnapshotStore.h"

#include <utility>

namespace autoviz_server {
namespace wire = ::autoviz;

namespace {
void addIntMetric(google::protobuf::RepeatedPtrField<wire::DiagnosticMetric>* metrics,
                  const std::string& key,
                  std::int64_t value)
{
    auto* metric = metrics->Add();
    metric->set_key(key);
    metric->set_int_value(value);
}

std::int32_t commandMode(const wire::ControlCommand& value)
{
    if (value.has_source_mode()) return value.source_mode();
    const bool yawInPlace = value.maneuver() == wire::ControlCommand::MANEUVER_YAW_IN_PLACE;
    return value.mode() == wire::ControlCommand::MODE_CRAWL
               ? (yawInPlace ? 11 : 6)
               : (value.mode() == wire::ControlCommand::MODE_SAILING
                      ? (yawInPlace ? 10 : 0)
                      : 0);
}
}  // namespace

SnapshotStore::SnapshotStore(std::vector<TopicSpec> topics)
{
    m_topics.reserve(topics.size());
    for (auto& topic : topics) {
        m_topics.push_back(TopicMonitor{std::move(topic)});
    }
}

void SnapshotStore::setSource(const wire::SourceInfo& source)
{
    m_snapshot.mutable_source()->CopyFrom(source);
    m_dirty = true;
}

void SnapshotStore::setVehicleParameters(const wire::VehicleParameters& parameters)
{
    m_snapshot.mutable_vehicle_parameters()->CopyFrom(parameters);
    m_dirty = true;
}

void SnapshotStore::updateVehicleState(wire::VehicleState value, std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_vehicle_state()->Swap(&value);
    record(wire::DATA_KIND_VEHICLE_STATE, receiveTimeNs);
}

void SnapshotStore::updateChassisState(wire::ChassisState value, std::uint64_t receiveTimeNs)
{
    value.mutable_header()->set_sequence(nextSequence(wire::DATA_KIND_CHASSIS_STATE));
    m_snapshot.mutable_chassis_state()->Swap(&value);
    record(wire::DATA_KIND_CHASSIS_STATE, receiveTimeNs);
}

void SnapshotStore::updateControlCommand(wire::ControlCommand value,
                                         std::uint64_t receiveTimeNs)
{
    value.mutable_header()->set_sequence(nextSequence(wire::DATA_KIND_CONTROL_COMMAND));
    const bool centerTurn = value.maneuver() == wire::ControlCommand::MANEUVER_YAW_IN_PLACE;
    if (centerTurn && !m_centerTurnActive) {
        m_snapshot.clear_global_trajectory();
        m_snapshot.clear_local_trajectory();
    }
    m_centerTurnActive = centerTurn;
    m_snapshot.mutable_control_command()->Swap(&value);
    record(wire::DATA_KIND_CONTROL_COMMAND, receiveTimeNs);
}

void SnapshotStore::updateGlobalTrajectory(wire::Trajectory value,
                                           std::uint64_t receiveTimeNs)
{
    if (!m_centerTurnActive) {
        m_snapshot.mutable_global_trajectory()->Swap(&value);
    }
    record(wire::DATA_KIND_GLOBAL_TRAJECTORY, receiveTimeNs);
}

void SnapshotStore::updateLocalTrajectory(wire::Trajectory value,
                                          std::uint64_t receiveTimeNs)
{
    if (!m_centerTurnActive) {
        m_snapshot.mutable_local_trajectory()->Swap(&value);
    }
    record(wire::DATA_KIND_LOCAL_TRAJECTORY, receiveTimeNs);
}

void SnapshotStore::updateObstacles(wire::ObstacleSet value, std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_obstacles()->Swap(&value);
    record(wire::DATA_KIND_OBSTACLES, receiveTimeNs);
}

void SnapshotStore::updateRangeMotionDirective(wire::RangeMotionDirective value,
                                               std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_perception_state()->mutable_range_motion_directive()->Swap(&value);
    record(wire::DATA_KIND_RANGE_MOTION_DIRECTIVE, receiveTimeNs);
}

void SnapshotStore::updateInspectionGoal(wire::InspectionGoal value,
                                         std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_perception_state()->mutable_inspection_goal()->Swap(&value);
    record(wire::DATA_KIND_INSPECTION_GOAL, receiveTimeNs);
}

void SnapshotStore::updateActionState(wire::ActionState value, std::uint64_t receiveTimeNs)
{
    value.mutable_header()->set_sequence(nextSequence(wire::DATA_KIND_ACTION_STATE));
    // SystemRunStates: 2=canceling, 3=succeeded, 4=aborted. 只保存最近一次
    // 终态，用于详情页；当前快照仍始终表达最新公开聚合状态。
    if (value.state() == 2 || value.state() == 3 || value.state() == 4) {
        m_recentTerminalAction = value;
        m_recentTerminalAction.clear_recent_terminal();
    }
    if (m_recentTerminalAction.has_header()) {
        value.mutable_recent_terminal()->CopyFrom(m_recentTerminalAction);
    }
    m_snapshot.mutable_action_state()->Swap(&value);
    record(wire::DATA_KIND_ACTION_STATE, receiveTimeNs);
}

void SnapshotStore::updateActionDiagnostics(wire::ActionState value)
{
    if (!m_snapshot.has_action_state()) {
        return;
    }

    value.mutable_header()->CopyFrom(m_snapshot.action_state().header());
    if (m_snapshot.action_state().has_recent_terminal()) {
        value.mutable_recent_terminal()->CopyFrom(
            m_snapshot.action_state().recent_terminal());
    }
    m_snapshot.mutable_action_state()->Swap(&value);
    m_dirty = true;
}

void SnapshotStore::updateTaskState(wire::TaskState value, std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_task_state()->Swap(&value);
    record(wire::DATA_KIND_TASK_STATE, receiveTimeNs);
}

void SnapshotStore::expire(std::chrono::steady_clock::time_point now,
                           std::chrono::milliseconds timeout)
{
    m_timeoutNs = static_cast<std::uint64_t>(timeout.count()) * 1000000ULL;
    for (auto& topic : m_topics) {
        const bool timedOut = topic.messageCount == 0
                              || now - topic.lastReceive > timeout;
        if (timedOut && !topic.timedOut) {
            topic.timedOut = true;
            // 当前状态不可由过期缓存推断；事件历史仍保留在 snapshot 中供审计。
            clear(topic.spec.dataKind);
            m_dirty = true;
        } else if (!timedOut) {
            topic.timedOut = false;
        }
    }
}

bool SnapshotStore::dirty() const
{
    return m_dirty;
}

void SnapshotStore::markPublished()
{
    m_dirty = false;
}

wire::VisualizationSnapshot SnapshotStore::buildSnapshot(std::uint64_t serverTimeNs,
                                                         std::size_t clientCount) const
{
    auto result = m_snapshot;
    result.set_server_time_ns(serverTimeNs);

    auto* runtime = result.mutable_runtime_state();
    runtime->Clear();
    auto* header = runtime->mutable_header();
    header->set_source_time_ns(serverTimeNs);
    header->set_server_receive_time_ns(serverTimeNs);
    header->set_module_name("autoviz_server");
    for (const auto& monitor : m_topics) {
        auto* topic = runtime->add_topic();
        topic->set_name(monitor.spec.name);
        topic->set_type(monitor.spec.type);
        topic->set_data_kind(monitor.spec.dataKind);
        topic->set_last_update_time_ns(monitor.lastReceiveNs);
        topic->set_timeout_ns(m_timeoutNs);
        topic->set_frequency_hz(monitor.frequencyHz);
        topic->set_message_count(monitor.messageCount);
        topic->set_timed_out(monitor.timedOut);
    }
    auto* diagnostics = runtime->mutable_diagnostics();
    diagnostics->set_id("autoviz_server");
    diagnostics->set_display_name("AutoViz Server");
    diagnostics->set_level(wire::DiagnosticNode::LEVEL_OK);
    addIntMetric(diagnostics->mutable_metric(), "connected_clients",
                 static_cast<std::int64_t>(clientCount));
    return result;
}

SnapshotStore::TopicMonitor* SnapshotStore::monitor(wire::DataKind dataKind)
{
    for (auto& topic : m_topics) {
        if (topic.spec.dataKind == dataKind) {
            return &topic;
        }
    }
    return nullptr;
}

void SnapshotStore::record(wire::DataKind dataKind, std::uint64_t receiveTimeNs)
{
    auto* state = monitor(dataKind);
    if (state == nullptr) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (state->messageCount > 0) {
        const double seconds = std::chrono::duration<double>(now - state->lastReceive).count();
        if (seconds > 0.0) {
            state->frequencyHz = 1.0 / seconds;
        }
    }
    state->lastReceive = now;
    state->lastReceiveNs = receiveTimeNs;
    ++state->messageCount;
    state->timedOut = false;
    m_dirty = true;
}

std::uint64_t SnapshotStore::nextSequence(wire::DataKind dataKind)
{
    const auto* state = monitor(dataKind);
    return state == nullptr ? 0 : state->messageCount + 1;
}

void SnapshotStore::clear(wire::DataKind dataKind)
{
    switch (dataKind) {
    case wire::DATA_KIND_VEHICLE_STATE:
        m_snapshot.clear_vehicle_state();
        break;
    case wire::DATA_KIND_CHASSIS_STATE:
        m_snapshot.clear_chassis_state();
        break;
    case wire::DATA_KIND_CONTROL_COMMAND:
        m_snapshot.clear_control_command();
        m_centerTurnActive = false;
        break;
    case wire::DATA_KIND_GLOBAL_TRAJECTORY:
        m_snapshot.clear_global_trajectory();
        break;
    case wire::DATA_KIND_LOCAL_TRAJECTORY:
        m_snapshot.clear_local_trajectory();
        break;
    case wire::DATA_KIND_REFERENCE_LINE:
        m_snapshot.clear_reference_line();
        break;
    case wire::DATA_KIND_OBSTACLES:
        m_snapshot.clear_obstacles();
        break;
    case wire::DATA_KIND_RANGE_MOTION_DIRECTIVE:
        if (m_snapshot.has_perception_state()) {
            m_snapshot.mutable_perception_state()->clear_range_motion_directive();
            if (!m_snapshot.perception_state().has_inspection_goal()) {
                m_snapshot.clear_perception_state();
            }
        }
        break;
    case wire::DATA_KIND_INSPECTION_GOAL:
        if (m_snapshot.has_perception_state()) {
            m_snapshot.mutable_perception_state()->clear_inspection_goal();
            if (!m_snapshot.perception_state().has_range_motion_directive()) {
                m_snapshot.clear_perception_state();
            }
        }
        break;
    case wire::DATA_KIND_ACTION_STATE:
        m_snapshot.clear_action_state();
        m_recentTerminalAction.Clear();
        break;
    case wire::DATA_KIND_TASK_STATE:
        m_snapshot.clear_task_state();
        break;
    default:
        break;
    }
}

}  // namespace autoviz_server
