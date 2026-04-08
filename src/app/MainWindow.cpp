#include "app/MainWindow.h"

#include <QAction>
#include <QAbstractButton>
#include <QDockWidget>
#include <QFileDialog>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStatusBar>
#include <QTreeView>

#include "core/ros/RosEnvironmentDetector.h"
#include "core/ros/RosPackageBuilder.h"
#include "core/ros/RosPackageRegistry.h"
#include "core/ros/RosPackageValidator.h"
#include "core/ros/RosWorkspaceManager.h"
#include "core/render/SceneManager.h"
#include "ui/ControlStatusPanel.h"
#include "ui/DetailPanel.h"
#include "ui/LogPanel.h"
#include "ui/MessagePackagePanel.h"
#include "ui/VisualizationView.h"
#include "utils/Logger.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_packageRegistry = new autoviz::ros::RosPackageRegistry();
    m_packageBuilder = new autoviz::ros::RosPackageBuilder(this);
    m_workspaceManager = new autoviz::ros::RosWorkspaceManager();

    setupUi();

    Logger::instance().setLogHandler([this](const QString& message) {
        if (m_logPanel != nullptr) {
            m_logPanel->appendLog(message);
        }
    });

    connectActions();

    if (m_controlStatusPanel != nullptr) {
        m_controlStatusPanel->setPlaceholderData();
    }

    detectRosEnvironment();
    refreshUiState();
    Logger::instance().info("主窗口初始化完成。");
}

MainWindow::~MainWindow()
{
    Logger::instance().clearLogHandler();

    delete m_packageRegistry;
    delete m_workspaceManager;
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
    setupStatusBar();
    setupDocks();
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("文件"));
    m_loadRosPackageAction = fileMenu->addAction(tr("加载 ROS 消息包..."));
    m_removeRosPackageAction = fileMenu->addAction(tr("删除选中消息包"));
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction(tr("退出"));

    m_viewMenu = menuBar()->addMenu(tr("视图"));
    m_resetViewAction = m_viewMenu->addAction(tr("重置视图"));
    m_restoreLayoutAction = m_viewMenu->addAction(tr("恢复默认布局"));
    m_viewMenu->addSeparator();

    QMenu* helpMenu = menuBar()->addMenu(tr("帮助"));
    m_aboutAction = helpMenu->addAction(tr("关于"));
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("正在检测 ROS 环境..."));
}

void MainWindow::setupDocks()
{
    m_messagePackagePanel = new MessagePackagePanel(this);
    m_detailPanel = new DetailPanel(this);
    m_controlStatusPanel = new ControlStatusPanel(this);
    m_logPanel = new LogPanel(this);

    m_leftDock = new QDockWidget(tr("消息包管理面板"), this);
    m_leftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_leftDock->setWidget(m_messagePackagePanel);
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

    bindDockLogging(m_leftDock, QStringLiteral("消息包管理面板"));
    bindDockLogging(m_rightDock, QStringLiteral("对象详情面板"));
    bindDockLogging(m_controlDock, QStringLiteral("控制状态面板"));
    bindDockLogging(m_logDock, QStringLiteral("日志面板"));

    auto* topicDockAction = m_leftDock->toggleViewAction();
    topicDockAction->setText(tr("显示消息包管理面板"));
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
}

void MainWindow::connectActions()
{
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    connect(m_loadRosPackageAction, &QAction::triggered, this, [this]() { loadRosMsgPackages(); });
    connect(m_removeRosPackageAction, &QAction::triggered, this, [this]() {
        removeSelectedPackages(m_messagePackagePanel != nullptr ? m_messagePackagePanel->selectedPackageNames() : QStringList());
    });

    connect(m_resetViewAction, &QAction::triggered, this, [this]() {
        if (m_visualizationView != nullptr) {
            m_visualizationView->resetView();
            statusBar()->showMessage(tr("视图已重置"), 2000);
            Logger::instance().info("二维视图已重置。");
        }
    });

    connect(m_restoreLayoutAction, &QAction::triggered, this, [this]() { restoreDefaultLayout(); });

    connect(m_messagePackagePanel, &MessagePackagePanel::addPackagesRequested, this, [this]() { loadRosMsgPackages(); });
    connect(m_messagePackagePanel, &MessagePackagePanel::removePackagesRequested, this, [this](const QStringList& packageNames) {
        removeSelectedPackages(packageNames);
    });
    connect(m_messagePackagePanel, &MessagePackagePanel::buildPackagesRequested, this, [this]() { buildRosMsgPackages(); });
    connect(m_messagePackagePanel, &MessagePackagePanel::packageEnabledChanged, this, [this](const QString& packageName, bool enabled) {
        if (m_packageRegistry != nullptr) {
            m_packageRegistry->setPackageEnabled(packageName, enabled);
            refreshUiState();
        }
    });

    connect(m_packageBuilder, &autoviz::ros::RosPackageBuilder::buildStarted, this, [this](const QString& commandSummary) {
        if (m_messagePackagePanel != nullptr) {
            m_messagePackagePanel->setBuildInProgress(true);
        }
        Logger::instance().info("开始编译消息包。");
        Logger::instance().info(QStringLiteral("执行的构建命令：%1").arg(commandSummary));
        statusBar()->showMessage(tr("消息包编译中..."));
    });

    connect(m_packageBuilder, &autoviz::ros::RosPackageBuilder::buildOutput, this, [](const QString& text) {
        const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            Logger::instance().info(line);
        }
    });

    connect(m_packageBuilder, &autoviz::ros::RosPackageBuilder::buildFinished, this, [this](bool success, const QString& summary, const QString&) {
        if (m_messagePackagePanel != nullptr) {
            m_messagePackagePanel->setBuildInProgress(false);
        }

        if (m_packageRegistry != nullptr) {
            m_packageRegistry->setAllEnabledPackagesBuildStatus(
                success ? autoviz::ros::PackageBuildStatus::BuildSucceeded : autoviz::ros::PackageBuildStatus::BuildFailed,
                success ? QString() : summary);
        }

        refreshUiState();

        if (success) {
            Logger::instance().info("编译成功。");
            statusBar()->showMessage(tr("消息包编译成功"), 3000);
            return;
        }

        Logger::instance().error(summary);
        statusBar()->showMessage(tr("消息包编译失败"), 5000);
        QMessageBox::warning(this, tr("编译失败"), summary);
    });

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

void MainWindow::refreshUiState()
{
    const int packageCount = m_packageRegistry != nullptr ? m_packageRegistry->packageCount() : 0;
    const int compiledCount = m_packageRegistry != nullptr ? m_packageRegistry->compiledPackageCount() : 0;
    const auto environmentInfo = m_packageRegistry != nullptr ? m_packageRegistry->environmentInfo() : autoviz::ros::RosEnvironmentInfo();

    if (m_visualizationView != nullptr) {
        m_visualizationView->setWelcomeState(packageCount, compiledCount);
    }

    if (m_messagePackagePanel != nullptr && m_packageRegistry != nullptr) {
        m_messagePackagePanel->setEnvironmentInfo(environmentInfo, m_packageRegistry->workspaceRoot());
        m_messagePackagePanel->setPackages(m_packageRegistry->packages());
    }

    statusBar()->showMessage(
        packageCount > 0
            ? tr("%1，当前消息包：%2 个，已编译：%3 个").arg(autoviz::ros::toEnvironmentSummary(environmentInfo)).arg(packageCount).arg(compiledCount)
            : tr("%1").arg(autoviz::ros::toEnvironmentSummary(environmentInfo)));
    m_removeRosPackageAction->setEnabled(packageCount > 0);
}

void MainWindow::detectRosEnvironment()
{
    Logger::instance().info("检测 ROS 环境开始。");
    const auto environmentInfo = autoviz::ros::RosEnvironmentDetector::detect();
    m_packageRegistry->setEnvironmentInfo(environmentInfo);

    if (environmentInfo.rosAvailable) {
        Logger::instance().info(QStringLiteral("检测到 %1。").arg(
            environmentInfo.rosVersion == autoviz::ros::RosVersion::Ros1 ? QStringLiteral("ROS1") : QStringLiteral("ROS2")));
        Logger::instance().info(QStringLiteral("检测到 %1。").arg(autoviz::ros::toEnvironmentSummary(environmentInfo)));
        if (!environmentInfo.errorMessage.isEmpty()) {
            Logger::instance().warning(environmentInfo.errorMessage);
        }
        initializeWorkspace();
    } else {
        Logger::instance().warning("未检测到 ROS 环境。");
        if (!environmentInfo.errorMessage.isEmpty()) {
            Logger::instance().warning(environmentInfo.errorMessage);
        }
        statusBar()->showMessage(tr("未检测到 ROS 环境"));
    }
}

QStringList MainWindow::selectPackageDirectories() const
{
    QFileDialog dialog(const_cast<MainWindow*>(this), tr("选择 ROS 消息包目录"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);

    for (auto* view : dialog.findChildren<QListView*>()) {
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    for (auto* view : dialog.findChildren<QTreeView*>()) {
        view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }

    return dialog.selectedFiles();
}

bool MainWindow::initializeWorkspace()
{
    QString workspaceRoot;
    QString errorMessage;
    if (!m_workspaceManager->initializeWorkspace(m_packageRegistry->environmentInfo(), &workspaceRoot, &errorMessage)) {
        Logger::instance().error(QStringLiteral("工作区初始化失败：%1").arg(errorMessage));
        return false;
    }

    m_packageRegistry->setWorkspaceRoot(workspaceRoot);
    Logger::instance().info(QStringLiteral("工作区初始化完成：%1").arg(workspaceRoot));
    return true;
}

void MainWindow::loadRosMsgPackages()
{
    Logger::instance().info("加载消息包目录。");

    const QStringList packageDirectories = selectPackageDirectories();
    if (packageDirectories.isEmpty()) {
        return;
    }

    if (!m_packageRegistry->environmentInfo().rosAvailable) {
        QMessageBox::warning(this, tr("未检测到 ROS"), tr("当前未检测到 ROS 环境，无法复制消息包到内部工作区。"));
        Logger::instance().warning("未检测到 ROS 环境，无法加载消息包到工作区。");
        return;
    }

    if (!initializeWorkspace()) {
        QMessageBox::warning(this, tr("工作区初始化失败"), tr("无法初始化 AutoViz 内部工作区，请查看日志。"));
        return;
    }

    for (const QString& packageDirectory : packageDirectories) {
        addPackageFromDirectory(packageDirectory);
    }

    refreshUiState();
}

bool MainWindow::addPackageFromDirectory(const QString& packageDirectory)
{
    Logger::instance().info(QStringLiteral("加载消息包目录：%1").arg(packageDirectory));

    const auto validationResult = autoviz::ros::RosPackageValidator::validate(packageDirectory);
    if (!validationResult.isValid) {
        Logger::instance().error(QStringLiteral("校验消息包失败：%1，原因：%2").arg(packageDirectory, validationResult.errorMessage));
        QMessageBox::warning(this, tr("消息包无效"), tr("目录无效：%1\n原因：%2").arg(packageDirectory, validationResult.errorMessage));
        return false;
    }

    Logger::instance().info(QStringLiteral("检测到 package.xml：%1").arg(validationResult.packageXmlPath));
    Logger::instance().info(QStringLiteral("检测到 CMakeLists.txt：%1").arg(validationResult.cmakeListsPath));
    Logger::instance().info(QStringLiteral("检测到 msg 目录：%1").arg(validationResult.msgDirectoryPath));
    Logger::instance().info(QStringLiteral("检测到 .msg 文件数量：%1").arg(validationResult.msgCount));
    Logger::instance().info(QStringLiteral("校验消息包成功：%1").arg(validationResult.packageName));

    bool overwriteExisting = false;
    if (m_workspaceManager->packageExists(m_packageRegistry->environmentInfo(), validationResult.packageName)) {
        const auto answer = QMessageBox::question(
            this,
            tr("覆盖确认"),
            tr("工作区中已存在同名消息包“%1”，是否覆盖？").arg(validationResult.packageName),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            Logger::instance().warning(QStringLiteral("用户取消覆盖：%1").arg(validationResult.packageName));
            return false;
        }
        overwriteExisting = true;
    }

    QString workspacePackagePath;
    QString errorMessage;
    if (!m_workspaceManager->copyPackageToWorkspace(
            m_packageRegistry->environmentInfo(),
            validationResult,
            overwriteExisting,
            &workspacePackagePath,
            &errorMessage)) {
        Logger::instance().error(QStringLiteral("复制消息包到工作区失败：%1").arg(errorMessage));
        QMessageBox::warning(this, tr("复制失败"), tr("复制消息包到工作区失败：%1").arg(errorMessage));
        return false;
    }

    Logger::instance().info(QStringLiteral("复制消息包到工作区成功：%1").arg(workspacePackagePath));

    autoviz::ros::ManagedRosPackage package;
    package.packageName = validationResult.packageName;
    package.sourcePath = validationResult.packagePath;
    package.workspacePath = workspacePackagePath;
    package.msgCount = validationResult.msgCount;
    package.rosCompatibility = QStringLiteral("匹配当前%1环境").arg(
        autoviz::ros::toDisplayString(m_packageRegistry->environmentInfo().rosVersion));
    package.copyStatus = autoviz::ros::PackageCopyStatus::Copied;
    package.buildStatus = autoviz::ros::PackageBuildStatus::NotBuilt;
    m_packageRegistry->upsertPackage(package);

    return true;
}

void MainWindow::removeSelectedPackages(const QStringList& packageNames)
{
    if (packageNames.isEmpty()) {
        Logger::instance().warning("未选择需要删除的消息包。");
        return;
    }

    QMessageBox confirmBox(this);
    confirmBox.setIcon(QMessageBox::Question);
    confirmBox.setWindowTitle(tr("删除消息包"));
    confirmBox.setText(tr("是否删除选中的消息包？"));
    auto* deleteWorkspaceButton = confirmBox.addButton(tr("删除列表和工作区副本"), QMessageBox::YesRole);
    auto* removeOnlyButton = confirmBox.addButton(tr("仅从列表移除"), QMessageBox::NoRole);
    auto* cancelButton = confirmBox.addButton(QMessageBox::Cancel);
    confirmBox.exec();

    if (confirmBox.clickedButton() == static_cast<QAbstractButton*>(cancelButton)) {
        return;
    }

    const bool deleteWorkspaceCopy =
        confirmBox.clickedButton() == static_cast<QAbstractButton*>(deleteWorkspaceButton);
    Q_UNUSED(removeOnlyButton);

    for (const QString& packageName : packageNames) {
        if (deleteWorkspaceCopy) {
            QString errorMessage;
            if (!m_workspaceManager->removePackageFromWorkspace(m_packageRegistry->environmentInfo(), packageName, &errorMessage)) {
                Logger::instance().error(errorMessage);
                QMessageBox::warning(this, tr("删除失败"), errorMessage);
                continue;
            }
        }

        if (m_packageRegistry->removePackage(packageName)) {
            Logger::instance().info(QStringLiteral("删除消息包：%1").arg(packageName));
        }
    }

    refreshUiState();
}

void MainWindow::buildRosMsgPackages()
{
    if (!m_packageRegistry->environmentInfo().rosAvailable) {
        Logger::instance().warning("未检测到 ROS 环境，编译按钮已禁用。");
        QMessageBox::warning(this, tr("无法编译"), tr("当前未检测到 ROS 环境，无法编译消息包。"));
        return;
    }

    QStringList enabledPackageNames;
    for (const auto& package : m_packageRegistry->packages()) {
        if (package.enabled) {
            enabledPackageNames.push_back(package.packageName);
        }
    }

    if (enabledPackageNames.isEmpty()) {
        Logger::instance().warning("没有可编译的已启用消息包。");
        QMessageBox::warning(this, tr("无法编译"), tr("请至少启用一个消息包后再执行编译。"));
        return;
    }

    m_packageRegistry->setAllEnabledPackagesBuildStatus(autoviz::ros::PackageBuildStatus::Building);
    refreshUiState();

    QString errorMessage;
    if (!m_packageBuilder->startBuild(
            m_packageRegistry->environmentInfo(),
            m_packageRegistry->workspaceRoot(),
            enabledPackageNames,
            &errorMessage)) {
        m_packageRegistry->setAllEnabledPackagesBuildStatus(autoviz::ros::PackageBuildStatus::BuildFailed, errorMessage);
        refreshUiState();
        Logger::instance().error(QStringLiteral("开始编译消息包失败：%1").arg(errorMessage));
        QMessageBox::warning(this, tr("编译失败"), errorMessage);
    }
}

void MainWindow::bindDockLogging(QDockWidget* dock, const QString& panelName)
{
    connect(dock, &QDockWidget::visibilityChanged, this, [panelName](bool visible) {
        Logger::instance().info(
            QStringLiteral("%1已%2。").arg(panelName, visible ? QStringLiteral("打开") : QStringLiteral("关闭")));
    });
}
