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
    location.speed = 0.35;
    location.yawRate = 0.04;
    location.acceleration = 0.03;
    location.Velocity = {0.35, 0.02, 0.0};
    location.Acceleration = {0.03, 0.01, 0.0};
    return location;
}

VehicleChassisInfo createMockVehicleChassisInfo()
{
    VehicleChassisInfo chassis;
    chassis.header = createMockVehicleLocation().header;
    chassis.currentSpeed = 0.34;
    chassis.currentAngularVelocity = 0.04;
    chassis.currentWheelAngle = 0.0;
    chassis.currentSteerWheelAngle = 0.0;
    chassis.throttleRatio = 0.18;
    chassis.brakeRatio = 0.0;
    chassis.currentGearPosition = static_cast<uint8_t>(GearPosition::Drive);
    chassis.energyRatio = 0.78;
    chassis.leftWheelSpeed = 0.34;
    chassis.rightWheelSpeed = 0.35;
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
