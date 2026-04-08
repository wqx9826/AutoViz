#pragma once

#include <QString>
#include <QVector>

#include "core/model/CommonTypes.h"

namespace autoviz::model {

struct ObstacleModel {
    TimestampedData meta;
    int id = -1;
    QString category;
    Pose2D pose;
    double length = 0.0;
    double width = 0.0;
    double height = 0.0;
    Velocity2D velocity;
    Polyline2D contour;
};

using ObstacleCollection = QVector<ObstacleModel>;

}  // namespace autoviz::model
