#include "core/render/SceneManager.h"

#include <algorithm>
#include <cmath>

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QFont>
#include <QDateTime>
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
        return QStringLiteral("垂向剖面");
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
    updateVerticalMotionSegment(m_snapshot);
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
    if (m_view != nullptr) {
        m_view->clearVerticalProfileFrame();
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

bool SceneManager::isVerticalMode(autoviz::model::RunVisualizationMode mode) const
{
    return mode == autoviz::model::RunVisualizationMode::VerticalMotion
           || mode == autoviz::model::RunVisualizationMode::BuoyancyAdjust;
}

bool SceneManager::shouldStartNewVerticalSegment(const autoviz::datacenter::VisualizationSnapshot& snapshot) const
{
    if (!isVerticalMode(snapshot.runVisualizationMode)) {
        return false;
    }
    if (!m_verticalSegment.active || m_verticalSegment.frozen) {
        return true;
    }
    if (!isVerticalMode(m_previousRunMode)) {
        return true;
    }

    const auto& task = snapshot.taskRuntimeStatus;
    if (task.valid) {
        if (task.taskType != m_verticalSegment.taskType || task.taskId != m_verticalSegment.taskId) {
            return true;
        }
    }

    const auto& action = snapshot.actionRuntimeStatus;
    if (action.valid && m_verticalSegment.chassisMode != 0 &&
        action.chassisMode != 0 && action.chassisMode != m_verticalSegment.chassisMode) {
        return true;
    }

    return false;
}

void SceneManager::startVerticalMotionSegment(const autoviz::datacenter::VisualizationSnapshot& snapshot, qint64 nowMs)
{
    const auto& loc = snapshot.localizationStatus;
    const auto& action = snapshot.actionRuntimeStatus;
    const auto& task = snapshot.taskRuntimeStatus;

    m_verticalSegment = VerticalMotionSegment{};
    m_verticalSegment.active = true;
    m_verticalSegment.frozen = false;
    m_verticalSegment.emergencyStop = task.valid && task.emergencyStop;
    m_verticalSegment.startTimestampMs = loc.valid && loc.timestampMs > 0 ? loc.timestampMs : nowMs;
    m_verticalSegment.lastVerticalTimestampMs = nowMs;
    m_verticalSegment.startDepth = loc.depth;
    m_verticalSegment.hasStartDepth = loc.valid;
    m_verticalSegment.startHeight = loc.height;
    m_verticalSegment.hasStartHeight = loc.valid;
    m_verticalSegment.startX = loc.valid ? loc.odomX : 0.0;
    m_verticalSegment.startY = loc.valid ? loc.odomY : 0.0;
    m_verticalSegment.startYaw = loc.valid ? loc.heading : 0.0;
    m_verticalSegment.targetDepth = action.targetDepth;
    m_verticalSegment.hasTargetDepth = action.valid;
    m_verticalSegment.targetHeight = action.targetHeight;
    m_verticalSegment.hasTargetHeight = action.valid;
    m_verticalSegment.taskType = task.valid ? task.taskType : 0;
    m_verticalSegment.taskId = task.valid ? task.taskId : 0;
    m_verticalSegment.chassisMode = action.valid ? action.chassisMode : 0;
}

void SceneManager::appendVerticalMotionSample(const autoviz::datacenter::VisualizationSnapshot& snapshot, qint64 nowMs)
{
    if (!m_verticalSegment.active || m_verticalSegment.startTimestampMs <= 0) {
        return;
    }

    const auto& loc = snapshot.localizationStatus;
    const auto& action = snapshot.actionRuntimeStatus;
    const auto& task = snapshot.taskRuntimeStatus;

    m_verticalSegment.emergencyStop = m_verticalSegment.emergencyStop || (task.valid && task.emergencyStop);
    m_verticalSegment.targetDepth = action.targetDepth;
    m_verticalSegment.hasTargetDepth = action.valid;
    m_verticalSegment.targetHeight = action.targetHeight;
    m_verticalSegment.hasTargetHeight = action.valid;
    m_verticalSegment.chassisMode = action.valid ? action.chassisMode : m_verticalSegment.chassisMode;

    const double elapsedSec = std::max(0.0, static_cast<double>(nowMs - m_verticalSegment.startTimestampMs) / 1000.0);
    bool emergencyChanged = false;
    if (!m_verticalSegment.samples.isEmpty()) {
        const auto& last = m_verticalSegment.samples.constLast();
        const bool targetChanged = action.valid && (!last.hasTargetDepth || std::abs(action.targetDepth - last.targetDepth) > 1.0e-6);
        emergencyChanged = task.valid && task.emergencyStop && !last.emergencyStop;
        if (elapsedSec - last.elapsedSec < 0.10 && !targetChanged && !emergencyChanged) {
            return;
        }
    }

    if (emergencyChanged || (task.valid && task.emergencyStop && m_verticalSegment.emergencyEventTimes.isEmpty())) {
        m_verticalSegment.emergencyEventTimes.push_back(elapsedSec);
    }

    VerticalMotionSegmentSample sample;
    sample.elapsedSec = elapsedSec;
    sample.depth = loc.depth;
    sample.hasDepth = loc.valid;
    sample.targetDepth = action.targetDepth;
    sample.hasTargetDepth = action.valid;
    sample.height = loc.height;
    sample.hasHeight = loc.valid;
    sample.targetHeight = action.targetHeight;
    sample.hasTargetHeight = action.valid;
    sample.depthError = sample.hasDepth && sample.hasTargetDepth ? sample.depth - sample.targetDepth : 0.0;
    sample.emergencyStop = task.valid && task.emergencyStop;
    m_verticalSegment.samples.push_back(sample);

    constexpr int kMaxVerticalSegmentSamples = 2400;
    while (m_verticalSegment.samples.size() > kMaxVerticalSegmentSamples) {
        m_verticalSegment.samples.pop_front();
    }
}

void SceneManager::freezeVerticalMotionSegment()
{
    if (!m_verticalSegment.active) {
        return;
    }
    m_verticalSegment.active = false;
    m_verticalSegment.frozen = true;
}

void SceneManager::updateVerticalMotionSegment(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool verticalMode = isVerticalMode(snapshot.runVisualizationMode);
    const bool emergencyStop = snapshot.runVisualizationMode == autoviz::model::RunVisualizationMode::EmergencyStop
                               || (snapshot.taskRuntimeStatus.valid && snapshot.taskRuntimeStatus.emergencyStop);

    if (shouldStartNewVerticalSegment(snapshot)) {
        startVerticalMotionSegment(snapshot, nowMs);
    }

    if (m_verticalSegment.active && (verticalMode || emergencyStop)) {
        if (verticalMode) {
            m_verticalSegment.lastVerticalTimestampMs = nowMs;
            m_verticalSegment.leftVerticalSinceMs = 0;
        }
        appendVerticalMotionSample(snapshot, nowMs);
    }

    if (m_verticalSegment.active) {
        if (m_verticalSegment.emergencyStop && snapshot.runVisualizationMode == autoviz::model::RunVisualizationMode::Idle) {
            freezeVerticalMotionSegment();
        } else if (!verticalMode && !emergencyStop) {
            if (snapshot.runVisualizationMode == autoviz::model::RunVisualizationMode::HorizontalMotion ||
                snapshot.runVisualizationMode == autoviz::model::RunVisualizationMode::Idle) {
                freezeVerticalMotionSegment();
            } else {
                if (m_verticalSegment.leftVerticalSinceMs == 0) {
                    m_verticalSegment.leftVerticalSinceMs = nowMs;
                }
                if (nowMs - m_verticalSegment.leftVerticalSinceMs > 1000) {
                    freezeVerticalMotionSegment();
                }
            }
        }

        const auto& loc = snapshot.localizationStatus;
        if (loc.valid && loc.timestampMs > 0 && nowMs - loc.timestampMs > 3000) {
            freezeVerticalMotionSegment();
        }
    }

    m_previousRunMode = snapshot.runVisualizationMode;
}

void SceneManager::redrawVerticalProfile()
{
    if (m_view == nullptr) {
        return;
    }

    VisualizationView::VerticalProfileFrame frame;
    frame.visible = true;
    frame.frozen = m_verticalSegment.frozen;
    frame.emergencyStop = m_verticalSegment.emergencyStop;

    QString modeText = autoviz::model::toDisplayString(m_snapshot.runVisualizationMode);
    if (m_verticalSegment.frozen) {
        modeText = m_verticalSegment.emergencyStop ? QStringLiteral("急停后已冻结") : QStringLiteral("已冻结 / 上次垂向动作");
    } else if (m_verticalSegment.emergencyStop) {
        modeText = QStringLiteral("急停");
    }
    frame.modeText = modeText;
    frame.startDepth = m_verticalSegment.startDepth;
    frame.hasStartDepth = m_verticalSegment.hasStartDepth;
    frame.emergencyEventTimes = m_verticalSegment.emergencyEventTimes;

    const VerticalMotionSegmentSample* latestDepthSample = nullptr;
    const VerticalMotionSegmentSample* latestTargetSample = nullptr;
    for (const auto& sample : m_verticalSegment.samples) {
        VisualizationView::VerticalProfileSample viewSample;
        viewSample.elapsedSec = sample.elapsedSec;
        viewSample.depth = sample.depth;
        viewSample.hasDepth = sample.hasDepth;
        viewSample.targetDepth = sample.targetDepth;
        viewSample.hasTargetDepth = sample.hasTargetDepth;
        viewSample.emergencyStop = sample.emergencyStop;
        frame.samples.push_back(viewSample);
    }
    for (int index = m_verticalSegment.samples.size() - 1; index >= 0; --index) {
        const auto& sample = m_verticalSegment.samples.at(index);
        if (latestDepthSample == nullptr && sample.hasDepth) {
            latestDepthSample = &sample;
        }
        if (latestTargetSample == nullptr && sample.hasTargetDepth) {
            latestTargetSample = &sample;
        }
        if (latestDepthSample != nullptr && latestTargetSample != nullptr) {
            break;
        }
    }
    if (latestDepthSample != nullptr) {
        frame.elapsedSec = latestDepthSample->elapsedSec;
        frame.currentDepth = latestDepthSample->depth;
        frame.hasCurrentDepth = true;
    } else if (!m_verticalSegment.samples.isEmpty()) {
        frame.elapsedSec = m_verticalSegment.samples.constLast().elapsedSec;
    }
    if (latestTargetSample != nullptr) {
        frame.targetDepth = latestTargetSample->targetDepth;
        frame.hasTargetDepth = true;
    }

    m_view->setVerticalProfileFrame(frame);
    return;
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
