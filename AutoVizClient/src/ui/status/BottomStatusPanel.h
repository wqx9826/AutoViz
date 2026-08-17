#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QPair>
#include <QQueue>
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
    struct CommandTransitionDisplay {
        bool hasMode = false;
        int mode = 0;
        bool hasGear = false;
        int gear = 0;
        bool hasEnabled = false;
        bool enabled = false;
    };

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
    autoviz::ui::status::ControlStatusSummary controlSummaryForDisplay(
        const autoviz::datacenter::VisualizationSnapshot& snapshot,
        const autoviz::ui::status::ControlStatusSummary& currentSummary);
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
    QQueue<CommandTransitionDisplay> m_pendingCommandTransitions;
    CommandTransitionDisplay m_activeCommandTransition;
    QElapsedTimer m_commandTransitionTimer;
    quint64 m_lastControlSnapshotSequence = 0;
    int m_seenControlEventCount = 0;
    int m_lastControlInputSource = -1;
    QString m_lastControlSessionId;
    bool m_controlEventCursorInitialized = false;
    bool m_commandTransitionActive = false;
};
