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

QPen makeAdaptivePathPen(const VisualizationView* view,
                         const QColor& color,
                         qreal physicalWidth,
                         qreal minimumPixels,
                         qreal maximumPixels,
                         Qt::PenStyle style = Qt::SolidLine)
{
    qreal sceneWidth = physicalWidth;
    if (view != nullptr) {
        const qreal pixelsPerMeter = std::abs(view->transform().m11());
        if (pixelsPerMeter > 1.0e-6) {
            const qreal screenWidth = std::clamp(physicalWidth * pixelsPerMeter,
                                                 minimumPixels,
                                                 maximumPixels);
            sceneWidth = screenWidth / pixelsPerMeter;
        }
    }

    QPen pen(color, sceneWidth, style);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

QPainterPath buildTrajectoryPath(const autoviz::model::Trajectory& trajectory)
{
    QPainterPath path;
    if (trajectory.points.isEmpty()) {
        return path;
    }

    // ROS bag 异常或 adapter 回归不应让一条路径占满 UI 线程。渲染只需保持路径形状，
    // 所以对极长轨迹等间隔抽样，同时保留最后一个点；状态和终点仍使用完整模型数据。
    constexpr int kMaxRenderedSegments = 8000;
    const int pointCount = trajectory.points.size();
    const int stride = qMax(1, (pointCount - 1 + kMaxRenderedSegments - 1)
                               / kMaxRenderedSegments);
    path.moveTo(trajectory.points.first().position.x, -trajectory.points.first().position.y);
    for (int index = stride; index < pointCount; index += stride) {
        const auto& point = trajectory.points.at(index);
        path.lineTo(point.position.x, -point.position.y);
    }
    if ((pointCount - 1) % stride != 0) {
        const auto& point = trajectory.points.constLast();
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

void SceneManager::resetPlaybackSession()
{
    m_snapshot = autoviz::datacenter::VisualizationSnapshot{};
    m_verticalSegment = VerticalMotionSegment{};
    m_previousRunMode = autoviz::model::RunVisualizationMode::Unknown;
    m_visibleContentBounds = QRectF{};
    m_lastAutoFitTargetRegion = QRectF{};
    m_hasVisibleContentBounds = false;
    m_hasAutoFitTargetRegion = false;
    if (m_view != nullptr) {
        m_view->clearVerticalProfileFrame();
    }
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
    if (m_vehicleCenteredMode == enabled) {
        return;
    }
    m_vehicleCenteredMode = enabled;
    m_hasAutoFitTargetRegion = false;
    redraw();
}

bool SceneManager::vehicleCenteredMode() const
{
    return m_vehicleCenteredMode;
}

void SceneManager::refitVisibleData()
{
    if (m_view == nullptr) {
        return;
    }

    m_view->resetView();
    m_hasAutoFitTargetRegion = false;
    if (m_mainViewMode == MainViewMode::TopDownXY) {
        autoFitAndCenter();
    }
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
    m_visibleContentBounds = QRectF();
    m_hasVisibleContentBounds = false;

    // 原点只作为坐标参照，不能参与自动取景；否则远距离车辆会被原点拉得过小。
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
    return mode == autoviz::model::RunVisualizationMode::VerticalMotion;
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
    if (action.valid && !m_verticalSegment.goalId.isEmpty()
        && !action.goalUuid.isEmpty() && action.goalUuid != m_verticalSegment.goalId) {
        return true;
    }
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
    // 关键修复：横轴 elapsed 的"增长基准"改用 Client 侧本轮 nowMs 锚定，不再
    // 依赖 Server 下发的 action.timestampMs 做差值。Server 与 Client 主机存在
    // 数十秒级时钟偏移 / 任务已运行多时再接入等情况下，原做法会导致
    // nowMs - startTimestampMs 被 clamp 到 0 并持续数秒，数值深度已变化而曲线
    // 停在 t=0 不动。保留 startTimestampMs 仅用于推断 preElapsedSec，不参与
    // 后续每帧的 elapsedSec 计算。
    m_verticalSegment.localTickAnchorMs = nowMs;
    m_verticalSegment.startTimestampMs = action.valid && action.timestampMs > 0
                                            ? action.timestampMs : nowMs;
    if (action.valid && action.timestampMs > 0) {
        const double pre = static_cast<double>(nowMs - action.timestampMs) / 1000.0;
        m_verticalSegment.preElapsedSec = std::max(0.0, pre);
    } else {
        m_verticalSegment.preElapsedSec = 0.0;
    }
    m_verticalSegment.lastVerticalTimestampMs = nowMs;
    m_verticalSegment.startDepth = loc.depth;
    m_verticalSegment.hasStartDepth = loc.valid;
    m_verticalSegment.startHeight = loc.height;
    m_verticalSegment.hasStartHeight = loc.valid;
    m_verticalSegment.startX = loc.valid ? loc.odomX : 0.0;
    m_verticalSegment.startY = loc.valid ? loc.odomY : 0.0;
    m_verticalSegment.startYaw = loc.valid ? loc.heading : 0.0;
    // 目标线优先取 ChassisCommand 下发的实时期望深度/高度（运动中会变化），
    // 回退到 SystemRunStates 的任务级目标（通常静态）。避免出现"反馈值一直在变、
    // 目标值不动"的错觉——真正在跟踪的实时命令目标才是曲线应该画的"目标线"。
    const auto& cmd = snapshot.controlCmd;
    if (cmd.hasTargetDepth) {
        m_verticalSegment.targetDepth = cmd.targetDepth;
        m_verticalSegment.hasTargetDepth = true;
    } else {
        m_verticalSegment.targetDepth = action.targetDepth;
        m_verticalSegment.hasTargetDepth = action.valid;
    }
    if (cmd.hasTargetHeight) {
        m_verticalSegment.targetHeight = cmd.targetHeight;
        m_verticalSegment.hasTargetHeight = true;
    } else {
        m_verticalSegment.targetHeight = action.targetHeight;
        m_verticalSegment.hasTargetHeight = action.valid;
    }
    m_verticalSegment.taskType = task.valid ? task.taskType : 0;
    m_verticalSegment.taskId = task.valid ? task.taskId : 0;
    m_verticalSegment.chassisMode = action.valid ? action.chassisMode : 0;
    m_verticalSegment.goalId = action.valid ? action.goalUuid : QString{};
}

void SceneManager::appendVerticalMotionSample(const autoviz::datacenter::VisualizationSnapshot& snapshot, qint64 nowMs)
{
    if (!m_verticalSegment.active || m_verticalSegment.localTickAnchorMs <= 0) {
        return;
    }

    const auto& loc = snapshot.localizationStatus;
    const auto& action = snapshot.actionRuntimeStatus;
    const auto& task = snapshot.taskRuntimeStatus;

    m_verticalSegment.emergencyStop = m_verticalSegment.emergencyStop || (task.valid && task.emergencyStop);
    // 同 startVerticalMotionSegment：目标线优先取 controlCmd 的实时命令目标。
    const auto& cmd = snapshot.controlCmd;
    if (cmd.hasTargetDepth) {
        m_verticalSegment.targetDepth = cmd.targetDepth;
        m_verticalSegment.hasTargetDepth = true;
    } else {
        m_verticalSegment.targetDepth = action.targetDepth;
        m_verticalSegment.hasTargetDepth = action.valid;
    }
    if (cmd.hasTargetHeight) {
        m_verticalSegment.targetHeight = cmd.targetHeight;
        m_verticalSegment.hasTargetHeight = true;
    } else {
        m_verticalSegment.targetHeight = action.targetHeight;
        m_verticalSegment.hasTargetHeight = action.valid;
    }
    m_verticalSegment.chassisMode = action.valid ? action.chassisMode : m_verticalSegment.chassisMode;

    if (m_verticalSegment.localTickAnchorMs <= 0) {
        // 极端兜底：段启动时锚点未正确写入，在此补设以避免永远算不出 elapsed。
        m_verticalSegment.localTickAnchorMs = nowMs;
    }
    const double tickElapsedSec = std::max(
        0.0, static_cast<double>(nowMs - m_verticalSegment.localTickAnchorMs) / 1000.0);
    const double elapsedSec = m_verticalSegment.preElapsedSec + tickElapsedSec;
    bool emergencyChanged = false;
    if (!m_verticalSegment.samples.isEmpty()) {
        const auto& last = m_verticalSegment.samples.constLast();
        // 用已更新的 m_verticalSegment 目标值判断变化，覆盖 controlCmd 与 action
        // 两种来源；只要当前目标与上一个采样点的目标有差异就强制采样。
        const bool targetChanged =
            (m_verticalSegment.hasTargetDepth
             && (!last.hasTargetDepth || std::abs(m_verticalSegment.targetDepth - last.targetDepth) > 1.0e-6))
            || (m_verticalSegment.hasTargetHeight
                && (!last.hasTargetHeight || std::abs(m_verticalSegment.targetHeight - last.targetHeight) > 1.0e-6));
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
    sample.targetDepth = m_verticalSegment.targetDepth;
    sample.hasTargetDepth = m_verticalSegment.hasTargetDepth;
    sample.height = loc.height;
    sample.hasHeight = loc.valid;
    sample.targetHeight = m_verticalSegment.targetHeight;
    sample.hasTargetHeight = m_verticalSegment.hasTargetHeight;
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
    // sourceTimeMs is the authoritative clock for both Ros2Bag (virtual playback) and Remote
    // (server wall clock).  Using sourceTimeMs keeps startTimestampMs (from action.timestampMs,
    // also server-derived) and nowMs in the same reference frame, eliminating clock-skew
    // induced stalls where elapsedSec was clamped to 0 for the first several seconds.
    // Mock source falls back to local wall clock because sourceTimeMs is not populated there.
    const qint64 nowMs = snapshot.runtimeStatus.sourceTimeMs > 0
                          ? snapshot.runtimeStatus.sourceTimeMs
                          : QDateTime::currentMSecsSinceEpoch();
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
        // 视图切换、Action 聚合 topic 的健康超时及定位短暂抖动都不能终止一个
        // 已开始的分段。只有公开聚合状态明确转为非执行态（或切入下一 Action）才冻结。
        if (!verticalMode && !emergencyStop) {
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
    const bool useHeight = m_snapshot.actionRuntimeStatus.verticalControlMode
                           == autoviz::model::VerticalControlMode::HeightHold;
    frame.quantityText = useHeight ? QStringLiteral("离底高度") : QStringLiteral("深度");

    QString modeText = autoviz::model::toDisplayString(m_snapshot.runVisualizationMode);
    if (m_verticalSegment.frozen) {
        modeText = m_verticalSegment.emergencyStop ? QStringLiteral("急停后已冻结") : QStringLiteral("已冻结 / 上次垂向动作");
    } else if (m_verticalSegment.emergencyStop) {
        modeText = QStringLiteral("急停");
    }
    frame.modeText = modeText;
    frame.startDepth = useHeight ? m_verticalSegment.startHeight : m_verticalSegment.startDepth;
    frame.hasStartDepth = useHeight ? m_verticalSegment.hasStartHeight : m_verticalSegment.hasStartDepth;
    frame.emergencyEventTimes = m_verticalSegment.emergencyEventTimes;

    const VerticalMotionSegmentSample* latestDepthSample = nullptr;
    const VerticalMotionSegmentSample* latestTargetSample = nullptr;
    for (const auto& sample : m_verticalSegment.samples) {
        VisualizationView::VerticalProfileSample viewSample;
        viewSample.elapsedSec = sample.elapsedSec;
        viewSample.depth = useHeight ? sample.height : sample.depth;
        viewSample.hasDepth = useHeight ? sample.hasHeight : sample.hasDepth;
        viewSample.targetDepth = useHeight ? sample.targetHeight : sample.targetDepth;
        viewSample.hasTargetDepth = useHeight ? sample.hasTargetHeight : sample.hasTargetDepth;
        viewSample.emergencyStop = sample.emergencyStop;
        frame.samples.push_back(viewSample);
    }
    for (int index = m_verticalSegment.samples.size() - 1; index >= 0; --index) {
        const auto& sample = m_verticalSegment.samples.at(index);
        if (latestDepthSample == nullptr && (useHeight ? sample.hasHeight : sample.hasDepth)) {
            latestDepthSample = &sample;
        }
        if (latestTargetSample == nullptr && (useHeight ? sample.hasTargetHeight : sample.hasTargetDepth)) {
            latestTargetSample = &sample;
        }
        if (latestDepthSample != nullptr && latestTargetSample != nullptr) {
            break;
        }
    }
    if (latestDepthSample != nullptr) {
        frame.elapsedSec = latestDepthSample->elapsedSec;
        frame.currentDepth = useHeight ? latestDepthSample->height : latestDepthSample->depth;
        frame.hasCurrentDepth = true;
    } else if (!m_verticalSegment.samples.isEmpty()) {
        frame.elapsedSec = m_verticalSegment.samples.constLast().elapsedSec;
    }
    if (latestTargetSample != nullptr) {
        frame.targetDepth = useHeight ? latestTargetSample->targetHeight : latestTargetSample->targetDepth;
        frame.hasTargetDepth = true;
    }

    if (!m_verticalSegment.active && m_verticalSegment.samples.isEmpty()) {
        m_view->setVerticalStatusMessage(QStringLiteral("等待垂向 action/定位反馈"));
    } else if (!frame.hasCurrentDepth) {
        m_view->setVerticalStatusMessage(QStringLiteral("已接收垂向目标，等待定位反馈"));
    } else {
        m_view->setVerticalStatusMessage(QString());
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
    includeVisibleContentBounds(body->sceneBoundingRect());

    QPolygonF nose;
    nose << QPointF(x + vehicleConfig.vehicleLength * 0.35, y)
         << QPointF(x + vehicleConfig.vehicleLength * 0.1, y - 0.55)
         << QPointF(x + vehicleConfig.vehicleLength * 0.1, y + 0.55);
    auto* headingMarker = m_scene->addPolygon(nose, QPen(QColor("#ffd166"), 0.0), QBrush(QColor("#ffd166")));
    headingMarker->setTransformOriginPoint(x, y);
    headingMarker->setRotation(-vehicleLocation.heading * kRadToDeg);
    includeVisibleContentBounds(headingMarker->sceneBoundingRect());

    // 总览缩小到千米级时，物理尺寸的车辆会缩成几个像素；该定位环保持屏幕尺寸。
    auto* locator = m_scene->addEllipse(-7.0, -7.0, 14.0, 14.0,
                                        QPen(QColor("#f97316"), 1.4),
                                        QBrush(QColor(249, 115, 22, 45)));
    locator->setPos(center);
    locator->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    locator->setZValue(50.0);
}

void SceneManager::drawHistoryTrail(const autoviz::model::Trajectory& trajectory)
{
    const QPainterPath path = buildTrajectoryPath(trajectory);
    if (path.isEmpty()) {
        return;
    }

    const QPen pen = makeAdaptivePathPen(m_view,
                                         QColor(156, 163, 175, 150),
                                         0.12,
                                         1.0,
                                         2.0);
    auto* item = m_scene->addPath(path, pen);
    includeVisibleContentBounds(path.boundingRect());
}

void SceneManager::drawTrajectory(const autoviz::model::Trajectory& trajectory, const QColor& color, qreal width)
{
    const QPainterPath path = buildTrajectoryPath(trajectory);
    if (path.isEmpty()) {
        return;
    }

    const bool isLocalPath = color == QColor("#ff7f50");
    const QPen pen = makeAdaptivePathPen(m_view,
                                         color,
                                         width,
                                         isLocalPath ? 2.0 : 1.5,
                                         isLocalPath ? 4.0 : 3.0);
    auto* item = m_scene->addPath(path, pen);
    includeVisibleContentBounds(path.boundingRect());
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
    includeVisibleContentBounds(marker->sceneBoundingRect());

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

    QPen pen = makeAdaptivePathPen(m_view, QColor("#9ad1d4"), 0.12, 1.0, 2.0, Qt::DashLine);
    pen.setDashPattern({4.0, 3.0});
    auto* item = m_scene->addPath(path, pen);
    includeVisibleContentBounds(path.boundingRect());
}

void SceneManager::drawObstacles(const autoviz::model::ObstacleList& obstacles)
{
    for (const auto& obstacle : obstacles) {
        if (obstacle.shape == autoviz::model::ObstacleShapeType::Polygon && !obstacle.polygon.vertices.isEmpty()) {
            QPolygonF polygon;
            for (const auto& point : obstacle.polygon.vertices) {
                polygon << QPointF(point.x, -point.y);
            }
            auto* item = m_scene->addPolygon(polygon, QPen(QColor("#ef476f"), 0.0), QBrush(QColor(239, 71, 111, 120)));
            includeVisibleContentBounds(item->sceneBoundingRect());
            continue;
        }

        const QPointF center = toScenePoint(obstacle.position.position);
        const double x = center.x();
        const double y = center.y();
        if (obstacle.shape == autoviz::model::ObstacleShapeType::Point) {
            auto* marker = m_scene->addEllipse(x - 0.25, y - 0.25, 0.5, 0.5,
                                               QPen(QColor("#ef476f"), 0.0),
                                               QBrush(QColor(239, 71, 111, 180)));
            marker->setToolTip(QStringLiteral("%1\nid=%2 class=%3\n点目标（无有效尺寸）")
                                   .arg(obstacle.sourceTopic.isEmpty() ? QStringLiteral("obstacle") : obstacle.sourceTopic)
                                   .arg(obstacle.id)
                                   .arg(obstacle.classLabel.isEmpty() ? QString::number(obstacle.sourceClass) : obstacle.classLabel));
            includeVisibleContentBounds(marker->sceneBoundingRect());
            continue;
        }
        if (obstacle.shape == autoviz::model::ObstacleShapeType::Circle) {
            const double radius = 0.5 * std::hypot(obstacle.length, obstacle.width);
            auto* marker = m_scene->addEllipse(x - radius, y - radius, 2.0 * radius, 2.0 * radius,
                                               QPen(QColor("#ef476f"), 0.0),
                                               QBrush(QColor(239, 71, 111, 90)));
            marker->setToolTip(QStringLiteral("%1\nid=%2 class=%3\n尺寸有效，朝向未知")
                                   .arg(obstacle.sourceTopic.isEmpty() ? QStringLiteral("obstacle") : obstacle.sourceTopic)
                                   .arg(obstacle.id)
                                   .arg(obstacle.classLabel.isEmpty() ? QString::number(obstacle.sourceClass) : obstacle.classLabel));
            includeVisibleContentBounds(marker->sceneBoundingRect());
            continue;
        }
        auto* rect = m_scene->addRect(x - obstacle.length * 0.5,
                                      y - obstacle.width * 0.5,
                                      obstacle.length,
                                      obstacle.width,
                                      QPen(QColor("#ef476f"), 0.0),
                                      QBrush(QColor(239, 71, 111, 90)));
        rect->setTransformOriginPoint(x, y);
        rect->setRotation(-obstacle.position.theta * kRadToDeg);
        includeVisibleContentBounds(rect->sceneBoundingRect());
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

    if (!m_hasVisibleContentBounds) {
        // Topic 超时或图层全部关闭时保留最后视角，避免画面突跳回原点。
        return;
    }

    const QPointF vehicleCenter = toScenePoint(m_snapshot.vehicleLocation.position);
    const bool canCenterOnVehicle = m_vehicleCenteredMode
                                    && m_layerVisibility.showVehicle
                                    && m_snapshot.runtimeStatus.hasVehicleLocationData;
    const QRectF targetRegion = canCenterOnVehicle
                                    ? QRectF(vehicleCenter.x() - 18.0, vehicleCenter.y() - 12.0, 36.0, 24.0)
                                    : fittedRegionFor(m_visibleContentBounds);
    if (canCenterOnVehicle) {
        // 跟车视角保持现有的局部观察尺度，并持续跟随车辆。
        m_hasAutoFitTargetRegion = false;
    }

    // 让远距离范围两侧仍保有可平移余量，但不再把原点标记算入数据边界。
    QRectF sceneRegion = m_visibleContentBounds.united(targetRegion);
    const QRectF visibleRegion = m_view->mapToScene(m_view->viewport()->rect()).boundingRect();
    const qreal horizontalMargin = std::max<qreal>(100.0, visibleRegion.width());
    const qreal verticalMargin = std::max<qreal>(100.0, visibleRegion.height());
    sceneRegion.adjust(-horizontalMargin, -verticalMargin, horizontalMargin, verticalMargin);
    if (canCenterOnVehicle) {
        sceneRegion = sceneRegion.united(QRectF(vehicleCenter.x() - horizontalMargin,
                                                 vehicleCenter.y() - verticalMargin,
                                                 horizontalMargin * 2.0,
                                                 verticalMargin * 2.0));
    }
    // 手动缩放/平移时，保留当前视口所在场景区域。否则每帧重建 sceneRect
    // 会让 QGraphicsView 的滚动位置被重新夹紧，表现为缩放跳回原点。
    if (!m_view->autoFitEnabled()) {
        sceneRegion = sceneRegion.united(visibleRegion);
    }
    m_scene->setSceneRect(sceneRegion);

    if (canCenterOnVehicle) {
        m_view->fitToRegion(targetRegion);
        m_view->centerOn(vehicleCenter);
        return;
    }

    if (m_view->autoFitEnabled() && hasMaterialVisibleBoundsChange(targetRegion)) {
        m_view->fitToRegion(targetRegion);
        m_lastAutoFitTargetRegion = targetRegion;
        m_hasAutoFitTargetRegion = true;
    }
}

void SceneManager::includeVisibleContentBounds(const QRectF& bounds)
{
    if (!bounds.isValid() || bounds.isEmpty()) {
        return;
    }
    m_visibleContentBounds = m_hasVisibleContentBounds ? m_visibleContentBounds.united(bounds) : bounds;
    m_hasVisibleContentBounds = true;
}

QRectF SceneManager::fittedRegionFor(const QRectF& contentBounds) const
{
    QRectF target = contentBounds;
    const qreal marginX = std::max<qreal>(4.0, target.width() * 0.10);
    const qreal marginY = std::max<qreal>(4.0, target.height() * 0.10);
    target.adjust(-marginX, -marginY, marginX, marginY);

    const QPointF center = target.center();
    const qreal width = std::max<qreal>(36.0, target.width());
    const qreal height = std::max<qreal>(24.0, target.height());
    return QRectF(center.x() - width * 0.5, center.y() - height * 0.5, width, height);
}

bool SceneManager::hasMaterialVisibleBoundsChange(const QRectF& targetRegion) const
{
    if (!m_hasAutoFitTargetRegion) {
        return true;
    }

    const qreal previousLongSide = std::max(m_lastAutoFitTargetRegion.width(), m_lastAutoFitTargetRegion.height());
    const qreal centerThreshold = std::max<qreal>(5.0, previousLongSide * 0.10);
    const bool centerChanged = QLineF(targetRegion.center(), m_lastAutoFitTargetRegion.center()).length() > centerThreshold;
    const bool widthChanged = std::abs(targetRegion.width() - m_lastAutoFitTargetRegion.width())
                              > std::max<qreal>(1.0, m_lastAutoFitTargetRegion.width() * 0.10);
    const bool heightChanged = std::abs(targetRegion.height() - m_lastAutoFitTargetRegion.height())
                               > std::max<qreal>(1.0, m_lastAutoFitTargetRegion.height() * 0.10);
    return centerChanged || widthChanged || heightChanged;
}

QPointF SceneManager::toScenePoint(const autoviz::model::Point2D& point) const
{
    return QPointF(point.x, -point.y);
}

}  // namespace autoviz::render
