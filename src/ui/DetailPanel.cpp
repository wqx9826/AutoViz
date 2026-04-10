#include "ui/DetailPanel.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

DetailPanel::DetailPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
}

void DetailPanel::setSummary(
    const autoviz::ros::RosEnvironmentInfo& environmentInfo,
    const autoviz::ros::ManagedRosPackageList& packages)
{
    if (m_treeWidget == nullptr || m_rosPackageSection == nullptr) {
        return;
    }

    int compiledPackageCount = 0;
    int totalMsgCount = 0;
    for (const auto& package : packages) {
        totalMsgCount += package.msgCount;
        if (package.buildStatus == autoviz::ros::PackageBuildStatus::BuildSucceeded) {
            ++compiledPackageCount;
        }
    }

    updateSectionItem(
        m_rosPackageSection,
        QStringLiteral("当前状态"),
        packages.isEmpty() ? QStringLiteral("未加载自定义消息包，使用默认配置") : QStringLiteral("已加载自定义消息包"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("当前 ROS 环境"), simplifyEnvironmentValue(environmentInfo));
    updateSectionItem(m_rosPackageSection, QStringLiteral("已加载消息包数量"), QString::number(packages.size()));
    updateSectionItem(m_rosPackageSection, QStringLiteral("已编译成功包数量"), QString::number(compiledPackageCount));
    updateSectionItem(m_rosPackageSection, QStringLiteral("当前消息定义总数"), QString::number(totalMsgCount));
    updateSectionItem(m_rosPackageSection, QStringLiteral("已加载包名"), summarizePackageNames(packages));
    m_rosPackageSection->setExpanded(true);
}

void DetailPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(0);

    setAutoFillBackground(true);
    setStyleSheet("background: #f3f4f6;");

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setColumnCount(2);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setItemsExpandable(true);
    m_treeWidget->setIndentation(16);
    m_treeWidget->setUniformRowHeights(false);
    m_treeWidget->setAlternatingRowColors(false);
    m_treeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_treeWidget->setFocusPolicy(Qt::NoFocus);
    m_treeWidget->setExpandsOnDoubleClick(true);
    m_treeWidget->header()->setStretchLastSection(true);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_treeWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_treeWidget->setStyleSheet(
        "QTreeWidget { background: #f3f4f6; color: #20262d; border: none; }"
        "QTreeWidget::item { padding: 4px 0px; }"
        "QTreeWidget::branch { background: transparent; }");
    connect(m_treeWidget, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item) {
        if (auto* button = m_sectionButtons.value(item, nullptr); button != nullptr) {
            button->setArrowType(Qt::DownArrow);
        }
    });
    connect(m_treeWidget, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item) {
        if (auto* button = m_sectionButtons.value(item, nullptr); button != nullptr) {
            button->setArrowType(Qt::RightArrow);
        }
    });
    layout->addWidget(m_treeWidget, 1);
    setMinimumWidth(300);
    m_treeWidget->setColumnWidth(0, 150);

    m_rosPackageSection = addSection(QStringLiteral("ROS 包状态"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("当前状态"), QStringLiteral("未加载自定义消息包，使用默认配置"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("当前 ROS 环境"), QStringLiteral("未检测到"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("已加载消息包数量"), QStringLiteral("0"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("已编译成功包数量"), QStringLiteral("0"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("当前消息定义总数"), QStringLiteral("0"));
    updateSectionItem(m_rosPackageSection, QStringLiteral("已加载包名"), QStringLiteral("默认配置"));
    m_rosPackageSection->setExpanded(true);
}

QString DetailPanel::summarizePackageNames(const autoviz::ros::ManagedRosPackageList& packages)
{
    if (packages.isEmpty()) {
        return QStringLiteral("默认配置");
    }

    QStringList names;
    for (int index = 0; index < packages.size() && index < 4; ++index) {
        names.push_back(packages.at(index).packageName);
    }
    if (packages.size() > 4) {
        names.push_back(QStringLiteral("等 %1 个").arg(packages.size()));
    }
    return names.join(QStringLiteral("、"));
}

QString DetailPanel::simplifyEnvironmentValue(const autoviz::ros::RosEnvironmentInfo& environmentInfo)
{
    if (!environmentInfo.rosAvailable) {
        return QStringLiteral("未检测到");
    }

    const QString versionText = autoviz::ros::toDisplayString(environmentInfo.rosVersion);
    if (environmentInfo.rosDistro.isEmpty()) {
        return versionText;
    }

    return QStringLiteral("%1 / %2").arg(versionText, environmentInfo.rosDistro);
}

QTreeWidgetItem* DetailPanel::addSection(const QString& title)
{
    if (m_treeWidget == nullptr) {
        return nullptr;
    }

    auto* sectionItem = new QTreeWidgetItem(m_treeWidget);
    sectionItem->setText(0, title);
    sectionItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    sectionItem->setExpanded(true);
    sectionItem->setFlags(sectionItem->flags() & ~Qt::ItemIsSelectable);

    auto* headerWidget = new QWidget(m_treeWidget);
    headerWidget->setMinimumWidth(150);
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(4);

    auto* expandButton = new QToolButton(headerWidget);
    expandButton->setAutoRaise(true);
    expandButton->setArrowType(Qt::DownArrow);
    expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    expandButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");

    auto* titleLabel = new QLabel(title, headerWidget);
    QFont font = titleLabel->font();
    font.setBold(true);
    font.setPointSize(11);
    titleLabel->setFont(font);
    titleLabel->setStyleSheet("color: #2f363d;");

    headerLayout->addWidget(expandButton, 0);
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addStretch(1);

    m_treeWidget->setItemWidget(sectionItem, 0, headerWidget);
    m_sectionButtons.insert(sectionItem, expandButton);

    connect(expandButton, &QToolButton::clicked, this, [this, sectionItem]() {
        sectionItem->setExpanded(!sectionItem->isExpanded());
    });
    return sectionItem;
}

void DetailPanel::updateSectionItem(QTreeWidgetItem* section, const QString& key, const QString& value)
{
    if (section == nullptr) {
        return;
    }

    QHash<QString, QTreeWidgetItem*>* itemMap = nullptr;
    if (section == m_rosPackageSection) {
        itemMap = &m_rosPackageItems;
    }
    if (itemMap == nullptr) {
        return;
    }

    QTreeWidgetItem* item = itemMap->value(key, nullptr);
    if (item == nullptr) {
        item = new QTreeWidgetItem(section);
        item->setText(0, key);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);

        QFont keyFont = m_treeWidget->font();
        keyFont.setPointSize(10);
        keyFont.setWeight(QFont::DemiBold);
        item->setFont(0, keyFont);
        item->setForeground(0, QBrush(QColor("#4d5560")));

        QFont valueFont = m_treeWidget->font();
        valueFont.setPointSize(10);
        valueFont.setWeight(QFont::Normal);
        item->setFont(1, valueFont);
        item->setForeground(1, QBrush(QColor("#20262d")));
        (*itemMap)[key] = item;
    }

    item->setText(1, value);
    if (m_treeWidget != nullptr) {
        if (m_treeWidget->columnWidth(0) < 150) {
            m_treeWidget->setColumnWidth(0, 150);
        }
    }
}
