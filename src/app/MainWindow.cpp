#include "app/MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>

#include "core/datacenter/DataManager.h"
#include "core/render/SceneManager.h"
#include "core/ros/RosMsgSubscribeBase.h"
#include "core/ros/RosMsgSubsrcribeFactory.h"
#include "ui/ControlStatusPanel.h"
#include "ui/DisplayControlPanel.h"
#include "ui/LogPanel.h"
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
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
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
    m_displayControlPanel = new DisplayControlPanel(this);
    m_controlStatusPanel = new ControlStatusPanel(this);
    m_chartPanel = new ChartPanel(this);
    m_logPanel = new LogPanel(this);

    m_rightDock = new QDockWidget(tr("显示控制面板"), this);
    m_rightDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_rightDock->setWidget(m_displayControlPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);

    m_controlDock = new QDockWidget(tr("控制状态面板"), this);
    m_controlDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_controlDock->setWidget(m_controlStatusPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_controlDock);

    m_chartDock = new QDockWidget(tr("控制曲线面板"), this);
    m_chartDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_chartDock->setWidget(m_chartPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_chartDock);

    m_logDock = new QDockWidget(tr("日志面板"), this);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_logDock->setWidget(m_logPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    tabifyDockWidget(m_controlDock, m_chartDock);
    tabifyDockWidget(m_chartDock, m_logDock);
    m_controlDock->raise();
    m_logDock->hide();

    for (auto* dock : {m_rightDock, m_controlDock, m_chartDock, m_logDock}) {
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    }

    auto* displayDockAction = m_rightDock->toggleViewAction();
    displayDockAction->setText(tr("显示显示控制面板"));
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(displayDockAction);

    auto* controlDockAction = m_controlDock->toggleViewAction();
    controlDockAction->setText(tr("显示控制状态面板"));
    m_viewMenu->addAction(controlDockAction);

    auto* chartDockAction = m_chartDock->toggleViewAction();
    chartDockAction->setText(tr("显示控制曲线面板"));
    m_viewMenu->addAction(chartDockAction);

    auto* logDockAction = m_logDock->toggleViewAction();
    logDockAction->setText(tr("显示日志面板"));
    m_viewMenu->addAction(logDockAction);

    bindDockLogging(m_rightDock, QStringLiteral("显示控制面板"));
    bindDockLogging(m_controlDock, QStringLiteral("控制状态面板"));
    bindDockLogging(m_chartDock, QStringLiteral("控制曲线面板"));
    bindDockLogging(m_logDock, QStringLiteral("日志面板"));
    restoreDefaultLayout();
}

void MainWindow::connectActions()
{
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

    connect(m_displayControlPanel,
            &DisplayControlPanel::layerVisibilityChanged,
            this,
            [this](const autoviz::render::LayerVisibility& visibility) { m_sceneManager->setLayerVisibility(visibility); });
    connect(m_displayControlPanel,
            &DisplayControlPanel::vehicleCenteredModeChanged,
            this,
            [this](bool enabled) { m_sceneManager->setVehicleCenteredMode(enabled); });
    m_sceneManager->setVehicleCenteredMode(m_displayControlPanel->vehicleCenteredMode());
}

void MainWindow::restoreDefaultLayout()
{
    m_rightDock->show();
    m_controlDock->show();
    m_chartDock->show();
    m_logDock->hide();

    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_controlDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_chartDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    tabifyDockWidget(m_controlDock, m_chartDock);
    tabifyDockWidget(m_chartDock, m_logDock);
    m_controlDock->raise();

    resizeDocks({m_rightDock}, {320}, Qt::Horizontal);
    resizeDocks({m_controlDock}, {220}, Qt::Vertical);
}

void MainWindow::refreshVisualization()
{
    const auto snapshot = m_dataManager->getSnapshot();
    m_sceneManager->updateScene(snapshot);
    m_controlStatusPanel->setData(snapshot.vehicleState, snapshot.controlCmd);
}

void MainWindow::initializeMessageSubscriber()
{
    autoviz::ros::SubscribeBackend backend = autoviz::ros::SubscribeBackend::None;
#if defined(AUTOVIZ_ENABLE_ROS1)
    backend = autoviz::ros::SubscribeBackend::Ros1;
#elif defined(AUTOVIZ_ENABLE_ROS2)
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

    m_msgSubscriber->resetVisualizationData();

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
