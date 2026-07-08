#pragma once

#include <QObject>
#include <QColor>

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
    void setLayerVisibility(const LayerVisibility& visibility);
    LayerVisibility layerVisibility() const;
    void setVehicleCenteredMode(bool enabled);
    bool vehicleCenteredMode() const;
    void setMainViewMode(MainViewMode mode);
    MainViewMode mainViewMode() const;

private:
    void redraw();
    void redrawTopDownXY();
    void redrawVerticalProfile();
    void drawVehicle(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void drawHistoryTrail(const autoviz::model::Trajectory& trajectory);
    void drawTrajectory(const autoviz::model::Trajectory& trajectory, const QColor& color, qreal width);
    void drawPathEndpoint(const autoviz::model::PathEndpointStatus& endpoint);
    void drawReferenceLine(const autoviz::model::ReferenceLine& referenceLine);
    void drawObstacles(const autoviz::model::ObstacleList& obstacles);
    void autoFitAndCenter();
    QPointF toScenePoint(const autoviz::model::Point2D& point) const;

    VisualizationView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
    autoviz::datacenter::VisualizationSnapshot m_snapshot;
    LayerVisibility m_layerVisibility;
    bool m_vehicleCenteredMode = false;
    MainViewMode m_mainViewMode = MainViewMode::TopDownXY;
};

}  // namespace autoviz::render
