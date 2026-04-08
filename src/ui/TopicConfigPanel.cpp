#include "ui/TopicConfigPanel.h"

#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include "core/datasource/RosMsgPackageRegistry.h"
#include "core/datasource/TopicConfig.h"
#include "utils/Logger.h"

using autoviz::datasource::CompileStatus;
using autoviz::datasource::RosMsgPackageRegistry;
using autoviz::datasource::TopicConfig;
using autoviz::datasource::VisualizationRole;

namespace {
QComboBox* createRoleCombo(QWidget* parent)
{
    auto* comboBox = new QComboBox(parent);
    comboBox->addItem(QStringLiteral("全局路径"), static_cast<int>(VisualizationRole::GlobalPath));
    comboBox->addItem(QStringLiteral("局部路径"), static_cast<int>(VisualizationRole::LocalPath));
    comboBox->addItem(QStringLiteral("障碍物"), static_cast<int>(VisualizationRole::Obstacles));
    comboBox->addItem(QStringLiteral("自车状态"), static_cast<int>(VisualizationRole::Ego));
    comboBox->addItem(QStringLiteral("控制指令"), static_cast<int>(VisualizationRole::Control));
    return comboBox;
}

QComboBox* createMsgTypeCombo(QWidget* parent)
{
    auto* comboBox = new QComboBox(parent);
    return comboBox;
}
}  // namespace

TopicConfigPanel::TopicConfigPanel(RosMsgPackageRegistry* packageRegistry, QWidget* parent)
    : QWidget(parent)
    , m_packageRegistry(packageRegistry)
{
    setupUi();
    updateUiState();
}

QVector<TopicConfig> TopicConfigPanel::topicConfigs() const
{
    QVector<TopicConfig> configs;
    if (m_table == nullptr) {
        return configs;
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        configs.push_back(configFromRow(row));
    }
    return configs;
}

void TopicConfigPanel::refreshPackageState()
{
    refreshMsgTypeOptions();
    updateUiState();
}

void TopicConfigPanel::clearTopicConfigs()
{
    if (m_table != nullptr) {
        m_table->setRowCount(0);
    }
}

void TopicConfigPanel::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("话题与消息配置"), this);
    title->setStyleSheet("font-weight: 600;");
    layout->addWidget(title);

    m_packageSummaryLabel = new QLabel(tr("当前消息包：未加载"), this);
    m_packageSummaryLabel->setStyleSheet("color: #51606f;");
    layout->addWidget(m_packageSummaryLabel);

    m_hintLabel = new QLabel(tr("请先加载 ROS 消息包后再配置话题。"), this);
    m_hintLabel->setStyleSheet("padding: 8px; background: #1f2a35; border-radius: 4px; color: #cfd8e3;");
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(6);

    m_addTopicButton = new QPushButton(tr("添加话题"), this);
    m_removeButton = new QPushButton(tr("删除选中项"), this);

    buttonLayout->addWidget(m_addTopicButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addStretch(1);
    layout->addLayout(buttonLayout);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {tr("启用"), tr("话题名"), tr("消息类型"), tr("用途"), tr("编译状态")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

    connect(m_addTopicButton, &QPushButton::clicked, this, [this]() { addTopicRow(); });
    connect(m_removeButton, &QPushButton::clicked, this, [this]() { removeSelectedRows(); });
}

void TopicConfigPanel::addTopicRow()
{
    if (m_table == nullptr || m_packageRegistry == nullptr || !m_packageRegistry->hasLoadedPackage()) {
        return;
    }

    const int rowIndex = m_table->rowCount();
    m_table->insertRow(rowIndex);

    auto* enabledItem = new QTableWidgetItem();
    enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
    enabledItem->setCheckState(Qt::Checked);
    m_table->setItem(rowIndex, 0, enabledItem);

    auto* topicItem = new QTableWidgetItem();
    topicItem->setText(QStringLiteral("/"));
    m_table->setItem(rowIndex, 1, topicItem);

    m_table->setCellWidget(rowIndex, 2, createMsgTypeCombo(m_table));
    m_table->setCellWidget(rowIndex, 3, createRoleCombo(m_table));

    auto* compileItem = new QTableWidgetItem(autoviz::datasource::toDisplayString(CompileStatus::NotCompiled));
    compileItem->setFlags(compileItem->flags() & ~Qt::ItemIsEditable);
    m_table->setItem(rowIndex, 4, compileItem);

    refreshMsgTypeOptions();
    Logger::instance().info(QStringLiteral("已添加话题配置，第 %1 行。").arg(rowIndex + 1));
}

void TopicConfigPanel::removeSelectedRows()
{
    if (m_table == nullptr) {
        return;
    }

    const QModelIndexList selectedRows = m_table->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) {
        Logger::instance().warning("未选择需要删除的话题。");
        return;
    }

    for (int index = selectedRows.size() - 1; index >= 0; --index) {
        const int row = selectedRows.at(index).row();
        const QString topicName =
            m_table->item(row, 1) != nullptr ? m_table->item(row, 1)->text() : QString();
        m_table->removeRow(row);
        Logger::instance().info(QStringLiteral("已删除话题：%1").arg(topicName.isEmpty() ? QStringLiteral("未命名话题") : topicName));
    }
}

void TopicConfigPanel::refreshMsgTypeOptions()
{
    if (m_table == nullptr) {
        return;
    }

    QStringList msgTypeNames;
    if (m_packageRegistry != nullptr && m_packageRegistry->hasLoadedPackage()) {
        const auto* package = m_packageRegistry->currentPackage();
        if (package != nullptr) {
            for (const auto& definition : package->msgFiles) {
                msgTypeNames.push_back(definition.messageName);
            }
        }
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* msgTypeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(row, 2));
        if (msgTypeCombo != nullptr) {
            const QString currentText = msgTypeCombo->currentText();
            msgTypeCombo->clear();
            msgTypeCombo->addItem(QStringLiteral(""));
            for (const auto& msgTypeName : msgTypeNames) {
                msgTypeCombo->addItem(msgTypeName);
            }
            const int foundIndex = msgTypeCombo->findText(currentText);
            msgTypeCombo->setCurrentIndex(foundIndex >= 0 ? foundIndex : 0);
        }
    }
}

void TopicConfigPanel::updateUiState()
{
    if (m_table == nullptr || m_packageSummaryLabel == nullptr || m_hintLabel == nullptr) {
        return;
    }

    const bool hasPackage = m_packageRegistry != nullptr && m_packageRegistry->hasLoadedPackage();
    m_addTopicButton->setEnabled(hasPackage);
    m_removeButton->setEnabled(hasPackage);
    m_table->setEnabled(hasPackage);
    m_hintLabel->setVisible(!hasPackage);

    if (!hasPackage) {
        m_packageSummaryLabel->setText(tr("当前消息包：未加载"));
        return;
    }

    const auto* package = m_packageRegistry->currentPackage();
    if (package != nullptr) {
        m_packageSummaryLabel->setText(
            tr("当前消息包：%1，消息定义数：%2").arg(package->packageName).arg(package->msgCount()));
    }
}

TopicConfig TopicConfigPanel::configFromRow(int rowIndex) const
{
    TopicConfig config;
    if (m_table == nullptr || rowIndex < 0 || rowIndex >= m_table->rowCount()) {
        return config;
    }

    if (auto* enabledItem = m_table->item(rowIndex, 0); enabledItem != nullptr) {
        config.enabled = enabledItem->checkState() == Qt::Checked;
    }
    if (auto* topicItem = m_table->item(rowIndex, 1); topicItem != nullptr) {
        config.topicName = topicItem->text();
    }
    if (auto* msgTypeCombo = qobject_cast<QComboBox*>(m_table->cellWidget(rowIndex, 2)); msgTypeCombo != nullptr) {
        config.msgTypeName = msgTypeCombo->currentText();
    }
    if (m_packageRegistry != nullptr && m_packageRegistry->hasLoadedPackage()) {
        const auto* package = m_packageRegistry->currentPackage();
        if (package != nullptr) {
            for (const auto& definition : package->msgFiles) {
                if (definition.messageName == config.msgTypeName) {
                    config.msgFilePath = definition.filePath;
                    break;
                }
            }
        }
    }
    if (auto* roleCombo = qobject_cast<QComboBox*>(m_table->cellWidget(rowIndex, 3)); roleCombo != nullptr) {
        config.visualizationRole =
            static_cast<VisualizationRole>(roleCombo->currentData().toInt());
    }
    if (auto* statusItem = m_table->item(rowIndex, 4); statusItem != nullptr) {
        config.compileStatus = autoviz::datasource::compileStatusFromDisplayString(statusItem->text());
    }

    return config;
}
