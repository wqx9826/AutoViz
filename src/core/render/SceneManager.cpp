#include "core/render/SceneManager.h"

#include <algorithm>
#include <cmath>

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QFont>
#include <QLineF>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>

#include "ui/VisualizationView.h"

namespace autoviz::render {

namespace {
constexpr double kRadToDeg = 57.29577951308232;

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

QString toDisplayString(MainViewMode mode)
{
    switch (mode) {
    case MainViewMode::Auto:
        return QStringLiteral("自动");
    case MainViewMode::TopDownXY:
        return QStringLiteral("俯视 XY");
    case MainViewMode::VerticalProfile:
        return QStringLiteral("垂向剖面 X-Z");
    }
    return QStringLiteral("未知");
}

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

bool SceneManager::vehicleCenteredMode() const
{
    return m_vehicleCenteredMode;
}

void SceneManager::setMainViewMode(MainViewMode mode)
{
    if (mode == MainViewMode::Auto) {
        mode = MainViewMode::TopDownXY;
    }
    if (m_mainViewMode == mode) {
        return;
    }
    m_mainViewMode = mode;
    if (m_view != nullptr) {
        m_view->resetView();
    }
    redraw();
}

MainViewMode SceneManager::mainViewMode() const
{
    return m_mainViewMode;
}

void SceneManager::redraw()
{
    if (m_scene == nullptr) {
        return;
    }

    m_scene->clear();
    if (m_mainViewMode == MainViewMode::VerticalProfile) {
        redrawVerticalProfile();
        return;
    }
    redrawTopDownXY();
}

void SceneManager::redrawTopDownXY()
{
    auto* centerMarker = m_scene->addEllipse(-0.25, -0.25, 0.5, 0.5, QPen(QColor("#ffd166"), 0.0));
    centerMarker->setBrush(QBrush(QColor(255, 209, 102, 140)));

    const auto& status = m_snapshot.runtimeStatus;
    if (m_layerVisibility.showReferenceLine && status.hasReferenceLineData) {
        drawReferenceLine(m_snapshot.referenceLine);
    }
    if (m_layerVisibility.showHistoryTrail && !m_snapshot.historyTrail.points.isEmpty()) {
        drawHistoryTrail(m_snapshot.historyTrail);
    }
    if (m_layerVisibility.showGlobalPath && status.hasGlobalPathData) {
        drawTrajectory(m_snapshot.globalPath, QColor("#59c3c3"), 0.18);
        drawPathEndpoint(m_snapshot.pathEndpointStatus);
    }
    if (m_layerVisibility.showLocalPath && status.hasLocalPathData) {
        drawTrajectory(m_snapshot.localPath, QColor("#ff7f50"), 0.24);
    }
    if (m_layerVisibility.showObstacles && status.hasObstacleData) {
        drawObstacles(m_snapshot.obstacles);
    }
    if (m_layerVisibility.showVehicle && status.hasVehicleLocationData) {
        drawVehicle(m_snapshot);
    }

    autoFitAndCenter();
}

void SceneManager::redrawVerticalProfile()
{
    const auto& loc = m_snapshot.localizationStatus;
    const auto& action = m_snapshot.actionRuntimeStatus;
    const auto& chassis = m_snapshot.chassisRuntimeStatus;

    const double currentX = loc.valid ? loc.odomX : 0.0;
    const double currentDepth = loc.valid ? loc.depth : 0.0;
    const double targetDepth = action.valid ? action.targetDepth : currentDepth;
    const double maxDepth = std::max({1.0, currentDepth, targetDepth, currentDepth + std::max(0.0, loc.height)}) + 2.0;
    const double leftX = currentX - 10.0;
    const double rightX = currentX + 10.0;

    QPen axisPen(QColor("#8F8F94"), 0.0);
    QPen gridPen(QColor(77, 88, 104, 120), 0.0, Qt::DashLine);
    for (int depth = 0; depth <= static_cast<int>(std::ceil(maxDepth)); depth += 1) {
        const double y = static_cast<double>(depth);
        m_scene->addLine(QLineF(leftX, y, rightX, y), depth % 5 == 0 ? axisPen : gridPen);
    }

    m_scene->addLine(QLineF(leftX, 0.0, rightX, 0.0), QPen(QColor("#4CC3FF"), 0.0));
    m_scene->addLine(QLineF(currentX, 0.0, currentX, maxDepth), axisPen);

    auto* depthAxis = m_scene->addText(QStringLiteral("Depth +"));
    depthAxis->setDefaultTextColor(QColor("#8F8F94"));
    depthAxis->setFont(QFont(QStringLiteral("Sans Serif"), 1));
    depthAxis->setPos(currentX + 0.4, maxDepth - 0.9);

    if (action.valid) {
        QPen targetPen(QColor("#FFB45C"), 0.0, Qt::DashLine);
        targetPen.setDashPattern({4.0, 2.0});
        m_scene->addLine(QLineF(leftX, targetDepth, rightX, targetDepth), targetPen);
        auto* targetLabel = m_scene->addText(QStringLiteral("target depth %1 m").arg(QString::number(targetDepth, 'f', 2)));
        targetLabel->setDefaultTextColor(QColor("#FFB45C"));
        targetLabel->setFont(QFont(QStringLiteral("Sans Serif"), 1));
        targetLabel->setPos(leftX + 0.4, targetDepth - 0.9);
    }

    if (loc.valid) {
        auto* robot = m_scene->addEllipse(currentX - 0.35,
                                          currentDepth - 0.35,
                                          0.7,
                                          0.7,
                                          QPen(QColor("#6EF2A0"), 0.0),
                                          QBrush(QColor(110, 242, 160, 180)));
        robot->setZValue(10.0);

        auto* label = m_scene->addText(QStringLiteral("current depth %1 m").arg(QString::number(currentDepth, 'f', 2)));
        label->setDefaultTextColor(QColor("#6EF2A0"));
        label->setFont(QFont(QStringLiteral("Sans Serif"), 1));
        label->setPos(currentX + 0.6, currentDepth - 0.8);
    }

    const QString statusText =
        QStringLiteral("垂向剖面 X-Z\n"
                       "运行模式: %1\n"
                       "current height: %2 m\n"
                       "target height: %3 m\n"
                       "buoyancy_adjust: %4\n"
                       "water tank level: %5\n"
                       "water tank state: %6\n"
                       "垂向历史轨迹: 当前模型未保存 depth 历史")
            .arg(autoviz::model::toDisplayString(m_snapshot.runVisualizationMode))
            .arg(loc.valid ? QString::number(loc.height, 'f', 2) : QStringLiteral("--"))
            .arg(action.valid ? QString::number(action.targetHeight, 'f', 2) : QStringLiteral("--"))
            .arg(action.valid ? QString::number(action.buoyancyAdjust) : QStringLiteral("--"))
            .arg(chassis.valid ? QString::number(chassis.waterTankLevelStatus) : QStringLiteral("--"))
            .arg(chassis.valid ? QString::number(chassis.waterTankStatus) : QStringLiteral("--"));

    auto* status = m_scene->addText(statusText);
    status->setDefaultTextColor(QColor("#E3E3E6"));
    status->setFont(QFont(QStringLiteral("Sans Serif"), 1));
    status->setPos(leftX, -2.5);
    status->setZValue(20.0);

    QRectF targetRegion(leftX - 1.5, -3.0, rightX - leftX + 3.0, maxDepth + 5.0);
    m_view->fitToRegion(targetRegion);
}

void SceneManager::drawVehicle(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const auto& vehicleLocation = snapshot.vehicleLocation;
    const auto& vehicleConfig = snapshot.vehicleConfig;
    const QPointF center = toScenePoint(vehicleLocation.position);
    const double x = center.x();
    const double y = center.y();

    auto* body = m_scene->addRect(x - vehicleConfig.vehicleLength * 0.5,
                                  y - vehicleConfig.vehicleWidth * 0.5,
                                  vehicleConfig.vehicleLength,
                                  vehicleConfig.vehicleWidth,
                                  QPen(QColor("#f7b267"), 0.0),
                                  QBrush(QColor(247, 178, 103, 110)));
    body->setTransformOriginPoint(x, y);
    body->setRotation(-vehicleLocation.heading * kRadToDeg);

    QPolygonF nose;
    nose << QPointF(x + vehicleConfig.vehicleLength * 0.35, y)
         << QPointF(x + vehicleConfig.vehicleLength * 0.1, y - 0.55)
         << QPointF(x + vehicleConfig.vehicleLength * 0.1, y + 0.55);
    auto* headingMarker = m_scene->addPolygon(nose, QPen(QColor("#ffd166"), 0.0), QBrush(QColor("#ffd166")));
    headingMarker->setTransformOriginPoint(x, y);
    headingMarker->setRotation(-vehicleLocation.heading * kRadToDeg);
}

void SceneManager::drawHistoryTrail(const autoviz::model::Trajectory& trajectory)
{
    const QPainterPath path = buildTrajectoryPath(trajectory);
    if (path.isEmpty()) {
        return;
    }

    QPen pen(QColor(156, 163, 175, 150), 0.12);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    m_scene->addPath(path, pen);
}

void SceneManager::drawTrajectory(const autoviz::model::Trajectory& trajectory, const QColor& color, qreal width)
{
    const QPainterPath path = buildTrajectoryPath(trajectory);
    if (path.isEmpty()) {
        return;
    }

    m_scene->addPath(path, QPen(color, width));
}

void SceneManager::drawPathEndpoint(const autoviz::model::PathEndpointStatus& endpoint)
{
    if (!endpoint.valid) {
        return;
    }

    const QPointF center(endpoint.x, -endpoint.y);
    auto* marker = m_scene->addEllipse(center.x() - 0.45,
                                       center.y() - 0.45,
                                       0.9,
                                       0.9,
                                       QPen(QColor("#22c55e"), 0.0),
                                       QBrush(QColor(34, 197, 94, 150)));
    marker->setZValue(10.0);

    auto* label = m_scene->addText(QStringLiteral("路径终点\n非 action goal\n非任务目标点"));
    label->setDefaultTextColor(QColor("#bbf7d0"));
    label->setFont(QFont(QStringLiteral("Sans Serif"), 1));
    label->setPos(center.x() + 0.7, center.y() - 0.7);
    label->setZValue(10.0);
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
        rect->setRotation(-obstacle.position.theta * kRadToDeg);
        rect->setToolTip(QStringLiteral("%1\nid=%2 class=%3")
                             .arg(obstacle.sourceTopic.isEmpty() ? QStringLiteral("obstacle") : obstacle.sourceTopic)
                             .arg(obstacle.id)
                             .arg(obstacle.classLabel.isEmpty() ? QString::number(obstacle.sourceClass) : obstacle.classLabel));

        if (!obstacle.classLabel.isEmpty() || obstacle.sourceClass != 0) {
            auto* label = m_scene->addText(obstacle.classLabel.isEmpty()
                                               ? QStringLiteral("class %1").arg(obstacle.sourceClass)
                                               : obstacle.classLabel);
            label->setDefaultTextColor(QColor("#fecdd3"));
            label->setFont(QFont(QStringLiteral("Sans Serif"), 1));
            label->setPos(x + obstacle.length * 0.5 + 0.2, y - obstacle.width * 0.5);
            label->setZValue(9.0);
        }
    }
}

void SceneManager::autoFitAndCenter()
{
    if (m_view == nullptr || m_scene == nullptr) {
        return;
    }

    const QPointF vehicleCenter = toScenePoint(m_snapshot.vehicleLocation.position);
    QRectF targetRegion;

    const bool canCenterOnVehicle = m_vehicleCenteredMode && m_snapshot.runtimeStatus.hasVehicleLocationData;
    if (canCenterOnVehicle) {
        // 跟车视角优先显示车辆附近区域，不按整条路径做全局缩放。
        targetRegion = QRectF(vehicleCenter.x() - 18.0, vehicleCenter.y() - 12.0, 36.0, 24.0);
    } else {
        targetRegion = m_scene->itemsBoundingRect();
        if (m_snapshot.runtimeStatus.hasVehicleLocationData) {
            const QRectF minimumVehicleRegion(vehicleCenter.x() - 8.0, vehicleCenter.y() - 6.0, 16.0, 12.0);
            targetRegion = targetRegion.united(minimumVehicleRegion);
        }
        targetRegion.adjust(-4.0, -4.0, 4.0, 4.0);
    }

    m_view->fitToRegion(targetRegion);
}

QPointF SceneManager::toScenePoint(const autoviz::model::Point2D& point) const
{
    return QPointF(point.x, -point.y);
}

}  // namespace autoviz::render
