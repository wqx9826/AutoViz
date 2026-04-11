#include "core/model/VehicleState.h"

namespace autoviz::model {

VehicleState createMockVehicleState()
{
    VehicleState state;
    state.location.meta.timestamp = 1712800000000;
    state.location.meta.frameId = QStringLiteral("map");
    state.location.position = {0.0, 0.0};
    state.location.heading = 0.18;
    state.location.curvature = 0.012;
    state.location.speed = 8.2;
    state.location.yawRate = 0.08;
    state.location.acceleration = 0.45;
    state.location.linearVelocity = {8.2, 0.15, 0.0};
    state.location.linearAcceleration = {0.45, 0.02, 0.0};

    state.chassis.meta = state.location.meta;
    state.chassis.currentSpeed = 8.05;
    state.chassis.currentWheelAngle = 2.4;
    state.chassis.currentSteerWheelAngle = 18.6;
    state.chassis.throttleRatio = 0.24;
    state.chassis.brakeRatio = 0.0;
    state.chassis.gear = GearPosition::Drive;
    state.chassis.energyRatio = 0.78;
    state.chassis.leftWheelSpeed = 8.0;
    state.chassis.rightWheelSpeed = 8.1;
    return state;
}

void applyVehicleGeometryConfig(VehicleState& state, double vehicleLength, double vehicleWidth, double wheelBase)
{
    state.vehicleLength = vehicleLength;
    state.vehicleWidth = vehicleWidth;
    state.wheelBase = wheelBase;
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
