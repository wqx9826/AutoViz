#include "core/render/SceneManager.h"

#include <cmath>

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

#include "ui/VisualizationView.h"

namespace autoviz::render {

namespace {
constexpr double kRadToDeg = 57.29577951308232;
constexpr double kVehicleHeadingOffsetDeg = 180.0;

QPainterPath buildTrajectoryPath(const autoviz::model::Trajectory& trajectory)
{
    QPainterPath path;
    if (trajectory.points.isEmpty()) {
        return path;
    }

    path.moveTo(trajectory.points.first().position.x, -trajectory.points.first().position.y);
    for (int index = 1; index < trajectory.points.size(); ++index) {
        const auto& point = trajectory.points.at(index);
        path.lineTo(point.position.x, -point.position.y);
    }
    return path;
}

QPainterPath buildReferencePath(const autoviz::model::ReferenceLine& line)
{
    QPainterPath path;
    if (line.points.isEmpty()) {
        return path;
    }

    path.moveTo(line.points.first().position.x, -line.points.first().position.y);
    for (int index = 1; index < line.points.size(); ++index) {
        const auto& point = line.points.at(index);
        path.lineTo(point.position.x, -point.position.y);
    }
    return path;
}
}  // namespace

SceneManager::SceneManager(VisualizationView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_scene(view != nullptr ? view->scene() : nullptr)
{
}

void SceneManager::initializeScene()
{
    if (m_view != nullptr) {
        m_view->setBackgroundColor(QColor("#17212b"));
        m_view->setOverlayMessage(QStringLiteral("车辆规划控制可视化\n当前使用内部标准模型与 Mock 数据"));
    }
    redraw();
}

void SceneManager::clearScene()
{
    if (m_scene != nullptr) {
        m_scene->clear();
    }
}

void SceneManager::updateScene(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    m_snapshot = snapshot;
    redraw();
}

void SceneManager::setLayerVisibility(const LayerVisibility& visibility)
{
    m_layerVisibility = visibility;
    redraw();
}

LayerVisibility SceneManager::layerVisibility() const
{
    return m_layerVisibility;
}

void SceneManager::setVehicleCenteredMode(bool enabled)
{
    m_vehicleCenteredMode = enabled;
    redraw();
}

void SceneManager::redraw()
{
    if (m_scene == nullptr) {
        return;
    }

    m_scene->clear();

    auto* centerMarker = m_scene->addEllipse(-0.25, -0.25, 0.5, 0.5, QPen(QColor("#ffd166"), 0.0));
    centerMarker->setBrush(QBrush(QColor(255, 209, 102, 140)));

    if (m_layerVisibility.showReferenceLine) {
        drawReferenceLine(m_snapshot.referenceLine);
    }
    if (m_layerVisibility.showGlobalPath) {
        drawTrajectory(m_snapshot.globalPath, QColor("#59c3c3"), 0.18);
    }
    if (m_layerVisibility.showLocalPath) {
        drawTrajectory(m_snapshot.localPath, QColor("#ff7f50"), 0.24);
    }
    if (m_layerVisibility.showObstacles) {
        drawObstacles(m_snapshot.obstacles);
    }
    if (m_layerVisibility.showVehicle) {
        drawVehicle(m_snapshot);
    }

    autoFitAndCenter();
}

void SceneManager::drawVehicle(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const auto& vehicle = snapshot.vehicleState;
    const QPointF center = toScenePoint(vehicle.location.position);
    const double x = center.x();
    const double y = center.y();

    auto* body = m_scene->addRect(x - vehicle.vehicleLength * 0.5,
                                  y - vehicle.vehicleWidth * 0.5,
                                  vehicle.vehicleLength,
                                  vehicle.vehicleWidth,
                                  QPen(QColor("#f7b267"), 0.0),
                                  QBrush(QColor(247, 178, 103, 110)));
    body->setTransformOriginPoint(x, y);
    body->setRotation(kVehicleHeadingOffsetDeg - vehicle.location.heading * kRadToDeg);

    QPolygonF nose;
    nose << QPointF(x + vehicle.vehicleLength * 0.35, y)
         << QPointF(x + vehicle.vehicleLength * 0.1, y - 0.55)
         << QPointF(x + vehicle.vehicleLength * 0.1, y + 0.55);
    auto* headingMarker = m_scene->addPolygon(nose, QPen(QColor("#ffd166"), 0.0), QBrush(QColor("#ffd166")));
    headingMarker->setTransformOriginPoint(x, y);
    headingMarker->setRotation(kVehicleHeadingOffsetDeg - vehicle.location.heading * kRadToDeg);
}

void SceneManager::drawTrajectory(const autoviz::model::Trajectory& trajectory, const QColor& color, qreal width)
{
    const QPainterPath path = buildTrajectoryPath(trajectory);
    if (path.isEmpty()) {
        return;
    }

    m_scene->addPath(path, QPen(color, width));
}

void SceneManager::drawReferenceLine(const autoviz::model::ReferenceLine& referenceLine)
{
    const QPainterPath path = buildReferencePath(referenceLine);
    if (path.isEmpty()) {
        return;
    }

    QPen pen(QColor("#9ad1d4"), 0.12, Qt::DashLine);
    pen.setDashPattern({4.0, 3.0});
    m_scene->addPath(path, pen);
}

void SceneManager::drawObstacles(const autoviz::model::ObstacleList& obstacles)
{
    for (const auto& obstacle : obstacles) {
        if (obstacle.shape == autoviz::model::ObstacleShapeType::Polygon && !obstacle.polygon.vertices.isEmpty()) {
            QPolygonF polygon;
            for (const auto& point : obstacle.polygon.vertices) {
                polygon << QPointF(point.x, -point.y);
            }
            m_scene->addPolygon(polygon, QPen(QColor("#ef476f"), 0.0), QBrush(QColor(239, 71, 111, 120)));
            continue;
        }

        const QPointF center = toScenePoint(obstacle.position.position);
        const double x = center.x();
        const double y = center.y();
        auto* rect = m_scene->addRect(x - obstacle.length * 0.5,
                                      y - obstacle.width * 0.5,
                                      obstacle.length,
                                      obstacle.width,
                                      QPen(QColor("#ef476f"), 0.0),
                                      QBrush(QColor(239, 71, 111, 90)));
        rect->setTransformOriginPoint(x, y);
        rect->setRotation(kVehicleHeadingOffsetDeg - obstacle.position.theta * kRadToDeg);
    }
}

void SceneManager::autoFitAndCenter()
{
    if (m_view == nullptr || m_scene == nullptr) {
        return;
    }

    const QPointF vehicleCenter = toScenePoint(m_snapshot.vehicleState.location.position);
    QRectF targetRegion;

    if (m_vehicleCenteredMode) {
        // 跟车视角优先显示车辆附近区域，不按整条路径做全局缩放。
        targetRegion = QRectF(vehicleCenter.x() - 18.0, vehicleCenter.y() - 12.0, 36.0, 24.0);
    } else {
        targetRegion = m_scene->itemsBoundingRect();
        const QRectF minimumVehicleRegion(vehicleCenter.x() - 8.0, vehicleCenter.y() - 6.0, 16.0, 12.0);
        targetRegion = targetRegion.united(minimumVehicleRegion);
        targetRegion.adjust(-4.0, -4.0, 4.0, 4.0);
    }

    m_view->fitToRegion(targetRegion);
}

QPointF SceneManager::toScenePoint(const autoviz::model::Point2D& point) const
{
    return QPointF(point.x, -point.y);
}

}  // namespace autoviz::render
