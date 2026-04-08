#pragma once

#include <QString>
#include <QVector>

namespace autoviz::model {

// Shared geometric primitives used by the normalized internal model layer.
struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Pose2D {
    Point2D position;
    double heading = 0.0;
};

struct Velocity2D {
    double longitudinal = 0.0;
    double lateral = 0.0;
};

struct TimestampedData {
    double timestampSec = 0.0;
    QString sourceTopic;
};

using Polyline2D = QVector<Point2D>;

}  // namespace autoviz::model
