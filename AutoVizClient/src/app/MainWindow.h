#pragma once

#include <QMainWindow>

class QAction;
class QActionGroup;
class BottomStatusPanel;
class MainViewDisplayConfigDialog;
class QMenu;
class QSplitter;
class QTimer;
class VisualizationView;

namespace autoviz::network {
class RemoteVisualizationSource;
}
namespace autoviz::playback { class LocalRosbagPlaybackSource; enum class PlaybackState; }
namespace autoviz::ui::playback { class RosbagPlaybackDialog; }

namespace autoviz::ui {
class ConnectionDialog;
}

namespace autoviz::ui::theme {
enum class ThemeMode;
}

namespace autoviz::datacenter {
class DataManager;
struct VisualizationSnapshot;
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
    void setupConnectionMenu();
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
    void initializeRemoteDataSource();
    void initializeLocalPlaybackSource();
    void openConnectionDialog();
    void openRosbagPlaybackDialog();
    void openMainViewDisplayConfigDialog();
    void openConfigurationDirectory();
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
    autoviz::network::RemoteVisualizationSource* m_remoteSource = nullptr;
    autoviz::playback::LocalRosbagPlaybackSource* m_playbackSource = nullptr;
    autoviz::ui::playback::RosbagPlaybackDialog* m_playbackDialog = nullptr;
    autoviz::ui::ConnectionDialog* m_connectionDialog = nullptr;
    QTimer* m_refreshTimer = nullptr;

    QSplitter* m_rootSplitter = nullptr;
    QSplitter* m_leftSplitter = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_connectionMenu = nullptr;
    QMenu* m_mainViewModeMenu = nullptr;
    QActionGroup* m_mainViewModeActionGroup = nullptr;

    QAction* m_mainViewDisplayManageAction = nullptr;
    QAction* m_openConfigurationDirectoryAction = nullptr;
    QAction* m_connectServerAction = nullptr;
    QAction* m_playbackAction = nullptr;
    QAction* m_disconnectServerAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_resetViewAction = nullptr;
    QAction* m_fitVisibleDataAction = nullptr;
    QAction* m_clearHistoryTrailAction = nullptr;
    QAction* m_restoreLayoutAction = nullptr;
    QAction* m_aboutAction = nullptr;
    QAction* m_autoMainViewModeAction = nullptr;
    QAction* m_topDownMainViewModeAction = nullptr;
    QAction* m_verticalProfileMainViewModeAction = nullptr;
    autoviz::render::MainViewMode m_requestedMainViewMode;
    autoviz::render::MainViewMode m_effectiveMainViewMode;
    autoviz::render::MainViewMode m_pendingAutoMainViewMode;
    qint64 m_pendingAutoMainViewModeSinceMs = 0;
    QString m_connectionStatus = QStringLiteral("未连接 Server");
    bool m_serverConnected = false;
    qint64 m_lastPlaybackSceneRefreshMs = 0;
    qint64 m_lastPlaybackPositionNs = 0;
    QString m_playbackBagName;
    QString m_playbackStateText;
};
