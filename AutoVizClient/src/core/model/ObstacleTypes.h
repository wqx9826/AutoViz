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
    Circle,
    Polygon,
    Box
};

// The current robot_ws FinalTarget message supplies a reference point and a
// conservative radius, not a geometric centre and box dimensions. These
// fields retain the generic scene model while preventing that distinction from
// being lost before rendering and detail display.
enum class FinalTargetBoundaryState {
    NotApplicable,
    NotProvided,
    Valid,
    InvalidFallbackCircle
};

struct Obstacle {
    int id = -1;
    ObstacleType type = ObstacleType::Unknown;
    int sourceClass = 0;
    QString classLabel;
    QString sourceTopic;
    ObstacleShapeType shape = ObstacleShapeType::Box;
    Header header;
    bool isStatic = true;
    bool isVirtual = false;
    TrajectoryPoint position;
    double length = 0.0;
    double width = 0.0;
    double height = 0.0;
    Polygon2D polygon;
    Box2D boundingBox;
    Point2D anchorPoint;
    QVector<TrajectoryPoint> predictedTrajectory;
    bool geodeticValid = false;
    double longitude = 0.0, latitude = 0.0, depth = 0.0, heightAboveBottom = 0.0;
    bool dimensionsValid = false;
    bool headingValid = false;
    bool isFinalTarget = false;
    double conservativeRadius = 0.0;
    FinalTargetBoundaryState finalTargetBoundaryState = FinalTargetBoundaryState::NotApplicable;
    QString finalTargetBoundaryNote;
};

using ObstacleList = QVector<Obstacle>;

ObstacleList createMockObstacles();
QString toDisplayString(ObstacleType type);

}  // namespace autoviz::model
