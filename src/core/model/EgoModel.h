#pragma once

#include <QString>

#include "core/model/CommonTypes.h"

namespace autoviz::model {

// Internal ego vehicle state independent from any external message schema.
struct EgoModel {
    TimestampedData meta;
    QString vehicleId;
    Pose2D pose;
    Velocity2D velocity;
    double steeringAngle = 0.0;
    double wheelBase = 2.8;
};

}  // namespace autoviz::model
