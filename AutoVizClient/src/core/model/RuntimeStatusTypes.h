#pragma once

#include <QVector>
#include <QString>

#include "core/model/CommonTypes.h"

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
    VehicleParameters,
    RangeMotionDirective,
    InspectionGoal
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

enum class ControlEventSource { Unknown, ActionExpectation, ControlCommand, ChassisFeedback };

struct ControlStateEvent {
    Header header;
    ControlEventSource source = ControlEventSource::Unknown;
    QString goalUuid;
    bool hasPreviousMode = false;
    int previousMode = 0;
    bool hasCurrentMode = false;
    int currentMode = 0;
    bool hasPreviousGear = false;
    int previousGear = 0;
    bool hasCurrentGear = false;
    int currentGear = 0;
    bool hasPreviousEnabled = false;
    bool previousEnabled = false;
    bool hasCurrentEnabled = false;
    bool currentEnabled = false;
    bool hasPreviousCrawlOutputEnabled = false;
    bool previousCrawlOutputEnabled = false;
    bool hasCurrentCrawlOutputEnabled = false;
    bool currentCrawlOutputEnabled = false;
};

using ControlStateEventList = QVector<ControlStateEvent>;

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
    bool hasLongitude = false;
    double longitude = 0.0;
    bool hasLatitude = false;
    double latitude = 0.0;
    bool hasUsblX = false;
    double usblX = 0.0;
    bool hasUsblY = false;
    double usblY = 0.0;
    bool hasUsblZ = false;
    double usblZ = 0.0;
    qint64 startTimeS = 0;
    double gaussX = 0.0, gaussY = 0.0, gaussZ = 0.0;
    double originLongitude = 0.0, originLatitude = 0.0;
    double originX = 0.0, originY = 0.0, originZ = 0.0;
    double odomHeading = 0.0;
    double omegaX = 0.0, omegaY = 0.0;
    qint64 usblMessageWords[4] = {};
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
    HeightHold,
    Landing
};

struct ChassisRuntimeStatus {
    bool valid = false;
    Header header;
    qint64 timestampMs = 0;
    double currentSpeed = 0.0;
    double currentAngularVelocity = 0.0;
    bool hasHeadingKp = false;
    bool hasHeadingTargetValue = false;
    bool hasHeadingActualValue = false;
    bool hasHeadingError = false;
    bool hasHeadingOutput = false;
    double headingKp = 0.0;
    double headingTargetValue = 0.0;
    double headingActualValue = 0.0;
    double headingError = 0.0;
    double headingOutput = 0.0;
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
    struct TailMotor {
        QString id;
        double busCurrent = 0.0;
        int controllerTemperature = 0;
        double targetSpeedRpm = 0.0;
        double actualSpeedRpm = 0.0;
    };
    QVector<TailMotor> tailThrusterMotors;
};

struct ControlCommandStatus {
    bool valid = false;
    Header header;
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
    bool hasSourceMode = false;
    int sourceMode = 0;
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
    Header header;
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
    bool hasTaskType = false;
    int taskType = 0;
    bool hasTaskId = false;
    int taskId = 0;
    bool hasTaskEnable = false;
    bool taskEnable = false;
    bool hasTaskStartRequested = false;
    bool taskStartRequested = false;
    bool hasActionEnabled = false;
    bool actionEnabled = false;
    bool hasBuoyancyAdjust = false;
    int buoyancyAdjust = 0;
    bool hasEmergencyStop = false;
    bool emergencyStop = false;
    bool hasReleaseEmergencyAscent = false;
    bool releaseEmergencyAscent = false;
    bool hasRemoteMode = false;
    int remoteMode = 0;
    bool hasPowerEnable = false;
    int powerEnable = 0;
    bool hasRemoteControl = false;
    // TaskParams 中的遥控/调试指令字段
    bool hasCrawlGear = false;
    int crawlGear = 0;
    bool hasCrawlSpeed = false;
    double crawlSpeed = 0.0;
    bool hasCrawlAngularVelocity = false;
    double crawlAngularVelocity = 0.0;
    bool hasForwardPercent = false;
    int forwardPercent = 0;
    bool hasTurnPercent = false;
    int turnPercent = 0;
    bool hasDivePercent = false;
    int divePercent = 0;
    bool hasLeftTailActuatorSpeed = false;
    int leftTailActuatorSpeed = 0;
    bool hasRightTailActuatorSpeed = false;
    int rightTailActuatorSpeed = 0;
    bool hasLeftVerticalActuatorSpeed = false;
    int leftVerticalActuatorSpeed = 0;
    bool hasRightVerticalActuatorSpeed = false;
    int rightVerticalActuatorSpeed = 0;
    bool hasBackVerticalActuatorSpeed = false;
    int backVerticalActuatorSpeed = 0;
    QVector<int> powerSupplyCommands;
};

struct RangeMotionRuntimeStatus {
    bool valid = false;
    Header header;
    qint64 timestampMs = 0;
    bool hasTaskId = false;
    int taskId = 0;
    bool hasCommandSequence = false;
    quint32 commandSequence = 0;
    bool hasMotion = false;
    int motion = 0;
    bool hasSpeedLimit = false;
    double speedLimitMps = 0.0;
    bool hasReason = false;
    QString reason;
};

struct InspectionGoalRuntimeStatus {
    bool valid = false;
    Header header;
    qint64 timestampMs = 0;
    bool hasTaskId = false;
    int taskId = 0;
    bool hasGoalId = false;
    quint32 goalId = 0;
    bool hasTargetId = false;
    quint32 targetId = 0;
    bool hasTargetPosition = false;
    Vector3D targetPosition;
    bool hasObservationPosition = false;
    Vector3D observationPosition;
    bool hasHeading = false;
    double headingRad = 0.0;
    bool hasHoldOnArrival = false;
    bool holdOnArrival = false;
    bool hasMode = false;
    int mode = 0;
    bool hasSpeedLimit = false;
    double speedLimitMps = 0.0;
};

struct FinalTargetSetRuntimeStatus {
    bool valid = false;
    Header header;
    qint64 timestampMs = 0;
    bool hasTaskId = false;
    int taskId = 0;
    int targetCount = 0;
    QString rejectionReason;
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
    return (mode >= 1 && mode <= 5) || mode == 7 || mode == 9 || mode == 10;
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
