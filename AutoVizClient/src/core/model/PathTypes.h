#pragma once

#include <QVector>

#include "core/model/CommonTypes.h"

namespace autoviz::model {

struct TrajectoryPoint {
    Point2D position;
    double theta = 0.0;
    double kappa = 0.0;
    double dkappa = 0.0;
    double velocity = 0.0;
    double acceleration = 0.0;
    double jerk = 0.0;
    double s = 0.0;
    double dsDt = 0.0;
    double ddsDt = 0.0;
    double dddsDt = 0.0;
    double l = 0.0;
    double dlDt = 0.0;
    double ddlDt = 0.0;
    double dddlDt = 0.0;
    double dlDs = 0.0;
    double ddlDs = 0.0;
    double dddlDs = 0.0;
    double relativeTime = 0.0;
    double absoluteTime = 0.0;
    double depth = 0.0;
    double height = 0.0;
    bool hasDepth = false;
    bool hasHeight = false;
    double z = 0.0;
    double quaternionX = 0.0, quaternionY = 0.0, quaternionZ = 0.0, quaternionW = 1.0;
    Vector3D velocity3d;
    Vector3D acceleration3d;
    Vector3D angularVelocity3d;
    Vector3D angularAcceleration3d;
    bool hasPose = false;
    bool hasVelocity3d = false;
    bool hasAcceleration3d = false;
};

struct ReferencePoint {
    Point2D position;
    double theta = 0.0;
    double kappa = 0.0;
    double dkappa = 0.0;
    double s = 0.0;
};

struct Trajectory {
    Header header;
    QVector<TrajectoryPoint> points;
};

struct ReferenceLine {
    Header header;
    QVector<ReferencePoint> points;
};

Trajectory createMockGlobalPath();
Trajectory createMockLocalPath();
ReferenceLine createMockReferenceLine();

}  // namespace autoviz::model
