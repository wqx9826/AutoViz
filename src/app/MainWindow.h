#pragma once

#include <memory>

#include <QMainWindow>

class QAction;
class ChartPanel;
class ControlStatusPanel;
class DisplayControlPanel;
class LogPanel;
class QDockWidget;
class QTimer;
class VisualizationView;

namespace autoviz::datacenter {
class DataManager;
}

namespace autoviz::ros {
class RosMsgSubscribeBase;
}

namespace autoviz::render {
class SceneManager;
}

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void setupMenuBar();
    void setupStatusBar();
    void setupDocks();
    void connectActions();
    void restoreDefaultLayout();
    void refreshVisualization();
    void initializeMessageSubscriber();
    void bindDockLogging(QDockWidget* dock, const QString& panelName);

    VisualizationView* m_visualizationView = nullptr;
    DisplayControlPanel* m_displayControlPanel = nullptr;
    ControlStatusPanel* m_controlStatusPanel = nullptr;
    ChartPanel* m_chartPanel = nullptr;
    LogPanel* m_logPanel = nullptr;
    autoviz::render::SceneManager* m_sceneManager = nullptr;
    autoviz::datacenter::DataManager* m_dataManager = nullptr;
    std::shared_ptr<autoviz::ros::RosMsgSubscribeBase> m_msgSubscriber;
    QTimer* m_refreshTimer = nullptr;

    QDockWidget* m_rightDock = nullptr;
    QDockWidget* m_controlDock = nullptr;
    QDockWidget* m_chartDock = nullptr;
    QDockWidget* m_logDock = nullptr;
    QMenu* m_viewMenu = nullptr;

    QAction* m_exitAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_restoreLayoutAction = nullptr;
    QAction* m_aboutAction = nullptr;
};
