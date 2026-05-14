#pragma once

#include <memory>

#include <QMainWindow>

class QAction;
class LogPanel;
class MainViewDisplayConfigDialog;
class QDockWidget;
class QMenu;
class QTimer;
class VisualizationView;

namespace autoviz::datacenter {
class DataManager;
struct VisualizationSnapshot;
}

namespace autoviz::ros {
class RosMsgSubscribeBase;
}

namespace autoviz::render {
class SceneManager;
}

namespace autoviz::ui::charts {
class ControlPanelWidget;
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
    void openMainViewDisplayConfigDialog();
    void updateMainViewOverlay(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void updateMainViewDisplayDialog(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void loadMainViewDisplaySettings();
    void saveMainViewDisplaySettings() const;
    void bindDockLogging(QDockWidget* dock, const QString& panelName);

    VisualizationView* m_visualizationView = nullptr;
    MainViewDisplayConfigDialog* m_mainViewDisplayConfigDialog = nullptr;
    autoviz::ui::charts::ControlPanelWidget* m_controlPanel = nullptr;
    LogPanel* m_logPanel = nullptr;
    autoviz::render::SceneManager* m_sceneManager = nullptr;
    autoviz::datacenter::DataManager* m_dataManager = nullptr;
    std::shared_ptr<autoviz::ros::RosMsgSubscribeBase> m_msgSubscriber;
    QTimer* m_refreshTimer = nullptr;

    QDockWidget* m_chartDock = nullptr;
    QDockWidget* m_logDock = nullptr;
    QMenu* m_viewMenu = nullptr;

    QAction* m_mainViewDisplayManageAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_restoreLayoutAction = nullptr;
    QAction* m_aboutAction = nullptr;
};
