#pragma once

#include "core/model/CommonTypes.h"

namespace autoviz::model {

struct ControlCmd {
    TopicMetadata meta;
    double desiredVelocity = 0.0;
    double desiredAngularVelocity = 0.0;
    double desiredWheelAngle = 0.0;
    double desiredSteerWheelAngle = 0.0;
    int desiredGear = 0;
    double desiredBrake = 0.0;
    double desiredThrottle = 0.0;
    bool handBrake = false;
};

ControlCmd createMockControlCmd();

}  // namespace autoviz::model
