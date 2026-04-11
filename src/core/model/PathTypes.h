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
};

struct ReferencePoint {
    Point2D position;
    double theta = 0.0;
    double kappa = 0.0;
    double dkappa = 0.0;
    double s = 0.0;
};

struct Trajectory {
    TopicMetadata meta;
    QVector<TrajectoryPoint> points;
};

struct ReferenceLine {
    TopicMetadata meta;
    QVector<ReferencePoint> points;
};

Trajectory createMockGlobalPath();
Trajectory createMockLocalPath();
ReferenceLine createMockReferenceLine();

}  // namespace autoviz::model
