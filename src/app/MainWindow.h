#pragma once

#include <memory>

#include <QMainWindow>

class QAction;
class QActionGroup;
class BottomStatusPanel;
class MainViewDisplayConfigDialog;
class QMenu;
class QSplitter;
class QTimer;
class VisualizationView;

namespace autoviz::ui::theme {
enum class ThemeMode;
}

namespace autoviz::datacenter {
class DataManager;
struct VisualizationSnapshot;
}

namespace autoviz::ros {
class RosMsgSubscribeBase;
}

namespace autoviz::render {
class SceneManager;
enum class MainViewMode;
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
    void setupAppearanceMenu();
    void setupMainViewModeMenu();
    void setupStatusBar();
    void setupMainLayout();
    void connectActions();
    void restoreDefaultLayout();
    void changeTheme(autoviz::ui::theme::ThemeMode mode);
    QString loadThemeStyleSheet(autoviz::ui::theme::ThemeMode mode) const;
    void setRequestedMainViewMode(autoviz::render::MainViewMode mode);
    void updateMainViewMode(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void refreshVisualization();
    void initializeMessageSubscriber();
    void openMainViewDisplayConfigDialog();
    void updateMainViewOverlay(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void updateMainViewDisplayDialog(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void loadMainViewDisplaySettings();
    void saveMainViewDisplaySettings() const;

    VisualizationView* m_visualizationView = nullptr;
    MainViewDisplayConfigDialog* m_mainViewDisplayConfigDialog = nullptr;
    autoviz::ui::charts::ControlPanelWidget* m_controlPanel = nullptr;
    BottomStatusPanel* m_bottomStatusPanel = nullptr;
    autoviz::render::SceneManager* m_sceneManager = nullptr;
    autoviz::datacenter::DataManager* m_dataManager = nullptr;
    std::shared_ptr<autoviz::ros::RosMsgSubscribeBase> m_msgSubscriber;
    QTimer* m_refreshTimer = nullptr;

    QSplitter* m_rootSplitter = nullptr;
    QSplitter* m_leftSplitter = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_appearanceMenu = nullptr;
    QMenu* m_mainViewModeMenu = nullptr;
    QActionGroup* m_themeActionGroup = nullptr;
    QActionGroup* m_mainViewModeActionGroup = nullptr;

    QAction* m_mainViewDisplayManageAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_clearHistoryTrailAction = nullptr;
    QAction* m_restoreLayoutAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_autoThemeAction = nullptr;
    QAction* m_lightThemeAction = nullptr;
    QAction* m_darkThemeAction = nullptr;
    QAction* m_autoMainViewModeAction = nullptr;
    QAction* m_topDownMainViewModeAction = nullptr;
    QAction* m_verticalProfileMainViewModeAction = nullptr;
    autoviz::render::MainViewMode m_requestedMainViewMode;
    autoviz::render::MainViewMode m_effectiveMainViewMode;
    autoviz::render::MainViewMode m_pendingAutoMainViewMode;
    qint64 m_pendingAutoMainViewModeSinceMs = 0;
};
