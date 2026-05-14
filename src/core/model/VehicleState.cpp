#include "core/model/VehicleState.h"

namespace autoviz::model {

VehicleLocation createMockVehicleLocation()
{
    VehicleLocation location;
    location.header.timestamp = 1712800000000;
    location.header.frameId = QStringLiteral("map");
    location.position = {0.0, 0.0};
    location.heading = 0.18;
    location.curvature = 0.012;
    location.speed = 8.2;
    location.yawRate = 0.08;
    location.acceleration = 0.45;
    location.Velocity = {8.2, 0.15, 0.0};
    location.Acceleration = {0.45, 0.02, 0.0};
    return location;
}

VehicleChassisInfo createMockVehicleChassisInfo()
{
    VehicleChassisInfo chassis;
    chassis.header = createMockVehicleLocation().header;
    chassis.currentSpeed = 8.05;
    chassis.currentWheelAngle = 2.4;
    chassis.currentSteerWheelAngle = 18.6;
    chassis.throttleRatio = 0.24;
    chassis.brakeRatio = 0.0;
    chassis.currentGearPosition = static_cast<uint8_t>(GearPosition::Drive);
    chassis.energyRatio = 0.78;
    chassis.leftWheelSpeed = 8.0;
    chassis.rightWheelSpeed = 8.1;
    return chassis;
}

VehicleConfig createDefaultVehicleConfig()
{
    return VehicleConfig{};
}

QString toDisplayString(GearPosition gear)
{
    switch (gear) {
    case GearPosition::Neutral:
        return QStringLiteral("N");
    case GearPosition::Drive:
        return QStringLiteral("D");
    case GearPosition::Reverse:
        return QStringLiteral("R");
    case GearPosition::Park:
        return QStringLiteral("P");
    case GearPosition::Emergency:
        return QStringLiteral("急停");
    case GearPosition::Unknown:
    default:
        return QStringLiteral("--");
    }
}

}  // namespace autoviz::model
