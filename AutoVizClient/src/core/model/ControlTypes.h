#pragma once

#include "core/model/CommonTypes.h"

namespace autoviz::model {

enum class ControlMode {
    Unknown,
    Crawl,
    Sailing
};

struct ControlCmd {
    Header header;
    ControlMode mode = ControlMode::Unknown;
    double desiredVelocity = 0.0;
    double desiredAngularVelocity = 0.0;
    double desiredWheelAngle = 0.0;
    double desiredSteerWheelAngle = 0.0;
    double desiredHeading = 0.0;
    int desiredGear = 0;
    double desiredBrake = 0.0;
    double desiredThrottle = 0.0;
    bool handBrake = false;
    // 水下垂向目标：来自 ChassisCommand.UnderwaterCommand，表示控制器实时下发的
    // 期望深度/离底高度命令。在垂向运动过程中这两个值会随任务进度变化，
    // 远比 SystemRunStates.target_depth（任务级静态目标）更适合做曲线"目标线"。
    bool hasTargetDepth = false;
    double targetDepth = 0.0;
    bool hasTargetHeight = false;
    double targetHeight = 0.0;
};

ControlCmd createMockControlCmd();

}  // namespace autoviz::model
