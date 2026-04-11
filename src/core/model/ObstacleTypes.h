#pragma once

#include <QVector>

#include "core/model/CommonTypes.h"
#include "core/model/PathTypes.h"

namespace autoviz::model {

enum class ObstacleType {
    Unknown,
    Vehicle,
    Pedestrian,
    Cyclist,
    Cone
};

enum class ObstacleShapeType {
    Point,
    Polygon,
    Box
};

struct Obstacle {
    int id = -1;
    ObstacleType type = ObstacleType::Unknown;
    ObstacleShapeType shape = ObstacleShapeType::Box;
    TopicMetadata meta;
    bool isStatic = true;
    bool isVirtual = false;
    TrajectoryPoint position;
    double length = 0.0;
    double width = 0.0;
    Polygon2D polygon;
    Box2D boundingBox;
    Point2D anchorPoint;
    QVector<TrajectoryPoint> predictedTrajectory;
};

using ObstacleList = QVector<Obstacle>;

ObstacleList createMockObstacles();
QString toDisplayString(ObstacleType type);

}  // namespace autoviz::model
