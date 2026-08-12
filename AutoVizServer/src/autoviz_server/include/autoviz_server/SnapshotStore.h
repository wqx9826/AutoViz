#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "autoviz/transport.pb.h"

namespace autoviz_server {

// SnapshotStore 只保存“各数据种类的最新值”。ROS 回调更新它，发布定时器从它取得
// 一份完整快照。当前 Node 使用 SingleThreadedExecutor，因此这里不需要互斥锁。
class SnapshotStore final {
public:
    struct TopicSpec {
        std::string name;
        std::string type;
        ::autoviz::DataKind dataKind{::autoviz::DATA_KIND_UNKNOWN};
    };

    explicit SnapshotStore(std::vector<TopicSpec> topics);

    void setSource(const ::autoviz::SourceInfo& source);
    void setVehicleParameters(const ::autoviz::VehicleParameters& parameters);

    void updateVehicleState(::autoviz::VehicleState value, std::uint64_t receiveTimeNs);
    void updateChassisState(::autoviz::ChassisState value, std::uint64_t receiveTimeNs);
    void updateControlCommand(::autoviz::ControlCommand value, std::uint64_t receiveTimeNs);
    void updateGlobalTrajectory(::autoviz::Trajectory value, std::uint64_t receiveTimeNs);
    void updateLocalTrajectory(::autoviz::Trajectory value, std::uint64_t receiveTimeNs);
    void updateObstacles(::autoviz::ObstacleSet value, std::uint64_t receiveTimeNs);
    void updateActionState(::autoviz::ActionState value, std::uint64_t receiveTimeNs);
    void updateTaskState(::autoviz::TaskState value, std::uint64_t receiveTimeNs);

    // 将超过 timeout 的字段从快照移除。完整快照中字段缺失就是明确的清空语义。
    void expire(std::chrono::steady_clock::time_point now,
                std::chrono::milliseconds timeout);

    bool dirty() const;
    void markPublished();
    ::autoviz::VisualizationSnapshot buildSnapshot(std::uint64_t serverTimeNs,
                                                    std::size_t clientCount) const;

private:
    struct TopicMonitor {
        TopicSpec spec;
        std::chrono::steady_clock::time_point lastReceive{};
        std::uint64_t lastReceiveNs{0};
        std::uint64_t messageCount{0};
        double frequencyHz{0.0};
        bool timedOut{true};
    };

    TopicMonitor* monitor(::autoviz::DataKind dataKind);
    void record(::autoviz::DataKind dataKind, std::uint64_t receiveTimeNs);
    void clear(::autoviz::DataKind dataKind);

    ::autoviz::VisualizationSnapshot m_snapshot;
    std::vector<TopicMonitor> m_topics;
    std::uint64_t m_timeoutNs{5000000000ULL};
    bool m_dirty{true};
};

}  // namespace autoviz_server
