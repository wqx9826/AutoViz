#pragma once

#include <QString>
#include <QVector>

namespace autoviz::model {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Polygon2D {
    QVector<Point2D> vertices;
};

struct Box2D {
    Point2D center;
    double heading = 0.0;
    double length = 0.0;
    double width = 0.0;
};

struct TopicMetadata {
    qint64 timestamp = 0;
    QString frameId;
    QString sourceTopic;
};

using Polyline2D = QVector<Point2D>;

}  // namespace autoviz::model
