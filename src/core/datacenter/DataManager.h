#pragma once

#include <chrono>
#include <mutex>

#include "core/model/ControlTypes.h"
#include "core/model/ObstacleTypes.h"
#include "core/model/PathTypes.h"
#include "core/model/RuntimeStatusTypes.h"
#include "core/model/VehicleState.h"

namespace autoviz::datacenter {

enum class VisualizationInputSource {
    Mock,
    Ros1,
    Ros2
};

struct VisualizationRuntimeStatus {
    VisualizationInputSource inputSource = VisualizationInputSource::Mock;
    bool hasVehicleLocationData = false;
    bool hasVehicleChassisData = false;
    bool hasGlobalPathData = false;
    bool hasLocalPathData = false;
    bool hasReferenceLineData = false;
    bool hasObstacleData = false;
    bool hasControlCmdData = false;
};

struct VisualizationSnapshot {
    model::VehicleLocation vehicleLocation;
    model::VehicleChassisInfo vehicleChassisInfo;
    model::VehicleConfig vehicleConfig;
    model::Trajectory globalPath;
    model::Trajectory localPath;
    model::Trajectory historyTrail;
    model::ReferenceLine referenceLine;
    model::ObstacleList obstacles;
    model::ControlCmd controlCmd;
    model::TopicStatusList topicStatuses;
    model::LocalizationStatus localizationStatus;
    model::ChassisRuntimeStatus chassisRuntimeStatus;
    model::ControlCommandStatus controlCommandStatus;
    model::PathRuntimeStatus globalPathStatus;
    model::PathRuntimeStatus localPathStatus;
    model::PathEndpointStatus pathEndpointStatus;
    model::ActionRuntimeStatus actionRuntimeStatus;
    model::TaskRuntimeStatus taskRuntimeStatus;
    model::RunVisualizationMode runVisualizationMode = model::RunVisualizationMode::Unknown;
    VisualizationRuntimeStatus runtimeStatus;
};

class DataManager {
public:
    DataManager();

    void initializeMockData();
    void resetVisualizationData(VisualizationInputSource inputSource);

    void setVehicleLocation(const model::VehicleLocation& vehicleLocation);
    void setVehicleChassisInfo(const model::VehicleChassisInfo& vehicleChassisInfo);
    void setVehicleConfig(const model::VehicleConfig& vehicleConfig);
    void setGlobalPath(const model::Trajectory& globalPath);
    void setLocalPath(const model::Trajectory& localPath);
    void setReferenceLine(const model::ReferenceLine& referenceLine);
    void setObstacles(const model::ObstacleList& obstacles);
    void setControlCmd(const model::ControlCmd& controlCmd);
    void setTopicStatus(const model::TopicStatus& topicStatus);
    void setTopicStatuses(const model::TopicStatusList& topicStatuses);
    void setLocalizationStatus(const model::LocalizationStatus& status);
    void setChassisRuntimeStatus(const model::ChassisRuntimeStatus& status);
    void setControlCommandStatus(const model::ControlCommandStatus& status);
    void setGlobalPathStatus(const model::PathRuntimeStatus& status);
    void setLocalPathStatus(const model::PathRuntimeStatus& status);
    void setActionRuntimeStatus(const model::ActionRuntimeStatus& status);
    void setTaskRuntimeStatus(const model::TaskRuntimeStatus& status);
    void clearHistoryTrail();

    VisualizationSnapshot getSnapshot() const;

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct ChannelUpdateTimes {
        TimePoint vehicleLocation;
        TimePoint vehicleChassis;
        TimePoint globalPath;
        TimePoint localPath;
        TimePoint referenceLine;
        TimePoint obstacles;
        TimePoint controlCmd;
        TimePoint controlCommandStatus;
        TimePoint actionRuntimeStatus;
        TimePoint taskRuntimeStatus;
    };

    static bool hasVehicleLocationData(const model::VehicleLocation& vehicleLocation);
    static bool hasVehicleChassisData(const model::VehicleChassisInfo& vehicleChassisInfo);
    static bool hasControlCmdData(const model::ControlCmd& controlCmd);
    static bool isFresh(TimePoint lastUpdate, TimePoint now);
    static TimePoint timestampFor(bool hasData);
    static void applyFreshnessFilter(VisualizationSnapshot& snapshot, const ChannelUpdateTimes& updateTimes, TimePoint now);
    static void updateTopicAges(model::TopicStatusList& topicStatuses, qint64 nowMs);
    model::TrajectoryPoint buildHistoryTrailPointLocked() const;
    void appendHistoryTrailPointLocked();
    void updatePathEndpointLocked();
    void updateRunVisualizationModeLocked();

    mutable std::mutex m_mutex;
    VisualizationSnapshot m_snapshot;
    ChannelUpdateTimes m_updateTimes;
};

}  // namespace autoviz::datacenter
