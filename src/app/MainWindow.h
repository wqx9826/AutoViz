#pragma once

#include <QMainWindow>
#include <QStringList>

#include "core/ros/RosTypes.h"

class QAction;
class ControlStatusPanel;
class DetailPanel;
class LogPanel;
class MessagePackagePanel;
class QDialog;
class QDockWidget;
class QFileDialog;
class QListView;
class QMenu;
class QTreeView;
class SceneManager;
class VisualizationView;

namespace autoviz::ros {
class RosPackageBuilder;
class RosPackageRegistry;
class RosWorkspaceManager;
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
    void refreshUiState();
    void detectRosEnvironment();
    void restorePackageState();
    bool persistPackageState();
    void showMessagePackageManagerWindow();
    void loadRosMsgPackages();
    void removeSelectedPackages(const QStringList& packageNames);
    void buildRosMsgPackages();
    QStringList selectPackageDirectories() const;
    bool initializeWorkspace();
    bool addPackageFromDirectory(const QString& packageDirectory);
    void ensureLogDockBottomArea(bool visible);
    void bindDockLogging(QDockWidget* dock, const QString& panelName);

    VisualizationView* m_visualizationView = nullptr;
    MessagePackagePanel* m_messagePackagePanel = nullptr;
    QDialog* m_messagePackageDialog = nullptr;
    DetailPanel* m_detailPanel = nullptr;
    ControlStatusPanel* m_controlStatusPanel = nullptr;
    LogPanel* m_logPanel = nullptr;
    SceneManager* m_sceneManager = nullptr;
    autoviz::ros::RosPackageRegistry* m_packageRegistry = nullptr;
    autoviz::ros::RosPackageBuilder* m_packageBuilder = nullptr;
    autoviz::ros::RosWorkspaceManager* m_workspaceManager = nullptr;

    QDockWidget* m_rightDock = nullptr;
    QDockWidget* m_controlDock = nullptr;
    QDockWidget* m_logDock = nullptr;
    QMenu* m_viewMenu = nullptr;
    bool m_adjustingLogDock = false;

    QAction* m_loadRosPackageAction = nullptr;
    QAction* m_removeRosPackageAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_restoreLayoutAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_logDockAction = nullptr;
};
