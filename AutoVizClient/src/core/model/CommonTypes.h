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

struct Header {
    qint64 timestamp = 0;
    qint64 sourceTimestamp = 0;
    qint64 receiveTimestamp = 0;
    quint64 sequence = 0;
    QString frameId;
};

using Polyline2D = QVector<Point2D>;

}  // namespace autoviz::model
