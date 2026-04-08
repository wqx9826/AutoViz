#include "core/render/SceneManager.h"

#include <QBrush>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>

#include "core/model/EgoModel.h"
#include "core/model/ObstacleModel.h"
#include "core/model/TrajectoryModel.h"
#include "ui/VisualizationView.h"

SceneManager::SceneManager(VisualizationView* view, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_scene(view != nullptr ? view->scene() : nullptr)
{
}

void SceneManager::initializeScene()
{
    if (m_scene == nullptr) {
        return;
    }

    clearScene();
    setSceneBackground();

    auto* centerMarker = m_scene->addEllipse(-8.0, -8.0, 16.0, 16.0, QPen(QColor("#f7c948"), 2.0));
    centerMarker->setBrush(QBrush(QColor(247, 201, 72, 80)));

}

void SceneManager::clearScene()
{
    if (m_scene != nullptr) {
        m_scene->clear();
    }
}

void SceneManager::setSceneBackground()
{
    if (m_view != nullptr) {
        m_view->setBackgroundColor(QColor("#1f2630"));
    }
}

void SceneManager::updateTrajectory(const autoviz::model::TrajectoryModel& trajectory)
{
    Q_UNUSED(trajectory);
}

void SceneManager::updateObstacles(const autoviz::model::ObstacleCollection& obstacles)
{
    Q_UNUSED(obstacles);
}

void SceneManager::updateEgo(const autoviz::model::EgoModel& ego)
{
    Q_UNUSED(ego);
}
