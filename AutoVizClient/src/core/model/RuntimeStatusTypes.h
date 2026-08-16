#pragma once

#include <QVector>
#include <QString>

namespace autoviz::model {

enum class VisualizationChannel {
    Unknown,
    VehicleState,
    ChassisState,
    ControlCommand,
    GlobalTrajectory,
    LocalTrajectory,
    ReferenceLine,
    Obstacles,
    ActionState,
    TaskState,
    RuntimeState,
    VehicleParameters
};

struct TopicStatus {
    VisualizationChannel channel = VisualizationChannel::Unknown;
    QString name;
    QString type;
    qint64 lastUpdateMs = 0;
    qint64 ageMs = 0;
    qint64 timeoutMs = 1000;
    double frequencyHz = 0.0;
    quint64 messageCount = 0;
    bool timedOut = true;
};

using TopicStatusList = QVector<TopicStatus>;

struct LocalizationStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    qint64 gpsTime = 0;
    qint64 status = 0;
    qint64 error = 0;
    double odomX = 0.0;
    double odomY = 0.0;
    double odomZ = 0.0;
    double heading = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double velocityZ = 0.0;
    double velocity = 0.0;
    double omegaZ = 0.0;
    double acc = 0.0;
    double depth = 0.0;
    double height = 0.0;
    double longitude = 0.0;
    double latitude = 0.0;
    double usblX = 0.0;
    double usblY = 0.0;
    double usblZ = 0.0;
};

// ChassisStates carries a snapshot of the latest decoded CAN fields. These
// flags describe fields present in the ROS message; they do not imply that
// every underlying CAN frame has an independent freshness timestamp.
struct CrawlMotorRuntimeStatus {
    bool valid = false;
    double speedRpm = 0.0;
    double torqueOrQAxisCurrent = 0.0;
    int temperature = 0;
    double busVoltage = 0.0;
    bool controllerReady = false;
    bool outputEnabled = false;
    int controllerUTemperature = 0;
    int controllerVTemperature = 0;
    bool fault = false;
    int faultCode = 0;
    bool commandEnable = false;
    bool commandSpeedMode = false;
    bool commandReverse = false;
    double commandSpeedRpm = 0.0;
    double commandTorqueOrQAxisCurrent = 0.0;
};

struct BmsRuntimeStatus {
    bool valid = false;
    int selfCheckStatus = 0;
    int heartbeat = 0;
    int alarmLevel = 0;
    int currentStatus = 0;
    double packVoltage = 0.0;
    double packCurrent = 0.0;
    int maxCellVoltageIndex = 0;
    double maxCellVoltage = 0.0;
    int minCellVoltageIndex = 0;
    double minCellVoltage = 0.0;
    int maxTemperatureIndex = 0;
    int maxTemperature = 0;
    int minTemperatureIndex = 0;
    int minTemperature = 0;
    int soc = 0;
    QVector<int> warningCodes;
};

enum class WaterTankState {
    Unknown,
    Idle,
    Filling,
    Draining,
    ManualOverride,
    Fault,
    FillDone,
    DrainDone
};

enum class VerticalControlMode {
    None,
    DepthHold,
    HeightHold
};

struct ChassisRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    double currentSpeed = 0.0;
    double currentAngularVelocity = 0.0;
    int gearStatus = 0;
    int waterTankLevelStatus = 0;
    bool waterTankLevelIsRaw = false;
    WaterTankState waterTankState = WaterTankState::Unknown;
    int waterHeartbeat = 0;
    int crawlHeartbeat = 0;
    int leftTailActuatorStatus = 0;
    int rightTailActuatorStatus = 0;
    int leftVerticalActuatorStatus = 0;
    int rightVerticalActuatorStatus = 0;
    int backVerticalActuatorStatus = 0;
    int leftCrawlActuatorFaultCode = 0;
    int rightCrawlActuatorFaultCode = 0;
    bool highVoltageBmsStatus = false;
    bool dccdcStatus = false;
    int highVoltageBmsSocStatus = 0;
    double smartPowerInputVoltageStatus = 0.0;
    bool emergencyAscentActive = false;
    CrawlMotorRuntimeStatus leftCrawlMotor;
    CrawlMotorRuntimeStatus rightCrawlMotor;
    BmsRuntimeStatus bms;
    QVector<int> powerSupplyStatuses;
};

struct ControlCommandStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    int mode = 0;
    bool isEnable = false;
    double speed = 0.0;
    double angularVelocity = 0.0;  // rad/s
    int expectedGear = 0;
    bool isUseWaterActuator = false;
    int naviMode = 0;
    VerticalControlMode verticalControlMode = VerticalControlMode::None;
    double depth = 0.0;
    double height = 0.0;
    double heading = 0.0;
    bool emergencyAscent = false;
    int leftWaterActuatorSpeed = 0;
    int rightWaterActuatorSpeed = 0;
    int buoyancyAdjust = 0;
    bool isOpenSonarPower = false;
};

struct PathRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    QString frameId;
    QString goalUuid;
    int pointCount = 0;
    double length = 0.0;
};

struct ActionRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    int owner = 0;
    int state = 0;
    QString goalUuid;
    QString message;
    QString actionName;
    int chassisMode = 0;
    bool isEnable = false;
    int naviMode = 0;
    VerticalControlMode verticalControlMode = VerticalControlMode::None;
    double targetDepth = 0.0;
    double targetHeight = 0.0;
    int buoyancyAdjust = 0;
    double targetSpeed = 0.0;
    double targetHeading = 0.0;
    double targetAngularVelocity = 0.0;  // normalized to rad/s
    bool emergencyAscent = false;
    bool hasNativeStatus = false;
    int nativeStatus = 0;
    qint64 nativeStatusTimestampMs = 0;
    bool hasFeedbackProgress = false;
    double feedbackProgress = 0.0;
    qint64 feedbackTimestampMs = 0;
};

struct TaskRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    int taskType = 0;
    int taskId = 0;
    bool taskEnable = false;
    bool emergencyStop = false;
    bool releaseEmergencyAscent = false;
    int remoteMode = 0;
    int powerEnable = 0;
    // TaskParams 中的遥控/调试指令字段
    int crawlGear = 0;
    double crawlSpeed = 0.0;
    double crawlAngularVelocity = 0.0;
    int forwardPercent = 0;
    int turnPercent = 0;
    int divePercent = 0;
    int leftTailActuatorSpeed = 0;
    int rightTailActuatorSpeed = 0;
    int leftVerticalActuatorSpeed = 0;
    int rightVerticalActuatorSpeed = 0;
    int backVerticalActuatorSpeed = 0;
    QVector<int> powerSupplyCommands;
};

enum class RunVisualizationMode {
    Idle,
    HorizontalMotion,
    VerticalMotion,
    EmergencyStop,
    Unknown
};

struct PathEndpointStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    double x = 0.0;
    double y = 0.0;
    QString label;
};

inline bool isCrawlChassisMode(int mode)
{
    return mode == 6 || mode == 8 || mode == 11;
}

inline bool isSailingChassisMode(int mode)
{
    return (mode >= 1 && mode <= 5) || mode == 7 || mode == 10;
}

inline QString toDisplayString(RunVisualizationMode mode)
{
    switch (mode) {
    case RunVisualizationMode::Idle:
        return QStringLiteral("空闲");
    case RunVisualizationMode::HorizontalMotion:
        return QStringLiteral("水平运动");
    case RunVisualizationMode::VerticalMotion:
        return QStringLiteral("垂向动作");
    case RunVisualizationMode::EmergencyStop:
        return QStringLiteral("急停");
    case RunVisualizationMode::Unknown:
    default:
        return QStringLiteral("未知");
    }
}

inline RunVisualizationMode inferRunVisualizationMode(const ActionRuntimeStatus& action,
                                                      const TaskRuntimeStatus& task)
{
    if (task.valid && task.emergencyStop) {
        return RunVisualizationMode::EmergencyStop;
    }

    if (!action.valid && !task.valid) {
        return RunVisualizationMode::Unknown;
    }

    if (action.valid && action.state == 0 && action.owner == 0) {
        return RunVisualizationMode::Idle;
    }

    // 只有执行中的 Action 才驱动主视图和 T-Z 采样。终态仍保留在 Action 信息页，
    // 但不能让完成后的垂向动作继续表现为正在执行。
    if (action.valid && action.state == 1) {
        if (action.verticalControlMode == VerticalControlMode::DepthHold
            || action.verticalControlMode == VerticalControlMode::HeightHold) {
            return RunVisualizationMode::VerticalMotion;
        }
        if (action.owner == 1 || action.chassisMode == 4 || action.chassisMode == 5
            || action.chassisMode == 6 || action.chassisMode == 7 || action.chassisMode == 8
            || action.chassisMode == 10 || action.chassisMode == 11) {
            return RunVisualizationMode::HorizontalMotion;
        }
        if (action.owner == 2 || action.chassisMode == 1 || action.chassisMode == 2) {
            return RunVisualizationMode::VerticalMotion;
        }
        if (action.owner == 0) {
            return RunVisualizationMode::Idle;
        }
    }

    return RunVisualizationMode::Unknown;
}

}  // namespace autoviz::model
