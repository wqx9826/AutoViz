#include "core/model/ControlTypes.h"

namespace autoviz::model {

ControlCmd createMockControlCmd()
{
    ControlCmd command;
    command.header.timestamp = 1712800000000;
    command.header.frameId = QStringLiteral("base_link");
    command.mode = ControlMode::Crawl;
    command.desiredVelocity = 0.38;
    command.desiredAngularVelocity = 0.04;
    command.desiredWheelAngle = 0.0;
    command.desiredSteerWheelAngle = 0.0;
    command.desiredGear = 1;
    command.desiredThrottle = 0.22;
    command.desiredBrake = 0.0;
    return command;
}

}  // namespace autoviz::model
