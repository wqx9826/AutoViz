#include "core/datacenter/DataManager.h"

#include "core/config/VehicleConfig.h"

namespace autoviz::datacenter {

DataManager::DataManager()
{
    initializeMockData();
}

void DataManager::initializeMockData()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleState = model::createMockVehicleState();
    QString errorMessage;
    const auto vehicleConfig = config::VehicleConfigLoader::loadFromJson(QStringLiteral("configs/vehicle_params.json"), &errorMessage);
    model::applyVehicleGeometryConfig(
        m_snapshot.vehicleState, vehicleConfig.length, vehicleConfig.width, vehicleConfig.wheelBase);
    m_snapshot.globalPath = model::createMockGlobalPath();
    m_snapshot.localPath = model::createMockLocalPath();
    m_snapshot.referenceLine = model::createMockReferenceLine();
    m_snapshot.obstacles = model::createMockObstacles();
    m_snapshot.controlCmd = model::createMockControlCmd();
}

void DataManager::setVehicleState(const model::VehicleState& vehicleState)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleState = vehicleState;
}

void DataManager::setGlobalPath(const model::Trajectory& globalPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.globalPath = globalPath;
}

void DataManager::setLocalPath(const model::Trajectory& localPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.localPath = localPath;
}

void DataManager::setReferenceLine(const model::ReferenceLine& referenceLine)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.referenceLine = referenceLine;
}

void DataManager::setObstacles(const model::ObstacleList& obstacles)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.obstacles = obstacles;
}

void DataManager::setControlCmd(const model::ControlCmd& controlCmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.controlCmd = controlCmd;
}

VisualizationSnapshot DataManager::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

}  // namespace autoviz::datacenter
