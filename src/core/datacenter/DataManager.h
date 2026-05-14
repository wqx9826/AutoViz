#pragma once

#include <chrono>
#include <mutex>

#include "core/model/ControlTypes.h"
#include "core/model/ObstacleTypes.h"
#include "core/model/PathTypes.h"
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
    model::ReferenceLine referenceLine;
    model::ObstacleList obstacles;
    model::ControlCmd controlCmd;
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
    };

    static bool hasVehicleLocationData(const model::VehicleLocation& vehicleLocation);
    static bool hasVehicleChassisData(const model::VehicleChassisInfo& vehicleChassisInfo);
    static bool hasControlCmdData(const model::ControlCmd& controlCmd);
    static bool isFresh(TimePoint lastUpdate, TimePoint now);
    static TimePoint timestampFor(bool hasData);
    static void applyFreshnessFilter(VisualizationSnapshot& snapshot, const ChannelUpdateTimes& updateTimes, TimePoint now);

    mutable std::mutex m_mutex;
    VisualizationSnapshot m_snapshot;
    ChannelUpdateTimes m_updateTimes;
};

}  // namespace autoviz::datacenter
