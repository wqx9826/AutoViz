#include "app/MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QStatusBar>
#include <QTimer>

#include "core/datacenter/DataManager.h"
#include "core/render/SceneManager.h"
#include "core/ros/RosMsgSubscribeBase.h"
#include "core/ros/RosMsgSubsrcribeFactory.h"
#include "ui/LogPanel.h"
#include "ui/MainViewDisplayConfigDialog.h"
#include "ui/VisualizationView.h"
#include "ui/charts/ChartPanel.h"
#include "utils/Logger.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_dataManager = new autoviz::datacenter::DataManager();
    setupUi();

    Logger::instance().setLogHandler([this](const QString& message) {
        if (m_logPanel != nullptr) {
            m_logPanel->appendLog(message);
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
    resize(1460, 920);

    m_visualizationView = new VisualizationView(this);
    m_sceneManager = new autoviz::render::SceneManager(m_visualizationView, this);
    m_sceneManager->initializeScene();
    setCentralWidget(m_visualizationView);

    setupMenuBar();
    setupStatusBar();
    setupDocks();
    loadMainViewDisplaySettings();
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
    m_mainViewDisplayManageAction = fileMenu->addAction(tr("主视图显示管理"));
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction(tr("退出"));

    m_viewMenu = menuBar()->addMenu(tr("视图"));
    m_resetViewAction = m_viewMenu->addAction(tr("重置视图"));
    m_restoreLayoutAction = m_viewMenu->addAction(tr("恢复默认布局"));

    QMenu* helpMenu = menuBar()->addMenu(tr("帮助"));
    m_aboutAction = helpMenu->addAction(tr("关于"));
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("消息订阅初始化中"));
}

void MainWindow::setupDocks()
{
    m_chartPanel = new ChartPanel(this);
    m_logPanel = new LogPanel(this);

    m_chartDock = new QDockWidget(tr("控制曲线面板"), this);
    m_chartDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_chartDock->setWidget(m_chartPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_chartDock);

    m_logDock = new QDockWidget(tr("日志面板"), this);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_logDock->setWidget(m_logPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    tabifyDockWidget(m_chartDock, m_logDock);
    m_chartDock->raise();
    m_logDock->hide();

    for (auto* dock : {m_chartDock, m_logDock}) {
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    }

    auto* chartDockAction = m_chartDock->toggleViewAction();
    chartDockAction->setText(tr("显示控制曲线面板"));
    m_viewMenu->addAction(chartDockAction);

    auto* logDockAction = m_logDock->toggleViewAction();
    logDockAction->setText(tr("显示日志面板"));
    m_viewMenu->addAction(logDockAction);

    bindDockLogging(m_chartDock, QStringLiteral("控制曲线面板"));
    bindDockLogging(m_logDock, QStringLiteral("日志面板"));
    restoreDefaultLayout();
}

void MainWindow::connectActions()
{
    connect(m_mainViewDisplayManageAction, &QAction::triggered, this, &MainWindow::openMainViewDisplayConfigDialog);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_resetViewAction, &QAction::triggered, this, [this]() {
        m_visualizationView->resetView();
        statusBar()->showMessage(tr("视图已重置"), 2000);
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
    m_chartDock->show();
    m_logDock->hide();

    addDockWidget(Qt::BottomDockWidgetArea, m_chartDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    tabifyDockWidget(m_chartDock, m_logDock);
    m_chartDock->raise();
    resizeDocks({m_chartDock}, {220}, Qt::Vertical);
}

void MainWindow::refreshVisualization()
{
    const auto snapshot = m_dataManager->getSnapshot();
    m_sceneManager->updateScene(snapshot);
    updateMainViewOverlay(snapshot);
    updateMainViewDisplayDialog(snapshot);
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
        statusBar()->showMessage(m_msgSubscriber->statusSummary());
    }
}

void MainWindow::bindDockLogging(QDockWidget* dock, const QString& panelName)
{
    connect(dock, &QDockWidget::visibilityChanged, this, [panelName](bool visible) {
        Logger::instance().info(QStringLiteral("%1已%2。").arg(panelName, visible ? QStringLiteral("打开") : QStringLiteral("关闭")));
    });
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
        overlayMessage += QStringLiteral("\n当前已加载：vehicle / global_path / local_path / reference_line / obstacles");
    } else if (receivedChannels.isEmpty()) {
        overlayMessage += QStringLiteral("\n当前状态：等待实时数据");
    } else {
        overlayMessage += QStringLiteral("\n当前已接收：") + receivedChannels.join(QStringLiteral(" / "));
    }
    overlayMessage += QStringLiteral("\n网格：细格 1m / 粗格 5m");

    m_visualizationView->setOverlayMessage(overlayMessage);
}

void MainWindow::updateMainViewDisplayDialog(const autoviz::datacenter::VisualizationSnapshot& snapshot)
{
    if (m_mainViewDisplayConfigDialog == nullptr) {
        return;
    }

    MainViewDataAvailability availability;
    availability.hasVehicleData = snapshot.runtimeStatus.hasVehicleLocationData || snapshot.runtimeStatus.hasVehicleChassisData;
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
    settings.setValue(QStringLiteral("main_view/show_global_path"), visibility.showGlobalPath);
    settings.setValue(QStringLiteral("main_view/show_reference_line"), visibility.showReferenceLine);
    settings.setValue(QStringLiteral("main_view/show_local_path"), visibility.showLocalPath);
    settings.setValue(QStringLiteral("main_view/show_obstacles"), visibility.showObstacles);
    settings.setValue(QStringLiteral("main_view/vehicle_centered_mode"), m_sceneManager->vehicleCenteredMode());
}
