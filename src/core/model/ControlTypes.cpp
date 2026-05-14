#include "core/model/ControlTypes.h"

namespace autoviz::model {

ControlCmd createMockControlCmd()
{
    ControlCmd command;
    command.header.timestamp = 1712800000000;
    command.header.frameId = QStringLiteral("base_link");
    command.mode = ControlMode::Crawl;
    command.desiredVelocity = 8.2;
    command.desiredAngularVelocity = 0.08;
    command.desiredWheelAngle = 2.5;
    command.desiredSteerWheelAngle = 18.0;
    command.desiredGear = 2;
    command.desiredThrottle = 0.22;
    command.desiredBrake = 0.0;
    return command;
}

}  // namespace autoviz::model
