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
    Header header;
    Point2D position;
    double heading = 0.0;
    double curvature = 0.0;
    double jerk = 0.0;
    double speed = 0.0;
    double yawRate = 0.0;
    double acceleration = 0.0;
    double roll = 0.0;
    double pitch = 0.0;
    Vector3D Velocity;
    Vector3D Acceleration;
    qint64 startTimeS = 0;
    double gaussX = 0.0, gaussY = 0.0, gaussZ = 0.0;
    double originLongitude = 0.0, originLatitude = 0.0;
    double originX = 0.0, originY = 0.0, originZ = 0.0;
    double odomHeading = 0.0;
    double omegaX = 0.0, omegaY = 0.0;
    qint64 usblMessageWords[4] = {};
};

struct VehicleChassisInfo {
    Header header;
    double currentSpeed = 0.0;
    double currentAngularVelocity = 0.0;
    double currentWheelAngle = 0.0;
    double currentSteerWheelAngle = 0.0;
    double throttleRatio = 0.0;
    double brakeRatio = 0.0;
    uint8_t currentGearPosition = 0;
    bool handBrake = false;
    double energyRatio = 0.0;
    double leftWheelSpeed = 0.0;
    double rightWheelSpeed = 0.0;
};

struct VehicleConfig {
    double wheelBase = 2.85;
    double vehicleLength = 4.9;
    double vehicleWidth = 1.95;
};

VehicleLocation createMockVehicleLocation();
VehicleChassisInfo createMockVehicleChassisInfo();
VehicleConfig createDefaultVehicleConfig();
QString toDisplayString(GearPosition gear);

}  // namespace autoviz::model
