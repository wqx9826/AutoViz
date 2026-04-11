#pragma once

#include <QString>

#include "core/model/CommonTypes.h"

namespace autoviz::model {

enum class GearPosition {
    Unknown,
    Neutral,
    Drive,
    Reverse,
    Park,
    Emergency
};

struct VehicleLocation {
    TopicMetadata meta;
    Point2D position;
    double heading = 0.0;
    double curvature = 0.0;
    double jerk = 0.0;
    double speed = 0.0;
    double yawRate = 0.0;
    double acceleration = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    Vector3D linearVelocity;
    Vector3D linearAcceleration;
};

struct VehicleChassisInfo {
    TopicMetadata meta;
    double currentSpeed = 0.0;
    double currentWheelAngle = 0.0;
    double currentSteerWheelAngle = 0.0;
    double throttleRatio = 0.0;
    double brakeRatio = 0.0;
    GearPosition gear = GearPosition::Unknown;
    bool handBrake = false;
    double energyRatio = 0.0;
    unsigned long stateCode = 0;
    double leftWheelSpeed = 0.0;
    double rightWheelSpeed = 0.0;
};

struct VehicleState {
    VehicleLocation location;
    VehicleChassisInfo chassis;
    double wheelBase = 2.85;
    double vehicleLength = 4.9;
    double vehicleWidth = 1.95;
};

VehicleState createMockVehicleState();
void applyVehicleGeometryConfig(VehicleState& state, double vehicleLength, double vehicleWidth, double wheelBase);
QString toDisplayString(GearPosition gear);

}  // namespace autoviz::model
