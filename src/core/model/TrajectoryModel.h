#pragma once

#include <QString>
#include <QVector>

#include "core/model/CommonTypes.h"

namespace autoviz::model {

struct TrajectoryPoint {
    Point2D position;
    double heading = 0.0;
    double velocity = 0.0;
    double relativeTimeSec = 0.0;
};

// Unified trajectory representation after parser adaptation.
struct TrajectoryModel {
    TimestampedData meta;
    QString frameId;
    QVector<TrajectoryPoint> points;
};

}  // namespace autoviz::model
