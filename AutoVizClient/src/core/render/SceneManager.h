#pragma once

#include <QObject>
#include <QColor>
#include <QRectF>
#include <QVector>

#include "core/datacenter/DataManager.h"

class QGraphicsScene;
class QPointF;
class VisualizationView;

namespace autoviz::render {

struct LayerVisibility {
    bool showVehicle = true;
    bool showHistoryTrail = true;
    bool showGlobalPath = true;
    bool showReferenceLine = true;
    bool showLocalPath = true;
    bool showObstacles = true;
};

enum class MainViewMode {
    Auto,
    TopDownXY,
    VerticalProfile
};

QString toDisplayString(MainViewMode mode);

class SceneManager : public QObject {
    Q_OBJECT

public:
    explicit SceneManager(VisualizationView* view, QObject* parent = nullptr);

    void initializeScene();
    void clearScene();
    void updateScene(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    // 新的本地回放会话不能复用上一轮 T-Z 分段或自动取景状态。
    void resetPlaybackSession();
    void setLayerVisibility(const LayerVisibility& visibility);
    LayerVisibility layerVisibility() const;
    void setVehicleCenteredMode(bool enabled);
    bool vehicleCenteredMode() const;
    void refitVisibleData();
    void setMainViewMode(MainViewMode mode);
    MainViewMode mainViewMode() const;

private:
    struct VerticalMotionSegmentSample {
        double elapsedSec = 0.0;
        double depth = 0.0;
        bool hasDepth = false;
        double targetDepth = 0.0;
        bool hasTargetDepth = false;
        double height = 0.0;
        bool hasHeight = false;
        double targetHeight = 0.0;
        bool hasTargetHeight = false;
        double depthError = 0.0;
        bool emergencyStop = false;
    };

    struct VerticalMotionSegment {
        bool active = false;
        bool frozen = false;
        bool emergencyStop = false;
        qint64 startTimestampMs = 0;
        qint64 lastVerticalTimestampMs = 0;
        qint64 leftVerticalSinceMs = 0;
        double startDepth = 0.0;
        bool hasStartDepth = false;
        double startHeight = 0.0;
        bool hasStartHeight = false;
        double startX = 0.0;
        double startY = 0.0;
        double startYaw = 0.0;
        double targetDepth = 0.0;
        bool hasTargetDepth = false;
        double targetHeight = 0.0;
        bool hasTargetHeight = false;
        int taskType = 0;
        int taskId = 0;
        int chassisMode = 0;
        QVector<VerticalMotionSegmentSample> samples;
        QVector<double> emergencyEventTimes;
    };

    void redraw();
    void redrawTopDownXY();
    void redrawVerticalProfile();
    void updateVerticalMotionSegment(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void startVerticalMotionSegment(const autoviz::datacenter::VisualizationSnapshot& snapshot, qint64 nowMs);
    void appendVerticalMotionSample(const autoviz::datacenter::VisualizationSnapshot& snapshot, qint64 nowMs);
    void freezeVerticalMotionSegment();
    bool shouldStartNewVerticalSegment(const autoviz::datacenter::VisualizationSnapshot& snapshot) const;
    bool isVerticalMode(autoviz::model::RunVisualizationMode mode) const;
    void drawVehicle(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void drawHistoryTrail(const autoviz::model::Trajectory& trajectory);
    void drawTrajectory(const autoviz::model::Trajectory& trajectory, const QColor& color, qreal width);
    void drawPathEndpoint(const autoviz::model::PathEndpointStatus& endpoint);
    void drawReferenceLine(const autoviz::model::ReferenceLine& referenceLine);
    void drawObstacles(const autoviz::model::ObstacleList& obstacles);
    void autoFitAndCenter();
    void includeVisibleContentBounds(const QRectF& bounds);
    QRectF fittedRegionFor(const QRectF& contentBounds) const;
    bool hasMaterialVisibleBoundsChange(const QRectF& targetRegion) const;
    QPointF toScenePoint(const autoviz::model::Point2D& point) const;

    VisualizationView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
    autoviz::datacenter::VisualizationSnapshot m_snapshot;
    LayerVisibility m_layerVisibility;
    bool m_vehicleCenteredMode = false;
    QRectF m_visibleContentBounds;
    QRectF m_lastAutoFitTargetRegion;
    bool m_hasVisibleContentBounds = false;
    bool m_hasAutoFitTargetRegion = false;
    MainViewMode m_mainViewMode = MainViewMode::TopDownXY;
    VerticalMotionSegment m_verticalSegment;
    autoviz::model::RunVisualizationMode m_previousRunMode = autoviz::model::RunVisualizationMode::Unknown;
};

}  // namespace autoviz::render
