#pragma once

#include <QObject>

#include "core/model/EgoModel.h"
#include "core/model/ObstacleModel.h"
#include "core/model/TrajectoryModel.h"

class QGraphicsScene;
class VisualizationView;

// SceneManager owns the scene-level rendering policy. It is intentionally
// separate from the view so later stages can manage layered renderables here.
class SceneManager : public QObject
{
    Q_OBJECT

public:
    explicit SceneManager(VisualizationView* view, QObject* parent = nullptr);

    void initializeScene();
    void clearScene();
    void setSceneBackground();

    void updateTrajectory(const autoviz::model::TrajectoryModel& trajectory);
    void updateObstacles(const autoviz::model::ObstacleCollection& obstacles);
    void updateEgo(const autoviz::model::EgoModel& ego);

private:
    VisualizationView* m_view = nullptr;
    QGraphicsScene* m_scene = nullptr;
};
