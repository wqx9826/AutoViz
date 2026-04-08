#pragma once

#include <QVector>
#include <QWidget>

#include "core/datasource/TopicConfig.h"

namespace autoviz::datasource {
class RosMsgPackageRegistry;
}

class QLabel;
class QPushButton;
class QTableWidget;

// TopicConfigPanel is the user entry point for manual topic binding.
// It is intentionally dependent on the currently loaded ROS message package:
// topic names are entered manually by the user, while message types come from
// the package msg/ directory scan result.
class TopicConfigPanel : public QWidget
{
public:
    explicit TopicConfigPanel(autoviz::datasource::RosMsgPackageRegistry* packageRegistry, QWidget* parent = nullptr);

    QVector<autoviz::datasource::TopicConfig> topicConfigs() const;
    void refreshPackageState();
    void clearTopicConfigs();

private:
    void setupUi();
    void addTopicRow();
    void removeSelectedRows();
    void refreshMsgTypeOptions();
    void updateUiState();
    autoviz::datasource::TopicConfig configFromRow(int rowIndex) const;

    autoviz::datasource::RosMsgPackageRegistry* m_packageRegistry = nullptr;
    QLabel* m_packageSummaryLabel = nullptr;
    QLabel* m_hintLabel = nullptr;
    QPushButton* m_addTopicButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QTableWidget* m_table = nullptr;
};
