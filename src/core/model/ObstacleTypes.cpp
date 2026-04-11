#include "core/model/ObstacleTypes.h"

namespace autoviz::model {

ObstacleList createMockObstacles()
{
    ObstacleList obstacles;

    Obstacle vehicle;
    vehicle.id = 101;
    vehicle.type = ObstacleType::Vehicle;
    vehicle.isStatic = false;
    vehicle.meta.frameId = QStringLiteral("map");
    vehicle.position.position = {18.0, 3.8};
    vehicle.position.theta = 0.12;
    vehicle.length = 4.5;
    vehicle.width = 1.9;
    vehicle.boundingBox = {vehicle.position.position, vehicle.position.theta, vehicle.length, vehicle.width};
    obstacles.push_back(vehicle);

    Obstacle pedestrian;
    pedestrian.id = 201;
    pedestrian.type = ObstacleType::Pedestrian;
    pedestrian.shape = ObstacleShapeType::Polygon;
    pedestrian.meta.frameId = QStringLiteral("map");
    pedestrian.position.position = {11.5, -4.0};
    pedestrian.length = 0.8;
    pedestrian.width = 0.8;
    pedestrian.polygon.vertices = {{11.0, -4.3}, {11.8, -4.4}, {12.0, -3.6}, {11.1, -3.5}};
    obstacles.push_back(pedestrian);

    return obstacles;
}

QString toDisplayString(ObstacleType type)
{
    switch (type) {
    case ObstacleType::Vehicle:
        return QStringLiteral("车辆");
    case ObstacleType::Pedestrian:
        return QStringLiteral("行人");
    case ObstacleType::Cyclist:
        return QStringLiteral("骑行者");
    case ObstacleType::Cone:
        return QStringLiteral("锥桶");
    case ObstacleType::Unknown:
    default:
        return QStringLiteral("未知");
    }
}

}  // namespace autoviz::model
