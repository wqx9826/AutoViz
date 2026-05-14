#include "core/datacenter/DataManager.h"

#include <QtGlobal>

#include "core/config/VehicleConfig.h"

namespace autoviz::datacenter {

namespace {
constexpr auto kRealtimeDataTimeout = std::chrono::seconds(3);
}

DataManager::DataManager()
{
    initializeMockData();
}

void DataManager::initializeMockData()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleLocation = model::createMockVehicleLocation();
    m_snapshot.vehicleChassisInfo = model::createMockVehicleChassisInfo();
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
    m_updateTimes = ChannelUpdateTimes{};
}

void DataManager::resetVisualizationData(VisualizationInputSource inputSource)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleLocation = model::VehicleLocation{};
    m_snapshot.vehicleChassisInfo = model::VehicleChassisInfo{};
    m_snapshot.globalPath = model::Trajectory{};
    m_snapshot.localPath = model::Trajectory{};
    m_snapshot.referenceLine = model::ReferenceLine{};
    m_snapshot.obstacles = model::ObstacleList{};
    m_snapshot.controlCmd = model::ControlCmd{};
    m_snapshot.runtimeStatus = VisualizationRuntimeStatus{};
    m_snapshot.runtimeStatus.inputSource = inputSource;
    m_updateTimes = ChannelUpdateTimes{};
}

void DataManager::setVehicleLocation(const model::VehicleLocation& vehicleLocation)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleLocation = vehicleLocation;
    m_snapshot.runtimeStatus.hasVehicleLocationData = hasVehicleLocationData(vehicleLocation);
    m_updateTimes.vehicleLocation = timestampFor(m_snapshot.runtimeStatus.hasVehicleLocationData);
}

void DataManager::setVehicleChassisInfo(const model::VehicleChassisInfo& vehicleChassisInfo)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleChassisInfo = vehicleChassisInfo;
    m_snapshot.runtimeStatus.hasVehicleChassisData = hasVehicleChassisData(vehicleChassisInfo);
    m_updateTimes.vehicleChassis = timestampFor(m_snapshot.runtimeStatus.hasVehicleChassisData);
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
    m_updateTimes.globalPath = timestampFor(m_snapshot.runtimeStatus.hasGlobalPathData);
}

void DataManager::setLocalPath(const model::Trajectory& localPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.localPath = localPath;
    m_snapshot.runtimeStatus.hasLocalPathData = !localPath.points.isEmpty();
    m_updateTimes.localPath = timestampFor(m_snapshot.runtimeStatus.hasLocalPathData);
}

void DataManager::setReferenceLine(const model::ReferenceLine& referenceLine)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.referenceLine = referenceLine;
    m_snapshot.runtimeStatus.hasReferenceLineData = !referenceLine.points.isEmpty();
    m_updateTimes.referenceLine = timestampFor(m_snapshot.runtimeStatus.hasReferenceLineData);
}

void DataManager::setObstacles(const model::ObstacleList& obstacles)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.obstacles = obstacles;
    m_snapshot.runtimeStatus.hasObstacleData = !obstacles.isEmpty();
    m_updateTimes.obstacles = timestampFor(m_snapshot.runtimeStatus.hasObstacleData);
}

void DataManager::setControlCmd(const model::ControlCmd& controlCmd)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.controlCmd = controlCmd;
    m_snapshot.runtimeStatus.hasControlCmdData = hasControlCmdData(controlCmd);
    m_updateTimes.controlCmd = timestampFor(m_snapshot.runtimeStatus.hasControlCmdData);
}

VisualizationSnapshot DataManager::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto snapshot = m_snapshot;
    if (snapshot.runtimeStatus.inputSource != VisualizationInputSource::Mock) {
        applyFreshnessFilter(snapshot, m_updateTimes, Clock::now());
    }
    return snapshot;
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
           || !qFuzzyIsNull(vehicleChassisInfo.currentAngularVelocity)
           || !qFuzzyIsNull(vehicleChassisInfo.currentWheelAngle)
           || !qFuzzyIsNull(vehicleChassisInfo.currentSteerWheelAngle)
           || !qFuzzyIsNull(vehicleChassisInfo.throttleRatio)
           || !qFuzzyIsNull(vehicleChassisInfo.brakeRatio)
           || vehicleChassisInfo.currentGearPosition != 0
           || vehicleChassisInfo.handBrake
           || !qFuzzyIsNull(vehicleChassisInfo.energyRatio)
           || !qFuzzyIsNull(vehicleChassisInfo.leftWheelSpeed)
           || !qFuzzyIsNull(vehicleChassisInfo.rightWheelSpeed);
}

bool DataManager::hasControlCmdData(const model::ControlCmd& controlCmd)
{
    return controlCmd.header.timestamp != 0
           || !qFuzzyIsNull(controlCmd.desiredVelocity)
           || !qFuzzyIsNull(controlCmd.desiredAngularVelocity)
           || !qFuzzyIsNull(controlCmd.desiredWheelAngle)
           || !qFuzzyIsNull(controlCmd.desiredSteerWheelAngle)
           || !qFuzzyIsNull(controlCmd.desiredHeading)
           || controlCmd.mode != model::ControlMode::Unknown
           || controlCmd.desiredGear != 0
           || !qFuzzyIsNull(controlCmd.desiredBrake)
           || !qFuzzyIsNull(controlCmd.desiredThrottle)
           || controlCmd.handBrake;
}

bool DataManager::isFresh(TimePoint lastUpdate, TimePoint now)
{
    return lastUpdate != TimePoint{} && now - lastUpdate <= kRealtimeDataTimeout;
}

DataManager::TimePoint DataManager::timestampFor(bool hasData)
{
    return hasData ? Clock::now() : TimePoint{};
}

void DataManager::applyFreshnessFilter(VisualizationSnapshot& snapshot, const ChannelUpdateTimes& updateTimes, TimePoint now)
{
    if (!isFresh(updateTimes.vehicleLocation, now)) {
        snapshot.vehicleLocation = model::VehicleLocation{};
        snapshot.runtimeStatus.hasVehicleLocationData = false;
    }
    if (!isFresh(updateTimes.vehicleChassis, now)) {
        snapshot.vehicleChassisInfo = model::VehicleChassisInfo{};
        snapshot.runtimeStatus.hasVehicleChassisData = false;
    }
    if (!isFresh(updateTimes.globalPath, now)) {
        snapshot.globalPath = model::Trajectory{};
        snapshot.runtimeStatus.hasGlobalPathData = false;
    }
    if (!isFresh(updateTimes.localPath, now)) {
        snapshot.localPath = model::Trajectory{};
        snapshot.runtimeStatus.hasLocalPathData = false;
    }
    if (!isFresh(updateTimes.referenceLine, now)) {
        snapshot.referenceLine = model::ReferenceLine{};
        snapshot.runtimeStatus.hasReferenceLineData = false;
    }
    if (!isFresh(updateTimes.obstacles, now)) {
        snapshot.obstacles = model::ObstacleList{};
        snapshot.runtimeStatus.hasObstacleData = false;
    }
    if (!isFresh(updateTimes.controlCmd, now)) {
        snapshot.controlCmd = model::ControlCmd{};
        snapshot.runtimeStatus.hasControlCmdData = false;
    }
}

}  // namespace autoviz::datacenter
