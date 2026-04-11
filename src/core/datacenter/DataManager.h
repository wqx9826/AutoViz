#pragma once

#include <mutex>

#include "core/model/ControlTypes.h"
#include "core/model/ObstacleTypes.h"
#include "core/model/PathTypes.h"
#include "core/model/VehicleState.h"

namespace autoviz::datacenter {

struct VisualizationSnapshot {
    model::VehicleState vehicleState;
    model::Trajectory globalPath;
    model::Trajectory localPath;
    model::ReferenceLine referenceLine;
    model::ObstacleList obstacles;
    model::ControlCmd controlCmd;
};

class DataManager {
public:
    DataManager();

    void initializeMockData();

    void setVehicleState(const model::VehicleState& vehicleState);
    void setGlobalPath(const model::Trajectory& globalPath);
    void setLocalPath(const model::Trajectory& localPath);
    void setReferenceLine(const model::ReferenceLine& referenceLine);
    void setObstacles(const model::ObstacleList& obstacles);
    void setControlCmd(const model::ControlCmd& controlCmd);

    VisualizationSnapshot getSnapshot() const;

private:
    mutable std::mutex m_mutex;
    VisualizationSnapshot m_snapshot;
};

}  // namespace autoviz::datacenter
