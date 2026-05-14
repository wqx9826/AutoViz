#pragma once

#include <QString>

namespace autoviz::ui::charts {

enum class ControlDebugMode {
    Standby,
    Running,
    Error
};

struct ControlDebugData {
    qint64 timestampMs = 0;
    qint64 elapsedMs = 0;
    qint64 sourceTimestampMs = 0;
    ControlDebugMode mode = ControlDebugMode::Standby;
    QString feedbackSource;
    bool timedOut = false;

    bool hasCmdSpeed = false;
    bool hasFeedbackSpeed = false;
    bool hasSpeedError = false;
    bool hasCmdYaw = false;
    bool hasFeedbackYaw = false;
    bool hasYawError = false;
    bool hasLateralError = false;
    bool hasPathYawError = false;

    double cmdSpeed = 0.0;
    double feedbackSpeed = 0.0;
    double speedError = 0.0;
    double cmdYaw = 0.0;
    double feedbackYaw = 0.0;
    double yawError = 0.0;
    double lateralError = 0.0;
    double pathYawError = 0.0;
};

}  // namespace autoviz::ui::charts
