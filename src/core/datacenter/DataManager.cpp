#include "core/datacenter/DataManager.h"

#include <QtGlobal>

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
    m_snapshot.vehicleConfig = model::createDefaultVehicleConfig();
    QString errorMessage;
    m_snapshot.vehicleConfig = config::VehicleConfigLoader::loadFromJson(QStringLiteral("configs/vehicle_params.json"), &errorMessage);
    m_snapshot.globalPath = model::createMockGlobalPath();
    m_snapshot.localPath = model::createMockLocalPath();
    m_snapshot.referenceLine = model::createMockReferenceLine();
    m_snapshot.obstacles = model::createMockObstacles();
    m_snapshot.controlCmd = model::createMockControlCmd();
    m_snapshot.runtimeStatus.inputSource = VisualizationInputSource::Mock;
    m_snapshot.runtimeStatus.hasVehicleLocationData = true;
    m_snapshot.runtimeStatus.hasVehicleChassisData = true;
    m_snapshot.runtimeStatus.hasGlobalPathData = !m_snapshot.globalPath.points.isEmpty();
    m_snapshot.runtimeStatus.hasLocalPathData = !m_snapshot.localPath.points.isEmpty();
    m_snapshot.runtimeStatus.hasReferenceLineData = !m_snapshot.referenceLine.points.isEmpty();
    m_snapshot.runtimeStatus.hasObstacleData = !m_snapshot.obstacles.isEmpty();
    m_snapshot.runtimeStatus.hasControlCmdData = true;
}

void DataManager::resetVisualizationData(VisualizationInputSource inputSource)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleState = model::VehicleState{};
    m_snapshot.globalPath = model::Trajectory{};
    m_snapshot.localPath = model::Trajectory{};
    m_snapshot.referenceLine = model::ReferenceLine{};
    m_snapshot.obstacles = model::ObstacleList{};
    m_snapshot.controlCmd = model::ControlCmd{};
    m_snapshot.runtimeStatus = VisualizationRuntimeStatus{};
    m_snapshot.runtimeStatus.inputSource = inputSource;
}

void DataManager::setVehicleLocation(const model::VehicleLocation& vehicleLocation)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleState.location = vehicleLocation;
    m_snapshot.runtimeStatus.hasVehicleLocationData = hasVehicleLocationData(vehicleLocation);
}

void DataManager::setVehicleChassisInfo(const model::VehicleChassisInfo& vehicleChassisInfo)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleState.chassis = vehicleChassisInfo;
    m_snapshot.runtimeStatus.hasVehicleChassisData = hasVehicleChassisData(vehicleChassisInfo);
}

void DataManager::setVehicleConfig(const model::VehicleConfig& vehicleConfig)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleConfig = vehicleConfig;
}

void DataManager::setGlobalPath(const model::Trajectory& globalPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.globalPath = globalPath;
    m_snapshot.runtimeStatus.hasGlobalPathData = !globalPath.points.isEmpty();
}

void DataManager::setLocalPath(const model::Trajectory& localPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.localPath = localPath;
    m_snapshot.runtimeStatus.hasLocalPathData = !localPath.points.isEmpty();
}

void DataManager::setReferenceLine(const model::ReferenceLine& referenceLine)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.referenceLine = referenceLine;
    m_snapshot.runtimeStatus.hasReferenceLineData = !referenceLine.points.isEmpty();
}

void DataManager::setObstacles(const model::ObstacleList& obstacles)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.obstacles = obstacles;
    m_snapshot.runtimeStatus.hasObstacleData = !obstacles.isEmpty();
}

void DataManager::setControlCmd(const model::ControlCmd& controlCmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.controlCmd = controlCmd;
    m_snapshot.runtimeStatus.hasControlCmdData = hasControlCmdData(controlCmd);
}

VisualizationSnapshot DataManager::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

bool DataManager::hasVehicleLocationData(const model::VehicleLocation& vehicleLocation)
{
    return vehicleLocation.header.timestamp != 0
           || !qFuzzyIsNull(vehicleLocation.position.x)
           || !qFuzzyIsNull(vehicleLocation.position.y)
           || !qFuzzyIsNull(vehicleLocation.heading)
           || !qFuzzyIsNull(vehicleLocation.speed)
           || !qFuzzyIsNull(vehicleLocation.acceleration)
           || !qFuzzyIsNull(vehicleLocation.yawRate);
}

bool DataManager::hasVehicleChassisData(const model::VehicleChassisInfo& vehicleChassisInfo)
{
    return vehicleChassisInfo.header.timestamp != 0
           || !qFuzzyIsNull(vehicleChassisInfo.currentSpeed)
           || !qFuzzyIsNull(vehicleChassisInfo.currentWheelAngle)
           || !qFuzzyIsNull(vehicleChassisInfo.currentSteerWheelAngle)
           || !qFuzzyIsNull(vehicleChassisInfo.throttleRatio)
           || !qFuzzyIsNull(vehicleChassisInfo.brakeRatio)
           || vehicleChassisInfo.gear != model::GearPosition::Unknown
           || vehicleChassisInfo.handBrake
           || !qFuzzyIsNull(vehicleChassisInfo.energyRatio);
}

bool DataManager::hasControlCmdData(const model::ControlCmd& controlCmd)
{
    return controlCmd.header.timestamp != 0
           || !qFuzzyIsNull(controlCmd.desiredVelocity)
           || !qFuzzyIsNull(controlCmd.desiredAngularVelocity)
           || !qFuzzyIsNull(controlCmd.desiredWheelAngle)
           || !qFuzzyIsNull(controlCmd.desiredSteerWheelAngle)
           || controlCmd.desiredGear != 0
           || !qFuzzyIsNull(controlCmd.desiredBrake)
           || !qFuzzyIsNull(controlCmd.desiredThrottle)
           || controlCmd.handBrake;
}

}  // namespace autoviz::datacenter
