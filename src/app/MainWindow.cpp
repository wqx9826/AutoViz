#include "app/MainWindow.h"

#include <QAction>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>

#include "core/datasource/MsgCompiler.h"
#include "core/datasource/RosMsgPackageRegistry.h"
#include "core/render/SceneManager.h"
#include "ui/ControlStatusPanel.h"
#include "ui/DetailPanel.h"
#include "ui/LogPanel.h"
#include "ui/TopicConfigPanel.h"
#include "ui/VisualizationView.h"
#include "utils/Logger.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_packageRegistry = new autoviz::datasource::RosMsgPackageRegistry();
    m_msgCompiler = new autoviz::datasource::MsgCompiler();

    setupUi();
    connectActions();

    Logger::instance().setLogHandler([this](const QString& message) {
        if (m_logPanel != nullptr) {
            m_logPanel->appendLog(message);
        }
    });

    if (m_controlStatusPanel != nullptr) {
        m_controlStatusPanel->setPlaceholderData();
    }

    updatePackageUiState();
    Logger::instance().info("未加载消息包的欢迎状态已建立。");
    Logger::instance().info("主窗口初始化完成。");
}

MainWindow::~MainWindow()
{
    Logger::instance().clearLogHandler();

    delete m_msgCompiler;
    delete m_packageRegistry;
}

void MainWindow::setupUi()
{
    setWindowTitle("AutoViz");
    resize(1440, 900);

    m_visualizationView = new VisualizationView(this);
    m_sceneManager = new SceneManager(m_visualizationView, this);
    m_sceneManager->initializeScene();
    setCentralWidget(m_visualizationView);

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDocks();
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
    m_loadRosPackageAction = fileMenu->addAction(tr("加载 ROS 消息包..."));
    m_closeRosPackageAction = fileMenu->addAction(tr("关闭当前消息包"));
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction(tr("退出"));

    m_viewMenu = menuBar()->addMenu(tr("视图"));
    m_resetViewAction = m_viewMenu->addAction(tr("重置视图"));
    m_restoreLayoutAction = m_viewMenu->addAction(tr("恢复默认布局"));
    m_viewMenu->addSeparator();

    QMenu* helpMenu = menuBar()->addMenu(tr("帮助"));
    m_aboutAction = helpMenu->addAction(tr("关于"));
}

void MainWindow::setupToolBar()
{
    m_mainToolBar = addToolBar(tr("主工具栏"));
    m_mainToolBar->setMovable(false);
    m_mainToolBar->addAction(m_loadRosPackageAction);
    m_mainToolBar->addAction(m_resetViewAction);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("未加载消息包"));
}

void MainWindow::setupDocks()
{
    m_topicConfigPanel = new TopicConfigPanel(m_packageRegistry, this);
    m_detailPanel = new DetailPanel(this);
    m_controlStatusPanel = new ControlStatusPanel(this);
    m_logPanel = new LogPanel(this);

    m_leftDock = new QDockWidget(tr("话题配置面板"), this);
    m_leftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_leftDock->setWidget(m_topicConfigPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);

    m_rightDock = new QDockWidget(tr("对象详情面板"), this);
    m_rightDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_rightDock->setWidget(m_detailPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);

    m_controlDock = new QDockWidget(tr("控制状态面板"), this);
    m_controlDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_controlDock->setWidget(m_controlStatusPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_controlDock);

    m_logDock = new QDockWidget(tr("日志面板"), this);
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    m_logDock->setWidget(m_logPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    splitDockWidget(m_controlDock, m_logDock, Qt::Vertical);

    m_leftDock->setMinimumWidth(320);
    m_rightDock->setMinimumWidth(280);
    m_controlDock->setMinimumHeight(130);
    m_logDock->setMinimumHeight(80);

    bindDockLogging(m_leftDock, QStringLiteral("话题配置面板"));
    bindDockLogging(m_rightDock, QStringLiteral("对象详情面板"));
    bindDockLogging(m_controlDock, QStringLiteral("控制状态面板"));
    bindDockLogging(m_logDock, QStringLiteral("日志面板"));

    auto* topicDockAction = m_leftDock->toggleViewAction();
    topicDockAction->setText(tr("显示话题配置面板"));
    m_viewMenu->addAction(topicDockAction);

    auto* detailDockAction = m_rightDock->toggleViewAction();
    detailDockAction->setText(tr("显示对象详情面板"));
    m_viewMenu->addAction(detailDockAction);

    auto* controlDockAction = m_controlDock->toggleViewAction();
    controlDockAction->setText(tr("显示控制状态面板"));
    m_viewMenu->addAction(controlDockAction);

    auto* logDockAction = m_logDock->toggleViewAction();
    logDockAction->setText(tr("显示日志面板"));
    m_viewMenu->addAction(logDockAction);

    restoreDefaultLayout();

    m_leftDock->hide();
    m_rightDock->hide();
}

void MainWindow::connectActions()
{
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    connect(m_loadRosPackageAction, &QAction::triggered, this, [this]() { loadRosMsgPackage(); });
    connect(m_closeRosPackageAction, &QAction::triggered, this, [this]() { unloadCurrentPackage(); });

    connect(m_resetViewAction, &QAction::triggered, this, [this]() {
        if (m_visualizationView != nullptr) {
            m_visualizationView->resetView();
            statusBar()->showMessage(tr("视图已重置"), 2000);
            Logger::instance().info("二维视图已重置。");
        }
    });

    connect(m_restoreLayoutAction, &QAction::triggered, this, [this]() { restoreDefaultLayout(); });

    connect(m_aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(
            this,
            tr("关于 AutoViz"),
            tr("AutoViz\n\n用于无人车消息可视化的 Qt5 桌面工程骨架。"));
    });
}

void MainWindow::restoreDefaultLayout()
{
    m_leftDock->show();
    m_rightDock->show();
    m_controlDock->show();
    m_logDock->show();

    addDockWidget(Qt::LeftDockWidgetArea, m_leftDock);
    addDockWidget(Qt::RightDockWidgetArea, m_rightDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_controlDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    splitDockWidget(m_controlDock, m_logDock, Qt::Vertical);

    resizeDocks({m_leftDock, m_rightDock}, {300, 280}, Qt::Horizontal);
    resizeDocks({m_controlDock, m_logDock}, {170, 70}, Qt::Vertical);

    Logger::instance().info("已恢复默认布局。");
}

void MainWindow::updatePackageUiState()
{
    const bool hasPackage = m_packageRegistry != nullptr && m_packageRegistry->hasLoadedPackage();
    const auto* package = hasPackage ? m_packageRegistry->currentPackage() : nullptr;

    if (m_visualizationView != nullptr) {
        m_visualizationView->setWelcomeState(hasPackage, package != nullptr ? package->packageName : QString());
    }

    if (m_topicConfigPanel != nullptr) {
        m_topicConfigPanel->refreshPackageState();
    }

    statusBar()->showMessage(
        hasPackage
            ? tr("当前消息包：%1").arg(package->packageName)
            : tr("未加载消息包"));

    m_closeRosPackageAction->setEnabled(hasPackage);
}

void MainWindow::loadRosMsgPackage()
{
    Logger::instance().info("打开“加载 ROS 消息包”对话框。");

    const QString packageDir = QFileDialog::getExistingDirectory(
        this,
        tr("选择 ROS 消息包目录"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (packageDir.isEmpty()) {
        return;
    }

    if (!m_packageRegistry->loadPackage(packageDir)) {
        const QString error = m_packageRegistry->lastError();
        Logger::instance().error(QStringLiteral("消息包扫描失败：%1").arg(error));
        QMessageBox::warning(this, tr("加载失败"), tr("加载 ROS 消息包失败：%1").arg(error));
        return;
    }

    const auto* package = m_packageRegistry->currentPackage();
    if (package == nullptr) {
        return;
    }

    Logger::instance().info(QStringLiteral("检测到 package.xml：%1").arg(package->packageXmlPath));
    Logger::instance().info(QStringLiteral("检测到 msg 目录：%1").arg(package->msgDirectoryPath));
    Logger::instance().info(QStringLiteral("已加载 ROS 消息包：%1").arg(package->packageName));
    Logger::instance().info(QStringLiteral("包路径：%1").arg(package->packagePath));
    Logger::instance().info(QStringLiteral("扫描到的 msg 数量：%1").arg(package->msgCount()));

    m_topicConfigPanel->clearTopicConfigs();
    updatePackageUiState();
    m_leftDock->show();

    m_msgCompiler->compilePackage(*package);
    Logger::instance().warning(m_msgCompiler->lastError());
}

void MainWindow::unloadCurrentPackage()
{
    if (m_packageRegistry == nullptr || !m_packageRegistry->hasLoadedPackage()) {
        return;
    }

    const auto* package = m_packageRegistry->currentPackage();
    const QString packageName = package != nullptr ? package->packageName : QStringLiteral("未知包");

    m_packageRegistry->unloadCurrentPackage();
    if (m_topicConfigPanel != nullptr) {
        m_topicConfigPanel->clearTopicConfigs();
    }
    updatePackageUiState();
    m_leftDock->hide();

    Logger::instance().info(QStringLiteral("已关闭当前消息包：%1").arg(packageName));
}

void MainWindow::bindDockLogging(QDockWidget* dock, const QString& panelName)
{
    connect(dock, &QDockWidget::visibilityChanged, this, [panelName](bool visible) {
        Logger::instance().info(
            QStringLiteral("%1已%2。").arg(panelName, visible ? QStringLiteral("打开") : QStringLiteral("关闭")));
    });
}
