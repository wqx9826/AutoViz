#pragma once

#include <QVector>
#include <QString>

namespace autoviz::model {

struct TopicStatus {
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
};

struct ChassisRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    double currentSpeed = 0.0;
    double currentAngularVelocity = 0.0;
    int gearStatus = 0;
    int waterTankLevelStatus = 0;
    int waterTankStatus = 0;
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
};

struct ControlCommandStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    int mode = 0;
    bool isEnable = false;
    double velocity = 0.0;
    double angularVelocity = 0.0;
    int expectedGear = 0;
    bool isUseWaterActuator = false;
    double depth = 0.0;
    double height = 0.0;
    double heading = 0.0;
    double speed = 0.0;
    double diveSpeed = 0.0;
    int leftWaterActuatorSpeed = 0;
    int rightWaterActuatorSpeed = 0;
    int buoyancyAdjust = 0;
    bool isOpenSonarPower = false;
};

struct PathRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    QString frameId;
    int pointCount = 0;
    double length = 0.0;
};

struct ActionRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    int owner = 0;
    int state = 0;
    int chassisMode = 0;
    bool isEnable = false;
    double targetDepth = 0.0;
    double targetHeight = 0.0;
    int buoyancyAdjust = 0;
    double targetSpeed = 0.0;
};

struct TaskRuntimeStatus {
    bool valid = false;
    qint64 timestampMs = 0;
    int taskType = 0;
    int taskId = 0;
    bool taskEnable = false;
    bool emergencyStop = false;
    int remoteMode = 0;
    int powerEnable = 0;
};

enum class RunVisualizationMode {
    Idle,
    HorizontalMotion,
    VerticalMotion,
    BuoyancyAdjust,
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

inline QString toDisplayString(RunVisualizationMode mode)
{
    switch (mode) {
    case RunVisualizationMode::Idle:
        return QStringLiteral("空闲");
    case RunVisualizationMode::HorizontalMotion:
        return QStringLiteral("水平运动");
    case RunVisualizationMode::VerticalMotion:
        return QStringLiteral("垂向动作");
    case RunVisualizationMode::BuoyancyAdjust:
        return QStringLiteral("浮力调节");
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

    if (action.valid) {
        if (action.chassisMode == 3 || action.buoyancyAdjust == 1 || action.buoyancyAdjust == 2) {
            return RunVisualizationMode::BuoyancyAdjust;
        }
        if (action.owner == 1 || action.chassisMode == 4 || action.chassisMode == 5 || action.chassisMode == 6) {
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
