#include "ui/MessagePackagePanel.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

using autoviz::ros::ManagedRosPackage;
using autoviz::ros::ManagedRosPackageList;
using autoviz::ros::PackageBuildStatus;
using autoviz::ros::PackageCopyStatus;
using autoviz::ros::RosEnvironmentInfo;
using autoviz::ros::RosVersion;

MessagePackagePanel::MessagePackagePanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    refreshHeader();
    refreshButtonState();
}

void MessagePackagePanel::setEnvironmentInfo(const RosEnvironmentInfo& environmentInfo, const QString& workspaceRoot)
{
    m_environmentInfo = environmentInfo;
    m_workspaceRoot = workspaceRoot;
    refreshHeader();
    refreshButtonState();
}

void MessagePackagePanel::setPackages(const ManagedRosPackageList& packages)
{
    m_packages = packages;

    QSignalBlocker blocker(m_table);
    m_table->setRowCount(0);
    for (int row = 0; row < m_packages.size(); ++row) {
        const ManagedRosPackage& package = m_packages.at(row);
        m_table->insertRow(row);

        auto* enabledItem = new QTableWidgetItem();
        enabledItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        enabledItem->setCheckState(package.enabled ? Qt::Checked : Qt::Unchecked);
        enabledItem->setData(Qt::UserRole, package.packageName);
        m_table->setItem(row, 0, enabledItem);

        auto* nameItem = new QTableWidgetItem(package.packageName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1, nameItem);

        auto* sourceItem = new QTableWidgetItem(package.sourcePath);
        sourceItem->setFlags(sourceItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 2, sourceItem);

        auto* workspaceItem = new QTableWidgetItem(package.workspacePath);
        workspaceItem->setFlags(workspaceItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 3, workspaceItem);

        auto* msgCountItem = new QTableWidgetItem(QString::number(package.msgCount));
        msgCountItem->setFlags(msgCountItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 4, msgCountItem);

        auto* rosCompatibilityItem = new QTableWidgetItem(package.rosCompatibility);
        rosCompatibilityItem->setFlags(rosCompatibilityItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 5, rosCompatibilityItem);

        auto* copyStatusItem = new QTableWidgetItem(autoviz::ros::toDisplayString(package.copyStatus));
        copyStatusItem->setFlags(copyStatusItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 6, copyStatusItem);

        auto* buildStatusItem = new QTableWidgetItem(autoviz::ros::toDisplayString(package.buildStatus));
        buildStatusItem->setFlags(buildStatusItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 7, buildStatusItem);
    }

    refreshButtonState();
}

void MessagePackagePanel::setBuildInProgress(bool inProgress)
{
    m_buildInProgress = inProgress;
    refreshButtonState();
}

QStringList MessagePackagePanel::selectedPackageNames() const
{
    QStringList packageNames;
    const QModelIndexList selectedRows = m_table->selectionModel()->selectedRows();
    for (const QModelIndex& selectedRow : selectedRows) {
        if (auto* item = m_table->item(selectedRow.row(), 1); item != nullptr) {
            packageNames.push_back(item->text());
        }
    }
    return packageNames;
}

void MessagePackagePanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("消息包管理面板"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    m_environmentLabel = new QLabel(this);
    m_environmentLabel->setWordWrap(true);
    layout->addWidget(m_environmentLabel);

    m_workspaceLabel = new QLabel(this);
    m_workspaceLabel->setWordWrap(true);
    layout->addWidget(m_workspaceLabel);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet("padding: 8px; background: #1f2a35; border-radius: 4px; color: #cfd8e3;");
    layout->addWidget(m_hintLabel);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    m_addButton = new QPushButton(tr("添加消息包"), this);
    m_removeButton = new QPushButton(tr("删除选中包"), this);
    m_buildButton = new QPushButton(tr("编译消息包"), this);

    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_buildButton);
    layout->addLayout(buttonLayout);

    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels(
        {tr("启用"), tr("包名"), tr("原始路径"), tr("工作区路径"), tr("消息数"), tr("ROS 适配"), tr("复制状态"), tr("编译状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

    connect(m_addButton, &QPushButton::clicked, this, &MessagePackagePanel::addPackagesRequested);
    connect(m_removeButton, &QPushButton::clicked, this, [this]() {
        emit removePackagesRequested(selectedPackageNames());
    });
    connect(m_buildButton, &QPushButton::clicked, this, &MessagePackagePanel::buildPackagesRequested);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() { refreshButtonState(); });
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item == nullptr || item->column() != 0) {
            return;
        }
        emit packageEnabledChanged(item->data(Qt::UserRole).toString(), item->checkState() == Qt::Checked);
    });
}

void MessagePackagePanel::refreshHeader()
{
    m_environmentLabel->setText(
        QStringLiteral("%1\nroscore 可用：%2，catkin_make：%3，colcon：%4")
            .arg(autoviz::ros::toEnvironmentSummary(m_environmentInfo))
            .arg(m_environmentInfo.hasRoscore ? QStringLiteral("是") : QStringLiteral("否"))
            .arg(m_environmentInfo.hasCatkinMake ? QStringLiteral("是") : QStringLiteral("否"))
            .arg(m_environmentInfo.hasColcon ? QStringLiteral("是") : QStringLiteral("否")));
    m_workspaceLabel->setText(
        m_workspaceRoot.isEmpty()
            ? QStringLiteral("当前工作区路径：未创建")
            : QStringLiteral("当前工作区路径：%1").arg(m_workspaceRoot));

    if (!m_environmentInfo.rosAvailable) {
        m_hintLabel->setText(QStringLiteral("当前未检测到 ROS 环境，无法编译消息包。请先在启动 AutoViz 前加载 ROS 环境。"));
        return;
    }

    if (m_environmentInfo.rosVersion == RosVersion::Ros1) {
        m_hintLabel->setText(QStringLiteral(
            "ROS1 提示：编译 ROS1 消息包通常不要求 roscore 正在运行，但后续订阅或运行节点时需要 roscore。"));
        return;
    }

    m_hintLabel->setText(QStringLiteral("请选择标准 ROS 消息包目录，复制到内部工作区后执行编译。"));
}

void MessagePackagePanel::refreshButtonState()
{
    const bool hasSelection = !selectedPackageNames().isEmpty();
    bool hasEnabledPackages = false;
    for (const auto& package : m_packages) {
        if (package.enabled) {
            hasEnabledPackages = true;
            break;
        }
    }
    const bool canBuild = m_environmentInfo.rosAvailable && hasEnabledPackages && !m_buildInProgress;

    m_addButton->setEnabled(!m_buildInProgress);
    m_removeButton->setEnabled(hasSelection && !m_buildInProgress);
    m_buildButton->setEnabled(canBuild);
    m_table->setEnabled(!m_buildInProgress);
}
