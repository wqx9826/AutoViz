#pragma once

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
    Ros2,
    Ros2Bag,
    Remote
};

struct VisualizationRuntimeStatus {
    VisualizationInputSource inputSource = VisualizationInputSource::Mock;
    quint64 snapshotSequence = 0;
    QString sessionId;
    bool hasCommonPlanningControlCapability = true;
    bool hasVerticalMotionCapability = false;
    bool hasUnderwaterSystemCapability = false;
    bool hasPlatformDiagnosticsCapability = false;
    bool hasVehicleLocationData = false;
    bool hasVehicleChassisData = false;
    bool hasGlobalPathData = false;
    bool hasLocalPathData = false;
    bool hasReferenceLineData = false;
    bool hasObstacleData = false;
    bool hasControlCmdData = false;
    qint64 sourceTimeMs = 0;
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
    model::ControlStateEventList controlStateEvents;
    model::LocalizationStatus localizationStatus;
    model::ChassisRuntimeStatus chassisRuntimeStatus;
    model::ControlCommandStatus controlCommandStatus;
    model::PathRuntimeStatus globalPathStatus;
    model::PathRuntimeStatus localPathStatus;
    model::PathEndpointStatus pathEndpointStatus;
    model::ActionRuntimeStatus actionRuntimeStatus;
    model::ActionRuntimeStatus recentTerminalActionStatus;
    model::TaskRuntimeStatus taskRuntimeStatus;
    model::RunVisualizationMode runVisualizationMode = model::RunVisualizationMode::Unknown;
    VisualizationRuntimeStatus runtimeStatus;
};

class DataManager {
public:
    DataManager();

    void initializeMockData();
    void activateInputSource(VisualizationInputSource inputSource);
    VisualizationInputSource activeInputSource() const;
    bool resetVisualizationData(VisualizationInputSource inputSource);
    bool replaceVisualizationSnapshot(const VisualizationSnapshot& snapshot,
                                      VisualizationInputSource inputSource);

    void clearHistoryTrail();

    VisualizationSnapshot getSnapshot() const;

private:
    static void updateTopicAges(model::TopicStatusList& topicStatuses, qint64 nowMs);
    model::TrajectoryPoint buildHistoryTrailPointLocked() const;
    void appendHistoryTrailPointLocked();
    void updatePathEndpointLocked();
    void updateRunVisualizationModeLocked();

    mutable std::mutex m_mutex;
    VisualizationInputSource m_activeInputSource = VisualizationInputSource::Mock;
    VisualizationSnapshot m_snapshot;
};

}  // namespace autoviz::datacenter
