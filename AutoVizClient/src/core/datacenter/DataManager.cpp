#include "core/datacenter/DataManager.h"

#include <cmath>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QtGlobal>

#include "core/config/VehicleConfig.h"

namespace autoviz::datacenter {

namespace {
constexpr int kMaxHistoryTrailPoints = 2000;
// XY 轨迹仍按水平位移降采样；垂向动作需要 depth/height 和时间兜底共同触发采样。
constexpr double kHistoryTrailMinDistance = 1.0;
constexpr double kHistoryTrailMinDepthDelta = 0.1;
constexpr double kHistoryTrailMinHeightDelta = 0.1;
constexpr double kVerticalHistoryMinTimeIntervalSec = 1.0;

bool isVerticalHistoryMode(model::RunVisualizationMode mode)
{
    return mode == model::RunVisualizationMode::VerticalMotion;
}
}

DataManager::DataManager()
{
    initializeMockData();
}

void DataManager::initializeMockData()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const qint64 mockTimestamp = QDateTime::currentMSecsSinceEpoch();
    m_snapshot.vehicleLocation = model::createMockVehicleLocation();
    m_snapshot.vehicleChassisInfo = model::createMockVehicleChassisInfo();
    m_snapshot.vehicleConfig = model::createDefaultVehicleConfig();
    QString errorMessage;
    const QString deployedConfig =
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("configs/vehicle_params.json"));
    m_snapshot.vehicleConfig =
        config::VehicleConfigLoader::loadFromJson(deployedConfig, &errorMessage);
    if (!errorMessage.isEmpty()) {
        errorMessage.clear();
        m_snapshot.vehicleConfig =
            config::VehicleConfigLoader::loadFromJson(
                QDir::current().filePath(QStringLiteral("configs/vehicle_params.json")),
                &errorMessage);
    }
    m_snapshot.globalPath = model::createMockGlobalPath();
    m_snapshot.localPath = model::createMockLocalPath();
    m_snapshot.historyTrail = model::Trajectory{};
    m_snapshot.referenceLine = model::createMockReferenceLine();
    m_snapshot.obstacles = model::createMockObstacles();
    m_snapshot.controlCmd = model::createMockControlCmd();

    m_snapshot.vehicleLocation.header.timestamp = mockTimestamp;
    m_snapshot.vehicleChassisInfo.header.timestamp = mockTimestamp;
    m_snapshot.globalPath.header.timestamp = mockTimestamp;
    m_snapshot.localPath.header.timestamp = mockTimestamp;
    m_snapshot.referenceLine.header.timestamp = mockTimestamp;
    m_snapshot.controlCmd.header.timestamp = mockTimestamp;

    auto& localization = m_snapshot.localizationStatus;
    localization.valid = true;
    localization.timestampMs = mockTimestamp;
    localization.gpsTime = mockTimestamp / 1000;
    localization.status = 0;
    localization.error = 0;
    localization.odomX = m_snapshot.vehicleLocation.position.x;
    localization.odomY = m_snapshot.vehicleLocation.position.y;
    localization.odomZ = 0.0;
    localization.heading = m_snapshot.vehicleLocation.heading;
    localization.pitch = 0.02;
    localization.roll = -0.01;
    localization.velocityX = m_snapshot.vehicleLocation.Velocity.x;
    localization.velocityY = m_snapshot.vehicleLocation.Velocity.y;
    localization.velocityZ = m_snapshot.vehicleLocation.Velocity.z;
    localization.velocity = m_snapshot.vehicleLocation.speed;
    localization.omegaZ = m_snapshot.vehicleLocation.yawRate;
    localization.acc = m_snapshot.vehicleLocation.acceleration;
    localization.depth = 6.4;
    localization.height = 1.8;

    auto& chassis = m_snapshot.chassisRuntimeStatus;
    chassis.valid = true;
    chassis.timestampMs = mockTimestamp;
    chassis.currentSpeed = m_snapshot.vehicleChassisInfo.currentSpeed;
    chassis.currentAngularVelocity = m_snapshot.vehicleChassisInfo.currentAngularVelocity;
    chassis.gearStatus = m_snapshot.vehicleChassisInfo.currentGearPosition;
    chassis.waterTankLevelStatus = 62;
    chassis.waterTankLevelIsRaw = false;
    chassis.waterTankState = model::WaterTankState::Idle;
    chassis.waterHeartbeat = 42;
    chassis.crawlHeartbeat = 17;
    chassis.leftTailActuatorStatus = 0;
    chassis.rightTailActuatorStatus = 0;
    chassis.leftVerticalActuatorStatus = 0;
    chassis.rightVerticalActuatorStatus = 0;
    chassis.backVerticalActuatorStatus = 0;
    chassis.leftCrawlActuatorFaultCode = 0;
    chassis.rightCrawlActuatorFaultCode = 0;
    chassis.highVoltageBmsStatus = 0;
    chassis.dccdcStatus = true;
    chassis.highVoltageBmsSocStatus = 78;
    chassis.smartPowerInputVoltageStatus = 24.2;

    chassis.leftCrawlMotor = model::CrawlMotorRuntimeStatus{};
    chassis.leftCrawlMotor.valid = true;
    chassis.leftCrawlMotor.speedRpm = 42.0;
    chassis.leftCrawlMotor.temperature = 31;
    chassis.leftCrawlMotor.busVoltage = 24.1;
    chassis.leftCrawlMotor.controllerReady = true;
    chassis.leftCrawlMotor.outputEnabled = true;
    chassis.leftCrawlMotor.commandEnable = true;
    chassis.leftCrawlMotor.commandSpeedMode = true;
    chassis.leftCrawlMotor.commandSpeedRpm = 43.0;
    chassis.rightCrawlMotor = chassis.leftCrawlMotor;
    chassis.rightCrawlMotor.speedRpm = 43.0;
    chassis.rightCrawlMotor.commandSpeedRpm = 44.0;

    chassis.bms = model::BmsRuntimeStatus{};
    chassis.bms.valid = true;
    chassis.bms.selfCheckStatus = 0;
    chassis.bms.heartbeat = 128;
    chassis.bms.alarmLevel = 0;
    chassis.bms.currentStatus = 1;
    chassis.bms.packVoltage = 48.6;
    chassis.bms.packCurrent = 6.2;
    chassis.bms.maxCellVoltageIndex = 3;
    chassis.bms.maxCellVoltage = 4.08;
    chassis.bms.minCellVoltageIndex = 11;
    chassis.bms.minCellVoltage = 4.03;
    chassis.bms.maxTemperatureIndex = 2;
    chassis.bms.maxTemperature = 34;
    chassis.bms.minTemperatureIndex = 5;
    chassis.bms.minTemperature = 29;
    chassis.bms.soc = 78;
    chassis.bms.warningCodes = QVector<int>(12, 0);
    chassis.powerSupplyStatuses = QVector<int>(16, 1);

    auto& control = m_snapshot.controlCommandStatus;
    control.valid = true;
    control.timestampMs = mockTimestamp;
    control.mode = 6;
    control.isEnable = true;
    control.speed = m_snapshot.controlCmd.desiredVelocity;
    control.angularVelocity = m_snapshot.controlCmd.desiredAngularVelocity;
    control.expectedGear = m_snapshot.controlCmd.desiredGear;

    auto& action = m_snapshot.actionRuntimeStatus;
    action.valid = true;
    action.timestampMs = mockTimestamp;
    action.owner = 1;
    action.state = 1;
    action.goalUuid = QStringLiteral("mock-goal-crawl-001");
    action.chassisMode = 6;
    action.isEnable = true;
    action.targetSpeed = m_snapshot.controlCmd.desiredVelocity;
    action.targetHeading = m_snapshot.vehicleLocation.heading;
    action.targetAngularVelocity = m_snapshot.controlCmd.desiredAngularVelocity;

    auto& task = m_snapshot.taskRuntimeStatus;
    task.valid = true;
    task.timestampMs = mockTimestamp;
    task.taskType = 2;
    task.taskId = 1;
    task.taskEnable = true;
    task.emergencyStop = false;
    task.remoteMode = 0;
    task.powerEnable = 1;

    m_snapshot.globalPathStatus.valid = !m_snapshot.globalPath.points.isEmpty();
    m_snapshot.globalPathStatus.timestampMs = mockTimestamp;
    m_snapshot.globalPathStatus.frameId = m_snapshot.globalPath.header.frameId;
    m_snapshot.globalPathStatus.pointCount = m_snapshot.globalPath.points.size();
    m_snapshot.globalPathStatus.length = m_snapshot.globalPath.points.isEmpty()
                                             ? 0.0
                                             : m_snapshot.globalPath.points.constLast().position.x;
    m_snapshot.localPathStatus.valid = !m_snapshot.localPath.points.isEmpty();
    m_snapshot.localPathStatus.timestampMs = mockTimestamp;
    m_snapshot.localPathStatus.frameId = m_snapshot.localPath.header.frameId;
    m_snapshot.localPathStatus.goalUuid = action.goalUuid;
    m_snapshot.localPathStatus.pointCount = m_snapshot.localPath.points.size();
    m_snapshot.localPathStatus.length = m_snapshot.localPath.points.isEmpty()
                                            ? 0.0
                                            : m_snapshot.localPath.points.constLast().position.x;

    auto mockTopic = [mockTimestamp](model::VisualizationChannel channel,
                                     const QString& name,
                                     const QString& type) {
        model::TopicStatus status;
        status.channel = channel;
        status.name = name;
        status.type = type;
        status.lastUpdateMs = mockTimestamp;
        status.ageMs = 0;
        status.timeoutMs = 5000;
        status.frequencyHz = 20.0;
        status.messageCount = 1;
        status.timedOut = false;
        return status;
    };
    m_snapshot.topicStatuses = model::TopicStatusList{
        mockTopic(model::VisualizationChannel::VehicleState, QStringLiteral("vehicle_state"), QStringLiteral("AutoViz.VehicleState")),
        mockTopic(model::VisualizationChannel::Obstacles, QStringLiteral("obstacles"), QStringLiteral("AutoViz.ObstacleSet")),
        mockTopic(model::VisualizationChannel::ControlCommand, QStringLiteral("control_command"), QStringLiteral("AutoViz.ControlCommand")),
        mockTopic(model::VisualizationChannel::ChassisState, QStringLiteral("chassis_state"), QStringLiteral("AutoViz.ChassisState")),
        mockTopic(model::VisualizationChannel::ActionState, QStringLiteral("action_state"), QStringLiteral("AutoViz.ActionState")),
        mockTopic(model::VisualizationChannel::TaskState, QStringLiteral("task_state"), QStringLiteral("AutoViz.TaskState")),
        mockTopic(model::VisualizationChannel::LocalTrajectory, QStringLiteral("local_trajectory"), QStringLiteral("AutoViz.Trajectory")),
        mockTopic(model::VisualizationChannel::GlobalTrajectory, QStringLiteral("global_trajectory"), QStringLiteral("AutoViz.Trajectory"))};

    m_snapshot.pathEndpointStatus = model::PathEndpointStatus{};
    m_snapshot.runVisualizationMode = model::RunVisualizationMode::HorizontalMotion;
    m_snapshot.runtimeStatus.inputSource = VisualizationInputSource::Mock;
    m_snapshot.runtimeStatus.hasCommonPlanningControlCapability = true;
    m_snapshot.runtimeStatus.hasVerticalMotionCapability = true;
    m_snapshot.runtimeStatus.hasUnderwaterSystemCapability = true;
    m_snapshot.runtimeStatus.hasPlatformDiagnosticsCapability = true;
    m_snapshot.runtimeStatus.hasVehicleLocationData = true;
    m_snapshot.runtimeStatus.hasVehicleChassisData = true;
    m_snapshot.runtimeStatus.hasGlobalPathData = !m_snapshot.globalPath.points.isEmpty();
    m_snapshot.runtimeStatus.hasLocalPathData = !m_snapshot.localPath.points.isEmpty();
    m_snapshot.runtimeStatus.hasReferenceLineData = !m_snapshot.referenceLine.points.isEmpty();
    m_snapshot.runtimeStatus.hasObstacleData = !m_snapshot.obstacles.isEmpty();
    m_snapshot.runtimeStatus.hasControlCmdData = true;
    updatePathEndpointLocked();
    appendHistoryTrailPointLocked();
}

void DataManager::resetVisualizationData(VisualizationInputSource inputSource)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.vehicleLocation = model::VehicleLocation{};
    m_snapshot.vehicleChassisInfo = model::VehicleChassisInfo{};
    m_snapshot.globalPath = model::Trajectory{};
    m_snapshot.localPath = model::Trajectory{};
    m_snapshot.historyTrail = model::Trajectory{};
    m_snapshot.referenceLine = model::ReferenceLine{};
    m_snapshot.obstacles = model::ObstacleList{};
    m_snapshot.controlCmd = model::ControlCmd{};
    m_snapshot.topicStatuses = model::TopicStatusList{};
    m_snapshot.localizationStatus = model::LocalizationStatus{};
    m_snapshot.chassisRuntimeStatus = model::ChassisRuntimeStatus{};
    m_snapshot.controlCommandStatus = model::ControlCommandStatus{};
    m_snapshot.globalPathStatus = model::PathRuntimeStatus{};
    m_snapshot.localPathStatus = model::PathRuntimeStatus{};
    m_snapshot.pathEndpointStatus = model::PathEndpointStatus{};
    m_snapshot.actionRuntimeStatus = model::ActionRuntimeStatus{};
    m_snapshot.recentTerminalActionStatus = model::ActionRuntimeStatus{};
    m_snapshot.taskRuntimeStatus = model::TaskRuntimeStatus{};
    m_snapshot.runVisualizationMode = model::RunVisualizationMode::Unknown;
    m_snapshot.runtimeStatus = VisualizationRuntimeStatus{};
    m_snapshot.runtimeStatus.inputSource = inputSource;
}

void DataManager::replaceVisualizationSnapshot(const VisualizationSnapshot& snapshot,
                                               VisualizationInputSource inputSource)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto existingConfig = m_snapshot.vehicleConfig;
    const auto existingHistory = m_snapshot.historyTrail;
    m_snapshot = snapshot;
    if (m_snapshot.vehicleConfig.vehicleLength <= 0.0
        || m_snapshot.vehicleConfig.vehicleWidth <= 0.0) {
        m_snapshot.vehicleConfig = existingConfig;
    }
    m_snapshot.historyTrail = existingHistory;
    m_snapshot.runtimeStatus.inputSource = inputSource;
    updatePathEndpointLocked();
    updateRunVisualizationModeLocked();
    appendHistoryTrailPointLocked();
}

void DataManager::clearHistoryTrail()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.historyTrail.points.clear();
    m_snapshot.historyTrail.header = model::Header{};
}

VisualizationSnapshot DataManager::getSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto snapshot = m_snapshot;
    if (snapshot.runtimeStatus.inputSource == VisualizationInputSource::Mock) {
        for (auto& status : snapshot.topicStatuses) {
            status.ageMs = 0;
            status.timedOut = false;
        }
    } else {
        const qint64 referenceTime = snapshot.runtimeStatus.inputSource == VisualizationInputSource::Ros2Bag
                                         && snapshot.runtimeStatus.sourceTimeMs > 0
                                     ? snapshot.runtimeStatus.sourceTimeMs
                                     : QDateTime::currentMSecsSinceEpoch();
        updateTopicAges(snapshot.topicStatuses, referenceTime);
    }
    return snapshot;
}

void DataManager::updateTopicAges(model::TopicStatusList& topicStatuses, qint64 nowMs)
{
    for (auto& status : topicStatuses) {
        if (status.lastUpdateMs <= 0) {
            status.ageMs = 0;
            status.timedOut = true;
            continue;
        }
        status.ageMs = qMax<qint64>(0, nowMs - status.lastUpdateMs);
        status.timedOut = status.ageMs > status.timeoutMs;
    }
}

model::TrajectoryPoint DataManager::buildHistoryTrailPointLocked() const
{
    const auto& vehicleLocation = m_snapshot.vehicleLocation;
    const qint64 timestampMs = vehicleLocation.header.timestamp > 0
                                  ? vehicleLocation.header.timestamp
                                  : QDateTime::currentMSecsSinceEpoch();

    model::TrajectoryPoint point;
    point.position = vehicleLocation.position;
    point.theta = vehicleLocation.heading;
    point.velocity = vehicleLocation.speed;
    point.absoluteTime = static_cast<double>(timestampMs) / 1000.0;

    const auto& localization = m_snapshot.localizationStatus;
    if (localization.valid && localization.timestampMs == timestampMs) {
        point.depth = localization.depth;
        point.height = localization.height;
        point.hasDepth = true;
        point.hasHeight = true;
    }

    return point;
}

void DataManager::appendHistoryTrailPointLocked()
{
    if (!m_snapshot.runtimeStatus.hasVehicleLocationData) {
        return;
    }

    const auto& vehicleLocation = m_snapshot.vehicleLocation;
    const qint64 timestampMs = vehicleLocation.header.timestamp > 0
                                  ? vehicleLocation.header.timestamp
                                  : QDateTime::currentMSecsSinceEpoch();
    const model::TrajectoryPoint point = buildHistoryTrailPointLocked();

    auto& history = m_snapshot.historyTrail;
    history.header.timestamp = timestampMs;
    if (!history.points.isEmpty()) {
        auto& lastPoint = history.points.last();
        const double dx = point.position.x - lastPoint.position.x;
        const double dy = point.position.y - lastPoint.position.y;
        const bool sameSample = std::abs(point.absoluteTime - lastPoint.absoluteTime) < 1e-6
                                && std::hypot(dx, dy) < 1e-6;
        if (sameSample) {
            if (point.hasDepth) {
                lastPoint.depth = point.depth;
                lastPoint.hasDepth = true;
            }
            if (point.hasHeight) {
                lastPoint.height = point.height;
                lastPoint.hasHeight = true;
            }
            return;
        }

        const bool movedHorizontally = std::hypot(dx, dy) >= kHistoryTrailMinDistance;
        const bool depthChanged = point.hasDepth && lastPoint.hasDepth
                                  && std::abs(point.depth - lastPoint.depth) >= kHistoryTrailMinDepthDelta;
        const bool heightChanged = point.hasHeight && lastPoint.hasHeight
                                   && std::abs(point.height - lastPoint.height) >= kHistoryTrailMinHeightDelta;
        const bool verticalDataBecameAvailable = (point.hasDepth && !lastPoint.hasDepth)
                                                 || (point.hasHeight && !lastPoint.hasHeight);
        const bool verticalTimeFallback = isVerticalHistoryMode(m_snapshot.runVisualizationMode)
                                          && (point.absoluteTime - lastPoint.absoluteTime) >= kVerticalHistoryMinTimeIntervalSec;
        if (!movedHorizontally && !depthChanged && !heightChanged && !verticalDataBecameAvailable && !verticalTimeFallback) {
            return;
        }
    }

    history.points.push_back(point);
    while (history.points.size() > kMaxHistoryTrailPoints) {
        history.points.pop_front();
    }
}

void DataManager::updatePathEndpointLocked()
{
    auto& endpoint = m_snapshot.pathEndpointStatus;
    endpoint = model::PathEndpointStatus{};
    if (m_snapshot.globalPath.points.isEmpty()) {
        return;
    }

    const auto& point = m_snapshot.globalPath.points.constLast();
    endpoint.valid = true;
    endpoint.timestampMs = m_snapshot.globalPath.header.timestamp;
    endpoint.x = point.position.x;
    endpoint.y = point.position.y;
    endpoint.label = QStringLiteral("路径终点（非 action goal / 非任务目标点）");
}

void DataManager::updateRunVisualizationModeLocked()
{
    m_snapshot.runVisualizationMode = model::inferRunVisualizationMode(m_snapshot.actionRuntimeStatus,
                                                                       m_snapshot.taskRuntimeStatus);
}

}  // namespace autoviz::datacenter
