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
    m_snapshot.mutable_chassis_state()->Swap(&value);
    record(wire::DATA_KIND_CHASSIS_STATE, receiveTimeNs);
}

void SnapshotStore::updateControlCommand(wire::ControlCommand value,
                                         std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_control_command()->Swap(&value);
    record(wire::DATA_KIND_CONTROL_COMMAND, receiveTimeNs);
}

void SnapshotStore::updateGlobalTrajectory(wire::Trajectory value,
                                           std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_global_trajectory()->Swap(&value);
    record(wire::DATA_KIND_GLOBAL_TRAJECTORY, receiveTimeNs);
}

void SnapshotStore::updateLocalTrajectory(wire::Trajectory value,
                                          std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_local_trajectory()->Swap(&value);
    record(wire::DATA_KIND_LOCAL_TRAJECTORY, receiveTimeNs);
}

void SnapshotStore::updateObstacles(wire::ObstacleSet value, std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_obstacles()->Swap(&value);
    record(wire::DATA_KIND_OBSTACLES, receiveTimeNs);
}

void SnapshotStore::updateActionState(wire::ActionState value, std::uint64_t receiveTimeNs)
{
    m_snapshot.mutable_action_state()->Swap(&value);
    record(wire::DATA_KIND_ACTION_STATE, receiveTimeNs);
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
    case wire::DATA_KIND_ACTION_STATE:
        m_snapshot.clear_action_state();
        break;
    case wire::DATA_KIND_TASK_STATE:
        m_snapshot.clear_task_state();
        break;
    default:
        break;
    }
}

}  // namespace autoviz_server
