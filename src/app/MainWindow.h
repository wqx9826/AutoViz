#pragma once

#include <QMainWindow>

class QAction;
class ControlStatusPanel;
class DetailPanel;
class LogPanel;
class QDockWidget;
class QMenu;
class QToolBar;
class SceneManager;
class TopicConfigPanel;
class VisualizationView;

namespace autoviz::datasource {
class MsgCompiler;
class RosMsgPackageRegistry;
}

// Main application shell. It hosts the debugging-style layout and
// wires the placeholder modules together.
class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDocks();
    void connectActions();
    void restoreDefaultLayout();
    void updatePackageUiState();
    void loadRosMsgPackage();
    void unloadCurrentPackage();
    void bindDockLogging(QDockWidget* dock, const QString& panelName);

    VisualizationView* m_visualizationView = nullptr;
    TopicConfigPanel* m_topicConfigPanel = nullptr;
    DetailPanel* m_detailPanel = nullptr;
    ControlStatusPanel* m_controlStatusPanel = nullptr;
    LogPanel* m_logPanel = nullptr;
    SceneManager* m_sceneManager = nullptr;
    autoviz::datasource::RosMsgPackageRegistry* m_packageRegistry = nullptr;
    autoviz::datasource::MsgCompiler* m_msgCompiler = nullptr;

    QDockWidget* m_leftDock = nullptr;
    QDockWidget* m_rightDock = nullptr;
    QDockWidget* m_controlDock = nullptr;
    QDockWidget* m_logDock = nullptr;
    QToolBar* m_mainToolBar = nullptr;
    QMenu* m_viewMenu = nullptr;

    QAction* m_loadRosPackageAction = nullptr;
    QAction* m_closeRosPackageAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_restoreLayoutAction = nullptr;
    QAction* m_aboutAction = nullptr;
};
