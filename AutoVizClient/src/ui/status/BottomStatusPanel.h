#pragma once

#include <QHash>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

#include "ui/status/ControlStatusSummary.h"

class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QScrollArea;
class QTableWidget;
class QTabWidget;

namespace autoviz::datacenter {
struct VisualizationSnapshot;
}

class BottomStatusPanel : public QWidget
{
public:
    explicit BottomStatusPanel(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void appendLog(const QString& message);
    void updateSnapshot(const autoviz::datacenter::VisualizationSnapshot& snapshot);

private:
    void setupUi();
    void setupOverviewTab();
    void setupLogTab();
    void setupDetailsTab();
    void setupTopicTab();
    void setupStateTabs();
    void setupControlTimelineTab();
    QGroupBox* createOverviewGroup(QWidget* parent, const QString& title, const QVector<QPair<QString, QString>>& fields);
    QWidget* createDetailTab(const QString& title, const QVector<QPair<QString, QVector<QPair<QString, QString>>>>& groups, int columns = 2);
    QGroupBox* createDetailGroup(QWidget* parent, const QString& title, const QVector<QPair<QString, QString>>& fields);
    void setOverviewValue(const QString& key, const QString& value);
    void setDetailValue(const QString& key, const QString& value);
    void updateOverview(const autoviz::datacenter::VisualizationSnapshot& snapshot,
                        const autoviz::ui::status::ControlStatusSummary& controlSummary);
    void updateTopicTable(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void updateStateTabs(const autoviz::datacenter::VisualizationSnapshot& snapshot);
    void updateControlTimeline(const autoviz::datacenter::VisualizationSnapshot& snapshot,
                               const autoviz::ui::status::ControlStatusSummary& controlSummary);

    QTabWidget* m_tabs = nullptr;
    QTabWidget* m_detailTabs = nullptr;
    QScrollArea* m_overviewScrollArea = nullptr;
    QWidget* m_overviewContent = nullptr;
    QGroupBox* m_underwaterOverviewGroup = nullptr;
    QGroupBox* m_platformOverviewGroup = nullptr;
    QWidget* m_verticalDetailTab = nullptr;
    QPlainTextEdit* m_logOutput = nullptr;
    QHash<QString, QLabel*> m_overviewValues;
    QHash<QString, QLabel*> m_detailValues;
    QTableWidget* m_topicTable = nullptr;
    QTableWidget* m_controlAssociationTable = nullptr;
    QTableWidget* m_controlEventTable = nullptr;
    QLabel* m_topicSummaryLabel = nullptr;
};
