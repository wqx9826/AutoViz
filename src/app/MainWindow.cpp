#include "app/MainWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QSizePolicy>
#include <QSplitter>
#include <QStringList>
#include <QTextStream>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "core/datacenter/DataManager.h"
#include "core/model/RuntimeStatusTypes.h"
#include "core/render/SceneManager.h"
#include "core/ros/RosMsgSubscribeBase.h"
#include "core/ros/RosMsgSubsrcribeFactory.h"
#include "ui/MainViewDisplayConfigDialog.h"
#include "ui/VisualizationView.h"
#include "ui/charts/ControlPanelStyle.h"
#include "ui/charts/ControlPanelWidget.h"
#include "ui/status/BottomStatusPanel.h"
#include "ui/theme/UiScaleManager.h"
#include "ui/theme/UiThemeManager.h"
#include "utils/Logger.h"

namespace {
constexpr qint64 kMainViewAutoSwitchDebounceMs = 800;

QString topicStatusSummary(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (snapshot.topicStatuses.isEmpty()) {
        return QStringLiteral("Topic 状态：等待");
    }

    int onlineCount = 0;
    int waitingCount = 0;
    QStringList details;
    for (const auto& status : snapshot.topicStatuses) {
        const bool online = status.messageCount > 0 && !status.timedOut;
        if (online) {
            ++onlineCount;
        } else {
            ++waitingCount;
        }
        details << QStringLiteral("%1: %2")
                       .arg(status.name, online ? QStringLiteral("在线") : QStringLiteral("等待/超时"));
    }

    return QStringLiteral("ROS2 订阅：%1 个 topic，在线 %2，等待/超时 %3\n%4")
        .arg(snapshot.topicStatuses.size())
        .arg(onlineCount)
        .arg(waitingCount)
        .arg(details.join(QStringLiteral("\n")));
}

QString compactTopicStatusSummary(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (snapshot.topicStatuses.isEmpty()) {
        return QStringLiteral("Topic 状态：等待");
    }

    int onlineCount = 0;
    int waitingCount = 0;
    for (const auto& status : snapshot.topicStatuses) {
        if (status.messageCount > 0 && !status.timedOut) {
            ++onlineCount;
        } else {
            ++waitingCount;
        }
    }
    return QStringLiteral("ROS2 订阅：%1 个 topic，在线 %2，等待/超时 %3")
        .arg(snapshot.topicStatuses.size())
        .arg(onlineCount)
        .arg(waitingCount);
}

bool runModeHasMainViewCandidate(autoviz::model::RunVisualizationMode runMode,
                                 autoviz::render::MainViewMode* mode)
{
    if (mode == nullptr) {
        return false;
    }
    switch (runMode) {
    case autoviz::model::RunVisualizationMode::HorizontalMotion:
        *mode = autoviz::render::MainViewMode::TopDownXY;
        return true;
    case autoviz::model::RunVisualizationMode::VerticalMotion:
    case autoviz::model::RunVisualizationMode::BuoyancyAdjust:
        *mode = autoviz::render::MainViewMode::VerticalProfile;
        return true;
    case autoviz::model::RunVisualizationMode::EmergencyStop:
    case autoviz::model::RunVisualizationMode::Idle:
    case autoviz::model::RunVisualizationMode::Unknown:
    default:
        return false;
    }
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_requestedMainViewMode(autoviz::render::MainViewMode::Auto)
    , m_effectiveMainViewMode(autoviz::render::MainViewMode::TopDownXY)
    , m_pendingAutoMainViewMode(autoviz::render::MainViewMode::TopDownXY)
{
    m_dataManager = new autoviz::datacenter::DataManager();
    setupUi();

    Logger::instance().setLogHandler([this](const QString& message) {
        if (m_bottomStatusPanel != nullptr) {
            m_bottomStatusPanel->appendLog(message);
        }
    });

    initializeMessageSubscriber();
    connectActions();
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(50);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() { refreshVisualization(); });
    m_refreshTimer->start();
    refreshVisualization();
    Logger::instance().info("主窗口初始化完成，当前为规划控制可视化骨架版本。");
}

MainWindow::~MainWindow()
{
    Logger::instance().clearLogHandler();
    if (m_msgSubscriber != nullptr) {
        m_msgSubscriber->stop();
    }
    delete m_dataManager;
}

void MainWindow::setupUi()
{
    setWindowTitle(tr("AutoViz"));
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    resize(scale.scaled(1460), scale.scaled(920));

    setupMenuBar();
    setupStatusBar();
    setupMainLayout();
    changeTheme(autoviz::ui::theme::ThemeMode::Auto);
    restoreDefaultLayout();
    loadMainViewDisplaySettings();
}

void MainWindow::setupMainLayout()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    m_visualizationView = new VisualizationView(this);
    m_sceneManager = new autoviz::render::SceneManager(m_visualizationView, this);
    m_sceneManager->initializeScene();

    m_controlPanel = new autoviz::ui::charts::ControlPanelWidget(this);
    m_bottomStatusPanel = new BottomStatusPanel(this);

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    m_rootSplitter = new QSplitter(Qt::Horizontal, central);
    m_rootSplitter->setChildrenCollapsible(false);
    m_rootSplitter->setHandleWidth(scale.scaled(3));

    m_leftSplitter = new QSplitter(Qt::Vertical, m_rootSplitter);
    m_leftSplitter->setChildrenCollapsible(false);
    m_leftSplitter->setHandleWidth(scale.scaled(3));
    m_visualizationView->setMinimumHeight(scale.scaled(240));
    m_leftSplitter->addWidget(m_visualizationView);
    m_leftSplitter->addWidget(m_bottomStatusPanel);
    m_bottomStatusPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_leftSplitter->setStretchFactor(0, 1);
    m_leftSplitter->setStretchFactor(1, 0);

    m_controlPanel->setMinimumWidth(scale.scaled(340));
    m_rootSplitter->addWidget(m_leftSplitter);
    m_rootSplitter->addWidget(m_controlPanel);
    m_rootSplitter->setStretchFactor(0, 7);
    m_rootSplitter->setStretchFactor(1, 3);

    centralLayout->addWidget(m_rootSplitter);
    setCentralWidget(central);
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
    m_mainViewDisplayManageAction = fileMenu->addAction(tr("主视图显示管理"));
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction(tr("退出"));

    m_viewMenu = menuBar()->addMenu(tr("视图"));
    m_resetViewAction = m_viewMenu->addAction(tr("重置视图"));
    m_clearHistoryTrailAction = m_viewMenu->addAction(tr("清空历史轨迹"));
    m_restoreLayoutAction = m_viewMenu->addAction(tr("恢复默认布局"));
    setupMainViewModeMenu();
    setupAppearanceMenu();

    QMenu* helpMenu = menuBar()->addMenu(tr("帮助"));
    m_aboutAction = helpMenu->addAction(tr("关于"));
}

void MainWindow::setupMainViewModeMenu()
{
    m_mainViewModeMenu = m_viewMenu->addMenu(tr("主视图模式"));
    m_mainViewModeActionGroup = new QActionGroup(this);
    m_mainViewModeActionGroup->setExclusive(true);

    m_autoMainViewModeAction = m_mainViewModeMenu->addAction(tr("自动"));
    m_topDownMainViewModeAction = m_mainViewModeMenu->addAction(tr("俯视 XY"));
    m_verticalProfileMainViewModeAction = m_mainViewModeMenu->addAction(tr("垂向剖面"));

    for (auto* action : {m_autoMainViewModeAction, m_topDownMainViewModeAction, m_verticalProfileMainViewModeAction}) {
        action->setCheckable(true);
        m_mainViewModeActionGroup->addAction(action);
    }
    m_autoMainViewModeAction->setChecked(true);

    connect(m_autoMainViewModeAction, &QAction::triggered, this, [this]() {
        setRequestedMainViewMode(autoviz::render::MainViewMode::Auto);
    });
    connect(m_topDownMainViewModeAction, &QAction::triggered, this, [this]() {
        setRequestedMainViewMode(autoviz::render::MainViewMode::TopDownXY);
    });
    connect(m_verticalProfileMainViewModeAction, &QAction::triggered, this, [this]() {
        setRequestedMainViewMode(autoviz::render::MainViewMode::VerticalProfile);
    });
}

void MainWindow::setupAppearanceMenu()
{
    m_appearanceMenu = m_viewMenu->addMenu(tr("外观"));
    m_themeActionGroup = new QActionGroup(this);
    m_themeActionGroup->setExclusive(true);

    m_autoThemeAction = m_appearanceMenu->addAction(tr("自动"));
    m_lightThemeAction = m_appearanceMenu->addAction(tr("浅色"));
    m_darkThemeAction = m_appearanceMenu->addAction(tr("深色"));

    for (auto* action : {m_autoThemeAction, m_lightThemeAction, m_darkThemeAction}) {
        action->setCheckable(true);
        m_themeActionGroup->addAction(action);
    }
    m_autoThemeAction->setChecked(true);

    connect(m_autoThemeAction, &QAction::triggered, this, [this]() { changeTheme(autoviz::ui::theme::ThemeMode::Auto); });
    connect(m_lightThemeAction, &QAction::triggered, this, [this]() { changeTheme(autoviz::ui::theme::ThemeMode::Light); });
    connect(m_darkThemeAction, &QAction::triggered, this, [this]() { changeTheme(autoviz::ui::theme::ThemeMode::Dark); });
}

void MainWindow::setupStatusBar()
{
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    statusBar()->setFont(scale.font(scale.fontSizeSmall()));
    statusBar()->showMessage(tr("消息订阅初始化中"));
}

void MainWindow::connectActions()
{
    connect(m_mainViewDisplayManageAction, &QAction::triggered, this, &MainWindow::openMainViewDisplayConfigDialog);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_resetViewAction, &QAction::triggered, this, [this]() {
        m_visualizationView->resetView();
        statusBar()->showMessage(tr("视图已重置"), 2000);
    });
    connect(m_clearHistoryTrailAction, &QAction::triggered, this, [this]() {
        m_dataManager->clearHistoryTrail();
        statusBar()->showMessage(tr("历史轨迹已清空"), 2000);
    });
    connect(m_restoreLayoutAction, &QAction::triggered, this, [this]() { restoreDefaultLayout(); });
    connect(m_aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this,
                           tr("关于 AutoViz"),
                           tr("AutoViz\n\n面向车辆规划控制数据的可视化工具骨架。\n主视图只依赖内部标准模型。"));
    });
}

void MainWindow::restoreDefaultLayout()
{
    if (m_rootSplitter == nullptr || m_leftSplitter == nullptr) {
        return;
    }
    const auto& scale = autoviz::ui::theme::UiScaleManager::instance();
    m_rootSplitter->setSizes({scale.scaled(1020), scale.scaled(440)});
    m_leftSplitter->setSizes({scale.scaled(700), scale.scaled(260)});
}

void MainWindow::changeTheme(autoviz::ui::theme::ThemeMode mode)
{
    auto& theme = autoviz::ui::theme::UiThemeManager::instance();
    theme.setMode(mode);
    qApp->setStyleSheet(loadThemeStyleSheet(mode));

    const auto palette = theme.effectivePalette();
    if (m_visualizationView != nullptr) {
        m_visualizationView->applyTheme(palette);
    }
    if (m_controlPanel != nullptr) {
        m_controlPanel->setStyleSheet(autoviz::ui::charts::style::panelStyleSheet());
        m_controlPanel->update();
    }
    if (m_bottomStatusPanel != nullptr) {
        m_bottomStatusPanel->update();
    }
    if (m_mainViewDisplayConfigDialog != nullptr) {
        m_mainViewDisplayConfigDialog->update();
    }

    if (m_autoThemeAction != nullptr) {
        m_autoThemeAction->setChecked(mode == autoviz::ui::theme::ThemeMode::Auto);
    }
    if (m_lightThemeAction != nullptr) {
        m_lightThemeAction->setChecked(mode == autoviz::ui::theme::ThemeMode::Light);
    }
    if (m_darkThemeAction != nullptr) {
        m_darkThemeAction->setChecked(mode == autoviz::ui::theme::ThemeMode::Dark);
    }
}

QString MainWindow::loadThemeStyleSheet(autoviz::ui::theme::ThemeMode mode) const
{
    const auto& theme = autoviz::ui::theme::UiThemeManager::instance();
    const bool useDark = mode == autoviz::ui::theme::ThemeMode::Dark ||
                         (mode == autoviz::ui::theme::ThemeMode::Auto && theme.effectivePalette().dark);
    const QString fileName = useDark ? QStringLiteral("dark.qss") : QStringLiteral("light.qss");
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../configs/themes/%1").arg(fileName)),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("configs/themes/%1").arg(fileName)),
        QDir::current().filePath(QStringLiteral("configs/themes/%1").arg(fileName)),
    };

    for (const auto& path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        return stream.readAll();
    }

    return theme.styleSheet();
}

void MainWindow::setRequestedMainViewMode(autoviz::render::MainViewMode mode)
{
    m_requestedMainViewMode = mode;
    m_pendingAutoMainViewModeSinceMs = 0;

    if (m_autoMainViewModeAction != nullptr) {
        m_autoMainViewModeAction->setChecked(mode == autoviz::render::MainViewMode::Auto);
    }
    if (m_topDownMainViewModeAction != nullptr) {
        m_topDownMainViewModeAction->setChecked(mode == autoviz::render::MainViewMode::TopDownXY);
    }
    if (m_verticalProfileMainViewModeAction != nullptr) {
        m_verticalProfileMainViewModeAction->setChecked(mode == autoviz::render::MainViewMode::VerticalProfile);
    }

    if (mode == autoviz::render::MainViewMode::Auto) {
        const auto snapshot = m_dataManager->getSnapshot();
        autoviz::render::MainViewMode candidate;
        if (runModeHasMainViewCandidate(snapshot.runVisualizationMode, &candidate)) {
            m_effectiveMainViewMode = candidate;
            m_pendingAutoMainViewMode = candidate;
            if (m_sceneManager != nullptr) {
                m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
            }
        }
        return;
    }

    m_effectiveMainViewMode = mode;
    if (m_sceneManager != nullptr) {
        m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
    }
}

void MainWindow::updateMainViewMode(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (m_sceneManager == nullptr) {
        return;
    }

    if (m_requestedMainViewMode != autoviz::render::MainViewMode::Auto) {
        m_effectiveMainViewMode = m_requestedMainViewMode;
        m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
        return;
    }

    autoviz::render::MainViewMode candidate;
    if (!runModeHasMainViewCandidate(snapshot.runVisualizationMode, &candidate)) {
        m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
        return;
    }

    if (candidate == m_effectiveMainViewMode) {
        m_pendingAutoMainViewModeSinceMs = 0;
        m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_pendingAutoMainViewModeSinceMs == 0 || m_pendingAutoMainViewMode != candidate) {
        m_pendingAutoMainViewMode = candidate;
        m_pendingAutoMainViewModeSinceMs = nowMs;
        m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
        return;
    }

    if (nowMs - m_pendingAutoMainViewModeSinceMs >= kMainViewAutoSwitchDebounceMs) {
        m_effectiveMainViewMode = candidate;
        m_pendingAutoMainViewModeSinceMs = 0;
    }
    m_sceneManager->setMainViewMode(m_effectiveMainViewMode);
}

void MainWindow::refreshVisualization()
{
    const auto snapshot = m_dataManager->getSnapshot();
    updateMainViewMode(snapshot);
    m_sceneManager->updateScene(snapshot);
    m_controlPanel->updateSnapshot(snapshot);
    m_bottomStatusPanel->updateSnapshot(snapshot);
    updateMainViewOverlay(snapshot);
    updateMainViewDisplayDialog(snapshot);
    if (snapshot.runtimeStatus.inputSource != autoviz::datacenter::VisualizationInputSource::Mock) {
        statusBar()->showMessage(compactTopicStatusSummary(snapshot));
        statusBar()->setToolTip(topicStatusSummary(snapshot));
    }
}

void MainWindow::initializeMessageSubscriber()
{
    autoviz::ros::SubscribeBackend backend = autoviz::ros::SubscribeBackend::None;
#if AUTOVIZ_ENABLE_ROS1
    backend = autoviz::ros::SubscribeBackend::Ros1;
#elif AUTOVIZ_ENABLE_ROS2
    backend = autoviz::ros::SubscribeBackend::Ros2;
#endif

    switch (backend) {
    case autoviz::ros::SubscribeBackend::Ros1:
        Logger::instance().info("当前选择 ROS1 订阅实现。");
        break;
    case autoviz::ros::SubscribeBackend::Ros2:
        Logger::instance().info("当前选择 ROS2 订阅实现。");
        break;
    case autoviz::ros::SubscribeBackend::None:
    default:
        Logger::instance().info("当前未启用 ROS 编译开关，使用 DataManager 中的 Mock 数据。");
        break;
    }

    m_msgSubscriber = autoviz::ros::createRosMsgSubsrcribe(backend, m_dataManager);
    if (m_msgSubscriber == nullptr) {
        statusBar()->showMessage(tr("Mock 数据显示中"));
        return;
    }

    QString errorMessage;
    if (m_msgSubscriber != nullptr && !m_msgSubscriber->initialize(&errorMessage)) {
        Logger::instance().warning(errorMessage.isEmpty() ? QStringLiteral("订阅器初始化失败。") : errorMessage);
        return;
    }
    if (m_msgSubscriber != nullptr && !m_msgSubscriber->start(&errorMessage)) {
        Logger::instance().warning(errorMessage.isEmpty() ? QStringLiteral("订阅器启动失败。") : errorMessage);
        return;
    }

    if (m_msgSubscriber != nullptr) {
        const QString summary = m_msgSubscriber->statusSummary();
        statusBar()->setToolTip(summary);
        statusBar()->showMessage(summary.startsWith(QStringLiteral("ROS2 订阅中")) ? tr("ROS2 订阅中") : summary);
    }
}

void MainWindow::openMainViewDisplayConfigDialog()
{
    if (m_mainViewDisplayConfigDialog == nullptr) {
        m_mainViewDisplayConfigDialog = new MainViewDisplayConfigDialog(this);
        connect(m_mainViewDisplayConfigDialog,
                &MainViewDisplayConfigDialog::layerVisibilityChanged,
                this,
                [this](const autoviz::render::LayerVisibility& visibility) {
                    m_sceneManager->setLayerVisibility(visibility);
                    saveMainViewDisplaySettings();
                });
        connect(m_mainViewDisplayConfigDialog,
                &MainViewDisplayConfigDialog::vehicleCenteredModeChanged,
                this,
                [this](bool enabled) {
                    m_sceneManager->setVehicleCenteredMode(enabled);
                    saveMainViewDisplaySettings();
                });
    }

    const auto snapshot = m_dataManager->getSnapshot();
    m_mainViewDisplayConfigDialog->setLayerVisibility(m_sceneManager->layerVisibility());
    m_mainViewDisplayConfigDialog->setVehicleCenteredMode(m_sceneManager->vehicleCenteredMode());
    updateMainViewDisplayDialog(snapshot);
    m_mainViewDisplayConfigDialog->show();
    m_mainViewDisplayConfigDialog->raise();
    m_mainViewDisplayConfigDialog->activateWindow();
}

void MainWindow::updateMainViewOverlay(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    QStringList receivedChannels;
    const auto& status = snapshot.runtimeStatus;
    if (status.hasVehicleLocationData) {
        receivedChannels << QStringLiteral("location");
    }
    if (status.hasVehicleChassisData) {
        receivedChannels << QStringLiteral("chassis");
    }
    if (status.hasGlobalPathData) {
        receivedChannels << QStringLiteral("global_path");
    }
    if (status.hasLocalPathData) {
        receivedChannels << QStringLiteral("local_path");
    }
    if (status.hasReferenceLineData) {
        receivedChannels << QStringLiteral("reference_line");
    }
    if (status.hasObstacleData) {
        receivedChannels << QStringLiteral("obstacles");
    }
    if (status.hasControlCmdData) {
        receivedChannels << QStringLiteral("control_cmd");
    }

    QString overlayMessage;
    switch (status.inputSource) {
    case autoviz::datacenter::VisualizationInputSource::Ros1:
        overlayMessage = QStringLiteral("当前数据源：ROS1 实时数据");
        break;
    case autoviz::datacenter::VisualizationInputSource::Ros2:
        overlayMessage = QStringLiteral("当前数据源：ROS2 实时数据");
        break;
    case autoviz::datacenter::VisualizationInputSource::Mock:
    default:
        overlayMessage = QStringLiteral("当前数据源：内部 Mock 数据");
        break;
    }

    if (status.inputSource == autoviz::datacenter::VisualizationInputSource::Mock) {
        overlayMessage += QStringLiteral("\n当前状态：Mock 数据已加载");
    } else if (receivedChannels.isEmpty()) {
        overlayMessage += QStringLiteral("\n当前状态：等待实时数据");
    } else {
        overlayMessage += QStringLiteral("\n当前状态：已接收 %1 个通道").arg(receivedChannels.size());
    }
    overlayMessage += QStringLiteral("\n网格：细格 1m / 粗格 5m");
    overlayMessage += QStringLiteral("\n运行模式：%1").arg(autoviz::model::toDisplayString(snapshot.runVisualizationMode));
    overlayMessage += QStringLiteral("\n主视图：%1（%2）")
                          .arg(autoviz::render::toDisplayString(m_effectiveMainViewMode),
                               m_requestedMainViewMode == autoviz::render::MainViewMode::Auto ? QStringLiteral("自动") : QStringLiteral("手动"));

    m_visualizationView->setOverlayMessage(overlayMessage);
    m_visualizationView->setVerticalStatusMessage(QString());
}

void MainWindow::updateMainViewDisplayDialog(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (m_mainViewDisplayConfigDialog == nullptr) {
        return;
    }

    MainViewDataAvailability availability;
    availability.hasVehicleData = snapshot.runtimeStatus.hasVehicleLocationData;
    availability.hasHistoryTrailData = !snapshot.historyTrail.points.isEmpty();
    availability.hasGlobalPathData = snapshot.runtimeStatus.hasGlobalPathData;
    availability.hasReferenceLineData = snapshot.runtimeStatus.hasReferenceLineData;
    availability.hasLocalPathData = snapshot.runtimeStatus.hasLocalPathData;
    availability.hasObstacleData = snapshot.runtimeStatus.hasObstacleData;

    m_mainViewDisplayConfigDialog->setLayerVisibility(m_sceneManager->layerVisibility());
    m_mainViewDisplayConfigDialog->setVehicleCenteredMode(m_sceneManager->vehicleCenteredMode());
    m_mainViewDisplayConfigDialog->setDataAvailability(availability);
}

void MainWindow::loadMainViewDisplaySettings()
{
    QSettings settings(QStringLiteral("AutoViz"), QStringLiteral("AutoViz"));
    autoviz::render::LayerVisibility visibility;
    visibility.showVehicle = settings.value(QStringLiteral("main_view/show_vehicle"), true).toBool();
    visibility.showHistoryTrail = settings.value(QStringLiteral("main_view/show_history_trail"), true).toBool();
    visibility.showGlobalPath = settings.value(QStringLiteral("main_view/show_global_path"), true).toBool();
    visibility.showReferenceLine = settings.value(QStringLiteral("main_view/show_reference_line"), true).toBool();
    visibility.showLocalPath = settings.value(QStringLiteral("main_view/show_local_path"), true).toBool();
    visibility.showObstacles = settings.value(QStringLiteral("main_view/show_obstacles"), true).toBool();
    const bool vehicleCenteredMode = settings.value(QStringLiteral("main_view/vehicle_centered_mode"), true).toBool();

    m_sceneManager->setLayerVisibility(visibility);
    m_sceneManager->setVehicleCenteredMode(vehicleCenteredMode);
}

void MainWindow::saveMainViewDisplaySettings() const
{
    QSettings settings(QStringLiteral("AutoViz"), QStringLiteral("AutoViz"));
    const auto visibility = m_sceneManager->layerVisibility();
    settings.setValue(QStringLiteral("main_view/show_vehicle"), visibility.showVehicle);
    settings.setValue(QStringLiteral("main_view/show_history_trail"), visibility.showHistoryTrail);
    settings.setValue(QStringLiteral("main_view/show_global_path"), visibility.showGlobalPath);
    settings.setValue(QStringLiteral("main_view/show_reference_line"), visibility.showReferenceLine);
    settings.setValue(QStringLiteral("main_view/show_local_path"), visibility.showLocalPath);
    settings.setValue(QStringLiteral("main_view/show_obstacles"), visibility.showObstacles);
    settings.setValue(QStringLiteral("main_view/vehicle_centered_mode"), m_sceneManager->vehicleCenteredMode());
}
